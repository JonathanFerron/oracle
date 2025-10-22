/* oracle_rating.c
 * Implementation of Oracle rating system
 */

#include "oracle_rating.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Initialize rating system with default or custom config */
void rating_init(RatingSystem *rs, const RatingConfig *cfg) {
    memset(rs, 0, sizeof(RatingSystem));
    
    if (cfg) {
        rs->config = *cfg;
    } else {
        /* Default configuration */
        rs->config.a_max = 1.30;
        rs->config.a_min = 1.08;
        rs->config.a_decay_rate = 150.0;
        rs->config.convergence_threshold = 1e-6;
        rs->config.max_iterations = 1000;
        rs->config.use_draws = true;
        rs->config.initial_confidence = 100.0;
    }
    
    /* Register Keeper as first player */
    rs->keeper_id = rating_register_player(rs, "Keeper", 
                                           PLAYER_TYPE_AI_KEEPER);
    rs->players[rs->keeper_id].bt_strength = 1.0;
    rs->players[rs->keeper_id].rating = KEEPER_RATING;
}

/* Register a new player */
uint32_t rating_register_player(RatingSystem *rs, const char *name,
                                 PlayerType type) {
    if (rs->num_players >= MAX_PLAYERS) return UINT32_MAX;
    
    uint32_t id = rs->num_players++;
    PlayerRating *p = &rs->players[id];
    
    p->player_id = id;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    p->type = type;
    p->bt_strength = 1.0;
    p->rating = KEEPER_RATING;
    p->games_played = 0;
    p->games_won = 0;
    p->confidence = rs->config.initial_confidence;
    
    return id;
}

/* Adaptive A: decreases from a_max to a_min as games increase */
double rating_get_adaptive_a(const RatingSystem *rs, uint32_t player_id) {
    if (player_id >= rs->num_players) return rs->config.a_min;
    
    const PlayerRating *p = &rs->players[player_id];
    double decay = exp(-p->games_played / rs->config.a_decay_rate);
    
    return rs->config.a_min + 
           (rs->config.a_max - rs->config.a_min) * decay;
}

/* Convert strength to display rating (clamped to 1-99) */
int32_t rating_strength_to_display(double bt_strength) {
    double r = 100.0 * bt_strength / (bt_strength + 1.0);
    int32_t rating = (int)round(r);
    
    if (rating < MIN_RATING) return MIN_RATING;
    if (rating > MAX_RATING) return MAX_RATING;
    return rating;
}

/* Convert display rating to strength */
double rating_display_to_strength(int32_t rating) {
    /* Clamp to valid range */
    if (rating < MIN_RATING) rating = MIN_RATING;
    if (rating > MAX_RATING) rating = MAX_RATING;
    
    return (double)rating / (100 - rating);
}

/* Bradley-Terry win probability */
double rating_win_probability(const RatingSystem *rs,
                              uint32_t p1_id, uint32_t p2_id) {
    if (p1_id >= rs->num_players || p2_id >= rs->num_players) return 0.0;
    
    double s1 = rs->players[p1_id].bt_strength;
    double s2 = rs->players[p2_id].bt_strength;
    
    return s1 / (s1 + s2);
}

/* Update ratings for a single game */
static void rating_update_single_game(RatingSystem *rs,
                                      uint32_t p1_id, uint32_t p2_id,
                                      bool p1_won) {
    PlayerRating *p1 = &rs->players[p1_id];
    PlayerRating *p2 = &rs->players[p2_id];
    
    /* Calculate expected probability */
    double expected_p1 = p1->bt_strength / (p1->bt_strength + p2->bt_strength);
    
    /* Actual outcome */
    double actual = p1_won ? 1.0 : 0.0;
    double delta = actual - expected_p1;
    
    /* Get adaptive A for both players (use average) */
    double a1 = rating_get_adaptive_a(rs, p1_id);
    double a2 = rating_get_adaptive_a(rs, p2_id);
    double a_avg = (a1 + a2) / 2.0;
    
    /* Update strengths using A^delta */
    p1->bt_strength *= pow(a_avg, delta);
    p2->bt_strength *= pow(a_avg, -delta);
    
    /* Ensure positive strengths */
    if (p1->bt_strength < 1e-10) p1->bt_strength = 1e-10;
    if (p2->bt_strength < 1e-10) p2->bt_strength = 1e-10;
}

/* Rebalance all strengths to keep Keeper at exactly 1.0 */
void rating_rebalance_to_keeper(RatingSystem *rs) {
    double keeper_strength = rs->players[rs->keeper_id].bt_strength;
    double factor = 1.0 / keeper_strength;
    
    for (uint32_t i = 0; i < rs->num_players; i++) {
        rs->players[i].bt_strength *= factor;
    }
}

