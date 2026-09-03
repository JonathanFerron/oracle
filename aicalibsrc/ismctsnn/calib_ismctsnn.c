// calib_ismctsnn.c
// A11 IS-MCTS+NN ("AlphaOracle Prime") Stage 3 calibration harness -- see
// ideas/A11 ai agent is-mcts + nn (alphaoracle prime)/about.md's "Confirmed
// plan" step 3 (two-gate measurement). Structure ported directly from
// aicalibsrc/carto/calib_a13.c (same in-process run_simulation() pattern,
// CSV-per-invocation output, --print-defaults JSON dump) -- see this
// folder's README for why A13's calibration record is the direct precedent
// for how this agent gets measured.
//
// Usage:
//   calib_ismctsnn <weights_path> <numsim> <seed> <agent_a> <agent_b>
//     <20 ISMCTSParams fields for Player A, ai_strat_ismcts1.h's declared order>
//     <same 20 for Player B>
//   calib_ismctsnn --print-defaults
//
// Both ismcts_set_params() (A10's own copy) AND ismctsnn_set_params() (A11's)
// are set per seat from the SAME parsed 20-field block regardless of which
// agent actually plays there -- harmless (a non-active registry is simply
// never read), same "harness exposes everything" convention as calib_a13.c.
// `weights_path` is always loaded (even for an ismcts-vs-ismcts sanity run)
// since it's a fixed positional argument -- pass any valid exported weights
// file; wasted if neither seat is ismctsnn.
//
// Output: one CSV line to stdout, no header:
//   numsim,seed,agent_a,agent_b,<20 params A>,<20 params B>,wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_ismcts1.h"
#include "../../src/ai_strat/ai_strat_ismctsnn.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define ISMCTSNN_PARAM_ARGC 20 // fields in ISMCTSParams, ai_strat_ismcts1.h's declared order
#define ISMCTSNN_FIXED_ARGC 5  // weights_path, numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_ismctsnn: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <weights_path> <numsim> <seed> <agent_a> <agent_b> "
          "<20 ISMCTSParams fields, ai_strat_ismcts1.h's declared order> "
          "<same 20 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses one seat's 20-field ISMCTSParams block starting at argv[offset],
// in the struct's declared order (ai_strat_ismcts1.h).
static ISMCTSParams parse_params(char** argv, int offset)
{ return (ISMCTSParams)
  { .limit_iterations = (uint32_t)strtoul(argv[offset], NULL, 10),
      .limit_playout_steps = (uint32_t)strtoul(argv[offset + 1], NULL, 10),
      .limit_max_nodes = (uint32_t)strtoul(argv[offset + 2], NULL, 10),
      .limit_recall_variants = (uint8_t)strtoul(argv[offset + 3], NULL, 10),
      .limit_cash_variants = (uint8_t)strtoul(argv[offset + 4], NULL, 10),
      .limit_max_candidates = (uint8_t)strtoul(argv[offset + 5], NULL, 10),
      .search_exploration_constant = strtof(argv[offset + 6], NULL),
      .search_use_availability = strtol(argv[offset + 7], NULL, 10) != 0,
      .search_expand_threshold = (uint16_t)strtoul(argv[offset + 8], NULL, 10),
      .threshold_widening_k = strtof(argv[offset + 9], NULL),
      .threshold_widening_alpha = strtof(argv[offset + 10], NULL),
      .prior_use_heuristic = strtol(argv[offset + 11], NULL, 10) != 0,
      .rollout_max_turns = (uint16_t)strtoul(argv[offset + 12], NULL, 10),
      .rollout_cutoff_depth = (uint16_t)strtoul(argv[offset + 13], NULL, 10),
      .weight_energy_advantage = strtof(argv[offset + 14], NULL),
      .weight_cash_advantage = strtof(argv[offset + 15], NULL),
      .weight_hand_advantage = strtof(argv[offset + 16], NULL),
      .limit_flat_iterations = (uint32_t)strtoul(argv[offset + 17], NULL, 10),
      .limit_flat_candidates = (uint8_t)strtoul(argv[offset + 18], NULL, 10),
      .nn_value_trust = strtof(argv[offset + 19], NULL)
  };
} // parse_params

static void print_params_csv(const ISMCTSParams* p)
{ printf("%u,%u,%u,%u,%u,%u,%.6f,%d,%u,%.6f,%.6f,%d,%u,%u,%.6f,%.6f,%.6f,%u,%u,%.6f,",
         p->limit_iterations, p->limit_playout_steps, p->limit_max_nodes,
         p->limit_recall_variants, p->limit_cash_variants, p->limit_max_candidates,
         p->search_exploration_constant, p->search_use_availability ? 1 : 0,
         p->search_expand_threshold, p->threshold_widening_k, p->threshold_widening_alpha,
         p->prior_use_heuristic ? 1 : 0, p->rollout_max_turns, p->rollout_cutoff_depth,
         p->weight_energy_advantage, p->weight_cash_advantage, p->weight_hand_advantage,
         p->limit_flat_iterations, p->limit_flat_candidates, p->nn_value_trust);
} // print_params_csv

