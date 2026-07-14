# Oracle Simulation Data Export Format Specification

## Overview

This document specifies the format and conventions for exporting simulation results from the Oracle: The Champions of Arcadia game simulator. The export system supports both automated simulation modes and interactive play sessions.

## Primary Format: CSV

CSV (Comma-Separated Values) is the primary format due to its universal compatibility with Excel, R, Python (pandas), and other statistical analysis tools.

## File Naming Convention

### Format
```
oracle_sim_{mode}_{timestamp}_{simparam}.csv
```

### Components

- **mode**: Either "auto" or "interactive"
- **timestamp**: ISO 8601 format `YYYYMMDD_HHMMSS`
- **simparam**: Readable parameter string (max 20 characters)

### Examples
```
oracle_sim_auto_20251016_143022_rnd_rand_rand.csv
oracle_sim_auto_20251016_143530_rnd_bal_heur_e10.csv
oracle_sim_interactive_20251016_150245_mon_mcts_i1k.csv
```

## Simulation Parameter String (simparam)

### Format Structure
```
{deck}_{strat_a}_{strat_b}_{variant}
```

Maximum length: 20 characters

### Component Abbreviations

#### Deck Types (1-3 chars)
- `rnd` - Random distribution
- `mon` - Monochrome
- `cst` - Custom

#### Strategy Abbreviations (2-4 chars)
- `rand` - Random
- `bal` - Balanced
- `heur` - Heuristic
- `mc` - Monte Carlo single stage
- `mcts` - Monte Carlo Tree Search
- `ism` - Information Set MCTS

#### Variant/Parameters (optional, remaining chars)
- For balanced: `e10g05` (epsilon=1.0, gamma=0.5)
- For heuristic: `e15g08` (epsilon=1.5, gamma=0.8)
- For MC: `s100` (100 sims per move)
- For MCTS: `i1000` (1000 iterations)

### Examples
```
rnd_rand_rand          (13 chars) - random deck, both random
rnd_bal_rand_e10g05    (19 chars) - balanced vs random with params
mon_heur_heur_e12      (14 chars) - monochrome, both heuristic
rnd_mcts_mc_i500s100   (20 chars) - MCTS vs MC with params
cst_ism_bal_i2k        (15 chars) - custom deck, ISMCTS vs balanced
```

## CSV Structure

### Detail File (Per-Game Results)

Each row represents one completed game.

#### Header
```csv
sim_id,simparam,sim_mode,timestamp,total_sims,initial_cash,rng_seed,deck_type,player_a_strategy,player_a_params,player_b_strategy,player_b_params,game_number,winner,turns_played,rounds_played,final_energy_a,final_energy_b,final_cash_a,final_cash_b,final_hand_size_a,final_hand_size_b,final_deck_size_a,final_deck_size_b,total_damage_dealt_a,total_damage_dealt_b,champions_played_a,champions_played_b,draw_cards_played_a,draw_cards_played_b,cash_cards_played_a,cash_cards_played_b
```

#### Column Definitions

**Simulation Metadata** (same for all rows in a run):
- `sim_id`: Unique identifier for this simulation run
- `simparam`: Readable parameter string
- `sim_mode`: "auto" or "interactive"
- `timestamp`: ISO 8601 format
- `total_sims`: Total number of games in this run
- `initial_cash`: Starting cash for both players
- `rng_seed`: Random number generator seed
- `deck_type`: "RANDOM", "MONOCHROME", or "CUSTOM"

**Strategy Information**:
- `player_a_strategy`: Strategy name for player A
- `player_a_params`: JSON-like string with strategy parameters
- `player_b_strategy`: Strategy name for player B
- `player_b_params`: JSON-like string with strategy parameters

**Game Outcome**:
- `game_number`: Game number within this simulation run (1 to total_sims)
- `winner`: "PLAYER_A", "PLAYER_B", or "DRAW"
- `turns_played`: Total turns in the game
- `rounds_played`: Total rounds (turns_played / 2, rounded up)

