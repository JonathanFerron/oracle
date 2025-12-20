// strat_random.h
// Random strategy implementation

#ifndef STRAT_RANDOM_H
#define STRAT_RANDOM_H

#include "../core/game_types.h"
#include "../core/game_context.h"

// Random strategy functions
void random_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void random_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // STRAT_RANDOM_H
