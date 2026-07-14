# Oracle AI Agent Rating System Specification

## Overview

This document specifies a rating system for AI agents and human players in Oracle: The Champions of Arcadia. The system uses a benchmark AI agent as the standard for comparison, providing a consistent and understandable measure of skill.

## Core Concepts

### Benchmark Agent: "Keeper"

**Name**: "Keeper" (The Keeper of Arcadia)

**Type**: Deterministic rule-based AI using a calibrated heuristic/balanced hybrid strategy

**Purpose**: Serves as the standard against which all other agents and human players are measured

**Thematic Context**: The Keeper is the guardian of Arcadia, testing all who seek to prove their skill in the game. Players must face the Keeper to earn their rating.

### Rating Scale

- **Minimum Rating**: 0 (0% win rate vs Keeper)
- **Benchmark Rating**: 500 (50% win rate vs Keeper - equal to Keeper)
- **Maximum Rating**: 1000 (100% win rate vs Keeper)

### Rating Terminology

```c
// Rating tier descriptions
const char* get_rating_description(uint16_t rating)
{
    if(rating >= 900) return "Legendary";
    if(rating >= 800) return "Master";
    if(rating >= 700) return "Expert";
    if(rating >= 600) return "Advanced";
    if(rating >= 500) return "Proficient";
    if(rating >= 400) return "Competent";
    if(rating >= 300) return "Intermediate";
    if(rating >= 200) return "Novice";
    if(rating >= 100) return "Beginner";
    return "Learning";
}
```

## Data Structures

```c
// In src/rating.h

#ifndef RATING_H
#define RATING_H

#include "game_types.h"
#include "strategy.h"

#define MIN_RATING 0
#define MAX_RATING 1000
#define BENCHMARK_AGENT_NAME "keeper"
#define BENCHMARK_RATING 500  // By definition

// Primary rating structure (vs Keeper)
typedef struct {
    char agent_name[32];
    uint16_t wins_vs_keeper;
    uint16_t losses_vs_keeper;
    uint16_t draws_vs_keeper;
    uint16_t total_games_vs_keeper;
    float win_rate_vs_keeper;
    uint16_t keeper_rating;  // 0-1000
    char rating_description[16];  // "Expert", "Master", etc.
} AgentRating;

// Extended rating with confidence intervals
typedef struct {
    AgentRating base;
    float confidence_interval_lower;  // 95% CI lower bound
    float confidence_interval_upper;  // 95% CI upper bound
    uint16_t rating_lower_bound;      // Rating at CI lower
    uint16_t rating_upper_bound;      // Rating at CI upper
    bool rating_is_provisional;       // True if < 100 games
} AgentRatingExtended;

// ELO-style rating for multi-agent comparisons
typedef struct {
    char agent_name[32];
    uint16_t elo_rating;
    uint16_t games_played;
    uint16_t wins;
    uint16_t losses;
    uint16_t draws;
    float win_rate;
} AgentEloRating;

// Human player rating (can track multiple agents)
typedef struct {
    char player_name[32];
    uint16_t keeper_rating;
    
    // Track performance vs Keeper
    AgentRating vs_keeper;
    
    // Track performance vs other rated agents
    uint16_t num_opponents;
    struct {
        char opponent_name[32];
        uint16_t opponent_rating;
        uint16_t wins;
        uint16_t losses;
        uint16_t draws;
        float win_rate;
    } vs_opponents[16];  // Max 16 different opponents tracked
    
    // Estimated rating range based on all games
    uint16_t estimated_rating;
    uint16_t estimated_rating_lower;
    uint16_t estimated_rating_upper;
} HumanPlayerRating;

// Standard benchmark test configuration
typedef struct {
    DeckType deck_type;
    uint16_t initial_cash;
    PlayerID test_position;  // Test agent as A or B
    uint16_t num_sims;
} BenchmarkConfig;

// Calculate rating based on win rate against Keeper
uint16_t calculate_keeper_rating(float win_rate_vs_keeper);

// Alternative: logarithmic scaling for more granularity at extremes
uint16_t calculate_keeper_rating_log(float win_rate_vs_keeper);

// Run standard benchmark test suite
AgentRating* benchmark_agent(StrategySet* agent_strategies,
                              uint16_t sims_per_config);

// Run extended benchmark with confidence intervals
AgentRatingExtended* benchmark_agent_extended(StrategySet* agent_strategies,
                                              uint16_t sims_per_config);

// Update human player rating after games
void update_human_rating(HumanPlayerRating* player,
                         const char* opponent_name,
                         uint16_t opponent_rating,
                         GameStateEnum game_result);

// Calculate estimated rating using all opponents
uint16_t calculate_estimated_rating(HumanPlayerRating* player);

// Get rating tier description
const char* get_rating_description(uint16_t rating);

// Calculate confidence interval for win rate
void calculate_confidence_interval(uint16_t wins, uint16_t total,
                                   float* lower, float* upper);

#endif // RATING_H
```

## Core Implementation