**Final State**:
- `final_energy_a`: Player A's remaining energy
- `final_energy_b`: Player B's remaining energy
- `final_cash_a`: Player A's remaining cash
- `final_cash_b`: Player B's remaining cash
- `final_hand_size_a`: Cards in player A's hand
- `final_hand_size_b`: Cards in player B's hand
- `final_deck_size_a`: Cards in player A's deck
- `final_deck_size_b`: Cards in player B's deck

**Aggregate Statistics**:
- `total_damage_dealt_a`: Total damage dealt by player A
- `total_damage_dealt_b`: Total damage dealt by player B
- `champions_played_a`: Champion cards played by player A
- `champions_played_b`: Champion cards played by player B
- `draw_cards_played_a`: Draw cards played by player A
- `draw_cards_played_b`: Draw cards played by player B
- `cash_cards_played_a`: Cash exchange cards played by player A
- `cash_cards_played_b`: Cash exchange cards played by player B

#### Example Data
```csv
1,rnd_rand_rand,auto,2025-10-16T14:30:22,1000,30,1337,RANDOM,random,"{}",random,"{}",1,PLAYER_A,64,32,23,0,12,8,5,3,18,15,99,76,18,15,3,2,0,1
1,rnd_rand_rand,auto,2025-10-16T14:30:22,1000,30,1337,RANDOM,random,"{}",random,"{}",2,PLAYER_B,58,29,0,31,9,15,2,6,16,17,68,99,16,17,2,3,1,0
1,rnd_rand_rand,auto,2025-10-16T14:30:22,1000,30,1337,RANDOM,random,"{}",random,"{}",3,DRAW,500,250,45,42,5,6,4,5,12,13,54,57,12,13,4,4,0,0
```

### Summary File (Aggregate Results)

One row per simulation run with aggregate statistics.

#### Filename
```
oracle_sim_{mode}_{timestamp}_{simparam}_summary.csv
```

#### Header
```csv
sim_id,simparam,sim_mode,timestamp,total_sims,initial_cash,rng_seed,deck_type,player_a_strategy,player_a_params,player_b_strategy,player_b_params,player_a_wins,player_b_wins,draws,avg_turns,min_turns,max_turns,median_turns,std_dev_turns,avg_damage_a,avg_damage_b,avg_champions_a,avg_champions_b,win_rate_a,win_rate_b
```

#### Example Data
```csv
1,rnd_rand_rand,auto,2025-10-16T14:30:22,1000,30,1337,RANDOM,random,"{}",random,"{}",487,501,12,62.3,24,128,61,18.4,83.2,84.1,15.6,15.8,0.487,0.501
```

## Implementation

### Data Structures

```c
// In src/sim_export.h

#ifndef SIM_EXPORT_H
#define SIM_EXPORT_H

#include "game_types.h"
#include "strategy.h"
#include <stdio.h>
#include <time.h>

#define MAX_SIMPARAM_LEN 20

typedef struct {
    FILE* detail_file;
    FILE* summary_file;
    char detail_filename[256];
    char summary_filename[256];
    time_t start_time;
    uint32_t rng_seed;
    uint16_t sim_id;
    
    // Accumulate statistics for summary
    uint32_t total_turns;
    uint16_t min_turns;
    uint16_t max_turns;
    uint32_t sum_turns_squared;  // For std dev calculation
} SimExporter;

// CSV record structure
typedef struct {
    // Simulation metadata
    uint16_t sim_id;
    char simparam[MAX_SIMPARAM_LEN + 1];
    char sim_mode[16];
    char timestamp[20];
    uint16_t total_sims;
    uint16_t initial_cash;
    uint32_t rng_seed;
    char deck_type[16];
    
    // Strategy info
    char player_a_strategy[32];
    char player_a_params[128];
    char player_b_strategy[32];
    char player_b_params[128];
    
    // Game outcome
    uint16_t game_number;
    char winner[10];
    uint16_t turns_played;
    uint16_t rounds_played;
    
    // Final state
    uint8_t final_energy_a;
    uint8_t final_energy_b;
    uint16_t final_cash_a;
    uint16_t final_cash_b;
    uint8_t final_hand_size_a;
    uint8_t final_hand_size_b;
    uint8_t final_deck_size_a;
    uint8_t final_deck_size_b;
    
    // Aggregate statistics
    uint16_t total_damage_dealt_a;
    uint16_t total_damage_dealt_b;
    uint16_t champions_played_a;
    uint16_t champions_played_b;
    uint16_t draw_cards_played_a;
    uint16_t draw_cards_played_b;
    uint16_t cash_cards_played_a;
    uint16_t cash_cards_played_b;
} SimulationRecord;

// Initialize exporter for automated simulation mode
SimExporter* init_auto_exporter(DeckType deck_type, uint16_t total_sims,
                                uint16_t initial_cash, uint32_t rng_seed,
                                StrategySet* strategies);

// Initialize exporter for interactive mode
SimExporter* init_interactive_exporter(DeckType deck_type,
                                       uint16_t initial_cash,
                                       uint32_t rng_seed,
                                       StrategySet* strategies);

// Generate simulation parameter string
void generate_simparam_string(char* output, DeckType deck_type,
                              StrategySet* strategies);

// Write single game result to detail file
void export_game_result(SimExporter* exporter, struct gamestate* gstate,
                        uint16_t game_num, StrategySet* strategies);

// Write summary at end of simulation run
void export_summary(SimExporter* exporter, struct gamestats* gstats,
                    StrategySet* strategies);

// Cleanup and close files
void close_sim_exporter(SimExporter* exporter);

// Helper functions
const char* get_strategy_name(StrategySet* strategies, PlayerID player);
void get_strategy_params_string(StrategySet* strategies, PlayerID player,
                                char* output, size_t max_len);

#endif // SIM_EXPORT_H
```

