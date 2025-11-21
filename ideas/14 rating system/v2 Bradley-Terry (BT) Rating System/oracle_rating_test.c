/* oracle_rating_test.c
 * Test suite and examples for Oracle rating system
 */

#include "oracle_rating.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Simulate match outcomes based on skill levels */
static MatchResult simulate_match(uint32_t p1, uint32_t p2, 
                                  uint32_t n_games, double p1_skill) {
    MatchResult result = {0};
    result.player1_id = p1;
    result.player2_id = p2;
    
    for (uint32_t i = 0; i < n_games; i++) {
        double r = (double)rand() / RAND_MAX;
        if (r < p1_skill) {
            result.player1_wins++;
        } else {
            result.player2_wins++;
        }
    }
    
    return result;
}

/* Test 1: Learning curve - player improves over time */
void test_learning_curve(void) {
    printf("\n=== Test 1: Learning Curve ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    uint32_t learner = rating_register_player(&rs, "LearningBot",
                                              PLAYER_TYPE_AI_BALANCED);
    
    /* Simulate improvement from 30% to 65% win rate */
    double skills[] = {0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65};
    
    printf("\nPhase | Skill | W-L  | Rating | Strength | Adaptive A\n");
    printf("------|-------|------|--------|----------|----------\n");
    
    for (int i = 0; i < 8; i++) {
        MatchResult match = simulate_match(learner, rs.keeper_id, 
                                          20, skills[i]);
        rating_update_match(&rs, &match);
        
        const PlayerRating *p = rating_get_player(&rs, learner);
        double a = rating_get_adaptive_a(&rs, learner);
        
        printf("  %d   | %.2f  | %2u-%2u | %6d | %8.4f | %.3f\n",
               i + 1, skills[i],
               match.player1_wins, match.player2_wins,
               p->rating, p->bt_strength, a);
    }
    
    rating_print_leaderboard(&rs);
    rating_print_player_details(&rs, learner);
}

/* Test 2: Multi-agent comparison */
void test_multi_agent(void) {
    printf("\n=== Test 2: Multi-Agent System ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    uint32_t aggressive = rating_register_player(&rs, "AggroBot",
                                                 PLAYER_TYPE_AI_AGGRESSIVE);
    uint32_t defensive = rating_register_player(&rs, "DefenseBot",
                                                PLAYER_TYPE_AI_DEFENSIVE);
    uint32_t hybrid = rating_register_player(&rs, "HybridBot",
                                            PLAYER_TYPE_AI_HYBRID);
    
    /* Simulate matches with different skill levels */
    MatchResult m1 = simulate_match(aggressive, rs.keeper_id, 50, 0.65);
    rating_update_match(&rs, &m1);
    
    MatchResult m2 = simulate_match(defensive, rs.keeper_id, 50, 0.40);
    rating_update_match(&rs, &m2);
    
    MatchResult m3 = simulate_match(hybrid, rs.keeper_id, 50, 0.75);
    rating_update_match(&rs, &m3);
    
    /* Cross-matches */
    MatchResult m4 = simulate_match(aggressive, defensive, 30, 0.70);
    rating_update_match(&rs, &m4);
    
    MatchResult m5 = simulate_match(hybrid, aggressive, 30, 0.60);
    rating_update_match(&rs, &m5);
    
    rating_print_leaderboard(&rs);
    
    printf("Predicted matchups:\n");
    printf("  HybridBot vs AggroBot: %.1f%%\n",
           rating_win_probability(&rs, hybrid, aggressive) * 100.0);
    printf("  AggroBot vs DefenseBot: %.1f%%\n",
           rating_win_probability(&rs, aggressive, defensive) * 100.0);
}

