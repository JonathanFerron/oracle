// calib_ismctsnn_timing.c
// A11 IS-MCTS+NN per-decision timing harness -- mirrors
// aicalibsrc/ismcts/calib_ismcts_timing.c's exact structure (timing lives
// only in harnesses like this one, never in src/), but for A11's
// leaf_value() blend instead of A10's plain rollout. Exists specifically
// to answer about.md's flagged question before any real Stage 3 sweep:
// NN-eval-per-leaf has a different cost than heuristic-rollout-to-terminal,
// so the same limit_iterations=4000 budget that's ~1s/decision-scale cheap
// for A10 may not be for A11 at a given nn_value_trust -- measure before
// committing to a large-n calibration run, not after.
//
// Usage: calib_ismctsnn_timing <weights_path> <trust> <limit_iterations> <numgames> <seed>

#define _POSIX_C_SOURCE 199309L // clock_gettime()/CLOCK_MONOTONIC -- -std=c23 hides these otherwise

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/core/game_constants.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_ismctsnn.h"
#include "../../src/roles/stda/stda_auto.h"

#define MAX_SAMPLES 200000

static double g_samples[MAX_SAMPLES];
static uint16_t g_sample_turn[MAX_SAMPLES];
static uint32_t g_sample_count = 0;

static double now_seconds(void)
{ struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
} // now_seconds

static void record_sample(const struct gamestate* gstate, double elapsed)
{ if(g_sample_count >= MAX_SAMPLES) return;
  g_samples[g_sample_count] = elapsed;
  g_sample_turn[g_sample_count] = gstate->turn;
  g_sample_count++;
} // record_sample

static void timed_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ double start = now_seconds();
  ismctsnn_attack_strategy(gstate, ctx);
  record_sample(gstate, now_seconds() - start);
} // timed_attack_strategy

static void timed_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ double start = now_seconds();
  ismctsnn_defense_strategy(gstate, ctx);
  record_sample(gstate, now_seconds() - start);
} // timed_defense_strategy

static int compare_double(const void* a, const void* b)
{ double da = *(const double*)a, db = *(const double*)b;
  return (da > db) - (da < db);
} // compare_double

static double percentile(const double* sorted, uint32_t n, double p)
{ if(n == 0) return 0.0;
  uint32_t idx = (uint32_t)(p * (double)(n - 1));
  return sorted[idx];
} // percentile

static void report(const char* label, double* samples_copy, uint32_t n)
{ if(n == 0)
  { printf("%-16s: no samples\n", label);
    return;
  }
  double sum = 0.0;
  for(uint32_t i = 0; i < n; i++) sum += samples_copy[i];
  qsort(samples_copy, n, sizeof(double), compare_double);
  printf("%-16s: n=%-6u mean=%.4fs median=%.4fs p95=%.4fs min=%.4fs max=%.4fs\n",
         label, n, sum / (double)n, percentile(samples_copy, n, 0.5),
         percentile(samples_copy, n, 0.95), samples_copy[0], samples_copy[n - 1]);
} // report

static StrategySet* build_timed_strategy_set(void)
{ StrategySet* strategies = create_strategy_set();
  set_player_strategy_by_type(strategies, PLAYER_A, AI_STRATEGY_ISMCTS_NN);
  set_player_strategy_by_type(strategies, PLAYER_B, AI_STRATEGY_ISMCTS_NN);
  strategies->attack_strategy[PLAYER_A] = timed_attack_strategy;
  strategies->attack_strategy[PLAYER_B] = timed_attack_strategy;
  strategies->defense_strategy[PLAYER_A] = timed_defense_strategy;
  strategies->defense_strategy[PLAYER_B] = timed_defense_strategy;
  return strategies;
} // build_timed_strategy_set

static void report_all_splits(void)
{ double* all = (double*)malloc(sizeof(double) * g_sample_count);
  double* early = (double*)malloc(sizeof(double) * g_sample_count);
  double* late = (double*)malloc(sizeof(double) * g_sample_count);
  uint32_t n_early = 0, n_late = 0;

  for(uint32_t i = 0; i < g_sample_count; i++)
  { all[i] = g_samples[i];
    if(g_sample_turn[i] <= 2) early[n_early++] = g_samples[i];
    if(g_sample_turn[i] >= 15) late[n_late++] = g_samples[i];
  }

  report("ALL", all, g_sample_count);
  report("EARLY (turn<=2)", early, n_early);
  report("LATE (turn>=15)", late, n_late);

  free(all);
  free(early);
  free(late);
} // report_all_splits

int main(int argc, char** argv)
{ if(argc != 6)
  { fprintf(stderr,
            "Usage: %s <weights_path> <trust> <limit_iterations> <numgames> <seed>\n", argv[0]);
    return EXIT_FAILURE;
  }
  const char* weights_path = argv[1];
  float trust = strtof(argv[2], NULL);
  uint32_t limit_iterations = (uint32_t)strtoul(argv[3], NULL, 10);
  uint16_t numgames = (uint16_t)strtoul(argv[4], NULL, 10);
  unsigned long seed = strtoul(argv[5], NULL, 10);

  if(trust > 0.0f && !ismctsnn_load_weights(weights_path))
  { fprintf(stderr, "calib_ismctsnn_timing: failed to load weights from '%s'\n", weights_path);
    return EXIT_FAILURE;
  }

  ISMCTSParams params = ismctsnn_get_default_params();
  params.limit_iterations = limit_iterations;
  params.nn_value_trust = trust;
  ismctsnn_set_params(PLAYER_A, &params);
  ismctsnn_set_params(PLAYER_B, &params);

  config_t cfg = {0};
  cfg.prng_seed = seed;
  GameContext* ctx = create_game_context(&cfg);
  StrategySet* strategies = build_timed_strategy_set();

  struct gamestats gstats;
  memset(&gstats, 0, sizeof(gstats));
  run_simulation(numgames, INITIAL_CASH_DEFAULT, &gstats, strategies, ctx);

  printf("weights=%s trust=%.2f limit_iterations=%u numgames=%u seed=%lu total_decisions=%u\n",
         weights_path, trust, limit_iterations, numgames, seed, g_sample_count);
  report_all_splits();

  free_strategy_set(strategies);
  destroy_game_context(ctx);
  ismctsnn_reset_params();

  return EXIT_SUCCESS;
} // main
