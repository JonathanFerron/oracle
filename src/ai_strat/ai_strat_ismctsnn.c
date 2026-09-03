// ai_strat_ismctsnn.c
// See ai_strat_ismctsnn.h for the full design rationale.

#include "ai_strat_ismctsnn.h"
#include "ai_strat_ismcts_search.h"
#include "ai_strat_ismctsnn_net.h"
#include "ai_strat_playout.h"
#include "../actions/move_apply.h"

static ISMCTSParams g_params[2] = { ISMCTS_DEFAULTS, ISMCTS_DEFAULTS };

ISMCTSParams ismctsnn_get_default_params(void)
{ ISMCTSParams defaults = ISMCTS_DEFAULTS;
  defaults.nn_value_trust = 1.0f;
  return defaults;
} // ismctsnn_get_default_params

void ismctsnn_set_params(PlayerID player, const ISMCTSParams* params)
{ g_params[player] = *params;
} // ismctsnn_set_params

// Resets to *this agent's own* default (nn_value_trust=1.0f), not A10's --
// each agent's reset means "back to my own shipped default", consistent
// with ismcts_reset_params() resetting A10 to ISMCTS_DEFAULTS (A10's
// default). Safe to call before weights are loaded: decide_and_apply()'s
// own guard still forces nn_value_trust=0.0f for any decision made while
// unloaded, regardless of what g_params[] holds.
void ismctsnn_reset_params(void)
{ ISMCTSParams defaults = ismctsnn_get_default_params();
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // ismctsnn_reset_params

bool ismctsnn_load_weights(const char* path)
{ bool ok = ismctsnn_net_load(path);
  if(ok)
    ismctsnn_reset_params(); // promote g_params[] out of the safe ISMCTS_DEFAULTS baseline
  return ok;
} // ismctsnn_load_weights

// Same forked-RNG-stream decision seeding as A10 (ai_strat_ismcts1.c) --
// duplicated rather than exported/shared, matching this project's
// manual-code-over-macro-magic convention (see ai_strat_ismctsnn.h).
static GameContext fork_for_decision(GameContext* ctx)
{ uint32_t seed = genRandLong(&ctx->rng);
  return mc_fork_context(ctx, seed);
} // fork_for_decision

// Same rollout/advance policy as A10 (A5 Heuristic on both seats) -- this
// agent changes what evaluates a leaf, not the tree's own descent/rollout
// machinery (about.md).
static StrategySet heuristic_rollout_strategy_set(void)
{ StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_HEURISTIC);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_HEURISTIC);
  return strats;
} // heuristic_rollout_strategy_set

static void decide_and_apply(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ ISMCTSParams params = g_params[player];
  if(!ismctsnn_net_is_loaded())
    params.nn_value_trust = 0.0f; // never worse than plain A10 if weights aren't loaded

  GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = heuristic_rollout_strategy_set();

  GameMove move = ismcts_search_best_move(gstate, player, &sim_ctx, &params, &rollout_strats);
  apply_move(gstate, player, &move, ctx);
} // decide_and_apply

void ismctsnn_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, gstate->current_player, ctx);
} // ismctsnn_attack_strategy

void ismctsnn_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, 1 - gstate->current_player, ctx);
} // ismctsnn_defense_strategy