### Core Implementation

```c
// In src/sim_export.c

#include "sim_export.h"
#include "game_constants.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void generate_simparam_string(char* output, DeckType deck_type,
                              StrategySet* strategies)
{
    const char* deck_str;
    switch(deck_type) {
        case DECK_RANDOM: deck_str = "rnd"; break;
        case DECK_MONOCHROME: deck_str = "mon"; break;
        case DECK_CUSTOM: deck_str = "cst"; break;
        default: deck_str = "unk"; break;
    }
    
    const char* strat_a = get_strategy_name(strategies, PLAYER_A);
    const char* strat_b = get_strategy_name(strategies, PLAYER_B);
    
    char params_a[10] = "";
    char params_b[10] = "";
    get_strategy_params_string(strategies, PLAYER_A, params_a, 9);
    get_strategy_params_string(strategies, PLAYER_B, params_b, 9);
    
    // Construct final string (max 20 chars)
    if(strlen(params_a) > 0 || strlen(params_b) > 0) {
        char combined_params[20];
        if(strlen(params_a) > 0 && strlen(params_b) > 0)
            snprintf(combined_params, 20, "%s%s", params_a, params_b);
        else if(strlen(params_a) > 0)
            snprintf(combined_params, 20, "%s", params_a);
        else
            snprintf(combined_params, 20, "%s", params_b);
            
        snprintf(output, MAX_SIMPARAM_LEN + 1, "%s_%s_%s_%s",
                 deck_str, strat_a, strat_b, combined_params);
    } else {
        snprintf(output, MAX_SIMPARAM_LEN + 1, "%s_%s_%s",
                 deck_str, strat_a, strat_b);
    }
}

SimExporter* init_auto_exporter(DeckType deck_type, uint16_t total_sims,
                                uint16_t initial_cash, uint32_t rng_seed,
                                StrategySet* strategies)
{
    SimExporter* exporter = (SimExporter*)malloc(sizeof(SimExporter));
    if(!exporter) return NULL;
    
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    
    char simparam[MAX_SIMPARAM_LEN + 1];
    generate_simparam_string(simparam, deck_type, strategies);
    
    // Generate detail filename
    snprintf(exporter->detail_filename, 255,
             "oracle_sim_auto_%04d%02d%02d_%02d%02d%02d_%s.csv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec,
             simparam);
    
    // Generate summary filename
    snprintf(exporter->summary_filename, 255,
             "oracle_sim_auto_%04d%02d%02d_%02d%02d%02d_%s_summary.csv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec,
             simparam);
    
    // Open files
    exporter->detail_file = fopen(exporter->detail_filename, "w");
    exporter->summary_file = fopen(exporter->summary_filename, "w");
    
    if(!exporter->detail_file || !exporter->summary_file) {
        close_sim_exporter(exporter);
        return NULL;
    }
    
    // Write headers
    fprintf(exporter->detail_file,
            "sim_id,simparam,sim_mode,timestamp,total_sims,initial_cash,"
            "rng_seed,deck_type,player_a_strategy,player_a_params,"
            "player_b_strategy,player_b_params,game_number,winner,"
            "turns_played,rounds_played,final_energy_a,final_energy_b,"
            "final_cash_a,final_cash_b,final_hand_size_a,final_hand_size_b,"
            "final_deck_size_a,final_deck_size_b,total_damage_dealt_a,"
            "total_damage_dealt_b,champions_played_a,champions_played_b,"
            "draw_cards_played_a,draw_cards_played_b,cash_cards_played_a,"
            "cash_cards_played_b\n");
    
    fprintf(exporter->summary_file,
            "sim_id,simparam,sim_mode,timestamp,total_sims,initial_cash,"
            "rng_seed,deck_type,player_a_strategy,player_a_params,"
            "player_b_strategy,player_b_params,player_a_wins,player_b_wins,"
            "draws,avg_turns,min_turns,max_turns,median_turns,std_dev_turns,"
            "avg_damage_a,avg_damage_b,avg_champions_a,avg_champions_b,"
            "win_rate_a,win_rate_b\n");
    
    exporter->start_time = now;
    exporter->rng_seed = rng_seed;
    exporter->sim_id = 1;  // Could be auto-incremented or timestamp-based
    exporter->total_turns = 0;
    exporter->min_turns = 65535;
    exporter->max_turns = 0;
    exporter->sum_turns_squared = 0;
    
    return exporter;
}

SimExporter* init_interactive_exporter(DeckType deck_type,
                                       uint16_t initial_cash,
                                       uint32_t rng_seed,
                                       StrategySet* strategies)
{
    SimExporter* exporter = (SimExporter*)malloc(sizeof(SimExporter));
    if(!exporter) return NULL;
    
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    
    char simparam[MAX_SIMPARAM_LEN + 1];
    generate_simparam_string(simparam, deck_type, strategies);
    
    // Generate unique filename for this session
    snprintf(exporter->detail_filename, 255,
             "oracle_sim_interactive_%04d%02d%02d_%02d%02d%02d_%s.csv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec,
             simparam);
    
    // No summary file for interactive mode
    exporter->summary_file = NULL;
    
    exporter->detail_file = fopen(exporter->detail_filename, "w");
    if(!exporter->detail_file) {
        free(exporter);
        return NULL;
    }
    
    // Write header
    fprintf(exporter->detail_file,
            "sim_id,simparam,sim_mode,timestamp,total_sims,initial_cash,"
            "rng_seed,deck_type,player_a_strategy,player_a_params,"
            "player_b_strategy,player_b_params,game_number,winner,"
            "turns_played,rounds_played,final_energy_a,final_energy_b,"
            "final_cash_a,final_cash_b,final_hand_size_a,final_hand_size_b,"
            "final_deck_size_a,final_deck_size_b,total_damage_dealt_a,"
            "total_damage_dealt_b,champions_played_a,champions_played_b,"
            "draw_cards_played_a,draw_cards_played_b,cash_cards_played_a,"
            "cash_cards_played_b\n");
    
    exporter->start_time = now;
    exporter->rng_seed = rng_seed;
    exporter->sim_id = 1;
    
    return exporter;
}

void export_game_result(SimExporter* exporter, struct gamestate* gstate,
                        uint16_t game_num, StrategySet* strategies)
{
    if(!exporter || !exporter->detail_file) return;
    
    // Get timestamp
    struct tm* t = localtime(&exporter->start_time);
    char timestamp[20];
    snprintf(timestamp, 20, "%04d-%02d-%02dT%02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    
    // Get winner string
    const char* winner;
    switch(gstate->game_state) {
        case PLAYER_A_WINS: winner = "PLAYER_A"; break;
        case PLAYER_B_WINS: winner = "PLAYER_B"; break;
        case DRAW: winner = "DRAW"; break;
        default: winner = "UNKNOWN"; break;
    }
    
    // Calculate rounds
    uint16_t rounds = (gstate->turn - 1) / 2 + 1;
    
    // Write CSV row
    fprintf(exporter->detail_file,
            "%u,%s,%s,%s,%u,%u,%u,%s,%s,\"%s\",%s,\"%s\",%u,%s,%u,%u,"
            "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
            exporter->sim_id,
            "simparam_placeholder",  // TODO: Store in exporter
            "auto",  // Or "interactive"
            timestamp,
            0,  // total_sims - update with actual value
            30,  // initial_cash - store in exporter
            exporter->rng_seed,
            "RANDOM",  // deck_type - store in exporter
            get_strategy_name(strategies, PLAYER_A),
            "{}",  // player_a_params - TODO: implement
            get_strategy_name(strategies, PLAYER_B),
            "{}",  // player_b_params - TODO: implement
            game_num,
            winner,
            gstate->turn,
            rounds,
            gstate->current_energy[PLAYER_A],
            gstate->current_energy[PLAYER_B],
            gstate->current_cash_balance[PLAYER_A],
            gstate->current_cash_balance[PLAYER_B],
            gstate->hand[PLAYER_A].size,
            gstate->hand[PLAYER_B].size,
            gstate->deck[PLAYER_A].top + 1,
            gstate->deck[PLAYER_B].top + 1,
            0,  // TODO: Track total_damage_dealt_a
            0,  // TODO: Track total_damage_dealt_b
            0,  // TODO: Track champions_played_a
            0,  // TODO: Track champions_played_b
            0,  // TODO: Track draw_cards_played_a
            0,  // TODO: Track draw_cards_played_b
            0,  // TODO: Track cash_cards_played_a
            0   // TODO: Track cash_cards_played_b
    );
    
    // Update statistics for summary
    exporter->total_turns += gstate->turn;
    if(gstate->turn < exporter->min_turns)
        exporter->min_turns = gstate->turn;
    if(gstate->turn > exporter->max_turns)
        exporter->max_turns = gstate->turn;
    exporter->sum_turns_squared += gstate->turn * gstate->turn;
    
    fflush(exporter->detail_file);
}

void export_summary(SimExporter* exporter, struct gamestats* gstats,
                    StrategySet* strategies)
{
    if(!exporter || !exporter->summary_file) return;
    
    // Calculate statistics
    float avg_turns = (float)exporter->total_turns / gstats->simnum;
    
    // Calculate standard deviation
    float variance = ((float)exporter->sum_turns_squared / gstats->simnum) -
                     (avg_turns * avg_turns);
    float std_dev = sqrt(variance);
    
    // Calculate win rates
    float win_rate_a = (float)gstats->cumul_player_wins[PLAYER_A] /
                       gstats->simnum;
    float win_rate_b = (float)gstats->cumul_player_wins[PLAYER_B] /
                       gstats->simnum;
    
    // Write summary row
    fprintf(exporter->summary_file,
            "%u,%s,%s,%s,%u,%u,%u,%s,%s,\"%s\",%s,\"%s\",%u,%u,%u,"
            "%.1f,%u,%u,%u,%.2f,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f\n",
            exporter->sim_id,
            "simparam_placeholder",
            "auto",
            "timestamp_placeholder",
            gstats->simnum,
            30,  // initial_cash
            exporter->rng_seed,
            "RANDOM",  // deck_type
            get_strategy_name(strategies, PLAYER_A),
            "{}",
            get_strategy_name(strategies, PLAYER_B),
            "{}",
            gstats->cumul_player_wins[PLAYER_A],
            gstats->cumul_player_wins[PLAYER_B],
            gstats->cumul_number_of_draws,
            avg_turns,
            exporter->min_turns,
            exporter->max_turns,
            0,  // median_turns - TODO: calculate
            std_dev,
            0.0,  // avg_damage_a - TODO
            0.0,  // avg_damage_b - TODO
            0.0,  // avg_champions_a - TODO
            0.0,  // avg_champions_b - TODO
            win_rate_a,
            win_rate_b
    );
    
    fflush(exporter->summary_file);
}

void close_sim_exporter(SimExporter* exporter)
{
    if(!exporter) return;
    
    if(exporter->detail_file)
        fclose(exporter->detail_file);
    if(exporter->summary_file)
        fclose(exporter->summary_file);
    
    free(exporter);
}

// Placeholder implementations - need to be completed based on StrategySet
const char* get_strategy_name(StrategySet* strategies, PlayerID player)
{
    // TODO: Implement based on actual StrategySet structure
    return "unknown";
}

void get_strategy_params_string(StrategySet* strategies, PlayerID player,
                                char* output, size_t max_len)
{
    // TODO: Implement based on actual StrategySet structure
    output[0] = '\0';
}
```