```c
// In src/rating.c

#include "rating.h"
#include "game_state.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Rating tier descriptions
const char* get_rating_description(uint16_t rating)
{
    if(rating >= 900) return "Legendary";
    if(rating >= 800) return "Master";
    if(rating >= 700) return "Expert";
    if(rating >= 600) return "Advanced";
    if(rating >= 500) return "Proficient";
    if(rating >= 400) return "Competent";
    if(rating >= 300) return "Intermediate";
    if(rating >= 200) return "Novice";
    if(rating >= 100) return "Beginner";
    return "Learning";
}

// Simple linear scaling
uint16_t calculate_keeper_rating(float win_rate_vs_keeper)
{
    // Win rate of 0.0 -> rating 0
    // Win rate of 0.5 -> rating 500 (equal to Keeper)
    // Win rate of 1.0 -> rating 1000
    
    uint16_t rating = (uint16_t)(win_rate_vs_keeper * MAX_RATING);
    
    // Clamp to valid range
    if(rating < MIN_RATING) rating = MIN_RATING;
    if(rating > MAX_RATING) rating = MAX_RATING;
    
    return rating;
}

// Logarithmic scaling for more granularity at extremes
uint16_t calculate_keeper_rating_log(float win_rate_vs_keeper)
{
    // Protect against edge cases
    if(win_rate_vs_keeper <= 0.001) return MIN_RATING;
    if(win_rate_vs_keeper >= 0.999) return MAX_RATING;
    
    // Logistic-based scaling
    // Maps 0.5 -> 500, with steeper curve at extremes
    float odds = win_rate_vs_keeper / (1.0f - win_rate_vs_keeper);
    float log_odds = logf(odds);
    
    // Scale: log(odds) of 0 (50% win rate) -> 500
    // Each doubling of odds adds ~100 points
    uint16_t rating = 500 + (uint16_t)(log_odds * 144.27f);
    
    if(rating < MIN_RATING) rating = MIN_RATING;
    if(rating > MAX_RATING) rating = MAX_RATING;
    
    return rating;
}

// Calculate 95% confidence interval for win rate using normal approximation
void calculate_confidence_interval(uint16_t wins, uint16_t total,
                                   float* lower, float* upper)
{
    if(total == 0) {
        *lower = 0.0f;
        *upper = 1.0f;
        return;
    }
    
    float p = (float)wins / total;
    float z = 1.96f;  // 95% confidence interval
    float se = sqrtf(p * (1.0f - p) / total);
    
    *lower = p - z * se;
    *upper = p + z * se;
    
    // Clamp to [0, 1]
    if(*lower < 0.0f) *lower = 0.0f;
    if(*upper > 1.0f) *upper = 1.0f;
}

// Standard benchmark configurations
const BenchmarkConfig STANDARD_BENCHMARK[] = {
    {DECK_RANDOM, 30, PLAYER_A, 500},
    {DECK_RANDOM, 30, PLAYER_B, 500},
    {DECK_MONOCHROME, 30, PLAYER_A, 250},
    {DECK_MONOCHROME, 30, PLAYER_B, 250},
    // Total: 1500 games for full benchmark
};

const uint8_t NUM_BENCHMARK_CONFIGS = 
    sizeof(STANDARD_BENCHMARK) / sizeof(BenchmarkConfig);

// Run a single benchmark configuration
void run_benchmark_config(StrategySet* test_agent,
                          StrategySet* keeper_agent,
                          const BenchmarkConfig* config,
                          AgentRating* rating)
{
    struct gamestats gstats;
    memset(&gstats, 0, sizeof(struct gamestats));
    
    // Setup strategies based on test position
    StrategySet combined;
    if(config->test_position == PLAYER_A) {
        combined.attack_strategy[PLAYER_A] = test_agent->attack_strategy[PLAYER_A];
        combined.defense_strategy[PLAYER_A] = test_agent->defense_strategy[PLAYER_A];
        combined.attack_strategy[PLAYER_B] = keeper_agent->attack_strategy[PLAYER_B];
        combined.defense_strategy[PLAYER_B] = keeper_agent->defense_strategy[PLAYER_B];
    } else {
        combined.attack_strategy[PLAYER_A] = keeper_agent->attack_strategy[PLAYER_A];
        combined.defense_strategy[PLAYER_A] = keeper_agent->defense_strategy[PLAYER_A];
        combined.attack_strategy[PLAYER_B] = test_agent->attack_strategy[PLAYER_B];
        combined.defense_strategy[PLAYER_B] = test_agent->defense_strategy[PLAYER_B];
    }
    
    // Run simulations
    run_simulation(config->num_sims, config->initial_cash,
                   &gstats, &combined);
    
    // Accumulate results
    if(config->test_position == PLAYER_A) {
        rating->wins_vs_keeper += gstats.cumul_player_wins[PLAYER_A];
        rating->losses_vs_keeper += gstats.cumul_player_wins[PLAYER_B];
    } else {
        rating->wins_vs_keeper += gstats.cumul_player_wins[PLAYER_B];
        rating->losses_vs_keeper += gstats.cumul_player_wins[PLAYER_A];
    }
    
    rating->draws_vs_keeper += gstats.cumul_number_of_draws;
    rating->total_games_vs_keeper += config->num_sims;
}

AgentRating* benchmark_agent(StrategySet* agent_strategies,
                              uint16_t sims_per_config)
{
    AgentRating* rating = (AgentRating*)malloc(sizeof(AgentRating));
    if(!rating) return NULL;
    
    // Initialize
    memset(rating, 0, sizeof(AgentRating));
    strcpy(rating->agent_name, "test_agent");  // TODO: Get actual name
    
    // Create Keeper strategy
    StrategySet* keeper = create_keeper_strategy();
    
    // Run all benchmark configurations
    for(uint8_t i = 0; i < NUM_BENCHMARK_CONFIGS; i++) {
        BenchmarkConfig config = STANDARD_BENCHMARK[i];
        config.num_sims = sims_per_config;
        run_benchmark_config(agent_strategies, keeper, &config, rating);
    }
    
    // Calculate win rate
    rating->win_rate_vs_keeper = 
        (float)rating->wins_vs_keeper / rating->total_games_vs_keeper;
    
    // Calculate Keeper rating
    rating->keeper_rating = calculate_keeper_rating(rating->win_rate_vs_keeper);
    
    // Set description
    strcpy(rating->rating_description, 
           get_rating_description(rating->keeper_rating));
    
    // Cleanup
    free_strategy_set(keeper);
    
    return rating;
}

AgentRatingExtended* benchmark_agent_extended(StrategySet* agent_strategies,
                                              uint16_t sims_per_config)
{
    AgentRatingExtended* ext_rating = 
        (AgentRatingExtended*)malloc(sizeof(AgentRatingExtended));
    if(!ext_rating) return NULL;
    
    // Run base benchmark
    AgentRating* base = benchmark_agent(agent_strategies, sims_per_config);
    if(!base) {
        free(ext_rating);
        return NULL;
    }
    
    // Copy base rating
    memcpy(&ext_rating->base, base, sizeof(AgentRating));
    free(base);
    
    // Calculate confidence interval
    calculate_confidence_interval(ext_rating->base.wins_vs_keeper,
                                   ext_rating->base.total_games_vs_keeper,
                                   &ext_rating->confidence_interval_lower,
                                   &ext_rating->confidence_interval_upper);
    
    // Convert CI to rating bounds
    ext_rating->rating_lower_bound = 
        calculate_keeper_rating(ext_rating->confidence_interval_lower);
    ext_rating->rating_upper_bound = 
        calculate_keeper_rating(ext_rating->confidence_interval_upper);
    
    // Mark as provisional if < 100 games
    ext_rating->rating_is_provisional = 
        (ext_rating->base.total_games_vs_keeper < 100);
    
    return ext_rating;
}

// Create the Keeper strategy (placeholder - implement actual strategy)
StrategySet* create_keeper_strategy(void)
{
    StrategySet* keeper = create_strategy_set();
    
    // TODO: Implement actual Keeper strategy
    // This is a placeholder - use balanced/heuristic hybrid
    set_player_strategy(keeper, PLAYER_A,
                        keeper_attack_strategy,
                        keeper_defense_strategy);
    set_player_strategy(keeper, PLAYER_B,
                        keeper_attack_strategy,
                        keeper_defense_strategy);
    
    return keeper;
}

// Human player rating functions
void update_human_rating(HumanPlayerRating* player,
                         const char* opponent_name,
                         uint16_t opponent_rating,
                         GameStateEnum game_result)
{
    // Update vs Keeper if opponent is Keeper
    if(strcmp(opponent_name, BENCHMARK_AGENT_NAME) == 0) {
        player->vs_keeper.total_games_vs_keeper++;
        
        if(game_result == PLAYER_A_WINS || game_result == PLAYER_B_WINS) {
            // Determine if human won (assume human is always being tracked)
            bool human_won = true;  // TODO: Pass actual player position
            if(human_won)
                player->vs_keeper.wins_vs_keeper++;
            else
                player->vs_keeper.losses_vs_keeper++;
        } else {
            player->vs_keeper.draws_vs_keeper++;
        }
        
        // Recalculate Keeper rating
        player->vs_keeper.win_rate_vs_keeper = 
            (float)player->vs_keeper.wins_vs_keeper /
            player->vs_keeper.total_games_vs_keeper;
        player->keeper_rating = 
            calculate_keeper_rating(player->vs_keeper.win_rate_vs_keeper);
    }
    
    // Update vs other opponents
    int opponent_idx = -1;
    for(uint16_t i = 0; i < player->num_opponents; i++) {
        if(strcmp(player->vs_opponents[i].opponent_name, opponent_name) == 0) {
            opponent_idx = i;
            break;
        }
    }
    
    if(opponent_idx == -1 && player->num_opponents < 16) {
        // Add new opponent
        opponent_idx = player->num_opponents++;
        strcpy(player->vs_opponents[opponent_idx].opponent_name, opponent_name);
        player->vs_opponents[opponent_idx].opponent_rating = opponent_rating;
        player->vs_opponents[opponent_idx].wins = 0;
        player->vs_opponents[opponent_idx].losses = 0;
        player->vs_opponents[opponent_idx].draws = 0;
    }
    
    if(opponent_idx >= 0) {
        bool human_won = true;  // TODO: Pass actual result
        if(game_result == DRAW) {
            player->vs_opponents[opponent_idx].draws++;
        } else if(human_won) {
            player->vs_opponents[opponent_idx].wins++;
        } else {
            player->vs_opponents[opponent_idx].losses++;
        }
        
        uint16_t total = player->vs_opponents[opponent_idx].wins +
                        player->vs_opponents[opponent_idx].losses +
                        player->vs_opponents[opponent_idx].draws;
        player->vs_opponents[opponent_idx].win_rate = 
            (float)player->vs_opponents[opponent_idx].wins / total;
    }
    
    // Recalculate estimated rating
    player->estimated_rating = calculate_estimated_rating(player);
}

uint16_t calculate_estimated_rating(HumanPlayerRating* player)
{
    // Start with Keeper rating if available
    if(player->vs_keeper.total_games_vs_keeper > 0) {
        // If only Keeper games, return Keeper rating
        if(player->num_opponents == 0)
            return player->keeper_rating;
        
        // Weight Keeper rating with other opponents
        float total_weight = 0.0f;
        float weighted_sum = 0.0f;
        
        // Keeper gets weight based on number of games
        float keeper_weight = sqrtf(player->vs_keeper.total_games_vs_keeper);
        weighted_sum += player->keeper_rating * keeper_weight;
        total_weight += keeper_weight;
        
        // Add weighted ratings from other opponents
        for(uint16_t i = 0; i < player->num_opponents; i++) {
            uint16_t total_games = player->vs_opponents[i].wins +
                                  player->vs_opponents[i].losses +
                                  player->vs_opponents[i].draws;
            if(total_games == 0) continue;
            
            // Estimate player rating based on opponent rating and win rate
            float win_rate = player->vs_opponents[i].win_rate;
            uint16_t opponent_rating = player->vs_opponents[i].opponent_rating;
            
            // Simple model: if 50% win rate, equal rating
            // Each 10% above 50% adds ~100 rating points
            int rating_diff = (int)((win_rate - 0.5f) * 1000.0f);
            uint16_t estimated = opponent_rating + rating_diff;
            if(estimated < MIN_RATING) estimated = MIN_RATING;
            if(estimated > MAX_RATING) estimated = MAX_RATING;
            
            float weight = sqrtf(total_games);
            weighted_sum += estimated * weight;
            total_weight += weight;
        }
        
        return (uint16_t)(weighted_sum / total_weight);
    }
    
    return BENCHMARK_RATING;  // Default if no games played
}
```

