// rating_batch.c
// Order-independent batch MLE fit over accumulated match results -- the
// round-robin benchmark's rating engine (stda_rating.c). Two solvers
// share the same accumulated win/games matrices:
//
//   RATING_BATCH_MM       Minorization-Maximization (Zermelo/Newman/Hunter)
//                         fixed point: s_i <- W_i / sum_j N_ij/(s_i+s_j).
//                         Parameter-free, monotonically increases the
//                         log-likelihood every iteration. Default.
//   RATING_BATCH_GRADIENT the v2 spec's gradient ascent, kept for
//                         cross-checking against MM -- with its
//                         hardcoded learning rate moved into RatingConfig,
//                         its convergence check moved to run *after*
//                         renormalisation (the spec compared unnormalised
//                         new values against normalised old ones), and
//                         its gradient normalised by total game count
//                         (the spec's raw gradient scales with dataset
//                         size, so a fixed learning_rate that is stable
//                         on a handful of games diverges outright on the
//                         round-robin benchmark's thousands per pairing --
//                         confirmed diverging in testing before this fix).
//
// Both maximize the Bradley-Terry log-likelihood
//   L = sum_ij  W_ij * log( s_i / (s_i + s_j) )
// and renormalise every iteration so the Borealis anchor stays at
// s = 1.0 (rating 50) by construction, not just at the end.

#include "rating.h"
#include "rating_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct RatingBatchData
{ double wins[RATING_MAX_ENTRANTS][RATING_MAX_ENTRANTS];  // wins[i][j] = i's wins over j
  double games[RATING_MAX_ENTRANTS][RATING_MAX_ENTRANTS]; // symmetric total games
};

RatingBatchData* rating_batch_create(void)
{ RatingBatchData* batch = malloc(sizeof(RatingBatchData));
  if(batch) rating_batch_reset(batch);
  return batch;
} // rating_batch_create

void rating_batch_destroy(RatingBatchData* batch)
{ free(batch);
} // rating_batch_destroy

void rating_batch_reset(RatingBatchData* batch)
{ memset(batch, 0, sizeof(RatingBatchData));
} // rating_batch_reset

// Draws contribute half a win to each side (defect fix -- the v2 spec
// inflated games without ever crediting a win for a draw, biasing both
// players' fitted strength downward).
void rating_batch_add_match(RatingBatchData* batch, const MatchResult* result)
{ uint32_t i = result->entrant1_id;
  uint32_t j = result->entrant2_id;
  if(i >= RATING_MAX_ENTRANTS || j >= RATING_MAX_ENTRANTS || i == j) return;

  double draws = (double)result->draws;
  batch->wins[i][j] += (double)result->entrant1_wins + 0.5 * draws;
  batch->wins[j][i] += (double)result->entrant2_wins + 0.5 * draws;

  double total = (double)result->entrant1_wins + (double)result->entrant2_wins + draws;
  batch->games[i][j] += total;
  batch->games[j][i] += total;
} // rating_batch_add_match

// Separation guard: undefeated/winless entrants (e.g. `rand` at 99%+ vs
// Borealis) push the raw MLE toward infinity/zero. Adds `prior_games`
// fictitious wins and losses vs a fixed strength-1.0 ghost opponent for
// every non-Borealis entrant -- implemented here as extra games directly
// against Borealis, since Borealis is renormalised to exactly 1.0 by
// every iteration regardless of what these extra games do to its own fit.
static void apply_prior(RatingBatchData* work, uint32_t n, uint32_t borealis_id,
                        double prior_games)
{ for(uint32_t i = 0; i < n; i++)
  { if(i == borealis_id) continue;
    work->wins[i][borealis_id] += prior_games;
    work->wins[borealis_id][i] += prior_games;
    work->games[i][borealis_id] += 2.0 * prior_games;
    work->games[borealis_id][i] += 2.0 * prior_games;
  }
} // apply_prior

static void compute_total_wins(uint32_t n, const double wins[][RATING_MAX_ENTRANTS],
                               double* total_wins)
{ for(uint32_t i = 0; i < n; i++)
  { total_wins[i] = 0.0;
    for(uint32_t j = 0; j < n; j++) total_wins[i] += wins[i][j];
  }
} // compute_total_wins

static void renormalize(uint32_t n, double* s, uint32_t borealis_id)
{ if(borealis_id == RATING_INVALID_ID) return;

  double factor = 1.0 / s[borealis_id];
  for(uint32_t i = 0; i < n; i++) s[i] *= factor;
} // renormalize

