// calib_simplemc.c
// Calibration harness for A8 Simple Monte Carlo's twenty tunable parameters
// (SimpleMcParams -- see ai_strat_simplemc1.h). Same in-process pattern as
// aicalibsrc/tactical/calib_tactical.c/aicalibsrc/hbt/calib_hbt.c: links the
// engine directly so a driver script can call run_simulation() many times
// without a subprocess-spawn/text-parse round trip per data point, and
// prints one clean CSV line per run.
//
// Usage:
//   calib_simplemc <numsim> <seed> <agent_a> <agent_b>
//     <recall_variants_a> <cash_variants_a> <max_candidates_a>
//     <seed_sims_a> <round_sims_a> <prune_zero_win_a>
//     <confidence_level_a>
//     <stage1_ratio_a> <stage2_ratio_a> <stage3_ratio_a>
//     <stage1_keep_a> <stage2_keep_a> <stage3_keep_a>
//     <stage1_sims_a> <stage2_sims_a> <stage3_sims_a>
//     <max_sims_a> <total_rollouts_a>
//     <determinize_a> <max_turns_a>
//     <20 more of the same for Player B>
//   calib_simplemc --print-defaults
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "simplemc",
// "borealis", "rand"). All twenty SimpleMcParams are set for both seats
// regardless of which agent actually plays there -- harmless, since a
// non-SimpleMC agent never reads them.
//
// --print-defaults dumps the compiled SIMPLEMC_DEFAULTS as JSON and exits,
// so calibrate_simplemc.py never hardcodes its own copy of the baseline --
// the same discipline every calibration driver since aicalibsrc/balanced/
// uses.
//
// Output (positional run): one CSV line to stdout, no header (a driver
// script supplies its own so it can run this many times and concatenate).
// Params are echoed back after parsing -- the same round-trip discipline
// that caught an argv off-by-one in the A2 harness:
//   numsim,seed,agent_a,agent_b,
//   <20 params for A>, <20 params for B>,
//   wins_a,wins_b,draws
//
// NOTE (see calibrate_simplemc.py's module docstring / README.md for the
// full discussion): unlike every prior agent's params, most of
// SimpleMcParams is a compute-budget dial, not a decision-quality weight --
// more simulations is basically always at least as strong, just slower, so
// there is no interior optimum to "calibrate" for those fields the way A1-A7's
// weights were. Only the pruning-aggressiveness fields (threshold_*,
// limit_stage*_keep, limit_*_variants/candidates) have a real efficiency
// sweet spot worth searching; the simulation-count fields are swept for a
// cost/strength curve instead, and rollout_determinize is a binary A/B, not
// a continuous dial.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_simplemc1.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define SMC_PARAM_ARGC 20 // fields in SimpleMcParams, one CLI arg each
#define SMC_FIXED_ARGC 4  // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_simplemc: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<recall_variants> <cash_variants> <max_candidates> "
          "<seed_sims> <round_sims> <prune_zero_win> "
          "<confidence_level> "
          "<stage1_ratio> <stage2_ratio> <stage3_ratio> "
          "<stage1_keep> <stage2_keep> <stage3_keep> "
          "<stage1_sims> <stage2_sims> <stage3_sims> "
          "<max_sims> <total_rollouts> "
          "<determinize> <max_turns> <same 20 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses the twenty positional CLI args starting at argv[offset] into a
// SimpleMcParams, in the struct's declared field order.
static SimpleMcParams parse_params(char** argv, int offset)
{ return (SimpleMcParams)
  { .limit_recall_variants = (uint8_t)strtol(argv[offset], NULL, 10),
      .limit_cash_variants = (uint8_t)strtol(argv[offset + 1], NULL, 10),
      .limit_max_candidates = (uint8_t)strtol(argv[offset + 2], NULL, 10),
      .rollout_seed_simulations = (uint16_t)strtol(argv[offset + 3], NULL, 10),
      .rollout_round_simulations = (uint16_t)strtol(argv[offset + 4], NULL, 10),
      .prune_zero_win_seed = (bool)strtol(argv[offset + 5], NULL, 10),
      .threshold_confidence_level = strtof(argv[offset + 6], NULL),
      .threshold_stage1_keep_ratio = strtof(argv[offset + 7], NULL),
      .threshold_stage2_keep_ratio = strtof(argv[offset + 8], NULL),
      .threshold_stage3_keep_ratio = strtof(argv[offset + 9], NULL),
      .limit_stage1_keep = (uint8_t)strtol(argv[offset + 10], NULL, 10),
      .limit_stage2_keep = (uint8_t)strtol(argv[offset + 11], NULL, 10),
      .limit_stage3_keep = (uint8_t)strtol(argv[offset + 12], NULL, 10),
      .limit_stage1_simulations = (uint16_t)strtol(argv[offset + 13], NULL, 10),
      .limit_stage2_simulations = (uint16_t)strtol(argv[offset + 14], NULL, 10),
      .limit_stage3_simulations = (uint16_t)strtol(argv[offset + 15], NULL, 10),
      .limit_max_simulations = (uint16_t)strtol(argv[offset + 16], NULL, 10),
      .limit_total_rollouts = (uint32_t)strtoul(argv[offset + 17], NULL, 10),
      .rollout_determinize = (bool)strtol(argv[offset + 18], NULL, 10),
      .rollout_max_turns = (uint16_t)strtol(argv[offset + 19], NULL, 10)
  };
} // parse_params

