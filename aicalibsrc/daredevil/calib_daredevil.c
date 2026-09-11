// calib_daredevil.c
// Calibration harness for A15 Risk Threshold's tunable parameters
// (A15Params -- see ai_strat_a15.h). Same in-process pattern as every
// other aicalibsrc/<agent>/calib_<agent>.c: links the engine directly so a
// driver script can call run_simulation() many times without a
// subprocess-spawn/text-parse round trip per data point, and prints one
// clean CSV line per run.
//
// Usage:
//   calib_daredevil <numsim> <seed> <agent_a> <agent_b>
//     <defense_loss_threshold_a> <endgame_q1_a> <endgame_q2_a> <endgame_q3_a>
//     <endgame_q4_a> <endgame_enabled_a>
//     <same 6 for Player B>
//   calib_daredevil --print-defaults
//
// agent_a/agent_b are the usual -A/--ai shorthands. All 6 A15Params fields
// are set for both seats regardless of which agent actually plays there --
// harmless, since a non-daredevil agent never reads them.
//
// --print-defaults dumps a15_get_default_params() as flat JSON so
// calibrate_daredevil.py never hardcodes its own copy of the baseline --
// same discipline as every other aicalibsrc/ driver.
//
// Output (positional run): one CSV line to stdout, no header. Params are
// echoed back after parsing (round-trip discipline):
//   numsim,seed,agent_a,agent_b,
//   <6 params for A>, <6 params for B>,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_a15.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define DAREDEVIL_PARAM_ARGC 6 // fields in A15Params, ai_strat_a15.h's declared order
#define DAREDEVIL_FIXED_ARGC 4 // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_daredevil: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<defense_loss_threshold> <endgame_q1> <endgame_q2> <endgame_q3> "
          "<endgame_q4> <endgame_enabled> <same 6 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Same field order as A15Params (ai_strat_a15.h).
static A15Params parse_params(char** argv, int offset)
{ return (A15Params)
  { .defense_loss_threshold = strtof(argv[offset], NULL),
      .endgame_q1 = strtof(argv[offset + 1], NULL),
      .endgame_q2 = strtof(argv[offset + 2], NULL),
      .endgame_q3 = strtof(argv[offset + 3], NULL),
      .endgame_q4 = strtof(argv[offset + 4], NULL),
      .endgame_enabled = strtol(argv[offset + 5], NULL, 10) != 0
  };
} // parse_params

static void print_params_csv(const A15Params* p)
{ printf("%.6f,%.6f,%.6f,%.6f,%.6f,%d,",
         p->defense_loss_threshold, p->endgame_q1, p->endgame_q2, p->endgame_q3,
         p->endgame_q4, p->endgame_enabled ? 1 : 0);
} // print_params_csv

static void print_defaults_json(void)
{ A15Params d = a15_get_default_params();
  printf("{\n"
         "  \"defense_loss_threshold\": %.6f,\n"
         "  \"endgame_q1\": %.6f,\n"
         "  \"endgame_q2\": %.6f,\n"
         "  \"endgame_q3\": %.6f,\n"
         "  \"endgame_q4\": %.6f,\n"
         "  \"endgame_enabled\": %s\n"
         "}\n",
         d.defense_loss_threshold, d.endgame_q1, d.endgame_q2, d.endgame_q3, d.endgame_q4,
         d.endgame_enabled ? "true" : "false");
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != DAREDEVIL_FIXED_ARGC + 2 * DAREDEVIL_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..DAREDEVIL_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the
  // first param block starts one slot past DAREDEVIL_FIXED_ARGC.
  A15Params params_a = parse_params(argv, DAREDEVIL_FIXED_ARGC + 1);
  A15Params params_b = parse_params(argv, DAREDEVIL_FIXED_ARGC + 1 + DAREDEVIL_PARAM_ARGC);

  a15_set_params(PLAYER_A, &params_a);
  a15_set_params(PLAYER_B, &params_b);

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

  printf("%u,%u,%s,%s,", numsim, seed, argv[3], argv[4]);
  print_params_csv(&params_a);
  print_params_csv(&params_b);
  printf("%u,%u,%u\n",
         gstats.cumul_player_wins[PLAYER_A], gstats.cumul_player_wins[PLAYER_B],
         gstats.cumul_number_of_draws);

  free_strategy_set(strategies);
  destroy_game_context(ctx);
  a15_reset_params();

  return EXIT_SUCCESS;
} // main
