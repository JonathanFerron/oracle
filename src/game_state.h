// game_state.h
// Game state initialization and management

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "game_types.h"
#include "strategy.h"

// Game initialization and management
void setup_game(uint16_t initial_cash, struct gamestate* gstate);
void apply_mulligan(struct gamestate* gstate);
void play_game(uint16_t initial_cash, struct gamestats* gstats,
               StrategySet* strategies);

// Simulation functions
void run_simulation(uint16_t numsim, uint16_t initial_cash,
                    struct gamestats* gstats, StrategySet* strategies);

// Stats recording and presentation
void record_final_stats(struct gamestats* gstats, struct gamestate* gstate);
void present_results(struct gamestats* gstats);

#endif // GAME_STATE_H