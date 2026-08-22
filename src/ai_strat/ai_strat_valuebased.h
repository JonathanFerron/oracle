// ai_strat_valuebased.h
// A1 Value Based strategy ("The Apprentice") -- see
// ideas/A1 ai agent value based (the apprentice)/value_based_handout.md

#ifndef AI_STRAT_VALUEBASED_H
#define AI_STRAT_VALUEBASED_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void value_based_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void value_based_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_VALUEBASED_H
