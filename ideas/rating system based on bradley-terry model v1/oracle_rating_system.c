/* Oracle: The Champions of Arcadia - Rating System
   Based on Bradley-Terry model with Keeper benchmark scaling
   Rating scale: 0-1000, where 500 = equal to Keeper
*/

#ifndef ORACLE_RATING_H
#define ORACLE_RATING_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PLAYERS 100
#define KEEPER_RATING 500.0
#define MIN_RATING 0.0
#define MAX_RATING 1000.0

/* Player types */
typedef enum
{ PLAYER_TYPE_HUMAN,
  PLAYER_TYPE_AI_KEEPER,      /* Benchmark agent */
  PLAYER_TYPE_AI_AGGRESSIVE,
  PLAYER_TYPE_AI_DEFENSIVE,
  PLAYER_TYPE_AI_BALANCED,
  PLAYER_TYPE_AI_HEURISTIC,
  PLAYER_TYPE_AI_HYBRID
} PlayerType;

/* Match result */
typedef struct
{ uint32_t player1_id;
  uint32_t player2_id;
  uint8_t player1_wins;       /* Number of wins */
  uint8_t player2_wins;
  uint8_t draws;              /* Optional */
} MatchResult;

/* Player rating data */
typedef struct
{ uint32_t player_id;
  char name[64];
  PlayerType type;
  double rating;              /* 0-1000 scale */
  double bt_strength;         /* Bradley-Terry strength parameter */
  uint32_t games_played;
  uint32_t games_won;
  double confidence;          /* Uncertainty measure */
} PlayerRating;

/* Rating system configuration */
typedef struct
{ double k_factor;            /* Learning rate (default: 32.0) */
  double convergence_threshold; /* For iterative solving */
  uint32_t max_iterations;
  bool use_draws;             /* Count draws as 0.5 wins each */
  double initial_confidence;  /* Starting uncertainty */
} RatingConfig;

/* Global rating system state */
typedef struct
{ PlayerRating players[MAX_PLAYERS];
  uint32_t num_players;
  RatingConfig config;
  uint32_t keeper_id;         /* ID of benchmark Keeper agent */
} RatingSystem;

/* ========== Core Functions ========== */

/* Initialize rating system with Keeper benchmark */
void rating_init(RatingSystem *rs, const RatingConfig *cfg);

/* Register a new player (returns player_id) */
uint32_t rating_register_player(RatingSystem *rs, const char* name,
                                PlayerType type);

/* Update ratings after a match (incremental Bradley-Terry) */
void rating_update_match(RatingSystem *rs, const MatchResult *result);

/* Calculate win probability using Bradley-Terry model */
double rating_win_probability(const RatingSystem *rs,
                              uint32_t player1_id, uint32_t player2_id);

/* Convert Bradley-Terry strength to 0-1000 rating scale */
double rating_bt_to_scale(double bt_strength, double keeper_bt);

/* Convert 0-1000 rating to Bradley-Terry strength */
double rating_scale_to_bt(double rating, double keeper_bt);

/* Get player rating by ID */
const PlayerRating* rating_get_player(const RatingSystem *rs,
                                      uint32_t player_id);

/* ========== Benchmark Suite ========== */

/* Benchmark configuration */
typedef struct
{ uint32_t games_per_config;
  bool test_monochrome;       /* Test single-color decks */
  bool test_random;           /* Test random distribution */
  bool test_custom;           /* Test custom decks */
  bool alternate_positions;   /* Test first/second player */
} BenchmarkConfig;

/* Run benchmark suite for an agent vs Keeper */
typedef struct
{ uint32_t total_games;
  uint32_t wins;
  uint32_t losses;
  uint32_t draws;
  double win_rate;
  double final_rating;
} BenchmarkResult;

BenchmarkResult rating_run_benchmark(RatingSystem *rs, uint32_t agent_id,
                                     const BenchmarkConfig *bench_cfg);

/* ========== Utility Functions ========== */

/* Export ratings to CSV */
bool rating_export_csv(const RatingSystem *rs, const char* filename);

/* Import ratings from CSV */
bool rating_import_csv(RatingSystem *rs, const char* filename);

/* Calculate rating confidence interval (±) */
double rating_confidence_interval(const PlayerRating *player, double z_score);

