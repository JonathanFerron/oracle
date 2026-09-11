// ai_strat_a15_attack.h
// A15 Risk Threshold's non-endgame attack-phase rule chain, first match
// wins: R6 (immediate species-3 combo) -> R1 (Player A's turn 1) -> R9a
// (color/order combo exposed to the forced discard) -> R9c (draw/recall) ->
// R9b (pressure attack) -> R9d (pass). See ai_strat_a15.h for the full rule
// statements and priority table; R8's endgame mode is checked before this
// in ai_strat_a15.c and, when active, bypasses this chain entirely.

#ifndef AI_STRAT_A15_ATTACK_H
#define AI_STRAT_A15_ATTACK_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void a15_play_normal_attack(struct gamestate* gstate, PlayerID player, GameContext* ctx);

#endif // AI_STRAT_A15_ATTACK_H