/* Update ratings after a match (iterative per-game updates) */
void rating_update_match(RatingSystem *rs, const MatchResult *result) {
    uint32_t p1 = result->player1_id;
    uint32_t p2 = result->player2_id;
    
    if (p1 >= rs->num_players || p2 >= rs->num_players) return;
    
    PlayerRating *player1 = &rs->players[p1];
    PlayerRating *player2 = &rs->players[p2];
    
    bool keeper_involved = (p1 == rs->keeper_id || p2 == rs->keeper_id);
    
    /* Apply updates for each game in the match */
    for (uint32_t i = 0; i < result->player1_wins; i++) {
        rating_update_single_game(rs, p1, p2, true);
    }
    
    for (uint32_t i = 0; i < result->player2_wins; i++) {
        rating_update_single_game(rs, p1, p2, false);
    }
    
    /* Handle draws if enabled */
    if (rs->config.use_draws) {
        for (uint32_t i = 0; i < result->draws; i++) {
            double exp_p1 = player1->bt_strength / 
                           (player1->bt_strength + player2->bt_strength);
            double delta = 0.5 - exp_p1;
            
            double a1 = rating_get_adaptive_a(rs, p1);
            double a2 = rating_get_adaptive_a(rs, p2);
            double a_avg = (a1 + a2) / 2.0;
            
            player1->bt_strength *= pow(a_avg, delta);
            player2->bt_strength *= pow(a_avg, -delta);
        }
    }
    
    /* Rebalance if Keeper was involved */
    if (keeper_involved) {
        rating_rebalance_to_keeper(rs);
    }
    
    /* Update game statistics */
    uint32_t total_games = result->player1_wins + result->player2_wins;
    if (rs->config.use_draws) total_games += result->draws;
    
    player1->games_played += total_games;
    player2->games_played += total_games;
    player1->games_won += result->player1_wins;
    player2->games_won += result->player2_wins;
    
    /* Update display ratings */
    player1->rating = rating_strength_to_display(player1->bt_strength);
    player2->rating = rating_strength_to_display(player2->bt_strength);
    
    /* Update confidence (decreases with more games) */
    player1->confidence *= 0.95;
    player2->confidence *= 0.95;
}

const PlayerRating* rating_get_player(const RatingSystem *rs,
                                      uint32_t player_id) {
    if (player_id >= rs->num_players) return NULL;
    return &rs->players[player_id];
}

/* ========== Batch Processing (Gradient Ascent) ========== */

void batch_init(BatchMatchData *batch) {
    memset(batch, 0, sizeof(BatchMatchData));
}

void batch_add_match(BatchMatchData *batch, const MatchResult *result) {
    uint32_t p1 = result->player1_id;
    uint32_t p2 = result->player2_id;
    
    if (p1 >= MAX_PLAYERS || p2 >= MAX_PLAYERS) return;
    
    /* Record wins in both directions */
    batch->wins[p1][p2] += result->player1_wins;
    batch->wins[p2][p1] += result->player2_wins;
    
    /* Total games */
    uint32_t total = result->player1_wins + result->player2_wins + 
                     result->draws;
    batch->games[p1][p2] += total;
    batch->games[p2][p1] += total;
    
    /* Update total wins */
    batch->total_wins[p1] += result->player1_wins;
    batch->total_wins[p2] += result->player2_wins;
}

/* Gradient ascent to find maximum likelihood strengths */
void rating_batch_compute(RatingSystem *rs, const BatchMatchData *batch) {
    uint32_t n = rs->num_players;
    double learning_rate = 0.01;
    
    /* Initialize all strengths to 1.0 */
    for (uint32_t i = 0; i < n; i++) {
        rs->players[i].bt_strength = 1.0;
    }
    
    /* Iterate until convergence */
    for (uint32_t iter = 0; iter < rs->config.max_iterations; iter++) {
        double grad[MAX_PLAYERS];
        double s_new[MAX_PLAYERS];
        
        /* Calculate gradient for each player */
        for (uint32_t i = 0; i < n; i++) {
            double s_i = rs->players[i].bt_strength;
            
            /* First term: total wins / strength */
            grad[i] = batch->total_wins[i] / s_i;
            
            /* Second term: sum over opponents */
            for (uint32_t j = 0; j < n; j++) {
                if (i != j && batch->games[i][j] > 0) {
                    double s_j = rs->players[j].bt_strength;
                    grad[i] -= batch->games[i][j] / (s_i + s_j);
                }
            }
        }
        
        /* Update strengths */
        double max_change = 0.0;
        for (uint32_t i = 0; i < n; i++) {
            double s_i = rs->players[i].bt_strength;
            s_new[i] = s_i + learning_rate * s_i * grad[i];
            
            /* Ensure positivity */
            if (s_new[i] < 1e-10) s_new[i] = 1e-10;
            
            double change = fabs(s_new[i] - s_i);
            if (change > max_change) max_change = change;
        }
        
        /* Normalize to keep Keeper at 1.0 */
        double keeper_s = s_new[rs->keeper_id];
        for (uint32_t i = 0; i < n; i++) {
            s_new[i] /= keeper_s;
            rs->players[i].bt_strength = s_new[i];
        }
        
        /* Check convergence */
        if (max_change < rs->config.convergence_threshold) {
            printf("Gradient ascent converged in %u iterations\n", iter + 1);
            break;
        }
    }
    
    /* Update display ratings */
    for (uint32_t i = 0; i < n; i++) {
        rs->players[i].rating = 
            rating_strength_to_display(rs->players[i].bt_strength);
    }
}

