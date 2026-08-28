// ai_strat_ismcts1.c
// A10 IS-MCTS ("The Omniscient") -- see ai_strat_ismcts1.h.

#include "ai_strat_ismcts1.h"
#include "ai_strat_ismcts_search.h"
#include "ai_strat_playout.h"
#include "../actions/move_apply.h"
#include "../core/game_constants.h"

static ISMCTSParams g_params[2] = { ISMCTS_DEFAULTS, ISMCTS_DEFAULTS };

ISMCTSParams ismcts_get_default_params(void)
{ ISMCTSParams defaults = ISMCTS_DEFAULTS;
  return defaults;
} // ismcts_get_default_params

void ismcts_set_params(PlayerID player, const ISMCTSParams* params)
{ g_params[player] = *params;
} // ismcts_set_params

void ismcts_reset_params(void)
{ ISMCTSParams defaults = ISMCTS_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // ismcts_reset_params

const ISMCTSParams* ismcts_live_params(PlayerID player)
{ return &g_params[player];
} // ismcts_live_params

// Draws exactly one value from the *live* context to seed a forked stream --
// ai_strat_playout.h's contract: the live GameContext advances by exactly
// this one draw per decision, and every iteration of the search runs
// through the fork instead. The move this agent finally picks is still
// applied to the real game with the live `ctx` itself (see
// decide_and_apply()), never this fork.
static GameContext fork_for_decision(GameContext* ctx)
{ uint32_t seed = genRandLong(&ctx->rng);
  return mc_fork_context(ctx, seed);
} // fork_for_decision

// This agent's rollout/advance policy: A5 Heuristic on both seats (Phase 6,
// 2026-08-27) -- NOT uniformly random. A controlled diagnostic found this
// agent's win rate vs Borealis plateaued at 46-48% (below the anchor) with a
// random rollout policy, unmoved by a 64x increase in search budget --
// the same "more search can't fix a biased estimator" signature A8's own
// diagnosis found. Swapping the rollout policy to AI_STRATEGY_HEURISTIC
// (with the A5/A7 PASS-dominance defense fix applied -- see
// project_a5_a7_defense_pass_dominance) took the same 16k-iteration
// measurement to 63.0%, a ~15-point jump; see about.md and
// doc/changelog.md's 2026-08-27 entry for the full diagnostic. This
// supersedes about.md's original "deliberately out of scope: hand-written
// heuristics as primary evaluator" framing for the *rollout* policy
// specifically -- the tree's own UCT selection/backprop is still this
// agent's primary evaluator, matching the ISMCTS literature's own common
// finding that a purely random rollout policy is domain-dependent and, for
// Oracle specifically, not strong enough on its own (see about.md).
static StrategySet heuristic_rollout_strategy_set(void)
{ StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_HEURISTIC);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_HEURISTIC);
  return strats;
} // heuristic_rollout_strategy_set

static void decide_and_apply(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const ISMCTSParams* params = &g_params[player];
  GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = heuristic_rollout_strategy_set();

  GameMove move = ismcts_search_best_move(gstate, player, &sim_ctx, params, &rollout_strats);
  apply_move(gstate, player, &move, ctx);
} // decide_and_apply

void ismcts_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, gstate->current_player, ctx);
} // ismcts_attack_strategy

void ismcts_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, 1 - gstate->current_player, ctx);
} // ismcts_defense_strategy
