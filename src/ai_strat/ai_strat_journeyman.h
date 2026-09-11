// ai_strat_journeyman.h
// Journeyman gap-filler strategy ("Partial Synthesis" / The Journeyman) --
// see doc/ai_agents.md's gap-2 section

#ifndef AI_STRAT_JOURNEYMAN_H
#define AI_STRAT_JOURNEYMAN_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void journeyman_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void journeyman_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_JOURNEYMAN_H
