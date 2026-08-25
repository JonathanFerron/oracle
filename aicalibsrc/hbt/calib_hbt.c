// calib_hbt.c
// Calibration harness for A7 Hybrid HBT's thirty-four tunable parameters
// (HBTParams -- see ai_strat_hbt.h). Same in-process pattern as
// aicalibsrc/tactical/calib_tactical.c: links the engine directly so a
// driver script can call run_simulation() many times without a
// subprocess-spawn/text-parse round trip per data point, and prints one
// clean CSV line per run.
//
// Usage:
//   calib_hbt <numsim> <seed> <agent_a> <agent_b>
//     <weight_energy_a> <weight_cards_a> <weight_cash_a> <taper_a> <opp_card_discount_a>
//     <phase_mid_a> <phase_late_a> <phase_critical_a>
//     <energy_diff_w_a> <opp_late_a> <opp_critical_a> <self_late_a> <self_critical_a>
//     <hand_power_bonus_a> <hand_power_penalty_a>
//     <cash_surplus_threshold_a> <cash_surplus_bonus_a>
//     <aggr_energy_gain_a> <aggr_resource_fade_a> <critical_epsilon_mult_a>
//     <target_cash_slope_a> <target_cash_intercept_a>
//     <target_cards_slope_a> <target_cards_intercept_a>
//     <late_game_aggro_a> <lethal_horizon_a>
//     <target_aggr_cash_scale_a> <target_aggr_cards_scale_a>
//     <penalty_cash_weight_a> <penalty_cards_weight_a>
//     <hold_lethal_combos_a> <lethal_combo_bonus_a> <lethal_hold_ceiling_a>
//     <defense_stdev_mult_a>
//     <34 more of the same for Player B>
//   calib_hbt --print-defaults
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "hbt", "borealis",
// "heuristic", "rand"). All thirty-four HBTParams fields are set for both
// seats regardless of which agent actually plays there -- harmless, since a
// non-hbt agent never reads them.
//
// --print-defaults dumps the compiled HBT_DEFAULTS as JSON and exits, so
// calibrate_hbt.py never hardcodes its own copy of the baseline -- the same
// discipline aicalibsrc/heuristic/, aicalibsrc/balanced/, and
// aicalibsrc/tactical/ use.
//
// Output (positional run): one CSV line to stdout, no header (a driver
// script supplies its own so it can run this many times and concatenate).
// Params are echoed back after parsing -- the same round-trip discipline
// that caught an argv off-by-one in the A2 harness:
//   numsim,seed,agent_a,agent_b,
//   <34 params for A>, <34 params for B>,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_hbt.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define HBT_PARAM_ARGC 34 // fields in HBTParams, one CLI arg each
#define HBT_FIXED_ARGC 4  // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_hbt: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<weight_energy> <weight_cards> <weight_cash> <taper> <opp_card_discount> "
          "<phase_mid> <phase_late> <phase_critical> "
          "<energy_diff_w> <opp_late> <opp_critical> <self_late> <self_critical> "
          "<hand_power_bonus> <hand_power_penalty> "
          "<cash_surplus_threshold> <cash_surplus_bonus> "
          "<aggr_energy_gain> <aggr_resource_fade> <critical_epsilon_mult> "
          "<target_cash_slope> <target_cash_intercept> "
          "<target_cards_slope> <target_cards_intercept> "
          "<late_game_aggro> <lethal_horizon> "
          "<target_aggr_cash_scale> <target_aggr_cards_scale> "
          "<penalty_cash_weight> <penalty_cards_weight> "
          "<hold_lethal_combos> <lethal_combo_bonus> <lethal_hold_ceiling> "
          "<defense_stdev_mult> <same 34 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses the thirty-four positional CLI args starting at argv[offset] into
// an HBTParams, in the struct's declared field order (ai_strat_hbt.h).
static HBTParams parse_params(char** argv, int offset)
{ return (HBTParams)
  { .weight_energy_advantage = strtof(argv[offset], NULL),
      .weight_cards_advantage = strtof(argv[offset + 1], NULL),
      .weight_cash_advantage = strtof(argv[offset + 2], NULL),
      .weight_taper_exponent = strtof(argv[offset + 3], NULL),
      .opp_card_discount = strtof(argv[offset + 4], NULL),
      .phase_mid_threshold = (uint8_t)strtol(argv[offset + 5], NULL, 10),
      .phase_late_threshold = (uint8_t)strtol(argv[offset + 6], NULL, 10),
      .phase_critical_threshold = (uint8_t)strtol(argv[offset + 7], NULL, 10),
      .aggression_energy_diff_weight = strtof(argv[offset + 8], NULL),
      .aggression_opp_late_bonus = strtof(argv[offset + 9], NULL),
      .aggression_opp_critical_bonus = strtof(argv[offset + 10], NULL),
      .aggression_self_late_penalty = strtof(argv[offset + 11], NULL),
      .aggression_self_critical_penalty = strtof(argv[offset + 12], NULL),
      .aggression_hand_power_bonus = strtof(argv[offset + 13], NULL),
      .aggression_hand_power_penalty = strtof(argv[offset + 14], NULL),
      .aggression_cash_surplus_threshold = (uint16_t)strtol(argv[offset + 15], NULL, 10),
      .aggression_cash_surplus_bonus = strtof(argv[offset + 16], NULL),
      .aggr_energy_gain = strtof(argv[offset + 17], NULL),
      .aggr_resource_fade = strtof(argv[offset + 18], NULL),
      .critical_epsilon_mult = strtof(argv[offset + 19], NULL),
      .target_cash_slope = strtof(argv[offset + 20], NULL),
      .target_cash_intercept = strtof(argv[offset + 21], NULL),
      .target_cards_slope = strtof(argv[offset + 22], NULL),
      .target_cards_intercept = strtof(argv[offset + 23], NULL),
      .late_game_aggro = strtof(argv[offset + 24], NULL),
      .lethal_horizon = (int)strtol(argv[offset + 25], NULL, 10),
      .target_aggr_cash_scale = strtof(argv[offset + 26], NULL),
      .target_aggr_cards_scale = strtof(argv[offset + 27], NULL),
      .penalty_cash_weight = strtof(argv[offset + 28], NULL),
      .penalty_cards_weight = strtof(argv[offset + 29], NULL),
      .hold_lethal_combos = strtol(argv[offset + 30], NULL, 10) != 0,
      .lethal_combo_bonus = (int)strtol(argv[offset + 31], NULL, 10),
      .lethal_hold_ceiling = (int)strtol(argv[offset + 32], NULL, 10),
      .defense_stdev_mult = strtof(argv[offset + 33], NULL)
  };
} // parse_params

