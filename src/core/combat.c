// combat.c
// Combat resolution implementation
#include <stdio.h>

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

void clear_combat_zones(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;
  PlayerID defender = 1 - gstate->current_player;

  // Move attacker's cards to discard
  for (uint8_t i = 0; i < gstate->combat_zone[attacker].size; i++)
  { uint8_t card_idx = gstate->combat_zone[attacker].cards[i];
    Discard_add(&gstate->discard[attacker], card_idx);
   }
  CombatZone_clear(&gstate->combat_zone[attacker]);

  // Move defender's cards to discard
  for (uint8_t i = 0; i < gstate->combat_zone[defender].size; i++)
  { uint8_t card_idx = gstate->combat_zone[defender].cards[i];
    Discard_add(&gstate->discard[defender], card_idx);
   }
  CombatZone_clear(&gstate->combat_zone[defender]);
}
