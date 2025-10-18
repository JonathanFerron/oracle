/* Oracle Rating System - Usage Example & Testing
 * Demonstrates rating calculation with Bradley-Terry model
 */

#include "oracle_rating.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Simulate a match between two agents (placeholder) */
MatchResult simulate_match(uint32_t p1, uint32_t p2, 
                          uint32_t games, double p1_skill) {
    MatchResult result = {0};
    result.player1_id = p1;
    result.player2_id = p2;
    
    /* Simple simulation based on skill difference */
    for (uint32_t i = 0; i < games; i++) {
        double r = (double)rand() / RAND_MAX;
        if (r < p1_skill) {
            result.player1_wins++;
        } else {
            result.player2_wins++;
        }
    }
    
    return result;
}

/* Test scenario 1: New agent learning against Keeper */
void test_learning_curve(void) {
    printf("\n=== Test 1: Learning Curve Against Keeper ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    uint32_t learner = rating_register_player(&rs, "LearningBot", 
                                              PLAYER_TYPE_AI_BALANCED);
    
    /* Start weak (30% win rate), improve over time */
    double skill_levels[] = {0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60};
    
    printf("\nSimulating improvement over time:\n");
    printf("Phase | Skill | Wins-Losses | Rating | vs Keeper\n");
    printf("------|-------|-------------|--------|----------\n");
    
    for (int phase = 0; phase < 7; phase++) {
        MatchResult match = simulate_match(learner, rs.keeper_id, 
                                          20, skill_levels[phase]);
        rating_update_match(&rs, &match);
        
        const PlayerRating *p = rating_get_player(&rs, learner);
        double prob = rating_win_probability(&rs, learner, rs.keeper_id);
        
        printf("  %d   | %.2f  | %2u-%2u        | %6.1f | %.1f%%\n",
               phase + 1, skill_levels[phase],
               match.player1_wins, match.player2_wins,
               p->rating, prob * 100.0);
    }
    
    rating_print_leaderboard(&rs);
}