## Display and Reporting

```c
// In src/rating.c (continued)

void print_agent_rating(const AgentRating* rating)
{
    printf("\n=== Agent Rating Report ===\n");
    printf("Agent: %s\n", rating->agent_name);
    printf("Keeper Rating: %u (%s)\n\n",
           rating->keeper_rating,
           rating->rating_description);
    
    printf("Games vs Keeper: %u\n", rating->total_games_vs_keeper);
    printf("  Wins:   %u (%.1f%%)\n",
           rating->wins_vs_keeper,
           100.0f * rating->wins_vs_keeper / rating->total_games_vs_keeper);
    printf("  Losses: %u (%.1f%%)\n",
           rating->losses_vs_keeper,
           100.0f * rating->losses_vs_keeper / rating->total_games_vs_keeper);
    printf("  Draws:  %u (%.1f%%)\n",
           rating->draws_vs_keeper,
           100.0f * rating->draws_vs_keeper / rating->total_games_vs_keeper);
}

void print_agent_rating_extended(const AgentRatingExtended* rating)
{
    print_agent_rating(&rating->base);
    
    printf("\n95%% Confidence Interval:\n");
    printf("  Win Rate: %.1f%% - %.1f%%\n",
           100.0f * rating->confidence_interval_lower,
           100.0f * rating->confidence_interval_upper);
    printf("  Rating Range: %u - %u\n",
           rating->rating_lower_bound,
           rating->rating_upper_bound);
    
    if(rating->rating_is_provisional)
        printf("  Status: PROVISIONAL (< 100 games)\n");
    else
        printf("  Status: ESTABLISHED\n");
}

void print_human_player_rating(const HumanPlayerRating* player)
{
    printf("\n=== Human Player Rating ===\n");
    printf("Player: %s\n", player->player_name);
    printf("Keeper Rating: %u (%s)\n\n",
           player->keeper_rating,
           get_rating_description(player->keeper_rating));
    
    printf("Games vs Keeper: %u\n", player->vs_keeper.total_games_vs_keeper);
    if(player->vs_keeper.total_games_vs_keeper > 0) {
        printf("  Wins:   %u (%.1f%%)\n",
               player->vs_keeper.wins_vs_keeper,
               100.0f * player->vs_keeper.win_rate_vs_keeper);
        printf("  Losses: %u (%.1f%%)\n",
               player->vs_keeper.losses_vs_keeper,
               100.0f * player->vs_keeper.losses_vs_keeper /
               player->vs_keeper.total_games_vs_keeper);
        printf("  Draws:  %u (%.1f%%)\n\n",
               player->vs_keeper.draws_vs_keeper,
               100.0f * player->vs_keeper.draws_vs_keeper /
               player->vs_keeper.total_games_vs_keeper);
    }
    
    if(player->num_opponents > 0) {
        printf("Additional Stats:\n");
        for(uint16_t i = 0; i < player->num_opponents; i++) {
            uint16_t total = player->vs_opponents[i].wins +
                           player->vs_opponents[i].losses +
                           player->vs_opponents[i].draws;
            if(total == 0) continue;
            
            printf("  vs %s (%u): %uW-%uL (%.1f%%)\n",
                   player->vs_opponents[i].opponent_name,
                   player->vs_opponents[i].opponent_rating,
                   player->vs_opponents[i].wins,
                   player->vs_opponents[i].losses,
                   100.0f * player->vs_opponents[i].win_rate);
        }
        
        printf("\nEstimated Rating: %u\n", player->estimated_rating);
        if(player->estimated_rating_lower > 0 &&
           player->estimated_rating_upper > 0) {
            printf("Estimated Range: %u - %u\n",
                   player->estimated_rating_lower,
                   player->estimated_rating_upper);
        }
    }
}
```