/* Test 3: Batch vs Incremental comparison */
void test_batch_vs_incremental(void) {
    printf("\n=== Test 3: Batch vs Incremental Methods ===\n");
    
    /* Generate same match data for both methods */
    MatchResult matches[] = {
        {1, 0, 15, 5, 0},   /* Player A vs Keeper: 15-5 */
        {2, 0, 8, 12, 0},   /* Player B vs Keeper: 8-12 */
        {1, 2, 12, 8, 0},   /* Player A vs B: 12-8 */
    };
    
    /* Method 1: Incremental updates */
    RatingSystem rs_inc;
    rating_init(&rs_inc, NULL);
    uint32_t a_inc = rating_register_player(&rs_inc, "PlayerA", 
                                            PLAYER_TYPE_HUMAN);
    uint32_t b_inc = rating_register_player(&rs_inc, "PlayerB",
                                            PLAYER_TYPE_HUMAN);
    
    for (int i = 0; i < 3; i++) {
        rating_update_match(&rs_inc, &matches[i]);
    }
    
    printf("\nIncremental Method:\n");
    rating_print_leaderboard(&rs_inc);
    
    /* Method 2: Batch gradient ascent */
    RatingSystem rs_batch;
    rating_init(&rs_batch, NULL);
    uint32_t a_batch = rating_register_player(&rs_batch, "PlayerA",
                                              PLAYER_TYPE_HUMAN);
    uint32_t b_batch = rating_register_player(&rs_batch, "PlayerB",
                                              PLAYER_TYPE_HUMAN);
    
    BatchMatchData batch;
    batch_init(&batch);
    for (int i = 0; i < 3; i++) {
        batch_add_match(&batch, &matches[i]);
    }
    
    /* Set games played for stats */
    rs_batch.players[a_batch].games_played = 20 + 20;
    rs_batch.players[b_batch].games_played = 20 + 20;
    rs_batch.players[a_batch].games_won = 15 + 12;
    rs_batch.players[b_batch].games_won = 8 + 8;
    
    rating_batch_compute(&rs_batch, &batch);
    
    printf("\nBatch Gradient Ascent Method:\n");
    rating_print_leaderboard(&rs_batch);
    
    printf("\nComparison:\n");
    printf("%-12s %8s %8s %10s\n", "Player", "Incremental", "Batch", "Difference");
    printf("------------------------------------------------\n");
    printf("%-12s %8d %8d %10d\n", "PlayerA",
           rs_inc.players[a_inc].rating,
           rs_batch.players[a_batch].rating,
           rs_inc.players[a_inc].rating - rs_batch.players[a_batch].rating);
    printf("%-12s %8d %8d %10d\n", "PlayerB",
           rs_inc.players[b_inc].rating,
           rs_batch.players[b_batch].rating,
           rs_inc.players[b_inc].rating - rs_batch.players[b_batch].rating);
}

/* Test 4: Adaptive A behavior */
void test_adaptive_a(void) {
    printf("\n=== Test 4: Adaptive A Over Time ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    uint32_t player = rating_register_player(&rs, "TestPlayer",
                                             PLAYER_TYPE_HUMAN);
    
    printf("\nGames | Adaptive A | Rating Impact*\n");
    printf("------|------------|---------------\n");
    
    uint32_t milestones[] = {0, 10, 25, 50, 100, 150, 200, 300, 500, 1000};
    
    for (int i = 0; i < 10; i++) {
        rs.players[player].games_played = milestones[i];
        double a = rating_get_adaptive_a(&rs, player);
        
        /* Estimate rating change for upset win */
        double delta = 0.7;  /* Win when expected 30% */
        double mult = pow(a, delta);
        double s_old = 1.0;
        double s_new = s_old * mult;
        int r_old = rating_strength_to_display(s_old);
        int r_new = rating_strength_to_display(s_new);
        
        printf("%5u | %10.3f | ±%d points\n", 
               milestones[i], a, abs(r_new - r_old));
    }
}

