// calib_mulligan.c
// Batch harness for the mulligan/seat-advantage investigation (Next Up item
// 2, doc/oracle_roadmap.md) -- links the engine directly, same pattern as
// every aicalibsrc/<agent>/calib_<agent>.c harness, so a driver script can
// call run_simulation() many times in-process (no subprocess-spawn or
// text-parsing overhead per run) and get back one clean CSV line per run.
//
// Unlike every other calib_*.c harness, this one is not tuning an AI
// agent's playing strength -- it overrides mulligan_get_max_cards()
// (ai_strat_lib_heuristics.c/.h), a single SHARED game-rule parameter every
// agent's mulligan hook reads (except A10 IS-MCTS, whose mulligan search is
// a fixed enumeration, not driven by this value -- see
// ai_strat_lib_heuristics.h's comment on mulligan_get_max_cards()). So
// there are no per-player params to parse, just the one rule value applied
// to both seats.
//
// Usage:
//   calib_mulligan <numsim> <seed> <agent_a> <agent_b> <max_cards>
//
// agent_a/agent_b are the usual -A/--ai shorthands (e.g. "rand", "balanced",
// "hbt"). max_cards overrides mulligan_get_max_cards() for both seats for
// the duration of this process (must be <= MULLIGAN_HARD_CAP).
//
//   calib_mulligan --print-defaults
//
// --print-defaults dumps the compiled MULLIGAN_DEFAULT_MAX_CARDS as JSON and
// exits, so calibrate_mulligan.py never hardcodes its own copy.
//
// Output: one CSV line to stdout, no header (a driver script supplies its
// own so it can run this many times and concatenate):
//   numsim,seed,agent_a,agent_b,max_cards,wins_a,wins_b,draws

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_lib_heuristics.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

#define MUL_FIXED_ARGC 5 // numsim, seed, agent_a, agent_b, max_cards

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_mulligan: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void print_usage(const char* prog)
{ fprintf(stderr,
          "Usage: %s <numsim> <seed> <agent_a> <agent_b> <max_cards>\n"
          "   or: %s --print-defaults\n",
          prog, prog);
} // print_usage

// Dumps the shipped default as JSON, matching every calib_<agent>.c's
// --print-defaults so calibrate_mulligan.py never hardcodes its own copy.
// Calling mulligan_get_max_cards() before any mulligan_set_max_cards() in
// this process returns the true compiled MULLIGAN_DEFAULT_MAX_CARDS
// (ai_strat_lib_heuristics.c) -- no separate accessor needed for this.
static void print_defaults_json(void)
{ printf("{\n  \"max_cards\": %d\n}\n", mulligan_get_max_cards());
} // print_defaults_json

int main(int argc, char** argv)
{ if(argc == 2 && strcmp(argv[1], "--print-defaults") == 0)
  { print_defaults_json();
    return EXIT_SUCCESS;
  }

  if(argc != MUL_FIXED_ARGC + 1)
  { print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint16_t numsim = (uint16_t)oraclemin(strtoul(argv[1], NULL, 10), MAX_NUMBER_OF_SIM);
  uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 10);
  AIStrategyType agent_a = parse_agent_or_die(argv[3]);
  AIStrategyType agent_b = parse_agent_or_die(argv[4]);
  uint8_t max_cards = (uint8_t)strtoul(argv[5], NULL, 10);

  if(max_cards > MULLIGAN_HARD_CAP)
  { fprintf(stderr, "calib_mulligan: max_cards %u exceeds MULLIGAN_HARD_CAP (%d)\n",
            max_cards, MULLIGAN_HARD_CAP);
    return EXIT_FAILURE;
  }

  mulligan_set_max_cards(max_cards);

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

  printf("%u,%u,%s,%s,%u,%u,%u,%u\n",
         numsim, seed, argv[3], argv[4], max_cards,
         gstats.cumul_player_wins[PLAYER_A], gstats.cumul_player_wins[PLAYER_B],
         gstats.cumul_number_of_draws);

  free_strategy_set(strategies);
  destroy_game_context(ctx);
  mulligan_reset_max_cards();

  return EXIT_SUCCESS;
} // main