/* Test scenario 2: Multiple agents with different strengths */
void test_multi_agent(void) {
    printf("\n=== Test 2: Multi-Agent Rating System ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    /* Register various agents */
    uint32_t aggressive = rating_register_player(&rs, "AggroBot",
                                                 PLAYER_TYPE_AI_AGGRESSIVE);
    uint32_t defensive = rating_register_player(&rs, "DefenseBot",
                                                PLAYER_TYPE_AI_DEFENSIVE);
    uint32_t hybrid = rating_register_player(&rs, "HybridBot",
                                            PLAYER_TYPE_AI_HYBRID);
    uint32_t human = rating_register_player(&rs, "Player1",
                                           PLAYER_TYPE_HUMAN);
    
    /* Simulate matches with known skill levels */
    /* AggroBot: 65% vs Keeper */
    MatchResult m1 = simulate_match(aggressive, rs.keeper_id, 50, 0.65);
    rating_update_match(&rs, &m1);
    
    /* DefenseBot: 45% vs Keeper */
    MatchResult m2 = simulate_match(defensive, rs.keeper_id, 50, 0.45);
    rating_update_match(&rs, &m2);
    
    /* HybridBot: 70% vs Keeper */
    MatchResult m3 = simulate_match(hybrid, rs.keeper_id, 50, 0.70);
    rating_update_match(&rs, &m3);
    
    /* Human: 55% vs Keeper */
    MatchResult m4 = simulate_match(human, rs.keeper_id, 30, 0.55);
    rating_update_match(&rs, &m4);
    
    /* Cross-matches for more accurate ratings */
    MatchResult m5 = simulate_match(aggressive, defensive, 30, 0.65);
    rating_update_match(&rs, &m5);
    
    MatchResult m6 = simulate_match(hybrid, aggressive, 30, 0.55);
    rating_update_match(&rs, &m6);
    
    rating_print_leaderboard(&rs);
    
    /* Show predicted matchups */
    printf("Predicted win probabilities:\n");
    printf("  HybridBot vs AggroBot: %.1f%%\n",
           rating_win_probability(&rs, hybrid, aggressive) * 100.0);
    printf("  AggroBot vs DefenseBot: %.1f%%\n",
           rating_win_probability(&rs, aggressive, defensive) * 100.0);
    printf("  Player1 vs DefenseBot: %.1f%%\n",
           rating_win_probability(&rs, human, defensive) * 100.0);
}

/* Test scenario 3: Rating scale verification */
void test_rating_scale(void) {
    printf("\n=== Test 3: Rating Scale Verification ===\n");
    printf("Testing that rating scale matches win probability:\n\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    /* Create agents at specific ratings */
    struct {
        const char *name;
        double target_winrate;
    } agents[] = {
        {"Weak", 0.10},
        {"Below", 0.25},
        {"Medium", 0.50},
        {"Above", 0.75},
        {"Strong", 0.90}
    };
    
    printf("%-12s Target%% Actual%% Games  Rating  Error\n", "Agent");
    printf("------------------------------------------------\n");
    
    for (int i = 0; i < 5; i++) {
        uint32_t agent = rating_register_player(&rs, agents[i].name,
                                                PLAYER_TYPE_AI_BALANCED);
        
        /* Run many games to converge to target win rate */
        for (int round = 0; round < 10; round++) {
            MatchResult m = simulate_match(agent, rs.keeper_id, 50,
                                          agents[i].target_winrate);
            rating_update_match(&rs, &m);
        }
        
        const PlayerRating *p = rating_get_player(&rs, agent);
        double actual_wr = (double)p->games_won / p->games_played;
        double expected_wr = p->rating / 1000.0;
        double error = fabs(actual_wr - expected_wr);
        
        printf("%-12s %6.1f%% %6.1f%% %5u %7.1f %6.3f\n",
               agents[i].name,
               agents[i].target_winrate * 100.0,
               actual_wr * 100.0,
               p->games_played,
               p->rating,
               error);
    }
    
    printf("\nVerifying extreme cases:\n");
    printf("Rating   0 → Win prob vs Keeper: %.4f (expect ~0.000)\n",
           rating_win_probability(&rs, 0, rs.keeper_id));
    printf("Rating 500 → Win prob vs Keeper: %.4f (expect ~0.500)\n",
           rating_win_probability(&rs, 2, rs.keeper_id));
}

/* Test scenario 4: Human player rating progression */
void test_human_progression(void) {
    printf("\n=== Test 4: Human Player Rating Progression ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    uint32_t human = rating_register_player(&rs, "NewPlayer",
                                           PLAYER_TYPE_HUMAN);
    
    /* Simulate human learning: starts at 30%, improves to 60% */
    printf("\nSession | Games | W-L   | Rating | ±95%% CI | vs Keeper\n");
    printf("--------|-------|-------|--------|---------|----------\n");
    
    double skill_progression[] = {0.30, 0.35, 0.38, 0.42, 0.45, 
                                  0.48, 0.52, 0.55, 0.58, 0.60};
    
    for (int session = 0; session < 10; session++) {
        MatchResult m = simulate_match(human, rs.keeper_id, 10,
                                      skill_progression[session]);
        rating_update_match(&rs, &m);
        
        const PlayerRating *p = rating_get_player(&rs, human);
        double ci = rating_confidence_interval(p, 1.96); /* 95% confidence */
        double prob = rating_win_probability(&rs, human, rs.keeper_id);
        
        printf("   %2d   |  %3u  | %u-%u  | %6.1f | ±%-5.1f | %.1f%%\n",
               session + 1,
               p->games_played,
               m.player1_wins, m.player2_wins,
               p->rating, ci, prob * 100.0);
    }
    
    const PlayerRating *final = rating_get_player(&rs, human);
    printf("\nFinal rating: %.1f (%.0f%% win rate vs Keeper)\n",
           final->rating, (final->rating / 10.0));
    printf("Confidence interval: ±%.1f points\n",
           rating_confidence_interval(final, 1.96));
}

/* Test scenario 5: CSV export/import */
void test_persistence(void) {
    printf("\n=== Test 5: Rating Persistence (CSV) ===\n");
    
    RatingSystem rs;
    rating_init(&rs, NULL);
    
    /* Add some agents with ratings */
    uint32_t a1 = rating_register_player(&rs, "Agent1", 
                                        PLAYER_TYPE_AI_AGGRESSIVE);
    uint32_t a2 = rating_register_player(&rs, "Agent2",
                                        PLAYER_TYPE_AI_DEFENSIVE);
    
    MatchResult m1 = simulate_match(a1, rs.keeper_id, 50, 0.65);
    rating_update_match(&rs, &m1);
    
    MatchResult m2 = simulate_match(a2, rs.keeper_id, 50, 0.40);
    rating_update_match(&rs, &m2);
    
    printf("Before export:\n");
    rating_print_leaderboard(&rs);
    
    /* Export */
    if (rating_export_csv(&rs, "ratings_test.csv")) {
        printf("Successfully exported to ratings_test.csv\n");
    }
    
    /* Could test import here with a new RatingSystem */
}

int main(void) {
    srand(time(NULL));
    
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  Oracle: Champions of Arcadia Rating System   ║\n");
    printf("║  Bradley-Terry Model with Keeper Benchmark    ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    test_learning_curve();
    test_multi_agent();
    test_rating_scale();
    test_human_progression();
    test_persistence();
    
    printf("\n=== All Tests Complete ===\n");
    printf("\nKey Features:\n");
    printf("  • Rating scale: 0-1000 (Keeper = 500)\n");
    printf("  • 0%% win rate vs Keeper → rating 0\n");
    printf("  • 50%% win rate vs Keeper → rating 500\n");
    printf("  • 100%% win rate vs Keeper → rating 1000\n");
    printf("  • Bradley-Terry probability model\n");
    printf("  • Confidence intervals for uncertainty\n");
    printf("  • CSV export/import for persistence\n");
    
    return 0;
}