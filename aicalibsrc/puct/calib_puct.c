// calib_puct.c
// A14 AlphaOracle Prime Plus I Stage 5 calibration harness -- see
// doc/ai_agents.md's A14 section and aicalibsrc/puct/README.md. Structure
// ported from aicalibsrc/ismctsnn/calib_ismctsnn.c (same in-process
// run_simulation() pattern, CSV-per-invocation output, --print-defaults
// JSON dump).
//
// Usage:
//   calib_puct <weights_path> <numsim> <seed> <agent_a> <agent_b>
//     <21 ISMCTSParams fields for Player A, ai_strat_ismcts1.h's declared order>
//     <8 PUCTParams fields for Player A, ai_strat_puct.h's declared order>
//     <same 21+8 for Player B>
//   calib_puct --print-defaults
//
// Unlike calib_ismctsnn.c, this harness does NOT blanket-set every agent's
// registry from one parsed block -- it applies each seat's parsed
// ISMCTSParams/PUCTParams ONLY to the registry matching that seat's REAL
// agent type (agent_a/agent_b), via apply_seat_params() below. This
// sidesteps the documented shared-struct gotcha (aicalibsrc/ismctsnn/README.md)
// structurally rather than needing a one-off nn_value_trust=0.0f patch per
// registry -- with THREE agents now sharing ISMCTSParams (A10, A11, A14), a
// blanket-set pattern would need three separate patches to stay safe;
// per-identity application needs none, by construction.
//
// weights_path is always loaded (even for an ismcts-vs-ismcts sanity run),
// same convention as calib_ismctsnn.c -- pass any valid exported
// export_puct_weights.py output; wasted if neither seat is puct.
//
// Output: one CSV line to stdout, no header:
//   numsim,seed,agent_a,agent_b,<21 ISMCTSParams A>,<8 PUCTParams A>,
//   <21 ISMCTSParams B>,<8 PUCTParams B>,wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_ismcts1.h"
#include "../../src/ai_strat/ai_strat_ismctsnn.h"
#include "../../src/ai_strat/ai_strat_puct.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define PUCT_ISMCTS_PARAM_ARGC 21 // fields in ISMCTSParams, ai_strat_ismcts1.h's declared order
#define PUCT_PARAM_ARGC 8 // fields in PUCTParams, ai_strat_puct.h's declared order
#define PUCT_SEAT_ARGC (PUCT_ISMCTS_PARAM_ARGC + PUCT_PARAM_ARGC)
#define PUCT_FIXED_ARGC 5 // weights_path, numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_puct: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <weights_path> <numsim> <seed> <agent_a> <agent_b> "
          "<21 ISMCTSParams + 8 PUCTParams fields for Player A> "
          "<same 29 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses one seat's 21-field ISMCTSParams block, in the struct's declared
// order (ai_strat_ismcts1.h) -- identical field order to calib_ismctsnn.c's
// own parse_params().
static ISMCTSParams parse_ismcts_params(char** argv, int offset)
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
      .nn_value_trust = strtof(argv[offset + 19], NULL),
      .nn_value_use_mover_seat = strtol(argv[offset + 20], NULL, 10) != 0
  };
} // parse_ismcts_params

// Parses one seat's 8-field PUCTParams block, in the struct's declared
// order (ai_strat_puct.h).
static PUCTParams parse_puct_params(char** argv, int offset)
{ return (PUCTParams)
  { .use_puct = strtol(argv[offset], NULL, 10) != 0,
      .c_puct = strtof(argv[offset + 1], NULL),
      .fpu_reduction = strtof(argv[offset + 2], NULL),
      .prior_trust = strtof(argv[offset + 3], NULL),
      .policy_temperature = strtof(argv[offset + 4], NULL),
      .use_widening = strtol(argv[offset + 5], NULL, 10) != 0,
      .root_dirichlet_alpha = strtof(argv[offset + 6], NULL),
      .root_noise_frac = strtof(argv[offset + 7], NULL)
  };
} // parse_puct_params

static void print_ismcts_params_csv(const ISMCTSParams* p)
{ printf("%u,%u,%u,%u,%u,%u,%.6f,%d,%u,%.6f,%.6f,%d,%u,%u,%.6f,%.6f,%.6f,%u,%u,%.6f,%d,",
         p->limit_iterations, p->limit_playout_steps, p->limit_max_nodes,
         p->limit_recall_variants, p->limit_cash_variants, p->limit_max_candidates,
         p->search_exploration_constant, p->search_use_availability ? 1 : 0,
         p->search_expand_threshold, p->threshold_widening_k, p->threshold_widening_alpha,
         p->prior_use_heuristic ? 1 : 0, p->rollout_max_turns, p->rollout_cutoff_depth,
         p->weight_energy_advantage, p->weight_cash_advantage, p->weight_hand_advantage,
         p->limit_flat_iterations, p->limit_flat_candidates, p->nn_value_trust,
         p->nn_value_use_mover_seat ? 1 : 0);
} // print_ismcts_params_csv

