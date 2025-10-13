// strat_random.h
// Random strategy implementation

#ifndef STRAT_RANDOM_H
#define STRAT_RANDOM_H

#include "game_types.h"

// Random strategy functions
void random_attack_strategy(struct gamestate* gstate);
void random_defense_strategy(struct gamestate* gstate);

#endif // STRAT_RANDOM_H