static void print_params_csv(const SimpleMcParams* p)
{ printf("%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,",
         p->limit_recall_variants, p->limit_cash_variants, p->limit_max_candidates,
         p->rollout_seed_simulations, p->rollout_round_simulations, p->prune_zero_win_seed,
         p->threshold_confidence_level, p->threshold_stage1_keep_ratio,
         p->threshold_stage2_keep_ratio, p->threshold_stage3_keep_ratio,
         p->limit_stage1_keep, p->limit_stage2_keep, p->limit_stage3_keep,
         p->limit_stage1_simulations, p->limit_stage2_simulations, p->limit_stage3_simulations,
         p->limit_max_simulations, p->limit_total_rollouts,
         p->rollout_determinize, p->rollout_max_turns);
} // print_params_csv

static void print_defaults_json(void)
{ SimpleMcParams d = simplemc_get_default_params();
  printf("{\n"
         "  \"limit_recall_variants\": %d,\n"
         "  \"limit_cash_variants\": %d,\n"
         "  \"limit_max_candidates\": %d,\n"
         "  \"rollout_seed_simulations\": %d,\n"
         "  \"rollout_round_simulations\": %d,\n"
         "  \"prune_zero_win_seed\": %d,\n"
         "  \"threshold_confidence_level\": %.6f,\n"
         "  \"threshold_stage1_keep_ratio\": %.6f,\n"
         "  \"threshold_stage2_keep_ratio\": %.6f,\n"
         "  \"threshold_stage3_keep_ratio\": %.6f,\n"
         "  \"limit_stage1_keep\": %d,\n"
         "  \"limit_stage2_keep\": %d,\n"
         "  \"limit_stage3_keep\": %d,\n"
         "  \"limit_stage1_simulations\": %d,\n"
         "  \"limit_stage2_simulations\": %d,\n"
         "  \"limit_stage3_simulations\": %d,\n"
         "  \"limit_max_simulations\": %d,\n"
         "  \"limit_total_rollouts\": %u,\n"
         "  \"rollout_determinize\": %d,\n"
         "  \"rollout_max_turns\": %d\n"
         "}\n",
         d.limit_recall_variants, d.limit_cash_variants, d.limit_max_candidates,
         d.rollout_seed_simulations, d.rollout_round_simulations, d.prune_zero_win_seed,
         d.threshold_confidence_level, d.threshold_stage1_keep_ratio,
         d.threshold_stage2_keep_ratio, d.threshold_stage3_keep_ratio,
         d.limit_stage1_keep, d.limit_stage2_keep, d.limit_stage3_keep,
         d.limit_stage1_simulations, d.limit_stage2_simulations, d.limit_stage3_simulations,
         d.limit_max_simulations, d.limit_total_rollouts,
         d.rollout_determinize, d.rollout_max_turns);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != SMC_FIXED_ARGC + 2 * SMC_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..SMC_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the first
  // param block starts one slot past SMC_FIXED_ARGC.
  SimpleMcParams params_a = parse_params(argv, SMC_FIXED_ARGC + 1);
  SimpleMcParams params_b = parse_params(argv, SMC_FIXED_ARGC + 1 + SMC_PARAM_ARGC);

  simplemc_set_params(PLAYER_A, &params_a);
  simplemc_set_params(PLAYER_B, &params_b);

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
  simplemc_reset_params();

  return EXIT_SUCCESS;
} // main