/* Print rating leaderboard */
void rating_print_leaderboard(const RatingSystem *rs);

#endif /* ORACLE_RATING_H */

/* ========== Implementation ========== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Initialize rating system */
void rating_init(RatingSystem *rs, const RatingConfig *cfg)
{ memset(rs, 0, sizeof(RatingSystem));
  if(cfg)
    rs->config = *cfg;
  else
  { /* Default configuration */
    rs->config.k_factor = 32.0;
    rs->config.convergence_threshold = 0.001;
    rs->config.max_iterations = 100;
    rs->config.use_draws = true;
    rs->config.initial_confidence = 100.0;
  }

  /* Register Keeper as first player */
  rs->keeper_id = rating_register_player(rs, "Keeper",
                                         PLAYER_TYPE_AI_KEEPER);
  rs->players[rs->keeper_id].rating = KEEPER_RATING;
  rs->players[rs->keeper_id].bt_strength = 1.0; /* Reference strength */
}

/* Register new player */
uint32_t rating_register_player(RatingSystem *rs, const char* name,
                                PlayerType type)
{ if(rs->num_players >= MAX_PLAYERS) return UINT32_MAX;

  uint32_t id = rs->num_players++;
  PlayerRating *p = &rs->players[id];

  p->player_id = id;
  strncpy(p->name, name, sizeof(p->name) - 1);
  p->type = type;
  p->rating = KEEPER_RATING; /* Start at Keeper level */
  p->bt_strength = 1.0;
  p->games_played = 0;
  p->games_won = 0;
  p->confidence = rs->config.initial_confidence;

  return id;
}

/* Bradley-Terry win probability: P(i beats j) = si / (si + sj) */
double rating_win_probability(const RatingSystem *rs,
                              uint32_t p1_id, uint32_t p2_id)
{ if(p1_id >= rs->num_players || p2_id >= rs->num_players) return 0.0;

  double s1 = rs->players[p1_id].bt_strength;
  double s2 = rs->players[p2_id].bt_strength;

  return s1 / (s1 + s2);
}

/* Update ratings using incremental Bradley-Terry with Elo-style update */
void rating_update_match(RatingSystem *rs, const MatchResult *result)
{ uint32_t p1 = result->player1_id;
  uint32_t p2 = result->player2_id;

  if(p1 >= rs->num_players || p2 >= rs->num_players) return;

  PlayerRating *player1 = &rs->players[p1];
  PlayerRating *player2 = &rs->players[p2];

  /* Calculate total games */
  uint32_t total = result->player1_wins + result->player2_wins;
  if(rs->config.use_draws) total += result->draws;

  /* Actual score (0 to 1) */
  double score1 = result->player1_wins;
  if(rs->config.use_draws) score1 += 0.5 * result->draws;
  score1 /= total;

  /* Expected score based on current strengths */
  double expected1 = rating_win_probability(rs, p1, p2);

  /* Update Bradley-Terry strengths using gradient ascent */
  double k = rs->config.k_factor / 100.0; /* Scale down for stability */
  double delta = k * (score1 - expected1);

  /* Update strengths (multiplicative update for positivity) */
  player1->bt_strength *= exp(delta);
  player2->bt_strength *= exp(-delta);

  /* Keep Keeper fixed as reference point */
  if(p1 == rs->keeper_id || p2 == rs->keeper_id)
  { uint32_t keeper = rs->keeper_id;
    uint32_t other = (p1 == keeper) ? p2 : p1;
    double ratio = rs->players[other].bt_strength;
    rs->players[keeper].bt_strength = 1.0;
    /* Adjust other to maintain ratio */
  }

  /* Convert to 0-1000 scale */
  player1->rating = rating_bt_to_scale(player1->bt_strength, 1.0);
  player2->rating = rating_bt_to_scale(player2->bt_strength, 1.0);

  /* Clamp ratings */
  player1->rating = fmax(MIN_RATING, fmin(MAX_RATING, player1->rating));
  player2->rating = fmax(MIN_RATING, fmin(MAX_RATING, player2->rating));

  /* Update statistics */
  player1->games_played += total;
  player2->games_played += total;
  player1->games_won += result->player1_wins;
  player2->games_won += result->player2_wins;

  /* Update confidence (decreases with more games) */
  player1->confidence *= 0.95;
  player2->confidence *= 0.95;
}

