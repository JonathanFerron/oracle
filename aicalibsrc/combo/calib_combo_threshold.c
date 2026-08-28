// calib_combo_threshold.c
// Calibration harness for A2 Combo Threshold's nine tunable parameters
// (ComboThresholdParams -- see ai_strat_combo_threshold.h). Links the engine
// directly, the same way aicalibsrc/value/calib_valuebased.c and
// testsrc/test_recall.c etc. do, so a driver script can call
// run_simulation() many times in-process (no subprocess-spawn or
// text-parsing overhead per parameter combination) and get back one clean
// CSV line per run.
//
// Usage:
//   calib_combo_threshold <numsim> <seed> <agent_a> <agent_b>
//     <aggression_a> <combo2_a> <combo3_a> <weight_a> <floor_a>
//     <defend_prob_a> <defend_dmg_a> <min_hand_a> <save_lethal_a>
//     <aggression_b> <combo2_b> <combo3_b> <weight_b> <floor_b>
//     <defend_prob_b> <defend_dmg_b> <min_hand_b> <save_lethal_b>
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "combo", "value",
// "rand"). save_lethal_{a,b} are 0/1. All nine ComboThresholdParams are set
// for both seats regardless of which agent actually plays there -- harmless,
// since a non-Combo-Threshold agent never reads them.
//
//   calib_combo_threshold --print-defaults
//
// --print-defaults dumps the compiled CT_DEFAULTS as JSON and exits, so
// calibrate_combo_threshold.py never hardcodes its own stale copy of the
// baseline (added 2026-08-28 -- see doc/oracle_todo.md's Calibration
// section for the drift this was tracking).
//
// Output: one CSV line to stdout, no header (a driver script supplies its
// own so it can run this many times and concatenate):
//   numsim,seed,agent_a,agent_b,
//   aggression_a,combo2_a,combo3_a,weight_a,floor_a,defend_prob_a,defend_dmg_a,min_hand_a,save_lethal_a,
//   aggression_b,combo2_b,combo3_b,weight_b,floor_b,defend_prob_b,defend_dmg_b,min_hand_b,save_lethal_b,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_combo_threshold.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define CT_PARAM_ARGC 9 // fields in ComboThresholdParams, one CLI arg each
#define CT_FIXED_ARGC 4 // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_combo_threshold: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<aggression_a> <combo2_a> <combo3_a> <weight_a> <floor_a> "
          "<defend_prob_a> <defend_dmg_a> <min_hand_a> <save_lethal_a> "
          "<aggression_b> <combo2_b> <combo3_b> <weight_b> <floor_b> "
          "<defend_prob_b> <defend_dmg_b> <min_hand_b> <save_lethal_b>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses the nine positional CLI args starting at argv[offset] into a
// ComboThresholdParams, in the struct's declared field order.
static ComboThresholdParams parse_params(char** argv, int offset)
{ return (ComboThresholdParams)
  { .aggression_level = strtof(argv[offset], NULL),
      .combo_bonus_threshold = (int)strtol(argv[offset + 1], NULL, 10),
      .combo3_bonus_threshold = (int)strtol(argv[offset + 2], NULL, 10),
      .combo_weight = strtof(argv[offset + 3], NULL),
      .min_play_score_floor = strtof(argv[offset + 4], NULL),
      .defend_probability_base = strtof(argv[offset + 5], NULL),
      .defend_damage_threshold = (int)strtol(argv[offset + 6], NULL, 10),
      .min_hand_size_target = (int)strtol(argv[offset + 7], NULL, 10),
      .save_big_combos_for_lethal = strtol(argv[offset + 8], NULL, 10) != 0
  };
} // parse_params

static void print_params_csv(const ComboThresholdParams* p)
{ printf("%.4f,%d,%d,%.4f,%.4f,%.4f,%d,%d,%d,",
         p->aggression_level, p->combo_bonus_threshold, p->combo3_bonus_threshold,
         p->combo_weight, p->min_play_score_floor, p->defend_probability_base,
         p->defend_damage_threshold, p->min_hand_size_target,
         p->save_big_combos_for_lethal ? 1 : 0);
} // print_params_csv

static void print_defaults_json(void)
{ ComboThresholdParams d = combo_threshold_get_default_params();
  printf("{\n"
         "  \"aggression_level\": %.6f,\n"
         "  \"combo_bonus_threshold\": %d,\n"
         "  \"combo3_bonus_threshold\": %d,\n"
         "  \"combo_weight\": %.6f,\n"
         "  \"min_play_score_floor\": %.6f,\n"
         "  \"defend_probability_base\": %.6f,\n"
         "  \"defend_damage_threshold\": %d,\n"
         "  \"min_hand_size_target\": %d,\n"
         "  \"save_big_combos_for_lethal\": %d\n"
         "}\n",
         d.aggression_level, d.combo_bonus_threshold, d.combo3_bonus_threshold,
         d.combo_weight, d.min_play_score_floor, d.defend_probability_base,
         d.defend_damage_threshold, d.min_hand_size_target,
         d.save_big_combos_for_lethal ? 1 : 0);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != CT_FIXED_ARGC + 2 * CT_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..CT_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the first
  // param block starts one slot past CT_FIXED_ARGC.
  ComboThresholdParams params_a = parse_params(argv, CT_FIXED_ARGC + 1);
  ComboThresholdParams params_b = parse_params(argv, CT_FIXED_ARGC + 1 + CT_PARAM_ARGC);

  combo_threshold_set_params(PLAYER_A, &params_a);
  combo_threshold_set_params(PLAYER_B, &params_b);

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
  combo_threshold_reset_params();

  return EXIT_SUCCESS;
} // main