## Integration with Simparam String

```c
// Benchmark agent name in simparam strings
const char* KEEPER_SIMPARAM = "keeper";

// Examples:
// "rnd_keeper_rand"      // Keeper as player A vs random
// "mon_rand_keeper"      // Random vs Keeper as player B
// "rnd_keeper_keeper"    // Keeper vs itself (calibration test)
```

## Usage Examples

### Benchmark an AI Agent

```c
// In main.c or test file

int main() {
    // Initialize random number generator
    MTwister_rand_struct = seedRand(M_TWISTER_SEED);
    
    // Create test agent strategy
    StrategySet* test_agent = create_strategy_set();
    set_player_strategy(test_agent, PLAYER_A,
                        balanced_attack_strategy,
                        balanced_defense_strategy);
    set_player_strategy(test_agent, PLAYER_B,
                        balanced_attack_strategy,
                        balanced_defense_strategy);
    
    // Run benchmark (500 games per config = 2000 total)
    AgentRatingExtended* rating = benchmark_agent_extended(test_agent, 500);
    
    // Display results
    print_agent_rating_extended(rating);
    
    // Cleanup
    free(rating);
    free_strategy_set(test_agent);
    
    return 0;
}
```

### Track Human Player Rating

```c
// Initialize human player
HumanPlayerRating player;
memset(&player, 0, sizeof(HumanPlayerRating));
strcpy(player.player_name, "Alice");

// Play games and update rating
for(int game = 0; game < 50; game++) {
    // Play game vs Keeper
    GameStateEnum result = play_interactive_game(/* ... */);
    
    // Update rating
    update_human_rating(&player, BENCHMARK_AGENT_NAME,
                        BENCHMARK_RATING, result);
}

// Display rating
print_human_player_rating(&player);

// Save rating to file
save_human_rating_to_file(&player, "player_ratings.csv");
```