static void mm_iterate(RatingSystem* rs, const double games[][RATING_MAX_ENTRANTS],
                       const double* total_wins)
{ uint32_t n = rs->num_entrants;
  double s[RATING_MAX_ENTRANTS];
  for(uint32_t i = 0; i < n; i++) s[i] = 1.0;

  for(uint32_t iter = 0; iter < rs->config.max_iterations; iter++)
  { double s_new[RATING_MAX_ENTRANTS];
    for(uint32_t i = 0; i < n; i++)
    { double denom = 0.0;
      for(uint32_t j = 0; j < n; j++)
        if(j != i && games[i][j] > 0.0) denom += games[i][j] / (s[i] + s[j]);

      s_new[i] = (denom > 0.0) ? (total_wins[i] / denom) : s[i];
      if(s_new[i] < RATING_STRENGTH_FLOOR) s_new[i] = RATING_STRENGTH_FLOOR;
    }
    renormalize(n, s_new, rs->borealis_id);

    double max_change = 0.0;
    for(uint32_t i = 0; i < n; i++)
    { double change = fabs(s_new[i] - s[i]);
      if(change > max_change) max_change = change;
      s[i] = s_new[i];
    }
    if(max_change < rs->config.convergence_threshold) break;
  }

  for(uint32_t i = 0; i < n; i++) rs->entrants[i].bt_strength = s[i];
} // mm_iterate

// Total decided games across the whole dataset -- every W_ij pair
// contributes exactly 1.0 to sum(total_wins) (a decisive win credits the
// winner 1.0; a draw credits each side 0.5, still summing to 1.0), so
// this equals the total game count. Dividing the raw log-likelihood
// gradient by it (below) makes a fixed learning_rate behave the same
// regardless of how many games were played -- without this, the
// unnormalised gradient scales with the dataset size and a
// dataset-independent learning_rate either crawls (small datasets) or
// diverges outright (large ones, e.g. the round-robin benchmark's
// thousands of games per pairing -- confirmed diverging in testing).
static double total_games_played(uint32_t n, const double* total_wins)
{ double total = 0.0;
  for(uint32_t i = 0; i < n; i++) total += total_wins[i];
  return total;
} // total_games_played

static void gradient_iterate(RatingSystem* rs, const double games[][RATING_MAX_ENTRANTS],
                             const double* total_wins)
{ uint32_t n = rs->num_entrants;
  double lr = rs->config.gradient_learning_rate;
  double total_n = total_games_played(n, total_wins);
  if(total_n <= 0.0) return;

  double s[RATING_MAX_ENTRANTS];
  for(uint32_t i = 0; i < n; i++) s[i] = 1.0;

  for(uint32_t iter = 0; iter < rs->config.max_iterations; iter++)
  { double grad[RATING_MAX_ENTRANTS], s_new[RATING_MAX_ENTRANTS];
    for(uint32_t i = 0; i < n; i++)
    { grad[i] = total_wins[i] / s[i];
      for(uint32_t j = 0; j < n; j++)
        if(j != i && games[i][j] > 0.0) grad[i] -= games[i][j] / (s[i] + s[j]);
      grad[i] /= total_n;
    }
    for(uint32_t i = 0; i < n; i++)
    { s_new[i] = s[i] + lr * s[i] * grad[i];
      if(s_new[i] < RATING_STRENGTH_FLOOR) s_new[i] = RATING_STRENGTH_FLOOR;
    }
    renormalize(n, s_new, rs->borealis_id);

    double max_change = 0.0;
    for(uint32_t i = 0; i < n; i++)
    { double change = fabs(s_new[i] - s[i]);
      if(change > max_change) max_change = change;
      s[i] = s_new[i];
    }
    if(max_change < rs->config.convergence_threshold) break;
  }

  for(uint32_t i = 0; i < n; i++) rs->entrants[i].bt_strength = s[i];
} // gradient_iterate

void rating_batch_compute(RatingSystem* rs, const RatingBatchData* batch)
{ if(rs->num_entrants == 0) return;

  RatingBatchData* work = malloc(sizeof(RatingBatchData));
  if(!work) return;
  memcpy(work, batch, sizeof(RatingBatchData));

  if(rs->config.prior_games > 0.0 && rs->borealis_id != RATING_INVALID_ID)
    apply_prior(work, rs->num_entrants, rs->borealis_id, rs->config.prior_games);

  double total_wins[RATING_MAX_ENTRANTS];
  compute_total_wins(rs->num_entrants, work->wins, total_wins);

  if(rs->config.batch_method == RATING_BATCH_GRADIENT)
    gradient_iterate(rs, work->games, total_wins);
  else
    mm_iterate(rs, work->games, total_wins);

  for(uint32_t i = 0; i < rs->num_entrants; i++)
    rs->entrants[i].rating = rating_strength_to_display(rs->entrants[i].bt_strength);

  free(work);
} // rating_batch_compute
