// ai_strat_adept.h
// Adept gap-filler strategy ("Reduced Heuristic" / The Adept) -- see
// doc/ai_agents.md's gap-3 section

#ifndef AI_STRAT_ADEPT_H
#define AI_STRAT_ADEPT_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void adept_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void adept_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_ADEPT_H
