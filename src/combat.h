// combat.h
// Combat resolution functions

#ifndef COMBAT_H
#define COMBAT_H

#include "game_types.h"

// Main combat resolution
void resolve_combat(struct gamestate* gstate);

// Combat calculation helpers
int16_t calculate_total_attack(struct gamestate* gstate, PlayerID player);
int16_t calculate_total_defense(struct gamestate* gstate, PlayerID player);
void apply_combat_damage(struct gamestate* gstate, int16_t total_attack,
                         int16_t total_defense);
void clear_combat_zones(struct gamestate* gstate);

#endif // COMBAT_H