## Enhanced StrategySet Structure

To support the export system, enhance the strategy structure:

```c
// In strategy.h (additions)

typedef struct {
    AttackStrategyFunc attack_strategy[2];
    DefenseStrategyFunc defense_strategy[2];
    
    // Add strategy identification
    char strategy_name[2][8];  // "rand", "bal", "heur", etc.
    
    // Add parameter tracking (strategy-specific)
    union {
        struct { float epsilon; float gamma; } balanced_params;
        struct { uint16_t sims_per_move; } mc_params;
        struct { uint16_t iterations; } mcts_params;
    } params[2];
    
} StrategySet;

// Enhanced setter function
void set_player_strategy_named(StrategySet* strat, PlayerID player,
                               const char* name,
                               AttackStrategyFunc att_func,
                               DefenseStrategyFunc def_func);
```

## Integration with Existing Code

### Update game_state.c

```c
// In play_game() function
void play_game(uint16_t initial_cash, struct gamestats* gstats,
               StrategySet* strategies, SimExporter* exporter)
{
    struct gamestate gstate;
    setup_game(initial_cash, &gstate);
    
    // ... existing game loop ...
    
    // Export result
    if(exporter) {
        export_game_result(exporter, &gstate, gstats->simnum + 1, strategies);
    }
    
    record_final_stats(gstats, &gstate);
    
    // ... cleanup ...
}

// In run_simulation() function
void run_simulation(uint16_t numsim, uint16_t initial_cash,
                    struct gamestats* gstats, StrategySet* strategies,
                    DeckType deck_type, uint32_t rng_seed)
{
    // Initialize exporter
    SimExporter* exporter = init_auto_exporter(deck_type, numsim,
                                               initial_cash, rng_seed,
                                               strategies);
    
    for(gstats->simnum = 0; gstats->simnum < numsim; gstats->simnum++) {
        play_game(initial_cash, gstats, strategies, exporter);
    }
    
    // Export summary
    if(exporter) {
        export_summary(exporter, gstats, strategies);
        close_sim_exporter(exporter);
    }
}
```

