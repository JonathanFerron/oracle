// rating.h
// Bradley-Terry rating system -- see doc/oracle_todo.md's "Rating System"
// item and ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/ for
// the design this ports. Ports the math and design only, not the
// prototype file -- see doc/changelog.md's dated entry for the list of
// defects fixed on port (uint8_t win-count overflow, leaderboard
// underflow, batch convergence-vs-normalisation ordering, draw handling,
// incremental update ordering/path-dependence, hardcoded learning rate,
// the file-scope-global and unprefixed-batch_* convention conflicts).
//
// Every entrant (AI agent or human) gets a rating on a 1-99 scale that
// *is* their percent win probability against Borealis (AI_STRATEGY_BOREALIS,
// see ai_strat_borealis.h) -- the Bradley-Terry anchor, fixed at strength
// 1.0 / rating 50 by definition. Two update paths share the same
// underlying strength: rating_update_match() for real-time/incremental
// play (path-dependent, fast, no history needed), rating_batch_compute()
// for an order-independent MLE fit over many results at once (see
// rating_batch.h-equivalent section below -- the round-robin benchmark
// mode uses this one).
//
// Depends only on game_types.h (for AIStrategyType) and libc --
// deliberately no src/ui/ or src/ai_strat/ dependency, so this module is
// unit-testable standalone (testsrc/test_rating.c) and mirrors why
// AIStrategyType itself lives in game_types.h rather than ui/shared/
// (game_types.h's own comment on that enum).

#ifndef RATING_H
#define RATING_H

#include <stdint.h>
#include <stdbool.h>
#include "../core/game_types.h"

#define RATING_MAX_ENTRANTS 64
#define RATING_NAME_LEN 64
#define BOREALIS_RATING 50
#define RATING_MIN 1
#define RATING_MAX 99
#define RATING_INVALID_ID UINT32_MAX

// Kind of rated entrant. Human entrants use `strategy` only as an unread
// placeholder (set to AI_STRATEGY_COUNT, the enum's own "not applicable"
// sentinel value -- see parse_ai_strategy_shorthand()'s use of it).
typedef enum
{ RATING_ENTRANT_AI = 0,
  RATING_ENTRANT_HUMAN = 1
} RatingEntrantKind;

typedef struct
{ uint32_t id;
  char name[RATING_NAME_LEN];
  RatingEntrantKind kind;
  AIStrategyType strategy;    // meaningful only when kind == RATING_ENTRANT_AI
  double bt_strength;         // Borealis == 1.0 by definition (the gauge fix)
  int32_t rating;             // display rating, 1-99, derived from bt_strength
  uint32_t games_played;
  uint32_t games_won;
  uint32_t games_drawn;
} RatingEntry;

// One aggregated result between two entrants -- may represent a single
// game (1 win, 0 losses) or a whole multi-game match. Counts are
// uint32_t: the v2 spec's uint8_t overflows past 255 games, and this
// system's benchmark mode routinely runs thousands per pairing.
typedef struct
{ uint32_t entrant1_id;
  uint32_t entrant2_id;
  uint32_t entrant1_wins;
  uint32_t entrant2_wins;
  uint32_t draws;
} MatchResult;

// Batch solver selection for rating_batch_compute().
typedef enum
{ RATING_BATCH_MM = 0,        // multiplicative Bradley-Terry fixed point (default)
  RATING_BATCH_GRADIENT = 1   // gradient ascent on the log-likelihood
} RatingBatchMethod;

typedef struct
{ double a_max;                  // initial adaptive-A multiplier. Default 1.30
  double a_min;                  // steady-state adaptive-A multiplier. Default 1.08
  double a_decay_rate;           // games for exponential decay (tau). Default 150
  double convergence_threshold;  // batch solver stopping criterion. Default 1e-6
  uint32_t max_iterations;       // batch solver iteration cap. Default 1000
  double gradient_learning_rate; // RATING_BATCH_GRADIENT step size, applied
  // to a game-count-normalised gradient so a
  // fixed value stays stable regardless of
  // dataset size (see rating_batch.c). Default 0.5
  RatingBatchMethod batch_method;
  double prior_games;            // optional separation guard: fictitious
  // win+loss vs Borealis added to every
  // other entrant before a batch fit. 0 = off.
} RatingConfig;

