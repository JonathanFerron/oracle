// ai_strat_experimenter.h
// Experimenter gap-filler strategy ("Weighted Mixture II" / The
// Experimenter) -- see doc/ai_agents.md's gap-3 section

#ifndef AI_STRAT_EXPERIMENTER_H
#define AI_STRAT_EXPERIMENTER_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void experimenter_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void experimenter_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_EXPERIMENTER_H
