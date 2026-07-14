/* oracle_rating.h
 * Oracle: The Champions of Arcadia - Rating System
 * 
 * Bradley-Terry model with Keeper benchmark
 * Rating scale: 0-100 (Keeper = 50)
 * Update method: A^delta with adaptive A
 */

#ifndef ORACLE_RATING_H
#define ORACLE_RATING_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PLAYERS 100
#define KEEPER_RATING 50
#define MIN_RATING 1
#define MAX_RATING 99

/* Player types */
typedef enum {
    PLAYER_TYPE_HUMAN,
    PLAYER_TYPE_AI_KEEPER,
    PLAYER_TYPE_AI_AGGRESSIVE,
    PLAYER_TYPE_AI_DEFENSIVE,
    PLAYER_TYPE_AI_BALANCED,
    PLAYER_TYPE_AI_HEURISTIC,
    PLAYER_TYPE_AI_HYBRID
} PlayerType;

/* Match result for recording game outcomes */
typedef struct {
    uint32_t player1_id;
    uint32_t player2_id;
    uint8_t player1_wins;
    uint8_t player2_wins;
    uint8_t draws;
} MatchResult;

/* Player rating data */
typedef struct {
    uint32_t player_id;
    char name[64];
    PlayerType type;
    double bt_strength;         /* Bradley-Terry strength (internal) */
    int32_t rating;             /* Display rating: 1-99 */
    uint32_t games_played;      /* Total games (for adaptive A) */
    uint32_t games_won;
    double confidence;          /* Uncertainty estimate */
} PlayerRating;

/* Rating system configuration */
typedef struct {
    double a_max;               /* Initial update multiplier (default: 1.30) */
    double a_min;               /* Final update multiplier (default: 1.08) */
    double a_decay_rate;        /* Games for exponential decay (default: 150) */
    double convergence_threshold; /* For gradient ascent */
    uint32_t max_iterations;    /* For gradient ascent */
    bool use_draws;             /* Count draws as 0.5 wins */
    double initial_confidence;
} RatingConfig;

/* Global rating system state */
typedef struct {
    PlayerRating players[MAX_PLAYERS];
    uint32_t num_players;
    RatingConfig config;
    uint32_t keeper_id;
} RatingSystem;

/* Batch processing data structures for gradient ascent */
typedef struct {
    uint32_t wins[MAX_PLAYERS][MAX_PLAYERS];   /* W[i][j] = i beat j */
    uint32_t games[MAX_PLAYERS][MAX_PLAYERS];  /* N[i][j] = total games */
    uint32_t total_wins[MAX_PLAYERS];          /* Row sums of wins */
} BatchMatchData;

/* ========== Core Functions ========== */

/* Initialize rating system */
void rating_init(RatingSystem *rs, const RatingConfig *cfg);

/* Register a new player (returns player_id) */
uint32_t rating_register_player(RatingSystem *rs, const char *name,
                                 PlayerType type);

/* Get adaptive update multiplier based on games played */
double rating_get_adaptive_a(const RatingSystem *rs, uint32_t player_id);

/* Update ratings after a match (incremental method) */
void rating_update_match(RatingSystem *rs, const MatchResult *result);

/* Calculate win probability using Bradley-Terry model */
double rating_win_probability(const RatingSystem *rs,
                              uint32_t player1_id, uint32_t player2_id);

/* Convert strength to rating (1-99 scale) */
int32_t rating_strength_to_display(double bt_strength);

/* Convert rating to strength */
double rating_display_to_strength(int32_t rating);

/* Get player rating by ID */
const PlayerRating* rating_get_player(const RatingSystem *rs,
                                      uint32_t player_id);

/* Rebalance all strengths to keep Keeper at 1.0 */
void rating_rebalance_to_keeper(RatingSystem *rs);

/* ========== Batch Processing (Gradient Ascent) ========== */

/* Initialize batch match data */
void batch_init(BatchMatchData *batch);

/* Add match result to batch */
void batch_add_match(BatchMatchData *batch, const MatchResult *result);

/* Compute ratings using gradient ascent (batch method) */
void rating_batch_compute(RatingSystem *rs, const BatchMatchData *batch);

/* ========== Utility Functions ========== */

/* Export ratings to CSV */
bool rating_export_csv(const RatingSystem *rs, const char *filename);

/* Import ratings from CSV */
bool rating_import_csv(RatingSystem *rs, const char *filename);

/* Calculate confidence interval */
double rating_confidence_interval(const PlayerRating *player, double z_score);

/* Print rating leaderboard */
void rating_print_leaderboard(const RatingSystem *rs);

/* Print detailed player stats */
void rating_print_player_details(const RatingSystem *rs, uint32_t player_id);

#endif /* ORACLE_RATING_H */