### Update main.c

```c
// In main() function
int main() {
    // ... existing initialization ...
    
    DeckType deck_type = DECK_RANDOM;
    
    // Run simulation with export
    run_simulation(numsim, initial_cash, &gstats, strategies,
                   deck_type, M_TWISTER_SEED);
    
    // ... existing code ...
}
```

## Usage Examples

### Excel Analysis

After generating CSV files, users can:

1. **Import to Excel**
   - Data → From Text/CSV
   - Select the detail or summary file
   - Excel will auto-detect delimiter

2. **Create Pivot Tables**
   - Filter by `player_a_strategy` and `player_b_strategy`
   - Analyze win rates by configuration
   - Group by `deck_type` for comparison

3. **Statistical Analysis**
   - Use `AVERAGE()`, `STDEV()` functions on columns
   - Create histograms of `turns_played`
   - Compare distributions across strategies

### Python Analysis

```python
import pandas as pd
import matplotlib.pyplot as plt

# Load detail data
df = pd.read_csv('oracle_sim_auto_20251016_143022_rnd_rand_rand.csv')

# Basic statistics
print(df.groupby('winner')['turns_played'].describe())

# Win rate by player
win_counts = df['winner'].value_counts()
print(f"Player A wins: {win_counts['PLAYER_A']}")
print(f"Player B wins: {win_counts['PLAYER_B']}")

# Plot turn distribution
plt.hist(df['turns_played'], bins=30)
plt.xlabel('Turns Played')
plt.ylabel('Frequency')
plt.title('Game Length Distribution')
plt.show()
```

