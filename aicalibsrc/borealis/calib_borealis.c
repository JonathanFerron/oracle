// calib_borealis.c
// Calibration harness for A3 Borealis's six tunable parameters
// (BorealisParams -- see ai_strat_borealis.h). Links the engine directly,
// the same way aicalibsrc/combo/calib_combo_threshold.c and
// aicalibsrc/value/calib_valuebased.c do, so a driver script can call
// run_simulation() many times in-process (no subprocess-spawn or
// text-parsing overhead per parameter combination) and get back one clean
// CSV line per run.
//
// Usage:
//   calib_borealis <numsim> <seed> <agent_a> <agent_b>
//     <luna_a> <epsilon_a> <hold_a> <combo_bonus_a> <ceiling_a> <hand_a>
//     <luna_b> <epsilon_b> <hold_b> <combo_bonus_b> <ceiling_b> <hand_b>
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "borealis", "combo",
// "rand"). hold_{a,b} are 0/1. All six BorealisParams are set for both seats
// regardless of which agent actually plays there -- harmless, since a
// non-Borealis agent never reads them.
//
// Output: one CSV line to stdout, no header (a driver script supplies its
// own so it can run this many times and concatenate). Params are echoed
// back after parsing -- this round-trip is what caught an argv off-by-one
// in the A2 harness (see aicalibsrc/combo/calib_combo_threshold.c's commit
// history), so it stays even though it duplicates argv:
//   numsim,seed,agent_a,agent_b,
//   luna_a,epsilon_a,hold_a,combo_bonus_a,ceiling_a,hand_a,
//   luna_b,epsilon_b,hold_b,combo_bonus_b,ceiling_b,hand_b,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_borealis.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define BOR_PARAM_ARGC 6 // fields in BorealisParams, one CLI arg each
#define BOR_FIXED_ARGC 4 // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_borealis: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<luna_a> <epsilon_a> <hold_a> <combo_bonus_a> <ceiling_a> <hand_a> "
          "<luna_b> <epsilon_b> <hold_b> <combo_bonus_b> <ceiling_b> <hand_b>\n",
          prog);
} // print_usage

// Parses the six positional CLI args starting at argv[offset] into a
// BorealisParams, in the struct's declared field order.
static BorealisParams parse_params(char** argv, int offset)
{ return (BorealisParams)
  { .luna_value = strtof(argv[offset], NULL),
      .tiebreak_epsilon = strtof(argv[offset + 1], NULL),
      .hold_lethal_combos = strtol(argv[offset + 2], NULL, 10) != 0,
      .lethal_combo_bonus = (int)strtol(argv[offset + 3], NULL, 10),
      .lethal_hold_ceiling = (int)strtol(argv[offset + 4], NULL, 10),
      .min_hand_size_target = (int)strtol(argv[offset + 5], NULL, 10)
  };
} // parse_params

static void print_params_csv(const BorealisParams* p)
{ printf("%.4f,%.4f,%d,%d,%d,%d,",
         p->luna_value, p->tiebreak_epsilon, p->hold_lethal_combos ? 1 : 0,
         p->lethal_combo_bonus, p->lethal_hold_ceiling, p->min_hand_size_target);
} // print_params_csv

int main(int argc, char** argv)
{ if(argc != BOR_FIXED_ARGC + 2 * BOR_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..BOR_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the first
  // param block starts one slot past BOR_FIXED_ARGC.
  BorealisParams params_a = parse_params(argv, BOR_FIXED_ARGC + 1);
  BorealisParams params_b = parse_params(argv, BOR_FIXED_ARGC + 1 + BOR_PARAM_ARGC);

  borealis_set_params(PLAYER_A, &params_a);
  borealis_set_params(PLAYER_B, &params_b);

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
  borealis_reset_params();

  return EXIT_SUCCESS;
} // main
