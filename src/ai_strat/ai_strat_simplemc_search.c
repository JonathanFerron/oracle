// ai_strat_simplemc_search.c
// A8 Simple Monte Carlo's progressive-pruning search -- see
// ai_strat_simplemc_search.h.

#include <math.h>
#include <stdio.h>

#include "ai_strat_simplemc_search.h"
#include "ai_strat_playout.h"
#include "../actions/move_gen.h"
#include "../core/game_constants.h"
#include "../util/debug.h"

#if DEBUG_ENABLED
static const char* const MOVE_TYPE_NAMES[] = { "PASS", "CHAMPIONS", "DRAW", "RECALL", "CASH" };
#endif

typedef struct
{ GameMove move;
  float    total_score; // sum of mc_playout() outcomes (0.0/0.5/1.0)
  uint32_t sims;
  bool     alive;
} McCandidate;

static uint8_t count_alive(const McCandidate* candidates, uint8_t n)
{ uint8_t c = 0;
  for(uint8_t i = 0; i < n; i++)
    if(candidates[i].alive) c++;
  return c;
} // count_alive

// One rollout: a fresh determinized clone of gstate (from player's point of
// view), then mc_playout() with `move` as the first action.
static float run_one_simulation(const struct gamestate* gstate, PlayerID player,
                                const GameMove* move, GameContext* sim_ctx,
                                const SimpleMcParams* params,
                                const StrategySet* rollout_strats)
{ struct gamestate det = *gstate;
  if(params->rollout_determinize)
    mc_determinize(&det, player, sim_ctx);
  return mc_playout(&det, player, move, rollout_strats, sim_ctx, params->rollout_max_turns);
} // run_one_simulation

static void mc_run_round(McCandidate* candidates, uint8_t n, uint16_t sims_to_add,
                         const struct gamestate* gstate, PlayerID player,
                         GameContext* sim_ctx, const SimpleMcParams* params,
                         const StrategySet* rollout_strats)
{ for(uint8_t i = 0; i < n; i++)
  { if(!candidates[i].alive) continue;

    for(uint16_t s = 0; s < sims_to_add; s++)
      candidates[i].total_score += run_one_simulation(gstate, player, &candidates[i].move,
                                                      sim_ctx, params, rollout_strats);
    candidates[i].sims += sims_to_add;
  }
} // mc_run_round

// The stub's seed-round idea: after a small first round, drop every 0-win
// candidate outright. Never empties the pool -- if every candidate still
// has 0 wins (small-sample noise, or a genuinely hopeless position), keep
// them all rather than leave mc_select_best_move() nothing to choose from.
static void mc_prune_zero_win(McCandidate* candidates, uint8_t n)
{ bool any_winner = false;
  for(uint8_t i = 0; i < n; i++)
    if(candidates[i].alive && candidates[i].total_score > 0.0f) any_winner = true;
  if(!any_winner) return;

  for(uint8_t i = 0; i < n; i++)
    if(candidates[i].alive && candidates[i].total_score <= 0.0f)
      candidates[i].alive = false;
} // mc_prune_zero_win

// Drops any candidate whose confidence-interval upper bound falls below the
// current leader's (highest win-rate candidate's) lower bound -- normal
// approximation to the binomial, per the stub's own second idea. The
// leader itself is never pruned, so this can never empty the pool.
static void mc_prune_by_ci(McCandidate* candidates, uint8_t n, float z)
{ int8_t leader = -1;
  float leader_p = -1.0f;

  for(uint8_t i = 0; i < n; i++)
  { if(!candidates[i].alive) continue;
    float p = candidates[i].total_score / (float)candidates[i].sims;
    if(p > leader_p)
    { leader_p = p;
      leader = (int8_t)i;
    }
  }
  if(leader < 0) return;

  float leader_se = sqrtf(leader_p * (1.0f - leader_p) / (float)candidates[leader].sims);
  float leader_lower = leader_p - z * leader_se;

  for(uint8_t i = 0; i < n; i++)
  { if(!candidates[i].alive || i == (uint8_t)leader) continue;
    float p = candidates[i].total_score / (float)candidates[i].sims;
    float se = sqrtf(p * (1.0f - p) / (float)candidates[i].sims);
    if(p + z * se < leader_lower) candidates[i].alive = false;
  }
} // mc_prune_by_ci

static uint8_t stage_keep_count(uint8_t n_original, float ratio, uint8_t hard_cap)
{ float raw = ceilf(powf((float)n_original, ratio));
  uint8_t capped = (raw < (float)hard_cap) ? (uint8_t)raw : hard_cap;
  return capped < 1 ? 1 : capped;
} // stage_keep_count

// Truncates the alive set to at most `keep_count`, by win rate descending,
// first-enumerated wins ties (a stable selection sort over the alive
// indices -- at most MOVE_GEN_MAX_MOVES of them, so O(n^2) is cheap).
static void mc_apply_stage_cap(McCandidate* candidates, uint8_t n, uint8_t keep_count)
{ uint8_t order[MOVE_GEN_MAX_MOVES];
  uint8_t order_n = 0;
  for(uint8_t i = 0; i < n; i++)
    if(candidates[i].alive) order[order_n++] = i;
  if(order_n <= keep_count) return;

  for(uint8_t a = 0; a < order_n; a++)
  { uint8_t best = a;
    float best_p = candidates[order[a]].total_score / (float)candidates[order[a]].sims;
    for(uint8_t b = (uint8_t)(a + 1); b < order_n; b++)
    { float p = candidates[order[b]].total_score / (float)candidates[order[b]].sims;
      if(p > best_p)
      { best = b;
        best_p = p;
      }
    }
    uint8_t tmp = order[a];
    order[a] = order[best];
    order[best] = tmp;
  }

  for(uint8_t a = keep_count; a < order_n; a++)
    candidates[order[a]].alive = false;
} // mc_apply_stage_cap