### R Analysis

```r
library(tidyverse)

# Load data
df <- read_csv('oracle_sim_auto_20251016_143022_rnd_rand_rand.csv')

# Summary statistics
df %>%
  group_by(winner) %>%
  summarise(
    count = n(),
    avg_turns = mean(turns_played),
    sd_turns = sd(turns_played)
  )

# Visualization
ggplot(df, aes(x = turns_played, fill = winner)) +
  geom_histogram(bins = 30, alpha = 0.7, position = "identity") +
  labs(title = "Game Length Distribution by Winner",
       x = "Turns Played",
       y = "Count") +
  theme_minimal()
```

## Recommendations Summary

1. ✅ **Use CSV as primary format** - Universal compatibility
2. ✅ **Keep one row per game** in detail file - Easy filtering and analysis
3. ✅ **Use separate summary file** for aggregate statistics - Quick overview
4. ✅ **Include strategy parameters** as JSON string - Flexibility for complex configs
5. ✅ **Add metadata columns** at start - Easy pivot table filtering
6. ⚠️ **Consider gzip compression** for large runs - Optional space savings
7. ✅ **For interactive mode, create new file per session** - Clean separation

## Future Enhancements

### Additional Metrics to Track

Consider adding these columns in future iterations:

- `avg_combo_bonus_a` / `avg_combo_bonus_b` - Average combo bonus per combat
- `max_combo_achieved_a` / `max_combo_achieved_b` - Highest combo in game
- `cards_drawn_a` / `cards_drawn_b` - Total cards drawn
- `deck_reshuffle_count_a` / `deck_reshuffle_count_b` - Times deck was reshuffled
- `mulligan_count_b` - Number of cards mulliganed by player B
- `max_hand_size_a` / `max_hand_size_b` - Largest hand size during game
- `total_cash_spent_a` / `total_cash_spent_b` - Total cash spent on cards
- `avg_attack_value` / `avg_defense_value` - Average combat values