static void print_puct_params_csv(const PUCTParams* p)
{ printf("%d,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,",
         p->use_puct ? 1 : 0, p->c_puct, p->fpu_reduction, p->prior_trust,
         p->policy_temperature, p->use_widening ? 1 : 0, p->root_dirichlet_alpha,
         p->root_noise_frac);
} // print_puct_params_csv

// --print-defaults dumps puct_get_default_ismcts_params() +
// puct_get_default_params() as flat JSON so calibrate_puct.py's DEFAULTS
// never drifts from the shipped C constants.
static void print_defaults_json(void)
{ ISMCTSParams ip = puct_get_default_ismcts_params();
  PUCTParams pp = puct_get_default_params();
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
         "  \"nn_value_trust\": %.6f,\n"
         "  \"nn_value_use_mover_seat\": %s,\n"
         "  \"use_puct\": %s,\n"
         "  \"c_puct\": %.6f,\n"
         "  \"fpu_reduction\": %.6f,\n"
         "  \"prior_trust\": %.6f,\n"
         "  \"policy_temperature\": %.6f,\n"
         "  \"use_widening\": %s,\n"
         "  \"root_dirichlet_alpha\": %.6f,\n"
         "  \"root_noise_frac\": %.6f\n"
         "}\n",
         ip.limit_iterations, ip.limit_playout_steps, ip.limit_max_nodes,
         ip.limit_recall_variants, ip.limit_cash_variants, ip.limit_max_candidates,
         ip.search_exploration_constant, ip.search_use_availability ? "true" : "false",
         ip.search_expand_threshold, ip.threshold_widening_k, ip.threshold_widening_alpha,
         ip.prior_use_heuristic ? "true" : "false", ip.rollout_max_turns,
         ip.rollout_cutoff_depth, ip.weight_energy_advantage, ip.weight_cash_advantage,
         ip.weight_hand_advantage, ip.limit_flat_iterations, ip.limit_flat_candidates,
         ip.nn_value_trust, ip.nn_value_use_mover_seat ? "true" : "false",
         pp.use_puct ? "true" : "false", pp.c_puct, pp.fpu_reduction, pp.prior_trust,
         pp.policy_temperature, pp.use_widening ? "true" : "false", pp.root_dirichlet_alpha,
         pp.root_noise_frac);
} // print_defaults_json

// Applies `ip`/`pp` ONLY to the registry matching `type`'s real identity --
// see this file's own header comment for why this differs from
// calib_ismctsnn.c's blanket-set-every-registry pattern.
static void apply_seat_params(PlayerID seat, AIStrategyType type,
                              const ISMCTSParams* ip, const PUCTParams* pp)
{ switch(type)
  { case AI_STRATEGY_ISMCTS:
    { ISMCTSParams safe = *ip;
      safe.nn_value_trust = 0.0f; // A10 must never inherit a stray trust value
      ismcts_set_params(seat, &safe);
      break;
    }
    case AI_STRATEGY_ISMCTS_NN:
      ismctsnn_set_params(seat, ip);
      break;
    case AI_STRATEGY_ISMCTS_PUCT:
      puct_set_ismcts_params(seat, ip);
      puct_set_params(seat, pp);
      break;
    default:
      break; // this agent doesn't read ISMCTSParams/PUCTParams at all
  }
} // apply_seat_params

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != PUCT_FIXED_ARGC + 2 * PUCT_SEAT_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  const char* weights_path = argv[1];
  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[2], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[3], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[4]);
  AIStrategyType agent_b = parse_agent_or_die(argv[5]);

  if(!puct_load_weights(weights_path))
  { fprintf(stderr, "calib_puct: failed to load weights from '%s'\n", weights_path);
    return EXIT_FAILURE;
  }

  int offset_a = PUCT_FIXED_ARGC + 1;
  int offset_b = offset_a + PUCT_SEAT_ARGC;
  ISMCTSParams ismcts_a = parse_ismcts_params(argv, offset_a);
  PUCTParams puct_a = parse_puct_params(argv, offset_a + PUCT_ISMCTS_PARAM_ARGC);
  ISMCTSParams ismcts_b = parse_ismcts_params(argv, offset_b);
  PUCTParams puct_b = parse_puct_params(argv, offset_b + PUCT_ISMCTS_PARAM_ARGC);

  apply_seat_params(PLAYER_A, agent_a, &ismcts_a, &puct_a);
  apply_seat_params(PLAYER_B, agent_b, &ismcts_b, &puct_b);

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
  print_ismcts_params_csv(&ismcts_a);
  print_puct_params_csv(&puct_a);
  print_ismcts_params_csv(&ismcts_b);
  print_puct_params_csv(&puct_b);
  printf("%u,%u,%u\n",
         gstats.cumul_player_wins[PLAYER_A], gstats.cumul_player_wins[PLAYER_B],
         gstats.cumul_number_of_draws);

  free_strategy_set(strategies);
  destroy_game_context(ctx);
  ismcts_reset_params();
  ismctsnn_reset_params();
  puct_reset_ismcts_params();
  puct_reset_params();

  return EXIT_SUCCESS;
} // main
