#ifndef STDA_AUTO_H
#define STDA_AUTO_H

#include <string.h>
#include "game_types.h"
#include "strategy.h"


/* Run mode functions */
int run_mode_stda_auto(config_t* cfg);

// Simulation functions
void run_simulation(uint16_t numsim, uint16_t initial_cash,
                    struct gamestats* gstats, StrategySet* strategies);

void apply_mulligan(struct gamestate* gstate);
void play_stda_auto_game(uint16_t initial_cash, struct gamestats* gstats,
                         StrategySet* strategies);

// Stats recording and presentation
void record_final_stats(struct gamestats* gstats, struct gamestate* gstate);
void present_results(struct gamestats* gstats);

#endif
