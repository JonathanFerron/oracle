// calib_valuebased.c
// Calibration harness for A1 Value Based's two tunable parameters
// (VB_COST_FLOOR, VB_DEFEND_THRESHOLD -- see ai_strat_valuebased.c). Links
// the engine directly, the same way testsrc/test_recall.c etc. do, so a
// driver script can call run_simulation() many times in-process (no
// subprocess-spawn or text-parsing overhead per parameter combination) and
// get back one clean CSV line per run.
//
// Usage:
//   calib_valuebased <numsim> <seed> <agent_a> <agent_b>
//                     <cost_floor_a> <defend_threshold_a>
//                     <cost_floor_b> <defend_threshold_b>
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "value", "rand").
// The two Value Based parameters are set for both seats regardless of which
// agent actually plays there -- harmless, since a non-Value-Based agent
// never reads them.
//
// Output: one CSV line to stdout, no header (a driver script supplies its
// own so it can run this many times and concatenate):
//   numsim,seed,agent_a,agent_b,cost_floor_a,defend_threshold_a,
//   cost_floor_b,defend_threshold_b,wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_valuebased.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_valuebased: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<cost_floor_a> <defend_threshold_a> <cost_floor_b> <defend_threshold_b>\n",
          prog);
} // print_usage

int main(int argc, char** argv)
{ if(argc != 9)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);
  float cost_floor_a = strtof(argv[5], NULL);
  float defend_threshold_a = strtof(argv[6], NULL);
  float cost_floor_b = strtof(argv[7], NULL);
  float defend_threshold_b = strtof(argv[8], NULL);

  value_based_set_params(PLAYER_A, cost_floor_a, defend_threshold_a);
  value_based_set_params(PLAYER_B, cost_floor_b, defend_threshold_b);

  config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.prng_seed = seed;
  cfg.use_random_seed = false;

  GameContext* ctx = create_game_context(&cfg);
  if(ctx == NULL) return EXIT_FAILURE;

  StrategySet* strategies = create_strategy_set();
  set_player_strategy_by_type(strategies, PLAYER_A, agent_a);
  set_player_strategy_by_type(strategies, PLAYER_B, agent_b);

  struct gamestats gstats;
  memset(&gstats, 0, sizeof(gstats));
  run_simulation(numsim, INITIAL_CASH_DEFAULT, &gstats, strategies, ctx);

  printf("%u,%u,%s,%s,%.4f,%.4f,%.4f,%.4f,%u,%u,%u\n",
         numsim, seed, argv[3], argv[4],
         cost_floor_a, defend_threshold_a, cost_floor_b, defend_threshold_b,
         gstats.cumul_player_wins[PLAYER_A], gstats.cumul_player_wins[PLAYER_B],
         gstats.cumul_number_of_draws);

  free_strategy_set(strategies);
  destroy_game_context(ctx);
  value_based_reset_params();

  return EXIT_SUCCESS;
} // main
