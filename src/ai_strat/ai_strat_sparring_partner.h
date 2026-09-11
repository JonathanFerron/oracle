// ai_strat_sparring_partner.h
// Sparring Partner gap-filler strategy ("HBT Lite" / The Sparring Partner)
// -- see doc/ai_agents.md's gap-3 section

#ifndef AI_STRAT_SPARRING_PARTNER_H
#define AI_STRAT_SPARRING_PARTNER_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void sparring_partner_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void sparring_partner_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_SPARRING_PARTNER_H
