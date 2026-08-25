// calib_heuristic.c
// Calibration harness for A5 Heuristic's five tunable parameters
// (HeuristicParams -- see ai_strat_heuristic.h). Same in-process pattern as
// aicalibsrc/balanced/calib_balanced.c: links the engine directly so a
// driver script can call run_simulation() many times without a
// subprocess-spawn/text-parse round trip per data point, and prints one
// clean CSV line per run.
//
// Usage:
//   calib_heuristic <numsim> <seed> <agent_a> <agent_b>
//     <epsilon_a> <gamma_a> <delta_a> <taper_a> <discount_a>
//     <5 more of the same for Player B>
//   calib_heuristic --print-defaults
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "heuristic",
// "borealis", "rand"). All five HeuristicParams are set for both seats
// regardless of which agent actually plays there -- harmless, since a
// non-Heuristic agent never reads them.
//
// --print-defaults dumps the compiled HEURISTIC_DEFAULTS as JSON and exits,
// so calibrate_heuristic.py never hardcodes its own copy of the baseline --
// the same discipline calib_balanced.c introduced (doc/oracle_todo.md's
// drift item tracks the three earlier harnesses that still hardcode a
// Python-side DEFAULTS dict).
//
// Output (positional run): one CSV line to stdout, no header (a driver
// script supplies its own so it can run this many times and concatenate).
// Params are echoed back after parsing -- the same round-trip discipline
// that caught an argv off-by-one in the A2 harness:
//   numsim,seed,agent_a,agent_b,
//   epsilon_a,gamma_a,delta_a,taper_a,discount_a,
//   <same five for Player B>,
//   wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_heuristic.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define HEUR_PARAM_ARGC 5 // fields in HeuristicParams, one CLI arg each
#define HEUR_FIXED_ARGC 4 // numsim, seed, agent_a, agent_b

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_heuristic: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> "
          "<epsilon_a> <gamma_a> <delta_a> <taper_a> <discount_a> "
          "<same 5 for Player B>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Parses the five positional CLI args starting at argv[offset] into a
// HeuristicParams, in the struct's declared field order.
static HeuristicParams parse_params(char** argv, int offset)
{ return (HeuristicParams)
  { .weight_energy_advantage = strtof(argv[offset], NULL),
      .weight_cards_advantage = strtof(argv[offset + 1], NULL),
      .weight_cash_advantage = strtof(argv[offset + 2], NULL),
      .weight_taper_exponent = strtof(argv[offset + 3], NULL),
      .opp_card_discount = strtof(argv[offset + 4], NULL)
  };
} // parse_params

static void print_params_csv(const HeuristicParams* p)
{ printf("%.6f,%.6f,%.6f,%.6f,%.6f,",
         p->weight_energy_advantage, p->weight_cards_advantage,
         p->weight_cash_advantage, p->weight_taper_exponent, p->opp_card_discount);
} // print_params_csv

static void print_defaults_json(void)
{ HeuristicParams d = heuristic_get_default_params();
  printf("{\n"
         "  \"weight_energy_advantage\": %.6f,\n"
         "  \"weight_cards_advantage\": %.6f,\n"
         "  \"weight_cash_advantage\": %.6f,\n"
         "  \"weight_taper_exponent\": %.6f,\n"
         "  \"opp_card_discount\": %.6f\n"
         "}\n",
         d.weight_energy_advantage, d.weight_cards_advantage,
         d.weight_cash_advantage, d.weight_taper_exponent, d.opp_card_discount);
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != HEUR_FIXED_ARGC + 2 * HEUR_PARAM_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);

  // argv[1..HEUR_FIXED_ARGC] are numsim/seed/agent_a/agent_b, so the first
  // param block starts one slot past HEUR_FIXED_ARGC.
  HeuristicParams params_a = parse_params(argv, HEUR_FIXED_ARGC + 1);
  HeuristicParams params_b = parse_params(argv, HEUR_FIXED_ARGC + 1 + HEUR_PARAM_ARGC);

  heuristic_set_params(PLAYER_A, &params_a);
  heuristic_set_params(PLAYER_B, &params_b);

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
  heuristic_reset_params();

  return EXIT_SUCCESS;
} // main