/* ========== Utility Functions ========== */

bool rating_export_csv(const RatingSystem *rs, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return false;
    
    fprintf(f, "player_id,name,type,rating,bt_strength,games,wins\n");
    for (uint32_t i = 0; i < rs->num_players; i++) {
        const PlayerRating *p = &rs->players[i];
        fprintf(f, "%u,%s,%d,%d,%.6f,%u,%u\n",
                p->player_id, p->name, p->type, p->rating,
                p->bt_strength, p->games_played, p->games_won);
    }
    
    fclose(f);
    return true;
}

bool rating_import_csv(RatingSystem *rs, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;
    
    char line[256];
    fgets(line, sizeof(line), f); /* Skip header */
    
    rs->num_players = 0;
    
    while (fgets(line, sizeof(line), f)) {
        PlayerRating *p = &rs->players[rs->num_players];
        int type_int;
        
        if (sscanf(line, "%u,%63[^,],%d,%d,%lf,%u,%u",
                   &p->player_id, p->name, &type_int, &p->rating,
                   &p->bt_strength, &p->games_played, 
                   &p->games_won) == 7) {
            p->type = (PlayerType)type_int;
            p->confidence = 50.0; /* Default for imported */
            
            if (p->type == PLAYER_TYPE_AI_KEEPER) {
                rs->keeper_id = rs->num_players;
            }
            
            rs->num_players++;
            if (rs->num_players >= MAX_PLAYERS) break;
        }
    }
    
    fclose(f);
    return true;
}

double rating_confidence_interval(const PlayerRating *player, double z_score) {
    if (player->games_played < 10) return player->confidence;
    
    double n = player->games_played;
    double p = (double)player->games_won / n;
    double se = sqrt(p * (1.0 - p) / n);
    
    return z_score * se * 100.0;
}

void rating_print_leaderboard(const RatingSystem *rs) {
    printf("\n=== Oracle Rating Leaderboard ===\n");
    printf("%-20s %-10s %6s %6s %6s %7s\n",
           "Player", "Type", "Rating", "Games", "Wins", "WinRate");
    printf("-----------------------------------------------------------\n");
    
    /* Simple bubble sort for display */
    uint32_t sorted[MAX_PLAYERS];
    for (uint32_t i = 0; i < rs->num_players; i++) sorted[i] = i;
    
    for (uint32_t i = 0; i < rs->num_players - 1; i++) {
        for (uint32_t j = i + 1; j < rs->num_players; j++) {
            if (rs->players[sorted[j]].rating > 
                rs->players[sorted[i]].rating) {
                uint32_t tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    
    const char *type_names[] = {"Human", "Keeper", "Aggro", "Defense",
                                "Balanced", "Heuristic", "Hybrid"};
    
    for (uint32_t i = 0; i < rs->num_players; i++) {
        const PlayerRating *p = &rs->players[sorted[i]];
        double wr = p->games_played > 0 ? 
                    100.0 * p->games_won / p->games_played : 0.0;
        
        printf("%-20s %-10s %6d %6u %6u %6.1f%%\n",
               p->name, type_names[p->type], p->rating,
               p->games_played, p->games_won, wr);
    }
    printf("\n");
}

void rating_print_player_details(const RatingSystem *rs, uint32_t player_id) {
    const PlayerRating *p = rating_get_player(rs, player_id);
    if (!p) return;
    
    printf("\n=== Player Details: %s ===\n", p->name);
    printf("Rating: %d (strength: %.4f)\n", p->rating, p->bt_strength);
    printf("Games: %u, Wins: %u (%.1f%%)\n", 
           p->games_played, p->games_won,
           p->games_played > 0 ? 100.0 * p->games_won / p->games_played : 0.0);
    
    double ci = rating_confidence_interval(p, 1.96);
    printf("95%% Confidence: ±%.1f points\n", ci);
    
    double a_current = rating_get_adaptive_a(rs, player_id);
    printf("Current update multiplier: %.3f\n", a_current);
    
    double prob_vs_keeper = rating_win_probability(rs, player_id, rs->keeper_id);
    printf("Win probability vs Keeper: %.1f%%\n", prob_vs_keeper * 100.0);
    printf("\n");
}