### Extended Summary Statistics

- Confidence intervals for win rates
- Quartile information (Q1, Q3)
- Win rate by starting player position
- Average game length by game outcome (wins vs losses vs draws)

### Multi-Run Comparisons

For comparing multiple simulation runs:

```
oracle_sim_comparison_{date}.csv
```

Each row would represent one complete simulation run, allowing comparison of different strategy configurations side-by-side.

## File Size Estimates

For planning storage requirements:

- **Detail File**: ~150 bytes per game
  - 1,000 games ≈ 150 KB
  - 10,000 games ≈ 1.5 MB
  - 100,000 games ≈ 15 MB

- **Summary File**: ~200 bytes per simulation run
  - Negligible size for most use cases

- **Compression**: gzip typically achieves 60-80% reduction on CSV files

## Error Handling

```c
// Add error checking to export functions

bool validate_exporter(SimExporter* exporter)
{
    if(!exporter) {
        fprintf(stderr, "Error: NULL exporter\n");
        return false;
    }
    if(!exporter->detail_file) {
        fprintf(stderr, "Error: Detail file not open\n");
        return false;
    }
    return true;
}

void export_game_result_safe(SimExporter* exporter, struct gamestate* gstate,
                              uint16_t game_num, StrategySet* strategies)
{
    if(!validate_exporter(exporter)) return;
    if(!gstate || !strategies) {
        fprintf(stderr, "Error: NULL parameters in export\n");
        return;
    }
    
    export_game_result(exporter, gstate, game_num, strategies);
}
```

## Testing Checklist

- [ ] Verify CSV files open correctly in Excel
- [ ] Confirm all numeric columns are properly formatted (no quotes around numbers)
- [ ] Check that commas in JSON parameter strings are properly quoted
- [ ] Test with various strategy name lengths
- [ ] Verify simparam string never exceeds 20 characters
- [ ] Ensure timestamps are ISO 8601 compliant
- [ ] Validate summary calculations match detail data
- [ ] Test file creation in interactive mode
- [ ] Verify proper handling when disk is full
- [ ] Test with Unicode characters in strategy names (if applicable)

## Conclusion

This CSV export format provides a robust, flexible, and widely compatible solution for Oracle simulation data export. The readable simparam string makes file management intuitive, while the structured CSV format ensures easy analysis in any statistical software package.