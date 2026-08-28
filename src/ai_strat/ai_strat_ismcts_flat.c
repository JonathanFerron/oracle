// ai_strat_ismcts_flat.c
// A10 IS-MCTS's flat-rollout mulligan/discard-to-7 scoring -- see
// ai_strat_ismcts_flat.h.

#include "ai_strat_ismcts_flat.h"
#include "ai_strat_ismcts1.h"
#include "ai_strat_playout.h"
#include "ai_strat_lib_heuristics.h"
#include "../core/card_actions.h"
#include "../core/game_state.h"

#define MAX_FLAT_CANDIDATES 36 // C(9,2) -- the largest case (discard-to-7 at hand size 9)

typedef struct
{ uint8_t count; // 0, 1, or 2
  uint8_t cards[2];
} FlatCandidate;

static GameContext fork_for_decision(GameContext* ctx)
{ uint32_t seed = genRandLong(&ctx->rng);
  return mc_fork_context(ctx, seed);
} // fork_for_decision

// A5 Heuristic on both seats, matching ai_strat_ismcts1.c's
// heuristic_rollout_strategy_set() -- same Phase 6 finding applies here:
// mulligan/discard-to-7 candidates are scored by rollout too, so a biased
// (random) rollout policy would misjudge them the same way it misjudged
// attack/defense before the fix.
static StrategySet heuristic_rollout_strategy_set(void)
{ StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_HEURISTIC);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_HEURISTIC);
  return strats;
} // heuristic_rollout_strategy_set

static uint8_t enumerate_singles(const uint8_t* cards, uint8_t n, FlatCandidate* out)
{ for(uint8_t i = 0; i < n; i++)
  { out[i].count = 1;
    out[i].cards[0] = cards[i];
  }
  return n;
} // enumerate_singles

static uint8_t enumerate_pairs(const uint8_t* cards, uint8_t n, FlatCandidate* out)
{ uint8_t count = 0;
  for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = (uint8_t)(i + 1); j < n; j++)
    { out[count].count = 2;
      out[count].cards[0] = cards[i];
      out[count].cards[1] = cards[j];
      count++;
    }
  return count;
} // enumerate_pairs

// Every 0/1/2-card toss subset of `hand` -- 1 + n + C(n,2) candidates (22 at
// the mulligan's fixed 6-card hand). Unfiltered -- unlike
// strat_lib_mulligan()'s below-AVERAGE_POWER_FOR_MULLIGAN heuristic, every
// subset is genuinely searched.
static uint8_t enumerate_mulligan_candidates(const Hand* hand, FlatCandidate* out)
{ out[0].count = 0;
  uint8_t n = 1;
  n = (uint8_t)(n + enumerate_singles(hand->cards, hand->size, &out[n]));
  n = (uint8_t)(n + enumerate_pairs(hand->cards, hand->size, &out[n]));
  return n;
} // enumerate_mulligan_candidates

// Every exactly-`discard_count`-card subset of `hand` (discard_count is 1 or
// 2 -- ismcts_discard_to_7() falls back to the shared heuristic otherwise).
static uint8_t enumerate_discard_candidates(const Hand* hand, uint8_t discard_count,
                                            FlatCandidate* out)
{ if(discard_count == 1) return enumerate_singles(hand->cards, hand->size, out);
  return enumerate_pairs(hand->cards, hand->size, out);
} // enumerate_discard_candidates

static void apply_toss(struct gamestate* sim, PlayerID player, const FlatCandidate* c)
{ for(uint8_t i = 0; i < c->count; i++)
  { Hand_remove(&sim->hand[player], c->cards[i]);
    Discard_add(&sim->discard[player], c->cards[i]);
  }
} // apply_toss

// One candidate's average outcome over `rollouts` determinized playouts.
// Mulligan candidates redraw `count` replacements before playing out from
// turn 1; discard-to-7 candidates instead mirror end_of_turn()'s own
// change_current_player() (the caller already ran collect_1_luna() before
// dispatching to discard_strategy[]) before playing out from the next turn.
static float score_candidate(const struct gamestate* root, PlayerID player,
                             const FlatCandidate* c, bool is_mulligan,
                             GameContext* sim_ctx, const StrategySet* rollout_strats,
                             uint32_t rollouts, uint16_t max_turns)
{ float total = 0.0f;

  for(uint32_t i = 0; i < rollouts; i++)
  { struct gamestate sim = *root;
    apply_toss(&sim, player, c);

    if(is_mulligan)
    { for(uint8_t d = 0; d < c->count; d++) draw_1_card(&sim, player, sim_ctx);
    }
    else
      change_current_player(&sim);

    total += mc_playout_from_turn_boundary(&sim, player, rollout_strats, sim_ctx, max_turns);
  }
  return total / (float)rollouts;
} // score_candidate

static FlatCandidate choose_best_candidate(const struct gamestate* gstate, PlayerID player,
                                           bool is_mulligan, uint8_t discard_count,
                                           GameContext* sim_ctx, const StrategySet* rollout_strats,
                                           const ISMCTSParams* params)
{ FlatCandidate candidates[MAX_FLAT_CANDIDATES];
  uint8_t n = is_mulligan
              ? enumerate_mulligan_candidates(&gstate->hand[player], candidates)
              : enumerate_discard_candidates(&gstate->hand[player], discard_count, candidates);

  uint32_t rollouts_per_candidate = params->limit_flat_iterations / n;
  if(rollouts_per_candidate < 1) rollouts_per_candidate = 1;

  uint8_t best = 0;
  float best_score = -1.0f;
  for(uint8_t i = 0; i < n; i++)
  { float score = score_candidate(gstate, player, &candidates[i], is_mulligan, sim_ctx,
                                  rollout_strats, rollouts_per_candidate, params->rollout_max_turns);
    if(score > best_score)
    { best_score = score;
      best = i;
    }
  }
  return candidates[best];
} // choose_best_candidate

void ismcts_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const ISMCTSParams* params = ismcts_live_params(player);
  GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = heuristic_rollout_strategy_set();

  FlatCandidate chosen = choose_best_candidate(gstate, player, true, 0, &sim_ctx,
                                               &rollout_strats, params);
  apply_toss(gstate, player, &chosen);
  for(uint8_t d = 0; d < chosen.count; d++) draw_1_card(gstate, player, ctx);
} // ismcts_mulligan

void ismcts_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ uint8_t discard_count = (uint8_t)(gstate->hand[player].size - 7);
  if(discard_count > 2) // defensive -- enumerate_discard_candidates only handles 1/2
  { strat_lib_discard_to_7(gstate, player, ctx);
    return;
  }

  const ISMCTSParams* params = ismcts_live_params(player);
  GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = heuristic_rollout_strategy_set();

  FlatCandidate chosen = choose_best_candidate(gstate, player, false, discard_count, &sim_ctx,
                                               &rollout_strats, params);
  apply_toss(gstate, player, &chosen);
} // ismcts_discard_to_7
