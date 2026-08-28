// ai_strat_valuebased.h
// A1 Value Based strategy ("The Apprentice") -- see
// ideas/A1 ai agent value based (the apprentice)/value_based_handout.md

#ifndef AI_STRAT_VALUEBASED_H
#define AI_STRAT_VALUEBASED_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void value_based_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void value_based_defense_strategy(struct gamestate* gstate, GameContext* ctx);

// Calibration-only override hook (see aicalibsrc/value/) for this agent's two
// tunables (VB_COST_FLOOR, VB_DEFEND_THRESHOLD), settable per player so two
// different parameter sets can play each other in one process/game. Not part
// of the general strategy framework -- normal play always uses the compiled
// defaults, since nothing else calls these.
void value_based_set_params(PlayerID player, float cost_floor, float defend_threshold);
void value_based_reset_params(void);
void value_based_get_default_params(float* cost_floor, float* defend_threshold);

#endif // AI_STRAT_VALUEBASED_H