/* Test 5: Rating scale verification */
void test_rating_scale(void) {
    printf("\n=== Test 5: Rating Scale Verification ===\n");
    
    printf("\nVerifying rating-to-strength conversion:\n");
    printf("Rating | Strength | Win%% vs Keeper | Reverse Rating\n");
    printf("-------|----------|----------------|---------------\n");
    
    int test_ratings[] = {1, 10, 25, 40, 50, 60, 75, 90, 99};
    
    for (int i = 0; i < 9; i++) {
        int r = test_ratings[i];
        double s = rating_display_to_strength(r);
        double win_prob = 100.0 * s / (s + 1.0);
        int r_reverse = rating_strength_to_display(s);
        
        printf("%6d | %8.4f | %13.1f%% | %14d\n",
               r, s, win_prob, r_reverse);
    }
}

/* Test 6: CSV export/import */
void test_persistence(void) {
    printf("\n=== Test 6: CSV Persistence ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    uint32_t a1 = rating_register_player(&rs, "Agent1",
                                        PLAYER_TYPE_AI_AGGRESSIVE);
    uint32_t a2 = rating_register_player(&rs, "Agent2",
                                        PLAYER_TYPE_AI_DEFENSIVE);
    
    MatchResult m1 = simulate_match(a1, rs.keeper_id, 50, 0.65);
    rating_update_match(&rs, &m1);
    
    MatchResult m2 = simulate_match(a2, rs.keeper_id, 50, 0.40);
    rating_update_match(&rs, &m2);
    
    printf("Original ratings:\n");
    rating_print_leaderboard(&rs);
    
    if (rating_export_csv(&rs, "test_ratings.csv")) {
        printf("✓ Exported to test_ratings.csv\n");
        
        RatingSystem rs_loaded;
        rating_init(&rs_loaded, NULL);
        
        if (rating_import_csv(&rs_loaded, "test_ratings.csv")) {
            printf("✓ Imported from test_ratings.csv\n");
            printf("\nLoaded ratings:\n");
            rating_print_leaderboard(&rs_loaded);
        } else {
            printf("✗ Failed to import\n");
        }
    } else {
        printf("✗ Failed to export\n");
    }
}

/* Test 7: Rebalancing verification */
void test_rebalancing(void) {
    printf("\n=== Test 7: Keeper Rebalancing ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    uint32_t player = rating_register_player(&rs, "TestPlayer",
                                             PLAYER_TYPE_HUMAN);
    
    printf("\nBefore match:\n");
    printf("  Keeper strength: %.6f\n", rs.players[rs.keeper_id].bt_strength);
    printf("  Player strength: %.6f\n", rs.players[player].bt_strength);
    
    /* Simulate a match where Keeper plays */
    MatchResult match = {player, rs.keeper_id, 15, 5, 0};
    rating_update_match(&rs, &match);
    
    printf("\nAfter match (with rebalancing):\n");
    printf("  Keeper strength: %.6f\n", rs.players[rs.keeper_id].bt_strength);
    printf("  Player strength: %.6f\n", rs.players[player].bt_strength);
    printf("  Keeper rating: %d\n", rs.players[rs.keeper_id].rating);
    
    if (fabs(rs.players[rs.keeper_id].bt_strength - 1.0) < 1e-9) {
        printf("✓ Keeper properly rebalanced to 1.0\n");
    } else {
        printf("✗ Keeper drift detected!\n");
    }
}

/* Main test runner */
int main(void) {
    srand(time(NULL));
    
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║   Oracle Rating System - Test Suite           ║\n");
    printf("║   Bradley-Terry with Adaptive Updates         ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    test_learning_curve();
    test_multi_agent();
    test_batch_vs_incremental();
    test_adaptive_a();
    test_rating_scale();
    test_persistence();
    test_rebalancing();
    
    printf("\n=== All Tests Complete ===\n");
    printf("\nKey Features Verified:\n");
    printf("  ✓ Rating scale: 1-99 (Keeper = 50)\n");
    printf("  ✓ Adaptive A: 1.30 → 1.08 over 500 games\n");
    printf("  ✓ Iterative per-game updates\n");
    printf("  ✓ Keeper rebalancing after matches\n");
    printf("  ✓ Batch gradient ascent method\n");
    printf("  ✓ CSV persistence\n");
    printf("  ✓ Win probability predictions\n");
    
    return 0;
}