// calib_ismcts_timing.c
// Phase 3 timing harness for A10 IS-MCTS: measures wall-clock per-decision
// latency at a given limit_iterations, on THIS machine, entirely OUTSIDE
// src/ -- src/ itself never touches a clock (see ai_strat_ismcts1.h's header
// comment: limit_iterations is a fixed count precisely so the shipped
// agent's rating is PC-independent). Used to pick the ISMCTS_DEFAULTS.
// limit_iterations value that approximates ~1s/decision on this PC. Phase
// 5's calib_ismcts.c (the full params sweep/optimize harness, same pattern
// as aicalibsrc/simplemc/calib_simplemc.c) is a separate, later file --
// this one only answers "how many iterations fit in about a second."
//
// Usage: calib_ismcts_timing <limit_iterations> <numgames> <seed>
//
// Drives real self-play games (both seats = A10, at the given
// limit_iterations) through stda_auto.c's run_simulation(), timing every
// individual ismcts_attack_strategy()/ismcts_defense_strategy() call via a
// thin StrategySet wrapper (clock_gettime() lives HERE, not in src/, and
// never touches the timed calls' own behaviour -- it wraps them, it doesn't
// participate in their RNG/decision logic). Reports mean/median/p95 over
// all decisions, plus separate turn<=2 / turn>=15 splits to quantify the
// early-game-vs-endgame spread the implementation plan calls out (early
// rollouts run longer, since more turns typically remain to simulate).

#define _POSIX_C_SOURCE 199309L // clock_gettime()/CLOCK_MONOTONIC -- -std=c23 hides these otherwise

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/core/game_constants.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_ismcts1.h"
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
  ismcts_attack_strategy(gstate, ctx);
  record_sample(gstate, now_seconds() - start);
} // timed_attack_strategy

static void timed_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ double start = now_seconds();
  ismcts_defense_strategy(gstate, ctx);
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
  set_player_strategy_by_type(strategies, PLAYER_A, AI_STRATEGY_ISMCTS);
  set_player_strategy_by_type(strategies, PLAYER_B, AI_STRATEGY_ISMCTS);
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
{ if(argc != 4)
  { fprintf(stderr, "Usage: %s <limit_iterations> <numgames> <seed>\n", argv[0]);
    return EXIT_FAILURE;
  }
  uint32_t limit_iterations = (uint32_t)strtoul(argv[1], NULL, 10);
  uint16_t numgames = (uint16_t)strtoul(argv[2], NULL, 10);
  unsigned long seed = strtoul(argv[3], NULL, 10);

  ISMCTSParams params = ISMCTS_DEFAULTS;
  params.limit_iterations = limit_iterations;
  ismcts_set_params(PLAYER_A, &params);
  ismcts_set_params(PLAYER_B, &params);

  config_t cfg = {0};
  cfg.prng_seed = seed;
  GameContext* ctx = create_game_context(&cfg);
  StrategySet* strategies = build_timed_strategy_set();

  struct gamestats gstats;
  memset(&gstats, 0, sizeof(gstats));
  run_simulation(numgames, INITIAL_CASH_DEFAULT, &gstats, strategies, ctx);

  printf("limit_iterations=%u numgames=%u seed=%lu total_decisions=%u\n",
         limit_iterations, numgames, seed, g_sample_count);
  report_all_splits();

  free_strategy_set(strategies);
  destroy_game_context(ctx);
  return EXIT_SUCCESS;
}