static void print_params_csv(const HBTParams* p)
{ printf("%.6f,%.6f,%.6f,%.6f,%.6f,"
           "%d,%d,%d,"
           "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,"
           "%.6f,%.6f,%.6f,"
           "%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,"
           "%.6f,%.6f,"
           "%d,%d,%d,"
           "%.6f,",
         p->weight_energy_advantage, p->weight_cards_advantage, p->weight_cash_advantage,
         p->weight_taper_exponent, p->opp_card_discount,
         p->phase_mid_threshold, p->phase_late_threshold, p->phase_critical_threshold,
         p->aggression_energy_diff_weight, p->aggression_opp_late_bonus,
         p->aggression_opp_critical_bonus, p->aggression_self_late_penalty,
         p->aggression_self_critical_penalty, p->aggression_hand_power_bonus,
         p->aggression_hand_power_penalty, p->aggression_cash_surplus_threshold,
         p->aggression_cash_surplus_bonus,
         p->aggr_energy_gain, p->aggr_resource_fade, p->critical_epsilon_mult,
         p->target_cash_slope, p->target_cash_intercept,
         p->target_cards_slope, p->target_cards_intercept,
         p->late_game_aggro, p->lethal_horizon,
         p->target_aggr_cash_scale, p->target_aggr_cards_scale,
         p->penalty_cash_weight, p->penalty_cards_weight,
         p->hold_lethal_combos ? 1 : 0, p->lethal_combo_bonus, p->lethal_hold_ceiling,
         p->defense_stdev_mult);
} // print_params_csv

static void print_defaults_json(void)
{ HBTParams d = hbt_get_default_params();
  printf("{\n"
         "  \"weight_energy_advantage\": %.6f,\n"
         "  \"weight_cards_advantage\": %.6f,\n"
         "  \"weight_cash_advantage\": %.6f,\n"
         "  \"weight_taper_exponent\": %.6f,\n"
         "  \"opp_card_discount\": %.6f,\n"
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
         "  \"aggr_energy_gain\": %.6f,\n"
         "  \"aggr_resource_fade\": %.6f,\n"
         "  \"critical_epsilon_mult\": %.6f,\n"
         "  \"target_cash_slope\": %.6f,\n"
         "  \"target_cash_intercept\": %.6f,\n"
         "  \"target_cards_slope\": %.6f,\n"
         "  \"target_cards_intercept\": %.6f,\n"
         "  \"late_game_aggro\": %.6f,\n"
         "  \"lethal_horizon\": %d,\n"
         "  \"target_aggr_cash_scale\": %.6f,\n"
         "  \"target_aggr_cards_scale\": %.6f,\n"
         "  \"penalty_cash_weight\": %.6f,\n"
         "  \"penalty_cards_weight\": %.6f,\n"
         "  \"hold_lethal_combos\": %s,\n"
         "  \"lethal_combo_bonus\": %d,\n"
         "  \"lethal_hold_ceiling\": %d,\n"
         "  \"defense_stdev_mult\": %.6f\n"
         "}\n",
         d.weight_energy_advantage, d.weight_cards_advantage, d.weight_cash_advantage,
         d.weight_taper_exponent, d.opp_card_discount,
         d.phase_mid_threshold, d.phase_late_threshold, d.phase_critical_threshold,
         d.aggression_energy_diff_weight, d.aggression_opp_late_bonus,
         d.aggression_opp_critical_bonus, d.aggression_self_late_penalty,
         d.aggression_self_critical_penalty, d.aggression_hand_power_bonus,
         d.aggression_hand_power_penalty, d.aggression_cash_surplus_threshold,
         d.aggression_cash_surplus_bonus,
         d.aggr_energy_gain, d.aggr_resource_fade, d.critical_epsilon_mult,
         d.target_cash_slope, d.target_cash_intercept,
         d.target_cards_slope, d.target_cards_intercept,
         d.late_game_aggro, d.lethal_horizon,
         d.target_aggr_cash_scale, d.target_aggr_cards_scale,
         d.penalty_cash_weight, d.penalty_cards_weight,
         d.hold_lethal_combos ? "true" : "false", d.lethal_combo_bonus, d.lethal_hold_ceiling,
         d.defense_stdev_mult);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != HBT_FIXED_ARGC + 2 * HBT_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..HBT_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the first
  // param block starts one slot past HBT_FIXED_ARGC.
  HBTParams params_a = parse_params(argv, HBT_FIXED_ARGC + 1);
  HBTParams params_b = parse_params(argv, HBT_FIXED_ARGC + 1 + HBT_PARAM_ARGC);

  hbt_set_params(PLAYER_A, &params_a);
  hbt_set_params(PLAYER_B, &params_b);

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
  hbt_reset_params();

  return EXIT_SUCCESS;
} // main
