// ai_strat_puct.c
// See ai_strat_puct.h for the full design rationale.

#include <stddef.h>

#include "ai_strat_puct.h"
#include "ai_strat_ismcts_search.h"
#include "ai_strat_puct_net.h"
#include "ai_strat_playout.h"
#include "../actions/move_apply.h"

// Both start at the plain, safe baseline -- ISMCTS_DEFAULTS (A10's own
// 200000-node arena; A14 bypasses nn_value_trust/nn_value_use_mover_seat
// entirely, see ai_strat_puct.h, so their value here never matters) and
// PUCT_DEFAULTS (use_puct=true is already safe on its own -- see
// decide_and_apply()'s own guard below). puct_load_weights() promotes
// g_ismcts_params[] to this agent's real, smaller-arena default on
// success, same "a successful load is what actually turns real play into
// this agent rather than a degraded fallback" pattern A11's
// ismctsnn_load_weights() already established.
static ISMCTSParams g_ismcts_params[2] = { ISMCTS_DEFAULTS, ISMCTS_DEFAULTS };
static PUCTParams g_puct_params[2] = { PUCT_DEFAULTS, PUCT_DEFAULTS };

ISMCTSParams puct_get_default_ismcts_params(void)
{ ISMCTSParams defaults = ISMCTS_DEFAULTS;
  defaults.limit_max_nodes = defaults.limit_iterations + 8; // see ai_strat_puct.h
  return defaults;
} // puct_get_default_ismcts_params

void puct_set_ismcts_params(PlayerID player, const ISMCTSParams* params)
{ g_ismcts_params[player] = *params;
} // puct_set_ismcts_params

void puct_reset_ismcts_params(void)
{ ISMCTSParams defaults = puct_get_default_ismcts_params();
  g_ismcts_params[PLAYER_A] = defaults;
  g_ismcts_params[PLAYER_B] = defaults;
} // puct_reset_ismcts_params

PUCTParams puct_get_default_params(void)
{ PUCTParams defaults = PUCT_DEFAULTS;
  return defaults;
} // puct_get_default_params

void puct_set_params(PlayerID player, const PUCTParams* params)
{ g_puct_params[player] = *params;
} // puct_set_params

void puct_reset_params(void)
{ PUCTParams defaults = PUCT_DEFAULTS;
  g_puct_params[PLAYER_A] = defaults;
  g_puct_params[PLAYER_B] = defaults;
} // puct_reset_params

bool puct_load_weights(const char* path)
{ bool ok = puctnet_load(path);
  if(ok)
  { puct_reset_ismcts_params(); // promote g_ismcts_params[] out of the
    puct_reset_params(); // safe ISMCTS_DEFAULTS/PUCT_DEFAULTS baseline
  }
  return ok;
} // puct_load_weights

// Same forked-RNG-stream decision seeding as A10/A11 -- duplicated rather
// than exported/shared, matching this project's manual-code-over-macro-
// magic convention (see ai_strat_puct.h).
static GameContext fork_for_decision(GameContext* ctx)
{ uint32_t seed = genRandLong(&ctx->rng);
  return mc_fork_context(ctx, seed);
} // fork_for_decision

// Same rollout/advance policy as A10/A11 (A5 Heuristic on both seats) --
// this agent's own leaf evaluation (puct_leaf_value(), ai_strat_puct_search.c)
// never calls mc_playout_from(), but mc_advance_to_decision() still
// dispatches to this StrategySet's mulligan/discard-to-7 hooks mid-descent
// regardless (ai_strat_playout.h's own contract), so a real StrategySet is
// still required here.
static StrategySet heuristic_rollout_strategy_set(void)
{ StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_HEURISTIC);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_HEURISTIC);
  return strats;
} // heuristic_rollout_strategy_set

static void decide_and_apply(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ ISMCTSParams ismcts_params = g_ismcts_params[player];
  const PUCTParams* puct_params = &g_puct_params[player];

  if(!puctnet_is_loaded())
  { ismcts_params.nn_value_trust = 0.0f; // degrade to plain A10 -- see
    puct_params = NULL; // ai_strat_puct.h's puct_load_weights() comment
  }

  GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = heuristic_rollout_strategy_set();

  GameMove move = ismcts_search_best_move(gstate, player, &sim_ctx, &ismcts_params,
                                          &rollout_strats, puct_params);
  apply_move(gstate, player, &move, ctx);
} // decide_and_apply

void puct_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, gstate->current_player, ctx);
} // puct_attack_strategy

void puct_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, 1 - gstate->current_player, ctx);
} // puct_defense_strategy
