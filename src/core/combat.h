// combat.h
// Combat resolution functions

#ifndef COMBAT_H
#define COMBAT_H

#include "game_types.h"
#include "game_context.h"

// Main combat resolution
void resolve_combat(struct gamestate* gstate, GameContext* ctx);

// Combat calculation helpers
int16_t calculate_total_attack(struct gamestate* gstate, PlayerID player, GameContext* ctx);
int16_t calculate_total_defense(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void apply_combat_damage(struct gamestate* gstate, int16_t total_attack,
                         int16_t total_defense, GameContext* ctx);
void clear_combat_zones(struct gamestate* gstate, GameContext* ctx);

// Per-champion combat detail, for display purposes only (e.g. interactive CLI).
// Mirrors the exact math/RNG order of resolve_combat() -- not used by stda_auto.
typedef struct
{ int num_attackers;
  ChampionSpecies attacker_species[3];
  uint8_t attacker_dice[3];
  uint8_t attacker_rolls[3];
  uint8_t attacker_base[3];
  int16_t attacker_total[3];
  int16_t attack_combo;
  int16_t total_attack;

  int num_defenders;
  ChampionSpecies defender_species[3];
  uint8_t defender_dice[3];
  uint8_t defender_rolls[3];
  int16_t defender_total[3];
  int16_t defense_combo;
  int16_t total_defense;

  int16_t damage;
  uint8_t defender_energy_before;
  uint8_t defender_energy_after;
} CombatDetails;

void resolve_combat_with_details(struct gamestate* gstate, CombatDetails* details, GameContext* ctx);

#endif // COMBAT_H