## Rating Persistence

### CSV Format for Player Ratings

```c
// In src/rating.h (add)

void save_human_rating_to_file(const HumanPlayerRating* player,
                                const char* filename);
HumanPlayerRating* load_human_rating_from_file(const char* player_name,
                                                const char* filename);
```

```c
// In src/rating.c (add)

void save_human_rating_to_file(const HumanPlayerRating* player,
                                const char* filename)
{
    FILE* f = fopen(filename, "a");  // Append mode
    if(!f) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return;
    }
    
    // Check if file is empty (need header)
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    
    if(size == 0) {
        // Write header
        fprintf(f, "player_name,keeper_rating,rating_description,"
                   "total_games_vs_keeper,wins_vs_keeper,losses_vs_keeper,"
                   "draws_vs_keeper,win_rate_vs_keeper,estimated_rating,"
                   "last_updated\n");
    }
    
    // Get timestamp
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[20];
    snprintf(timestamp, 20, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    
    // Write player data
    fprintf(f, "%s,%u,%s,%u,%u,%u,%u,%.4f,%u,%s\n",
            player->player_name,
            player->keeper_rating,
            get_rating_description(player->keeper_rating),
            player->vs_keeper.total_games_vs_keeper,
            player->vs_keeper.wins_vs_keeper,
            player->vs_keeper.losses_vs_keeper,
            player->vs_keeper.draws_vs_keeper,
            player->vs_keeper.win_rate_vs_keeper,
            player->estimated_rating,
            timestamp);
    
    fclose(f);
}

HumanPlayerRating* load_human_rating_from_file(const char* player_name,
                                                const char* filename)
{
    FILE* f = fopen(filename, "r");
    if(!f) return NULL;
    
    HumanPlayerRating* player = 
        (HumanPlayerRating*)malloc(sizeof(HumanPlayerRating));
    if(!player) {
        fclose(f);
        return NULL;
    }
    
    char line[512];
    bool found = false;
    
    // Skip header
    fgets(line, sizeof(line), f);
    
    // Search for player
    while(fgets(line, sizeof(line), f)) {
        char name[32];
        if(sscanf(line, "%31[^,]", name) == 1) {
            if(strcmp(name, player_name) == 0) {
                // Parse the line
                sscanf(line, "%31[^,],%hu,%*[^,],%hu,%hu,%hu,%hu,%f,%hu",
                       player->player_name,
                       &player->keeper_rating,
                       &player->vs_keeper.total_games_vs_keeper,
                       &player->vs_keeper.wins_vs_keeper,
                       &player->vs_keeper.losses_vs_keeper,
                       &player->vs_keeper.draws_vs_keeper,
                       &player->vs_keeper.win_rate_vs_keeper,
                       &player->estimated_rating);
                
                found = true;
                break;
            }
        }
    }
    
    fclose(f);
    
    if(!found) {
        free(player);
        return NULL;
    }
    
    return player;
}
```

