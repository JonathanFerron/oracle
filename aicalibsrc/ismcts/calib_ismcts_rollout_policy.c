// calib_ismcts_rollout_policy.c
// Diagnostic harness for A10 IS-MCTS's Phase 6 Step 1 finding: win rate vs
// Borealis rises from 1k to ~4k-16k iterations, then plateaus (46-48%,
// still below Borealis) all the way out to 64k -- the same "more search
// can't fix it" signature A8's own diagnosis found, traced there to
// mc_playout()'s rollout policy always modeling both seats as
// AI_STRATEGY_RANDOM regardless of the real opponent. This harness tests
// whether the same cause explains A10's plateau, WITHOUT touching src/ at
// all: it calls ismcts_search_best_move() directly with a swappable rollout
// StrategySet, bypassing ai_strat_ismcts1.c's decide_and_apply() (which
// hardcodes AI_STRATEGY_RANDOM for both rollout seats -- see
// random_rollout_strategy_set() there). If a heuristic rollout policy
// measurably improves the win rate at a fixed iteration budget, that
// confirms the hypothesis; if it doesn't, the plateau has some other cause.
//
// Known confound, reported alongside every result rather than hidden: the
// AI_STRATEGY_HEURISTIC (A5) rollout option shares a live defect with A7 --
// PASS dominates every block under their shipped defense weights (see
// ideas/A9 .../about.md and the project's own defense-formula note) -- so a
// heuristic-rollout win invalidates the "just needs any real policy"
// framing only partially; a heuristic-rollout non-improvement could still
// be explained by that specific bug rather than the hypothesis being wrong.
//
// Usage:
//   calib_ismcts_rollout_policy <rollout_policy> <limit_iterations> <numgames> <seed> <opponent>
//   rollout_policy: rand | heuristic | balanced | borealis | combo
//
// Prints a CSV line: rollout_policy, limit_iterations, numgames, seed,
// opponent, wins_a, wins_b, draws.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_ismcts1.h"
#include "../../src/ai_strat/ai_strat_ismcts_search.h"
#include "../../src/ai_strat/ai_strat_playout.h"
#include "../../src/ai_strat/ai_strat_lib_heuristics.h"
#include "../../src/actions/move_apply.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"
#include "../../src/util/mtwister.h"

static AIStrategyType g_rollout_policy = AI_STRATEGY_RANDOM;
static ISMCTSParams g_test_params;

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_ismcts_rollout_policy: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static GameContext fork_for_decision(GameContext* ctx)
{ uint32_t seed = genRandLong(&ctx->rng);
  return mc_fork_context(ctx, seed);
} // fork_for_decision

static StrategySet build_rollout_strategy_set(void)
{ StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, g_rollout_policy);
  set_player_strategy_by_type(&strats, PLAYER_B, g_rollout_policy);
  return strats;
} // build_rollout_strategy_set

static void decide_and_apply(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = build_rollout_strategy_set();

  GameMove move = ismcts_search_best_move(gstate, player, &sim_ctx, &g_test_params,
                                          &rollout_strats);
  apply_move(gstate, player, &move, ctx);
} // decide_and_apply

static void test_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, gstate->current_player, ctx);
} // test_attack_strategy

static void test_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, 1 - gstate->current_player, ctx);
} // test_defense_strategy

static void run_half(uint16_t numgames, bool ismcts_is_a, AIStrategyType opponent,
                     GameContext* ctx, uint32_t* wins_ismcts, uint32_t* wins_opp,
                     uint32_t* draws)
{ StrategySet* strategies = create_strategy_set();
  PlayerID ismcts_seat = ismcts_is_a ? PLAYER_A : PLAYER_B;
  PlayerID opp_seat = ismcts_is_a ? PLAYER_B : PLAYER_A;

  set_player_strategy_by_type(strategies, opp_seat, opponent);
  strategies->attack_strategy[ismcts_seat] = test_attack_strategy;
  strategies->defense_strategy[ismcts_seat] = test_defense_strategy;
  strategies->mulligan_strategy[ismcts_seat] = strat_lib_mulligan;
  strategies->discard_strategy[ismcts_seat] = strat_lib_discard_to_7;

  struct gamestats gstats;
  memset(&gstats, 0, sizeof(gstats));
  run_simulation(numgames, INITIAL_CASH_DEFAULT, &gstats, strategies, ctx);

  *wins_ismcts += gstats.cumul_player_wins[ismcts_seat];
  *wins_opp += gstats.cumul_player_wins[opp_seat];
  *draws += gstats.cumul_number_of_draws;

  free_strategy_set(strategies);
} // run_half

int main(int argc, char** argv)
{ if(argc != 6)
  { fprintf(stderr, "Usage: %s <rollout_policy> <limit_iterations> <numgames> <seed> <opponent>\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  g_rollout_policy = parse_agent_or_die(argv[1]);
  uint32_t limit_iterations = (uint32_t)strtoul(argv[2], NULL, 10);
  uint16_t numgames = (uint16_t)strtoul(argv[3], NULL, 10);
  unsigned long seed = strtoul(argv[4], NULL, 10);
  AIStrategyType opponent = parse_agent_or_die(argv[5]);

  g_test_params = ismcts_get_default_params();
  g_test_params.limit_iterations = limit_iterations;

  config_t cfg = {0};
  cfg.prng_seed = seed;
  GameContext* ctx = create_game_context(&cfg);

  uint32_t wins_ismcts = 0, wins_opp = 0, draws = 0;
  run_half((uint16_t)(numgames / 2), true, opponent, ctx, &wins_ismcts, &wins_opp, &draws);
  run_half((uint16_t)(numgames / 2), false, opponent, ctx, &wins_ismcts, &wins_opp, &draws);

  printf("%s,%u,%u,%lu,%s,%u,%u,%u\n",
         argv[1], limit_iterations, numgames, seed, argv[5], wins_ismcts, wins_opp, draws);

  destroy_game_context(ctx);
  return EXIT_SUCCESS;
}
