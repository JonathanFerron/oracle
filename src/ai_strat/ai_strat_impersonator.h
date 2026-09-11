// ai_strat_impersonator.h
// Impersonator gap-filler strategy ("Uncalibrated Power" / The
// Impersonator) -- see doc/ai_agents.md's gap-2 section

#ifndef AI_STRAT_IMPERSONATOR_H
#define AI_STRAT_IMPERSONATOR_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void impersonator_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void impersonator_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_IMPERSONATOR_H