/* Convert BT strength to rating scale
   We want: rating 0 → 0% win vs Keeper, rating 1000 → 100% win vs Keeper
   Using: P(win) = s / (s + 1) for player with strength s vs Keeper (s=1)
   Solving: s = P / (1 - P)
   Rating = 1000 * P, so P = rating / 1000
   Therefore: s = (rating/1000) / (1 - rating/1000) = rating / (1000 - rating)
*/
double rating_bt_to_scale(double bt_strength, double keeper_bt)
{ /* P(beat keeper) = s / (s + keeper_bt) */
  double p_win = bt_strength / (bt_strength + keeper_bt);
  return 1000.0 * p_win;
}

double rating_scale_to_bt(double rating, double keeper_bt)
{ /* Clamp rating */
  rating = fmax(MIN_RATING + 0.001, fmin(MAX_RATING - 0.001, rating));
  double p_win = rating / 1000.0;
  /* s = p * keeper_bt / (1 - p) */
  return (p_win * keeper_bt) / (1.0 - p_win);
}

const PlayerRating* rating_get_player(const RatingSystem *rs,
                                      uint32_t player_id)
{ if(player_id >= rs->num_players) return NULL;
  return &rs->players[player_id];
}

/* Confidence interval (assumes normal approximation) */
double rating_confidence_interval(const PlayerRating *player, double z_score)
{ if(player->games_played < 10) return player->confidence;

  /* Wilson score interval approximation */
  double n = player->games_played;
  double p = (double)player->games_won / n;
  double se = sqrt(p * (1.0 - p) / n);

  return z_score * se * 1000.0; /* Scale to rating points */
}

/* Print leaderboard */
void rating_print_leaderboard(const RatingSystem *rs)
{ printf("\n=== Oracle Rating Leaderboard ===\n");
  printf("%-20s %-10s %8s %8s %8s %8s\n",
         "Player", "Type", "Rating", "Games", "Wins", "WinRate");
  printf("--------------------------------------------------------\n");

  /* Simple bubble sort for display */
  uint32_t sorted[MAX_PLAYERS];
  for(uint32_t i = 0; i < rs->num_players; i++) sorted[i] = i;

  for(uint32_t i = 0; i < rs->num_players - 1; i++)
  { for(uint32_t j = i + 1; j < rs->num_players; j++)
    { if(rs->players[sorted[j]].rating >
         rs->players[sorted[i]].rating)
      { uint32_t tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }

  for(uint32_t i = 0; i < rs->num_players; i++)
  { const PlayerRating *p = &rs->players[sorted[i]];
    double wr = p->games_played > 0 ?
                100.0 * p->games_won / p->games_played : 0.0;

    const char* type_str[] = {"Human", "Keeper", "Aggro", "Def",
                              "Balanced", "Heuristic", "Hybrid"
                             };

    printf("%-20s %-10s %8.1f %8u %8u %7.1f%%\n",
           p->name, type_str[p->type], p->rating,
           p->games_played, p->games_won, wr);
  }
  printf("\n");
}

/* Export to CSV */
bool rating_export_csv(const RatingSystem *rs, const char* filename)
{ FILE *f = fopen(filename, "w");
  if(!f) return false;

  fprintf(f, "player_id,name,type,rating,bt_strength,games,wins,confidence\n");
  for(uint32_t i = 0; i < rs->num_players; i++)
  { const PlayerRating *p = &rs->players[i];
    fprintf(f, "%u,%s,%d,%.6f,%.6f,%u,%u,%.6f\n",
            p->player_id, p->name, p->type, p->rating,
            p->bt_strength, p->games_played, p->games_won, p->confidence);
  }

  fclose(f);
  return true;
}

/* Run benchmark suite - placeholder for game simulation integration */
BenchmarkResult rating_run_benchmark(RatingSystem *rs, uint32_t agent_id,
                                     const BenchmarkConfig *bench_cfg)
{ BenchmarkResult result = {0};

  /* This would integrate with your game simulation code */
  /* For each configuration, run games and track results */
  /* Then update ratings using rating_update_match() */

  printf("Benchmark suite: Agent %u vs Keeper %u\n",
         agent_id, rs->keeper_id);
  printf("(Integration with game simulation required)\n");

  return result;
}