// calib_balanced.c
// Calibration harness for A4 Balanced Rules's ten tunable parameters
// (BalancedRulesParams -- see ai_strat_balanced_rules.h). Links the engine
// directly, the same way aicalibsrc/borealis/calib_borealis.c and its
// predecessors do, so a driver script can call run_simulation() many times
// in-process (no subprocess-spawn or text-parsing overhead per parameter
// combination) and get back one clean CSV line per run.
//
// Usage:
//   calib_balanced <numsim> <seed> <agent_a> <agent_b>
//     <cash_slope_a> <cash_intercept_a> <cards_slope_a> <cards_intercept_a>
//     <beta_a> <aggro_a> <combo_weight_a> <horizon_a> <draw2_a> <draw3_a>
//     <10 more of the same for Player B>
//   calib_balanced --print-defaults
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "balanced", "combo",
// "rand"). All ten BalancedRulesParams are set for both seats regardless of
// which agent actually plays there -- harmless, since a non-Balanced agent
// never reads them.
//
// --print-defaults dumps the compiled BALANCED_DEFAULTS as JSON and exits,
// so calibrate_balanced.py never hardcodes its own copy of the baseline --
// doc/oracle_todo.md tracks exactly this kind of drift for the three older
// harnesses (aicalibsrc/value/, aicalibsrc/combo/, aicalibsrc/borealis/),
// each of which still hardcodes a Python-side DEFAULTS dict that has already
// gone stale against its shipped C constants.
//
// Output (positional run): one CSV line to stdout, no header (a driver
// script supplies its own so it can run this many times and concatenate).
// Params are echoed back after parsing -- this round-trip is what caught an
// argv off-by-one in the A2 harness (see aicalibsrc/combo/
// calib_combo_threshold.c's commit history), so it stays even though it
// duplicates argv:
//   numsim,seed,agent_a,agent_b,
//   cash_slope_a,cash_intercept_a,cards_slope_a,cards_intercept_a,beta_a,
//   aggro_a,combo_weight_a,horizon_a,draw2_a,draw3_a,
//   <same ten for Player B>,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_balanced_rules.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define BAL_PARAM_ARGC 10 // fields in BalancedRulesParams, one CLI arg each
#define BAL_FIXED_ARGC 4  // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_balanced: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<cash_slope_a> <cash_intercept_a> <cards_slope_a> <cards_intercept_a> "
          "<beta_a> <aggro_a> <combo_weight_a> <horizon_a> <draw2_a> <draw3_a> "
          "<same 10 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses the ten positional CLI args starting at argv[offset] into a
// BalancedRulesParams, in the struct's declared field order.
static BalancedRulesParams parse_params(char** argv, int offset)
{ return (BalancedRulesParams)
  { .target_cash_slope = strtof(argv[offset], NULL),
      .target_cash_intercept = strtof(argv[offset + 1], NULL),
      .target_cards_slope = strtof(argv[offset + 2], NULL),
      .target_cards_intercept = strtof(argv[offset + 3], NULL),
      .defense_beta = strtof(argv[offset + 4], NULL),
      .late_game_aggro = strtof(argv[offset + 5], NULL),
      .combo_weight = strtof(argv[offset + 6], NULL),
      .lethal_horizon = (int)strtol(argv[offset + 7], NULL, 10),
      .draw2_hand_threshold = (int)strtol(argv[offset + 8], NULL, 10),
      .draw3_hand_threshold = (int)strtol(argv[offset + 9], NULL, 10)
  };
} // parse_params

static void print_params_csv(const BalancedRulesParams* p)
{ printf("%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f,%d,%d,%d,",
         p->target_cash_slope, p->target_cash_intercept,
         p->target_cards_slope, p->target_cards_intercept,
         p->defense_beta, p->late_game_aggro, p->combo_weight,
         p->lethal_horizon, p->draw2_hand_threshold, p->draw3_hand_threshold);
} // print_params_csv

static void print_defaults_json(void)
{ BalancedRulesParams d = balanced_rules_get_default_params();
  printf("{\n"
         "  \"target_cash_slope\": %.6f,\n"
         "  \"target_cash_intercept\": %.6f,\n"
         "  \"target_cards_slope\": %.6f,\n"
         "  \"target_cards_intercept\": %.6f,\n"
         "  \"defense_beta\": %.4f,\n"
         "  \"late_game_aggro\": %.4f,\n"
         "  \"combo_weight\": %.4f,\n"
         "  \"lethal_horizon\": %d,\n"
         "  \"draw2_hand_threshold\": %d,\n"
         "  \"draw3_hand_threshold\": %d\n"
         "}\n",
         d.target_cash_slope, d.target_cash_intercept,
         d.target_cards_slope, d.target_cards_intercept,
         d.defense_beta, d.late_game_aggro, d.combo_weight,
         d.lethal_horizon, d.draw2_hand_threshold, d.draw3_hand_threshold);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != BAL_FIXED_ARGC + 2 * BAL_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..BAL_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the first
  // param block starts one slot past BAL_FIXED_ARGC.
  BalancedRulesParams params_a = parse_params(argv, BAL_FIXED_ARGC + 1);
  BalancedRulesParams params_b = parse_params(argv, BAL_FIXED_ARGC + 1 + BAL_PARAM_ARGC);

  balanced_rules_set_params(PLAYER_A, &params_a);
  balanced_rules_set_params(PLAYER_B, &params_b);

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
  balanced_rules_reset_params();

  return EXIT_SUCCESS;
} // main
