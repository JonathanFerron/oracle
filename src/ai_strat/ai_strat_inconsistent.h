// ai_strat_inconsistent.h
// Inconsistent gap-filler strategy ("Weighted Mixture" / The Inconsistent)
// -- see doc/ai_agents.md's gap-2 section

#ifndef AI_STRAT_INCONSISTENT_H
#define AI_STRAT_INCONSISTENT_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void inconsistent_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void inconsistent_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_INCONSISTENT_H