// --print-defaults dumps ismctsnn_get_default_params() (ISMCTS_DEFAULTS
// with nn_value_trust=1.0 -- Stage 3's first testable point, see
// ai_strat_ismctsnn.h) as flat JSON so calibrate_ismctsnn.py's DEFAULTS
// never drifts from the shipped C constants.
static void print_defaults_json(void)
{ ISMCTSParams d = ismctsnn_get_default_params();
  printf("{\n"
         "  \"limit_iterations\": %u,\n"
         "  \"limit_playout_steps\": %u,\n"
         "  \"limit_max_nodes\": %u,\n"
         "  \"limit_recall_variants\": %u,\n"
         "  \"limit_cash_variants\": %u,\n"
         "  \"limit_max_candidates\": %u,\n"
         "  \"search_exploration_constant\": %.6f,\n"
         "  \"search_use_availability\": %s,\n"
         "  \"search_expand_threshold\": %u,\n"
         "  \"threshold_widening_k\": %.6f,\n"
         "  \"threshold_widening_alpha\": %.6f,\n"
         "  \"prior_use_heuristic\": %s,\n"
         "  \"rollout_max_turns\": %u,\n"
         "  \"rollout_cutoff_depth\": %u,\n"
         "  \"weight_energy_advantage\": %.6f,\n"
         "  \"weight_cash_advantage\": %.6f,\n"
         "  \"weight_hand_advantage\": %.6f,\n"
         "  \"limit_flat_iterations\": %u,\n"
         "  \"limit_flat_candidates\": %u,\n"
         "  \"nn_value_trust\": %.6f\n"
         "}\n",
         d.limit_iterations, d.limit_playout_steps, d.limit_max_nodes,
         d.limit_recall_variants, d.limit_cash_variants, d.limit_max_candidates,
         d.search_exploration_constant, d.search_use_availability ? "true" : "false",
         d.search_expand_threshold, d.threshold_widening_k, d.threshold_widening_alpha,
         d.prior_use_heuristic ? "true" : "false", d.rollout_max_turns, d.rollout_cutoff_depth,
         d.weight_energy_advantage, d.weight_cash_advantage, d.weight_hand_advantage,
         d.limit_flat_iterations, d.limit_flat_candidates, d.nn_value_trust);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != ISMCTSNN_FIXED_ARGC + 2 * ISMCTSNN_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  const char* weights_path = argv[1];
  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[2], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[3], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[4]);
  AIStrategyType agent_b = parse_agent_or_die(argv[5]);

  if(!ismctsnn_load_weights(weights_path))
  { fprintf(stderr, "calib_ismctsnn: failed to load weights from '%s'\n", weights_path);
    return EXIT_FAILURE;
  }

  // argv[1..ISMCTSNN_FIXED_ARGC] are weights_path/numsim/seed/agent_a/agent_b,
  // so the first param block starts one slot past ISMCTSNN_FIXED_ARGC.
  ISMCTSParams params_a = parse_params(argv, ISMCTSNN_FIXED_ARGC + 1);
  ISMCTSParams params_b = parse_params(argv, ISMCTSNN_FIXED_ARGC + 1 + ISMCTSNN_PARAM_ARGC);

  // A10 and A11 share ONE ISMCTSParams struct and ONE search function
  // (ismcts_search_best_move()) -- nn_value_trust lives inside that shared
  // struct, so unlike A13's genuinely-disjoint-struct precedent, a seat
  // playing plain "ismcts" is NOT immune to whatever trust value happens to
  // be sitting in the block parsed for it (e.g. the sweep/validate driver's
  // baseline side is built from DEFAULTS, which carries nn_value_trust=1.0
  // -- see ai_strat_ismctsnn.h's ismctsnn_get_default_params()). Force it to
  // 0.0f for the ismcts registry specifically, so an "ismcts" seat is always
  // genuinely plain A10 regardless of what candidate trust is under test on
  // the other seat; ismctsnn_set_params() keeps the real value being tested.
  ISMCTSParams ismcts_params_a = params_a;
  ismcts_params_a.nn_value_trust = 0.0f;
  ISMCTSParams ismcts_params_b = params_b;
  ismcts_params_b.nn_value_trust = 0.0f;
  ismcts_set_params(PLAYER_A, &ismcts_params_a);
  ismcts_set_params(PLAYER_B, &ismcts_params_b);
  ismctsnn_set_params(PLAYER_A, &params_a);
  ismctsnn_set_params(PLAYER_B, &params_b);

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

  printf("%u,%u,%s,%s,", numsim, seed, argv[4], argv[5]);
  print_params_csv(&params_a);
  print_params_csv(&params_b);
  printf("%u,%u,%u\n",
         gstats.cumul_player_wins[PLAYER_A], gstats.cumul_player_wins[PLAYER_B],
         gstats.cumul_number_of_draws);

  free_strategy_set(strategies);
  destroy_game_context(ctx);
  ismcts_reset_params();
  ismctsnn_reset_params();

  return EXIT_SUCCESS;
} // main
