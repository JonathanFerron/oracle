// calib_hbt2ply.c
// Calibration harness for A9 HBT 2-Ply's tunable parameters (HBT2PlyParams
// -- see ai_strat_hbt2ply.h). Same in-process pattern as
// aicalibsrc/hbt/calib_hbt.c: links the engine directly so a driver script
// can call run_simulation() many times without a subprocess-spawn/
// text-parse round trip per data point, and prints one clean CSV line per
// run.
//
// Usage:
//   calib_hbt2ply <numsim> <seed> <agent_a> <agent_b>
//     <34 HBTParams fields for Player A, same order as calib_hbt's own CLI>
//     <reply_trust_a> <surrogate_pessimism_a> <ply_energy_ceiling_a> <ply_beam_width_a>
//     <same 38 for Player B>
//   calib_hbt2ply --print-defaults
//
// agent_a/agent_b are the usual -A/--ai shorthands. All 38 HBT2PlyParams
// fields are set for both seats regardless of which agent actually plays
// there -- harmless, since a non-hbt2ply agent never reads them.
//
// Every one of the 34 HBTParams fields IS exposed here, even though A9's
// planned calibration (doc/changelog.md, aicalibsrc/hbt2ply/README.md)
// freezes all of them at A7's shipped defaults and only ever frees the 4
// new fields -- same "harness exposes everything, the driver/plan decides
// what's actually free" split as calib_hbt.c itself (which exposes all 34
// even though A9's own about.md re-derives none of them).
//
// --print-defaults dumps hbt2ply_get_default_params() as FLAT JSON (the
// .base HBTParams fields inlined alongside the 4 new ones, not nested) so
// calibrate_hbt2ply.py never hardcodes its own copy of the baseline -- same
// discipline as every other aicalibsrc/ driver.
//
// Output (positional run): one CSV line to stdout, no header. Params are
// echoed back after parsing -- the same round-trip discipline as calib_hbt.c:
//   numsim,seed,agent_a,agent_b,
//   <38 params for A>, <38 params for B>,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_hbt2ply.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define HBT2PLY_BASE_ARGC 34  // fields in HBTParams (ai_strat_hbt.h), same order as calib_hbt.c
#define HBT2PLY_NEW_ARGC 4    // fields new to HBT2PlyParams
#define HBT2PLY_PARAM_ARGC (HBT2PLY_BASE_ARGC + HBT2PLY_NEW_ARGC)
#define HBT2PLY_FIXED_ARGC 4  // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_hbt2ply: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<34 HBTParams fields, ai_strat_hbt.h's declared order -- see calib_hbt.c> "
          "<reply_trust> <surrogate_pessimism> <ply_energy_ceiling> <ply_beam_width> "
          "<same 38 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Same 34-field parse as calib_hbt.c's parse_params() -- duplicated rather
// than shared, matching this codebase's per-agent calibration harness
// convention (each aicalibsrc/<agent>/ is self-contained).
static HBTParams parse_base_params(char** argv, int offset)
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
} // parse_base_params

// Parses one seat's full 38-field block starting at argv[offset]: the 34
// base HBTParams fields, then this agent's own 4 new fields, in
// HBT2PlyParams's declared order (ai_strat_hbt2ply.h).
static HBT2PlyParams parse_params(char** argv, int offset)
{ HBT2PlyParams p;
  p.base = parse_base_params(argv, offset);
  p.reply_trust = strtof(argv[offset + HBT2PLY_BASE_ARGC], NULL);
  p.surrogate_pessimism = strtof(argv[offset + HBT2PLY_BASE_ARGC + 1], NULL);
  p.ply_energy_ceiling = (uint8_t)strtol(argv[offset + HBT2PLY_BASE_ARGC + 2], NULL, 10);
  p.ply_beam_width = (uint8_t)strtol(argv[offset + HBT2PLY_BASE_ARGC + 3], NULL, 10);
  return p;
} // parse_params

