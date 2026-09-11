// ai_strat_auditor.h
// Auditor gap-filler strategy ("Corrected Ledger" / The Auditor) -- see
// doc/ai_agents.md's gap-2 section (the A4 Balanced Rules -> A15/Borealis
// gap; Junior filled the earlier Random -> A1 gap)

#ifndef AI_STRAT_AUDITOR_H
#define AI_STRAT_AUDITOR_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void auditor_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void auditor_defense_strategy(struct gamestate* gstate, GameContext* ctx);

#endif // AI_STRAT_AUDITOR_H