static void apply_stage_cap_if_due(McCandidate* candidates, uint8_t n, uint16_t cumulative,
                                   uint16_t stage_threshold, float ratio, uint8_t hard_cap,
                                   bool* done)
{ if(*done || cumulative < stage_threshold) return;
  mc_apply_stage_cap(candidates, n, stage_keep_count(n, ratio, hard_cap));
  *done = true;
} // apply_stage_cap_if_due

static void run_progressive_rounds(McCandidate* candidates, uint8_t n,
                                   const struct gamestate* gstate, PlayerID player,
                                   GameContext* sim_ctx, const SimpleMcParams* params,
                                   const StrategySet* rollout_strats,
                                   uint32_t* total_rollouts, uint16_t cumulative)
{ bool stage1_done = false, stage2_done = false, stage3_done = false;

  while(count_alive(candidates, n) > 1 &&
        cumulative < params->limit_max_simulations &&
        *total_rollouts < params->limit_total_rollouts)
  { uint8_t alive_count = count_alive(candidates, n);
    mc_run_round(candidates, n, params->rollout_round_simulations, gstate, player,
                 sim_ctx, params, rollout_strats);
    *total_rollouts += (uint32_t)alive_count * params->rollout_round_simulations;
    cumulative += params->rollout_round_simulations;

    mc_prune_by_ci(candidates, n, params->threshold_confidence_level);
    apply_stage_cap_if_due(candidates, n, cumulative, params->limit_stage1_simulations,
                           params->threshold_stage1_keep_ratio, params->limit_stage1_keep,
                           &stage1_done);
    apply_stage_cap_if_due(candidates, n, cumulative, params->limit_stage2_simulations,
                           params->threshold_stage2_keep_ratio, params->limit_stage2_keep,
                           &stage2_done);
    apply_stage_cap_if_due(candidates, n, cumulative, params->limit_stage3_simulations,
                           params->threshold_stage3_keep_ratio, params->limit_stage3_keep,
                           &stage3_done);
  }
} // run_progressive_rounds

// Deterministic argmax over alive candidates -- strictly greater only, so
// the first-enumerated candidate wins ties, matching
// ai_strat_heuristic.c's own convention.
static GameMove mc_select_best_move(const McCandidate* candidates, uint8_t n)
{ uint8_t best = 0;
  float best_p = -1.0f;
  uint8_t alive_at_end = 0;

  for(uint8_t i = 0; i < n; i++)
  { if(!candidates[i].alive) continue;
    alive_at_end++;
    float p = candidates[i].total_score / (float)candidates[i].sims;
    if(p > best_p)
    { best_p = p;
      best = i;
    }
  }

  DEBUG_PRINT(" SimpleMC: chose %s (p=%.3f, sims=%u) among %u survivor(s)\n",
              MOVE_TYPE_NAMES[candidates[best].move.type], (double)best_p,
              candidates[best].sims, alive_at_end);

  return candidates[best].move;
} // mc_select_best_move

GameMove mc_search_best_move(const struct gamestate* gstate, PlayerID player,
                             GameContext* sim_ctx, const SimpleMcParams* params,
                             const StrategySet* rollout_strats)
{ MoveGenLimits limits =
  { .max_recall_variants = params->limit_recall_variants,
    .max_cash_variants = params->limit_cash_variants
  };
  GameMove moves[MOVE_GEN_MAX_MOVES];
  uint8_t max_out = (uint8_t)oraclemin(MOVE_GEN_MAX_MOVES, params->limit_max_candidates);
  uint8_t n = get_available_moves(gstate, player, &limits, moves, max_out);
  DEBUG_PRINT(" SimpleMC: turn %u, player %u, %u phase, energy A=%u B=%u, "
              "%u candidate move(s) enumerated\n",
              gstate->turn, player, gstate->turn_phase,
              gstate->current_energy[PLAYER_A], gstate->current_energy[PLAYER_B], n);
  if(n <= 1) return moves[0]; // only MOVE_PASS was legal -- nothing to search

  McCandidate candidates[MOVE_GEN_MAX_MOVES];
  for(uint8_t i = 0; i < n; i++)
    candidates[i] = (McCandidate)
  { .move = moves[i], .total_score = 0.0f, .sims = 0, .alive = true
  };

  uint32_t total_rollouts = (uint32_t)n * params->rollout_seed_simulations;
  mc_run_round(candidates, n, params->rollout_seed_simulations, gstate, player, sim_ctx,
               params, rollout_strats);
  if(params->prune_zero_win_seed) mc_prune_zero_win(candidates, n);

  run_progressive_rounds(candidates, n, gstate, player, sim_ctx, params, rollout_strats,
                         &total_rollouts, params->rollout_seed_simulations);

  return mc_select_best_move(candidates, n);
} // mc_search_best_move