// Same field order/format as calib_hbt.c's print_params_csv(), plus the 4
// new fields appended.
static void print_params_csv(const HBT2PlyParams* p)
{ const HBTParams* b = &p->base;
  printf("%.6f,%.6f,%.6f,%.6f,%.6f,"
         "%d,%d,%d,"
         "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,"
         "%.6f,%.6f,%.6f,"
         "%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,"
         "%.6f,%.6f,"
         "%d,%d,%d,"
         "%.6f,"
         "%.6f,%.6f,%d,%d,",
         b->weight_energy_advantage, b->weight_cards_advantage, b->weight_cash_advantage,
         b->weight_taper_exponent, b->opp_card_discount,
         b->phase_mid_threshold, b->phase_late_threshold, b->phase_critical_threshold,
         b->aggression_energy_diff_weight, b->aggression_opp_late_bonus,
         b->aggression_opp_critical_bonus, b->aggression_self_late_penalty,
         b->aggression_self_critical_penalty, b->aggression_hand_power_bonus,
         b->aggression_hand_power_penalty, b->aggression_cash_surplus_threshold,
         b->aggression_cash_surplus_bonus,
         b->aggr_energy_gain, b->aggr_resource_fade, b->critical_epsilon_mult,
         b->target_cash_slope, b->target_cash_intercept,
         b->target_cards_slope, b->target_cards_intercept,
         b->late_game_aggro, b->lethal_horizon,
         b->target_aggr_cash_scale, b->target_aggr_cards_scale,
         b->penalty_cash_weight, b->penalty_cards_weight,
         b->hold_lethal_combos ? 1 : 0, b->lethal_combo_bonus, b->lethal_hold_ceiling,
         b->defense_stdev_mult,
         p->reply_trust, p->surrogate_pessimism, p->ply_energy_ceiling, p->ply_beam_width);
} // print_params_csv

// Flat JSON: the 34 base fields (same names as calib_hbt's own dump) plus
// the 4 new fields, all at the top level -- calibrate_hbt2ply.py's
// PARAM_NAMES is a flat 38-entry list matching this key order.
static void print_defaults_json(void)
{ HBT2PlyParams d = hbt2ply_get_default_params();
  const HBTParams* b = &d.base;
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
         "  \"defense_stdev_mult\": %.6f,\n"
         "  \"reply_trust\": %.6f,\n"
         "  \"surrogate_pessimism\": %.6f,\n"
         "  \"ply_energy_ceiling\": %d,\n"
         "  \"ply_beam_width\": %d\n"
         "}\n",
         b->weight_energy_advantage, b->weight_cards_advantage, b->weight_cash_advantage,
         b->weight_taper_exponent, b->opp_card_discount,
         b->phase_mid_threshold, b->phase_late_threshold, b->phase_critical_threshold,
         b->aggression_energy_diff_weight, b->aggression_opp_late_bonus,
         b->aggression_opp_critical_bonus, b->aggression_self_late_penalty,
         b->aggression_self_critical_penalty, b->aggression_hand_power_bonus,
         b->aggression_hand_power_penalty, b->aggression_cash_surplus_threshold,
         b->aggression_cash_surplus_bonus,
         b->aggr_energy_gain, b->aggr_resource_fade, b->critical_epsilon_mult,
         b->target_cash_slope, b->target_cash_intercept,
         b->target_cards_slope, b->target_cards_intercept,
         b->late_game_aggro, b->lethal_horizon,
         b->target_aggr_cash_scale, b->target_aggr_cards_scale,
         b->penalty_cash_weight, b->penalty_cards_weight,
         b->hold_lethal_combos ? "true" : "false", b->lethal_combo_bonus, b->lethal_hold_ceiling,
         b->defense_stdev_mult,
         d.reply_trust, d.surrogate_pessimism, d.ply_energy_ceiling, d.ply_beam_width);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != HBT2PLY_FIXED_ARGC + 2 * HBT2PLY_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..HBT2PLY_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the
  // first param block starts one slot past HBT2PLY_FIXED_ARGC.
  HBT2PlyParams params_a = parse_params(argv, HBT2PLY_FIXED_ARGC + 1);
  HBT2PlyParams params_b = parse_params(argv, HBT2PLY_FIXED_ARGC + 1 + HBT2PLY_PARAM_ARGC);

  hbt2ply_set_params(PLAYER_A, &params_a);
  hbt2ply_set_params(PLAYER_B, &params_b);

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
  hbt2ply_reset_params();

  return EXIT_SUCCESS;
} // main
