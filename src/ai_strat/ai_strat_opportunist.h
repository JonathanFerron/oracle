// ai_strat_opportunist.h
// Opportunist gap-filler strategy ("Tactical Plus" / The Opportunist) --
// see doc/ai_agents.md's gap-3 section

#ifndef AI_STRAT_OPPORTUNIST_H
#define AI_STRAT_OPPORTUNIST_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void opportunist_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void opportunist_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_OPPORTUNIST_H