## Rating System Statistics

### Keeper Calibration

To ensure the Keeper is properly calibrated at rating 500:

```c
// Calibration test: Keeper should win ~50% against itself
void calibrate_keeper(void)
{
    printf("=== Keeper Calibration Test ===\n");
    
    StrategySet* keeper = create_keeper_strategy();
    
    struct gamestats gstats;
    memset(&gstats, 0, sizeof(struct gamestats));
    
    // Run 10,000 games Keeper vs Keeper
    run_simulation(10000, 30, &gstats, keeper);
    
    float win_rate_a = (float)gstats.cumul_player_wins[PLAYER_A] /
                       gstats.simnum;
    
    printf("Keeper vs Keeper Results:\n");
    printf("  Player A wins: %u (%.2f%%)\n",
           gstats.cumul_player_wins[PLAYER_A],
           100.0f * win_rate_a);
    printf("  Player B wins: %u (%.2f%%)\n",
           gstats.cumul_player_wins[PLAYER_B],
           100.0f * (1.0f - win_rate_a));
    printf("  Draws: %u\n", gstats.cumul_number_of_draws);
    
    if(win_rate_a >= 0.48f && win_rate_a <= 0.52f)
        printf("  Status: PROPERLY CALIBRATED\n");
    else
        printf("  Status: NEEDS ADJUSTMENT\n");
    
    free_strategy_set(keeper);
}
```

### Rating Distribution Analysis

```c
// Analyze rating distribution across multiple agents
typedef struct {
    uint16_t rating_bins[11];  // 0-99, 100-199, ..., 900-999, 1000
    uint16_t total_agents;
    float avg_rating;
    uint16_t median_rating;
} RatingDistribution;

void analyze_rating_distribution(AgentRating** agents, uint16_t num_agents,
                                 RatingDistribution* dist)
{
    memset(dist, 0, sizeof(RatingDistribution));
    dist->total_agents = num_agents;
    
    uint32_t sum = 0;
    uint16_t ratings[num_agents];
    
    for(uint16_t i = 0; i < num_agents; i++) {
        uint16_t rating = agents[i]->keeper_rating;
        ratings[i] = rating;
        sum += rating;
        
        // Bin the rating
        uint8_t bin = rating / 100;
        if(bin > 10) bin = 10;
        dist->rating_bins[bin]++;
    }
    
    dist->avg_rating = (float)sum / num_agents;
    
    // Calculate median (simple bubble sort for small arrays)
    for(uint16_t i = 0; i < num_agents - 1; i++) {
        for(uint16_t j = 0; j < num_agents - i - 1; j++) {
            if(ratings[j] > ratings[j + 1]) {
                uint16_t temp = ratings[j];
                ratings[j] = ratings[j + 1];
                ratings[j + 1] = temp;
            }
        }
    }
    dist->median_rating = ratings[num_agents / 2];
}

void print_rating_distribution(const RatingDistribution* dist)
{
    printf("\n=== Rating Distribution ===\n");
    printf("Total Agents: %u\n", dist->total_agents);
    printf("Average Rating: %.1f (%s)\n",
           dist->avg_rating,
           get_rating_description((uint16_t)dist->avg_rating));
    printf("Median Rating: %u (%s)\n\n",
           dist->median_rating,
           get_rating_description(dist->median_rating));
    
    printf("Distribution:\n");
    const char* tier_names[] = {
        "Learning (0-99)",
        "Beginner (100-199)",
        "Novice (200-299)",
        "Intermediate (300-399)",
        "Competent (400-499)",
        "Proficient (500-599)",
        "Advanced (600-699)",
        "Expert (700-799)",
        "Master (800-899)",
        "Legendary (900-999)",
        "Perfect (1000)"
    };
    
    for(uint8_t i = 0; i <= 10; i++) {
        if(dist->rating_bins[i] > 0) {
            printf("  %s: %u (%.1f%%)\n",
                   tier_names[i],
                   dist->rating_bins[i],
                   100.0f * dist->rating_bins[i] / dist->total_agents);
        }
    }
}
```

## Advanced Rating Features

### ELO-Style Multi-Agent Ratings

For comparing agents against each other (not just Keeper):

