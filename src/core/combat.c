// combat.c
// Combat resolution implementation
#include <stdio.h>
#include <string.h>

#include "combat.h"
#include "combo_bonus.h"
#include "game_constants.h"
#include "../util/rnd.h"
#include "game_context.h"
#include "../util/debug.h"

void resolve_combat(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;
  PlayerID defender = 1 - gstate->current_player;

  // Calculate attack and defense
  int16_t total_attack = calculate_total_attack(gstate, attacker, ctx);
  int16_t total_defense = calculate_total_defense(gstate, defender, ctx);

  // Apply damage
  apply_combat_damage(gstate, total_attack, total_defense, ctx);

  // Clear combat zones
  clear_combat_zones(gstate, ctx);
}

int16_t calculate_total_attack(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ int16_t total = 0;

  // Build CombatCard array for combo calculation
  uint8_t num_cards = gstate->combat_zone[player].size;
  CombatCard combat_cards[3];

  for(uint8_t i = 0; i < num_cards; i++)
  { uint8_t card_idx = gstate->combat_zone[player].cards[i];

    // Add base attack + dice roll
    total += fullDeck[card_idx].attack_base +
             RND_dn(fullDeck[card_idx].defense_dice, ctx);

    // Store for combo calculation
    combat_cards[i].species = fullDeck[card_idx].species;
    combat_cards[i].color = fullDeck[card_idx].color;
    combat_cards[i].order = fullDeck[card_idx].order;

    DEBUG_PRINT(" Attack card %u: D%u+%u, cost %u\n",
                card_idx,
                fullDeck[card_idx].defense_dice,
                fullDeck[card_idx].attack_base,
                fullDeck[card_idx].cost);
  }

  // Add combo bonus (assuming DECK_RANDOM for now)

  int bonus = calculate_combo_bonus(combat_cards, num_cards, DECK_RANDOM);
  total += bonus;

  DEBUG_ONLY(if(bonus > 0) printf(" Combo bonus: +%d\n", bonus));

  DEBUG_PRINT(" Total attack: %d\n", total);

  return total;
}


int16_t calculate_total_defense(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ int16_t total = 0;

  // Build CombatCard array for combo calculation
  uint8_t num_cards = gstate->combat_zone[player].size;
  CombatCard combat_cards[3];

  for(uint8_t i = 0; i < num_cards; i++)
  { uint8_t card_idx = gstate->combat_zone[player].cards[i];

    // Add dice roll only (no base for defense)
    total += RND_dn(fullDeck[card_idx].defense_dice, ctx);

    // Store for combo calculation
    combat_cards[i].species = fullDeck[card_idx].species;
    combat_cards[i].color = fullDeck[card_idx].color;
    combat_cards[i].order = fullDeck[card_idx].order;

    DEBUG_PRINT(" Defense card %u: D%u, cost %u\n",
                card_idx,
                fullDeck[card_idx].defense_dice,
                fullDeck[card_idx].cost);
  }

  // Add combo bonus (assuming DECK_RANDOM for now)
  int bonus = calculate_combo_bonus(combat_cards, num_cards, DECK_RANDOM);
  total += bonus;

  DEBUG_ONLY(if(bonus > 0) printf(" Combo bonus: +%d\n", bonus));
  DEBUG_PRINT(" Total defense: %d\n", total);

  return total;
}


void apply_combat_damage(struct gamestate* gstate, int16_t total_attack,
                         int16_t total_defense, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  int16_t total_damage = oraclemax(total_attack - total_defense, 0);

  DEBUG_PRINT(" Defender energy before: %u\n", gstate->current_energy[defender]);

  gstate->current_energy[defender] -= oraclemin((uint8_t)total_damage,
                                                gstate->current_energy[defender]);

  DEBUG_PRINT(" Damage dealt: %d\n", total_damage);
  DEBUG_PRINT(" Defender energy after: %u\n", gstate->current_energy[defender]);

  // Check for game end
  if(gstate->current_energy[defender] == 0)
  { gstate->someone_has_zero_energy = true;
    gstate->game_state = (gstate->current_player == PLAYER_A) ?
                         PLAYER_A_WINS : PLAYER_B_WINS;
  }
}

// Display-only variant of resolve_combat() that captures per-champion rolls,
// combo bonuses, and the damage/energy transition. Mirrors the exact math and
// RNG consumption order of calculate_total_attack()/calculate_total_defense(),
// then delegates to the same apply_combat_damage()/clear_combat_zones() used
// by resolve_combat(). Only called from the interactive CLI path -- stda_auto
// always uses resolve_combat() so its RNG-dependent results are unaffected.
void resolve_combat_with_details(struct gamestate* gstate, CombatDetails* details, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;
  PlayerID defender = 1 - gstate->current_player;

  memset(details, 0, sizeof(CombatDetails));

  details->num_attackers = gstate->combat_zone[attacker].size;
  CombatCard attack_cards[3];
  int16_t attack_total = 0;

  for(int i = 0; i < details->num_attackers; i++)
  { uint8_t card_idx = gstate->combat_zone[attacker].cards[i];
    const struct card* c = &fullDeck[card_idx];
    uint8_t roll = RND_dn(c->defense_dice, ctx);

    details->attacker_species[i] = c->species;
    details->attacker_dice[i] = c->defense_dice;
    details->attacker_rolls[i] = roll;
    details->attacker_base[i] = c->attack_base;
    details->attacker_total[i] = c->attack_base + roll;
    attack_total += details->attacker_total[i];

    attack_cards[i] = (CombatCard)
    { c->species, c->color, c->order
    };
  }

  details->attack_combo = calculate_combo_bonus(attack_cards, details->num_attackers, DECK_RANDOM);
  details->total_attack = attack_total + details->attack_combo;

  details->num_defenders = gstate->combat_zone[defender].size;
  CombatCard defense_cards[3];
  int16_t defense_total = 0;

  for(int i = 0; i < details->num_defenders; i++)
  { uint8_t card_idx = gstate->combat_zone[defender].cards[i];
    const struct card* c = &fullDeck[card_idx];
    uint8_t roll = RND_dn(c->defense_dice, ctx);

    details->defender_species[i] = c->species;
    details->defender_dice[i] = c->defense_dice;
    details->defender_rolls[i] = roll;
    details->defender_total[i] = roll;
    defense_total += roll;

    defense_cards[i] = (CombatCard)
    { c->species, c->color, c->order
    };
  }

  details->defense_combo = calculate_combo_bonus(defense_cards, details->num_defenders, DECK_RANDOM);
  details->total_defense = defense_total + details->defense_combo;

  details->defender_energy_before = gstate->current_energy[defender];
  apply_combat_damage(gstate, details->total_attack, details->total_defense, ctx);
  details->defender_energy_after = gstate->current_energy[defender];
  details->damage = details->defender_energy_before - details->defender_energy_after;

  clear_combat_zones(gstate, ctx);
}

void clear_combat_zones(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;
  PlayerID defender = 1 - gstate->current_player;

  // Move attacker's cards to discard
  for(uint8_t i = 0; i < gstate->combat_zone[attacker].size; i++)
  { uint8_t card_idx = gstate->combat_zone[attacker].cards[i];
    Discard_add(&gstate->discard[attacker], card_idx);
  }
  CombatZone_clear(&gstate->combat_zone[attacker]);

  // Move defender's cards to discard
  for(uint8_t i = 0; i < gstate->combat_zone[defender].size; i++)
  { uint8_t card_idx = gstate->combat_zone[defender].cards[i];
    Discard_add(&gstate->discard[defender], card_idx);
  }
  CombatZone_clear(&gstate->combat_zone[defender]);
}
