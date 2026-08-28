// calib_ismcts_efficiency.c
// Lean Phase 5 calibration harness for A10 IS-MCTS's four EFFICIENCY dials
// (search_exploration_constant, threshold_widening_k, threshold_widening_alpha,
// search_expand_threshold) -- everything else stays at ISMCTS_DEFAULTS,
// including a small BUDGET override for fast sweeps (this is a decision-
// quality question, not a compute-budget one -- see about.md/ai_strat_ismcts1.h
// for why budget dials are swept, never optimized, on this roster).
// Deliberately not the full scipy-differential-evolution pipeline every
// earlier agent's aicalibsrc/<agent>/ has (see the implementation plan's
// Phase 5 status note for why): a direct win-rate comparison over a handful
// of hand-picked candidates is enough to sanity-check the principled UCT/
// progressive-widening defaults before Phase 6's real rating measurement.
//
// Usage:
//   calib_ismcts_efficiency <exploration_c> <widening_k> <widening_alpha>
//     <expand_threshold> <limit_iterations> <numgames> <seed> <opponent>
//
// Plays A10 (Player A, with the given overrides) vs <opponent> (Player B,
// its own default params), both seats each numgames/2 games via
// run_simulation(), and prints a CSV line: the four dials, limit_iterations,
// numgames, seed, opponent, wins_a, wins_b, draws.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_ismcts1.h"
#include "../../src/ui/shared/player_config.h"
#include "../../src/roles/stda/stda_auto.h"

static AIStrategyType parse_agent_or_die(const char* arg)
{ AIStrategyType type = parse_ai_strategy_shorthand(arg);
  if(type == AI_STRATEGY_COUNT)
  { fprintf(stderr, "calib_ismcts_efficiency: unknown agent '%s'\n", arg);
    exit(EXIT_FAILURE);
  }
  return type;
} // parse_agent_or_die

static void run_half(uint16_t numgames, AIStrategyType a, AIStrategyType b,
                     GameContext* ctx, uint32_t* wins_a, uint32_t* wins_b, uint32_t* draws)
{ StrategySet* strategies = create_strategy_set();
  set_player_strategy_by_type(strategies, PLAYER_A, a);
  set_player_strategy_by_type(strategies, PLAYER_B, b);

  struct gamestats gstats;
  memset(&gstats, 0, sizeof(gstats));
  run_simulation(numgames, INITIAL_CASH_DEFAULT, &gstats, strategies, ctx);

  *wins_a += (a == AI_STRATEGY_ISMCTS) ? gstats.cumul_player_wins[PLAYER_A]
             : gstats.cumul_player_wins[PLAYER_B];
  *wins_b += (a == AI_STRATEGY_ISMCTS) ? gstats.cumul_player_wins[PLAYER_B]
             : gstats.cumul_player_wins[PLAYER_A];
  *draws += gstats.cumul_number_of_draws;

  free_strategy_set(strategies);
} // run_half

int main(int argc, char** argv)
{ if(argc != 9)
  { fprintf(stderr, "Usage: %s <exploration_c> <widening_k> <widening_alpha> "
                      "<expand_threshold> <limit_iterations> <numgames> <seed> <opponent>\n", argv[0]);
    return EXIT_FAILURE;
  }

  float exploration_c = strtof(argv[1], NULL);
  float widening_k = strtof(argv[2], NULL);
  float widening_alpha = strtof(argv[3], NULL);
  uint16_t expand_threshold = (uint16_t)strtoul(argv[4], NULL, 10);
  uint32_t limit_iterations = (uint32_t)strtoul(argv[5], NULL, 10);
  uint16_t numgames = (uint16_t)strtoul(argv[6], NULL, 10);
  unsigned long seed = strtoul(argv[7], NULL, 10);
  AIStrategyType opponent = parse_agent_or_die(argv[8]);

  ISMCTSParams params = ISMCTS_DEFAULTS;
  params.search_exploration_constant = exploration_c;
  params.threshold_widening_k = widening_k;
  params.threshold_widening_alpha = widening_alpha;
  params.search_expand_threshold = expand_threshold;
  params.limit_iterations = limit_iterations;
  ismcts_set_params(PLAYER_A, &params);
  ismcts_set_params(PLAYER_B, &params); // symmetric -- either seat may play ISMCTS below

  config_t cfg = {0};
  cfg.prng_seed = seed;
  GameContext* ctx = create_game_context(&cfg);

  uint32_t wins_a = 0, wins_b = 0, draws = 0;
  run_half((uint16_t)(numgames / 2), AI_STRATEGY_ISMCTS, opponent, ctx, &wins_a, &wins_b, &draws);
  run_half((uint16_t)(numgames / 2), opponent, AI_STRATEGY_ISMCTS, ctx, &wins_a, &wins_b, &draws);

  printf("%.4f,%.4f,%.4f,%u,%u,%u,%lu,%s,%u,%u,%u\n",
         (double)exploration_c, (double)widening_k, (double)widening_alpha, expand_threshold,
         limit_iterations, numgames, seed, argv[8], wins_a, wins_b, draws);

  ismcts_reset_params();
  destroy_game_context(ctx);
  return EXIT_SUCCESS;
}