```c
// In src/rating.c

#define K_FACTOR 32.0f  // ELO K-factor

float calculate_expected_score(uint16_t rating_a, uint16_t rating_b)
{
    float diff = (float)(rating_b - rating_a);
    return 1.0f / (1.0f + powf(10.0f, diff / 400.0f));
}

void update_elo_rating(AgentEloRating* agent_a, AgentEloRating* agent_b,
                       GameStateEnum result)
{
    // Calculate expected scores
    float expected_a = calculate_expected_score(agent_a->elo_rating,
                                                 agent_b->elo_rating);
    float expected_b = 1.0f - expected_a;
    
    // Actual scores
    float actual_a, actual_b;
    if(result == PLAYER_A_WINS) {
        actual_a = 1.0f;
        actual_b = 0.0f;
        agent_a->wins++;
        agent_b->losses++;
    } else if(result == PLAYER_B_WINS) {
        actual_a = 0.0f;
        actual_b = 1.0f;
        agent_a->losses++;
        agent_b->wins++;
    } else {  // DRAW
        actual_a = 0.5f;
        actual_b = 0.5f;
        agent_a->draws++;
        agent_b->draws++;
    }
    
    // Update ratings
    agent_a->elo_rating += (uint16_t)(K_FACTOR * (actual_a - expected_a));
    agent_b->elo_rating += (uint16_t)(K_FACTOR * (actual_b - expected_b));
    
    // Clamp to valid range
    if(agent_a->elo_rating < MIN_RATING) agent_a->elo_rating = MIN_RATING;
    if(agent_a->elo_rating > MAX_RATING) agent_a->elo_rating = MAX_RATING;
    if(agent_b->elo_rating < MIN_RATING) agent_b->elo_rating = MIN_RATING;
    if(agent_b->elo_rating > MAX_RATING) agent_b->elo_rating = MAX_RATING;
    
    agent_a->games_played++;
    agent_b->games_played++;
}
```

### Rating Leaderboard

```c
typedef struct {
    AgentRating* agents[100];
    uint16_t num_agents;
} Leaderboard;

void sort_leaderboard(Leaderboard* board)
{
    // Simple bubble sort (fine for small leaderboards)
    for(uint16_t i = 0; i < board->num_agents - 1; i++) {
        for(uint16_t j = 0; j < board->num_agents - i - 1; j++) {
            if(board->agents[j]->keeper_rating <
               board->agents[j + 1]->keeper_rating) {
                AgentRating* temp = board->agents[j];
                board->agents[j] = board->agents[j + 1];
                board->agents[j + 1] = temp;
            }
        }
    }
}

void print_leaderboard(const Leaderboard* board, uint16_t top_n)
{
    printf("\n=== Rating Leaderboard (Top %u) ===\n", top_n);
    printf("%-4s %-20s %-8s %-12s %s\n",
           "Rank", "Agent", "Rating", "Tier", "Games");
    printf("%-4s %-20s %-8s %-12s %s\n",
           "----", "--------------------", "--------",
           "------------", "-----");
    
    uint16_t display_count = (top_n < board->num_agents) ?
                             top_n : board->num_agents;
    
    for(uint16_t i = 0; i < display_count; i++) {
        printf("%-4u %-20s %-8u %-12s %u\n",
               i + 1,
               board->agents[i]->agent_name,
               board->agents[i]->keeper_rating,
               board->agents[i]->rating_description,
               board->agents[i]->total_games_vs_keeper);
    }
}
```

## Keeper Strategy Implementation Notes

The actual Keeper strategy needs to be implemented as a balanced/heuristic hybrid. Key characteristics:

1. **Deterministic**: Same input always produces same output
2. **Well-Calibrated**: Wins ~50% against itself
3. **Reasonable Decisions**: Uses good heuristics but not perfect
4. **Balanced Play**: Neither too aggressive nor too passive

```c
// Placeholder - actual implementation needed
void keeper_attack_strategy(struct gamestate* gstate)
{
    // TODO: Implement balanced heuristic attack strategy
    // - Consider hand size, cash balance, opponent energy
    // - Target effective hand size based on game state
    // - Balance resource usage with attack effectiveness
}

void keeper_defense_strategy(struct gamestate* gstate)
{
    // TODO: Implement balanced heuristic defense strategy
    // - Estimate expected attack damage
    // - Defend only when efficient (don't over-defend)
    // - Prioritize high defense efficiency cards
    // - Consider cash and hand size constraints
}
```

## Testing and Validation

### Unit Tests

```c
void test_rating_calculations(void)
{
    // Test linear scaling
    assert(calculate_keeper_rating(0.0f) == 0);
    assert(calculate_keeper_rating(0.5f) == 500);
    assert(calculate_keeper_rating(1.0f) == 1000);
    
    // Test confidence intervals
    float lower, upper;
    calculate_confidence_interval(50, 100, &lower, &upper);
    assert(lower >= 0.0f && lower <= 1.0f);
    assert(upper >= 0.0f && upper <= 1.0f);
    assert(lower <= 0.5f && upper >= 0.5f);
    
    printf("Rating calculation tests passed\n");
}

void test_keeper_calibration(void)
{
    // Verify Keeper wins ~50% against itself
    calibrate_keeper();
}
```