typedef struct
{ RatingEntry entrants[RATING_MAX_ENTRANTS];
  uint32_t num_entrants;
  RatingConfig config;
  uint32_t borealis_id;     // RATING_INVALID_ID until an AI_STRATEGY_BOREALIS
  // entrant is registered
} RatingSystem;

// Opaque win/games matrix accumulator for the batch fit -- see
// rating_batch_create()/_destroy(). Kept off the RatingSystem/stack (at
// RATING_MAX_ENTRANTS^2 doubles per matrix it is too large to want as a
// stack local) and behind an opaque pointer so its layout can change
// without touching callers.
typedef struct RatingBatchData RatingBatchData;

// ===== Core (rating_core.c) =====

void rating_init(RatingSystem* rs, const RatingConfig* cfg);
RatingConfig rating_default_config(void);

// Registers an AI entrant. If strategy == AI_STRATEGY_BOREALIS, this
// becomes the system's anchor (rs->borealis_id) and every batch/incremental
// update keeps its strength pinned to 1.0 (rating 50) via rebalancing.
// Returns RATING_INVALID_ID if the roster is full or name contains a comma
// (CSV safety -- rating_export_csv() does not quote fields).
uint32_t rating_register_ai(RatingSystem* rs, const char* name, AIStrategyType strategy);
uint32_t rating_register_human(RatingSystem* rs, const char* name);

const RatingEntry* rating_get_entrant(const RatingSystem* rs, uint32_t id);
uint32_t rating_find_by_name(const RatingSystem* rs, const char* name);
uint32_t rating_find_by_strategy(const RatingSystem* rs, AIStrategyType strategy);

int32_t rating_strength_to_display(double bt_strength);
double rating_display_to_strength(int32_t rating);

// P(id1 beats id2). Returns -1.0 (never a valid probability) for an
// out-of-range id, instead of the v2 spec's ambiguous 0.0.
double rating_win_probability(const RatingSystem* rs, uint32_t id1, uint32_t id2);
double rating_get_adaptive_a(const RatingSystem* rs, uint32_t id);

// Rescales every entrant's strength so the anchor sits at exactly 1.0.
// No-op if no anchor is registered yet.
void rating_rebalance_to_borealis(RatingSystem* rs);

// Wilson score interval on an entrant's own overall win rate (its
// games_won/games_played across whatever opponents it has actually
// played), returned as a rating-point-equivalent half-width. -1.0 if the
// entrant has never played. Unlike the v2 spec's rating_confidence_interval()
// this is a proper Wilson interval (not the under-10-games/Wald hybrid),
// and z_score is the caller's choice (1.96 for ~95%).
double rating_confidence_interval(const RatingSystem* rs, uint32_t id, double z_score);

// Nearest-rating opponent within +/- rating_range, excluding self and the
// anchor. RATING_INVALID_ID if none qualifies or id is invalid.
uint32_t rating_find_opponent(const RatingSystem* rs, uint32_t id, int rating_range);

// ===== Incremental update (rating_update.c) =====

// Applies one MatchResult via the adaptive-A^delta rule, one game at a
// time, with wins/losses/draws interleaved proportionally rather than
// applied as three separate blocks (all-wins-then-all-losses is
// systematically biased, since the update is path-dependent -- see
// doc/changelog.md). Rebalances to Borealis afterward if either side is
// the anchor.
void rating_update_match(RatingSystem* rs, const MatchResult* result);

// ===== Batch fit (rating_batch.c) =====

RatingBatchData* rating_batch_create(void);
void rating_batch_destroy(RatingBatchData* batch);
void rating_batch_reset(RatingBatchData* batch);
void rating_batch_add_match(RatingBatchData* batch, const MatchResult* result);

// Order-independent MLE fit over every match accumulated in `batch`,
// dispatching on rs->config.batch_method. Overwrites every entrant's
// bt_strength/rating in `rs`; leaves games_played/games_won/games_drawn
// alone (those are the incremental path's bookkeeping, not the batch
// fit's).
void rating_batch_compute(RatingSystem* rs, const RatingBatchData* batch);

// ===== CSV persistence (rating_csv.c) =====

bool rating_export_csv(const RatingSystem* rs, const char* filename);
bool rating_import_csv(RatingSystem* rs, const char* filename);

#endif // RATING_H
