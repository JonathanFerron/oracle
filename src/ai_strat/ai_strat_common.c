// ai_strat_common.c
// Turn-strategy helpers shared between AI agents -- see ai_strat_common.h.

#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"
#include "../core/combo_bonus.h"

uint8_t build_affordable_champions(const struct gamestate* gstate, PlayerID player,
                                   uint16_t budget, uint8_t* out)
{ const Hand* hand = &gstate->hand[player];
  uint8_t count = 0;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type == CHAMPION_CARD &&
       fullDeck[card_idx].cost <= budget)
      out[count++] = card_idx;
  }

  return count;
} // build_affordable_champions

// Builds the CombatCard array calculate_combo_bonus() needs from a CombatZone.
static int fill_combat_cards(const CombatZone* zone, CombatCard* out)
{ for(uint8_t i = 0; i < zone->size; i++)
  { const struct card* c = &fullDeck[zone->cards[i]];
    out[i] = (CombatCard)
    { .species = c->species, .color = c->color, .order = c->order
    };
  }
  return zone->size;
} // fill_combat_cards

float expected_incoming_attack(const struct gamestate* gstate, PlayerID defender)
{ PlayerID attacker = 1 - defender;
  const CombatZone* zone = &gstate->combat_zone[attacker];

  float total = 0.0f;
  for(uint8_t i = 0; i < zone->size; i++)
    total += fullDeck[zone->cards[i]].expected_attack;

  CombatCard combat_cards[3];
  int num_cards = fill_combat_cards(zone, combat_cards);
  total += calculate_combo_bonus(combat_cards, num_cards, DECK_RANDOM);

  return total;
} // expected_incoming_attack

// Cheapest affordable DRAW_CARD in hand, or UINT8_MAX if none.
static uint8_t cheapest_affordable_draw_card(const struct gamestate* gstate,
                                             PlayerID player)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t cheapest = UINT8_MAX;
  uint8_t cheapest_cost = UINT8_MAX;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type == DRAW_CARD &&
       fullDeck[card_idx].cost <= budget &&
       fullDeck[card_idx].cost < cheapest_cost)
    { cheapest = card_idx;
      cheapest_cost = fullDeck[card_idx].cost;
    }
  }

  return cheapest;
} // cheapest_affordable_draw_card

bool try_play_draw_card(struct gamestate* gstate, PlayerID player,
                        uint8_t min_hand_size, uint8_t opp_energy_floor,
                        GameContext* ctx)
{ if(gstate->hand[player].size >= min_hand_size) return false;
  if(gstate->current_energy[1 - player] <= opp_energy_floor) return false;

  uint8_t card_idx = cheapest_affordable_draw_card(gstate, player);
  if(card_idx == UINT8_MAX) return false;

  play_draw_card(gstate, player, card_idx, ctx);
  return true;
} // try_play_draw_card
