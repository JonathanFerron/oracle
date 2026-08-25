// calib_tactical.c
// Calibration harness for A6 Tactical's sixteen tunable parameters
// (TacticalParams -- see ai_strat_tactical.h). Same in-process pattern as
// aicalibsrc/heuristic/calib_heuristic.c: links the engine directly so a
// driver script can call run_simulation() many times without a
// subprocess-spawn/text-parse round trip per data point, and prints one
// clean CSV line per run.
//
// Usage:
//   calib_tactical <numsim> <seed> <agent_a> <agent_b>
//     <phase_mid_a> <phase_late_a> <phase_critical_a>
//     <energy_diff_w_a> <opp_late_a> <opp_critical_a>
//     <self_late_a> <self_critical_a> <hand_power_bonus_a> <hand_power_penalty_a>
//     <cash_surplus_threshold_a> <cash_surplus_bonus_a>
//     <defense_damage_w_a> <defense_cash_w_a> <defense_stdev_mult_a>
//     <draw_min_hand_a>
//     <16 more of the same for Player B>
//   calib_tactical --print-defaults
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "tactical",
// "borealis", "rand"). All sixteen TacticalParams are set for both seats
// regardless of which agent actually plays there -- harmless, since a
// non-Tactical agent never reads them.
//
// --print-defaults dumps the compiled TACTICAL_DEFAULTS as JSON and exits,
// so calibrate_tactical.py never hardcodes its own copy of the baseline --
// the same discipline aicalibsrc/heuristic/ and aicalibsrc/balanced/ use.
//
// Output (positional run): one CSV line to stdout, no header (a driver
// script supplies its own so it can run this many times and concatenate).
// Params are echoed back after parsing -- the same round-trip discipline
// that caught an argv off-by-one in the A2 harness:
//   numsim,seed,agent_a,agent_b,
//   <16 params for A>, <16 params for B>,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_tactical.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define TAC_PARAM_ARGC 16 // fields in TacticalParams, one CLI arg each
#define TAC_FIXED_ARGC 4  // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_tactical: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<phase_mid> <phase_late> <phase_critical> "
          "<energy_diff_w> <opp_late> <opp_critical> <self_late> <self_critical> "
          "<hand_power_bonus> <hand_power_penalty> "
          "<cash_surplus_threshold> <cash_surplus_bonus> "
          "<defense_damage_w> <defense_cash_w> <defense_stdev_mult> "
          "<draw_min_hand> <same 16 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses the sixteen positional CLI args starting at argv[offset] into a
// TacticalParams, in the struct's declared field order.
static TacticalParams parse_params(char** argv, int offset)
{ return (TacticalParams)
  { .phase_mid_threshold = (uint8_t)strtol(argv[offset], NULL, 10),
      .phase_late_threshold = (uint8_t)strtol(argv[offset + 1], NULL, 10),
      .phase_critical_threshold = (uint8_t)strtol(argv[offset + 2], NULL, 10),
      .aggression_energy_diff_weight = strtof(argv[offset + 3], NULL),
      .aggression_opp_late_bonus = strtof(argv[offset + 4], NULL),
      .aggression_opp_critical_bonus = strtof(argv[offset + 5], NULL),
      .aggression_self_late_penalty = strtof(argv[offset + 6], NULL),
      .aggression_self_critical_penalty = strtof(argv[offset + 7], NULL),
      .aggression_hand_power_bonus = strtof(argv[offset + 8], NULL),
      .aggression_hand_power_penalty = strtof(argv[offset + 9], NULL),
      .aggression_cash_surplus_threshold = (uint16_t)strtol(argv[offset + 10], NULL, 10),
      .aggression_cash_surplus_bonus = strtof(argv[offset + 11], NULL),
      .defense_damage_weight = strtof(argv[offset + 12], NULL),
      .defense_cash_weight = strtof(argv[offset + 13], NULL),
      .defense_conservative_stdev_mult = strtof(argv[offset + 14], NULL),
      .draw_min_hand_size = (uint8_t)strtol(argv[offset + 15], NULL, 10)
  };
} // parse_params

static void print_params_csv(const TacticalParams* p)
{ printf("%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%d,",
         p->phase_mid_threshold, p->phase_late_threshold, p->phase_critical_threshold,
         p->aggression_energy_diff_weight, p->aggression_opp_late_bonus,
         p->aggression_opp_critical_bonus, p->aggression_self_late_penalty,
         p->aggression_self_critical_penalty, p->aggression_hand_power_bonus,
         p->aggression_hand_power_penalty, p->aggression_cash_surplus_threshold,
         p->aggression_cash_surplus_bonus, p->defense_damage_weight,
         p->defense_cash_weight, p->defense_conservative_stdev_mult,
         p->draw_min_hand_size);
} // print_params_csv

static void print_defaults_json(void)
{ TacticalParams d = tactical_get_default_params();
  printf("{\n"
         "  \"phase_mid_threshold\": %d,\n"
         "  \"phase_late_threshold\": %d,\n"
         "  \"phase_critical_threshold\": %d,\n"
         "  \"aggression_energy_diff_weight\": %.6f,\n"
         "  \"aggression_opp_late_bonus\": %.6f,\n"
         "  \"aggression_opp_critical_bonus\": %.6f,\n"
         "  \"aggression_self_late_penalty\": %.6f,\n"
         "  \"aggression_self_critical_penalty\": %.6f,\n"
         "  \"aggression_hand_power_bonus\": %.6f,\n"
         "  \"aggression_hand_power_penalty\": %.6f,\n"
         "  \"aggression_cash_surplus_threshold\": %d,\n"
         "  \"aggression_cash_surplus_bonus\": %.6f,\n"
         "  \"defense_damage_weight\": %.6f,\n"
         "  \"defense_cash_weight\": %.6f,\n"
         "  \"defense_conservative_stdev_mult\": %.6f,\n"
         "  \"draw_min_hand_size\": %d\n"
         "}\n",
         d.phase_mid_threshold, d.phase_late_threshold, d.phase_critical_threshold,
         d.aggression_energy_diff_weight, d.aggression_opp_late_bonus,
         d.aggression_opp_critical_bonus, d.aggression_self_late_penalty,
         d.aggression_self_critical_penalty, d.aggression_hand_power_bonus,
         d.aggression_hand_power_penalty, d.aggression_cash_surplus_threshold,
         d.aggression_cash_surplus_bonus, d.defense_damage_weight,
         d.defense_cash_weight, d.defense_conservative_stdev_mult,
         d.draw_min_hand_size);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != TAC_FIXED_ARGC + 2 * TAC_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..TAC_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the first
  // param block starts one slot past TAC_FIXED_ARGC.
  TacticalParams params_a = parse_params(argv, TAC_FIXED_ARGC + 1);
  TacticalParams params_b = parse_params(argv, TAC_FIXED_ARGC + 1 + TAC_PARAM_ARGC);

  tactical_set_params(PLAYER_A, &params_a);
  tactical_set_params(PLAYER_B, &params_b);

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
  tactical_reset_params();

  return EXIT_SUCCESS;
} // main
