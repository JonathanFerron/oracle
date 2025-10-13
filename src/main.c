// main.c
// Oracle: The Champions of Arcadia - Main entry point

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_types.h"
#include "game_constants.h"
#include "game_state.h"
#include "strategy.h"
#include "strat_random.h"
#include "mtwister.h"

#define M_TWISTER_SEED 1337

// Global variables
bool debug_enabled = false;
extern MTRand MTwister_rand_struct;

int main() {
    // Initialize random number generator
    MTwister_rand_struct = seedRand(M_TWISTER_SEED);
    
    // Initialize game statistics
    struct gamestats gstats;
    memset(&gstats, 0, sizeof(struct gamestats));
    
    // Simulation parameters
    uint16_t numsim = debug_enabled ? DEBUG_NUMBER_OF_SIM : MAX_NUMBER_OF_SIM;
    uint16_t initial_cash = 30;
    
    // Setup strategies for both players
    StrategySet* strategies = create_strategy_set();
    set_player_strategy(strategies, PLAYER_A, 
                       random_attack_strategy, random_defense_strategy);
    set_player_strategy(strategies, PLAYER_B, 
                       random_attack_strategy, random_defense_strategy);
    
    // Run simulation
    run_simulation(numsim, initial_cash, &gstats, strategies);
    
    // Present results
    present_results(&gstats);
    
    // Cleanup
    free_strategy_set(strategies);
    
    return EXIT_SUCCESS;
}