### Validation Suite

```c
void run_rating_validation_suite(void)
{
    printf("\n=== Rating System Validation Suite ===\n\n");
    
    // Test 1: Rating calculations
    test_rating_calculations();
    
    // Test 2: Keeper calibration
    test_keeper_calibration();
    
    // Test 3: Benchmark multiple agents
    printf("\n--- Benchmarking Test Agents ---\n");
    
    // Test very weak agent (should get low rating)
    StrategySet* weak = create_strategy_set();
    set_player_strategy(weak, PLAYER_A,
                        always_pass_strategy, never_defend_strategy);
    set_player_strategy(weak, PLAYER_B,
                        always_pass_strategy, never_defend_strategy);
    AgentRating* weak_rating = benchmark_agent(weak, 100);
    print_agent_rating(weak_rating);
    
    // Test random agent (should get ~400-500 rating)
    StrategySet* random = create_strategy_set();
    set_player_strategy(random, PLAYER_A,
                        random_attack_strategy, random_defense_strategy);
    set_player_strategy(random, PLAYER_B,
                        random_attack_strategy, random_defense_strategy);
    AgentRating* random_rating = benchmark_agent(random, 100);
    print_agent_rating(random_rating);
    
    // Test strong agent (should get high rating)
    StrategySet* strong = create_strategy_set();
    set_player_strategy(strong, PLAYER_A,
                        perfect_strategy, perfect_strategy);
    set_player_strategy(strong, PLAYER_B,
                        perfect_strategy, perfect_strategy);
    AgentRating* strong_rating = benchmark_agent(strong, 100);
    print_agent_rating(strong_rating);
    
    // Cleanup
    free_strategy_set(weak);
    free_strategy_set(random);
    free_strategy_set(strong);
    free(weak_rating);
    free(random_rating);
    free(strong_rating);
    
    printf("\n=== Validation Complete ===\n");
}
```

## Example Output

### Agent Rating Report
```
=== Agent Rating Report ===
Agent: balanced_e10g05
Keeper Rating: 487 (Competent)

Games vs Keeper: 2000
  Wins:   934 (46.7%)
  Losses: 1024 (51.2%)
  Draws:  42 (2.1%)

95% Confidence Interval:
  Win Rate: 44.5% - 48.9%
  Rating Range: 445 - 489
  Status: ESTABLISHED
```

### Human Player Rating Report
```
=== Human Player Rating ===
Player: Alice
Keeper Rating: 623 (Advanced)

Games vs Keeper: 150
  Wins:   94 (62.7%)
  Losses: 53 (35.3%)
  Draws:  3 (2.0%)

Additional Stats:
  vs balanced_e10g05 (487): 18W-12L (60.0%)
  vs heuristic_e15 (534): 23W-27L (46.0%)
  vs mcts_i1000 (712): 8W-22L (26.7%)

Estimated Rating: 618
Estimated Range: 595-641
```

### Leaderboard
```
=== Rating Leaderboard (Top 10) ===
Rank Agent                Rating   Tier         Games
---- -------------------- -------- ------------ -----
1    mcts_i2000           856      Master       2000
2    ismcts_i1500         801      Master       2000
3    heuristic_e18g09     687      Expert       2000
4    balanced_e12g08      571      Advanced     2000
5    keeper               500      Proficient   10000
6    random               412      Competent    5000
7    balanced_e08g05      389      Intermediate 2000
8    greedy_attack        245      Novice       2000
9    passive_defense      123      Beginner     2000
10   always_pass          12       Learning     2000
```

## Integration Checklist

- [ ] Implement Keeper strategy (balanced/heuristic hybrid)
- [ ] Calibrate Keeper to win ~50% against itself
- [ ] Add rating fields to StrategySet structure
- [ ] Implement rating calculation functions
- [ ] Implement benchmark test suite
- [ ] Add rating display functions
- [ ] Implement rating persistence (CSV)
- [ ] Add rating to simulation export (simparam)
- [ ] Create rating validation tests
- [ ] Document Keeper strategy parameters
- [ ] Create interactive rating tracker for human players
- [ ] Implement leaderboard display
- [ ] Add confidence interval calculations
- [ ] Test with multiple agents

## Future Enhancements

1. **Dynamic K-Factor**: Adjust ELO K-factor based on rating volatility
2. **Rating Decay**: Reduce rating over time without games
3. **Matchmaking**: Suggest opponents based on rating proximity
4. **Rating History**: Track rating changes over time
5. **Tournament Mode**: Round-robin or bracket tournaments with rating updates
6. **Rating Tiers**: Unlock features/achievements at rating milestones
7. **Skill Radar**: Visualize strengths (attack, defense, resource management)
8. **Keeper Variants**: Multiple Keeper difficulties (Easy, Normal, Hard)

## Conclusion

This rating system provides a robust, understandable method for evaluating AI agents and human players. The Keeper benchmark serves as a consistent reference point, while the 0-1000 scale offers intuitive interpretation. The system supports both simple win-rate-based ratings and more sophisticated multi-opponent analysis.