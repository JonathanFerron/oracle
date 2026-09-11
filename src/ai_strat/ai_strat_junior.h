// ai_strat_junior.h
// Junior gap-filler strategy ("Naive Greedy" / Junior) -- see
// doc/ai_agents.md's Junior section

#ifndef AI_STRAT_JUNIOR_H
#define AI_STRAT_JUNIOR_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void junior_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void junior_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_JUNIOR_H
