// ai_strat_simplemc1.c
// A8 Simple Monte Carlo ("The Soothsayer") -- see ai_strat_simplemc1.h.

#include "ai_strat_simplemc1.h"
#include "ai_strat_simplemc_search.h"
#include "ai_strat_playout.h"
#include "../actions/move_apply.h"
#include "../core/game_constants.h"

static SimpleMcParams g_params[2] = { SIMPLEMC_DEFAULTS, SIMPLEMC_DEFAULTS };

SimpleMcParams simplemc_get_default_params(void)
{ SimpleMcParams defaults = SIMPLEMC_DEFAULTS;
  return defaults;
} // simplemc_get_default_params

void simplemc_set_params(PlayerID player, const SimpleMcParams* params)
{ g_params[player] = *params;
} // simplemc_set_params

void simplemc_reset_params(void)
{ SimpleMcParams defaults = SIMPLEMC_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // simplemc_reset_params

// Draws exactly one value from the *live* context to seed a forked stream
// -- ai_strat_playout.h's contract: the live GameContext advances by
// exactly this one draw per decision, and nothing else in the search
// touches it. The move this agent finally picks is still applied to the
// real game with the live `ctx` itself (see decide_and_apply()), not this
// fork -- only the search's internal rollouts run through the fork.
static GameContext fork_for_decision(GameContext* ctx)
{ uint32_t seed = genRandLong(&ctx->rng);
  return mc_fork_context(ctx, seed);
} // fork_for_decision

// A8's own rollout policy: uniformly random on both seats. See
// ai_strat_simplemc_search.h's header comment -- A12 (ideas/A12 ai agent
// clairvoyant/) reuses the same search with a different StrategySet here.
static StrategySet random_rollout_strategy_set(void)
{ StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_RANDOM);
  return strats;
} // random_rollout_strategy_set

static void decide_and_apply(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const SimpleMcParams* params = &g_params[player];
  GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = random_rollout_strategy_set();

  GameMove move = mc_search_best_move(gstate, player, &sim_ctx, params, &rollout_strats);
  apply_move(gstate, player, &move, ctx);
} // decide_and_apply

void simplemc_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, gstate->current_player, ctx);
} // simplemc_attack_strategy

void simplemc_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, 1 - gstate->current_player, ctx);
} // simplemc_defense_strategy
