// ai_strat_lib_heuristics.c
// Shared power-based AI heuristics for mulligan/discard-to-7 -- see
// ai_strat_lib_heuristics.h. Bodies ported verbatim from the pre-refactor
// discard_to_7_cards() (card_actions.c) and apply_mulligan() (stda_auto.c),
// parameterised by player instead of hardcoding gstate->current_player /
// PLAYER_B so any StrategySet slot can dispatch here.

#include "ai_strat_lib_heuristics.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

#define MULLIGAN_DEFAULT_MAX_CARDS 2

static uint8_t g_mulligan_max_cards = MULLIGAN_DEFAULT_MAX_CARDS;

uint8_t mulligan_get_max_cards(void)
{ return g_mulligan_max_cards;
} // mulligan_get_max_cards

void mulligan_set_max_cards(uint8_t max_cards)
{ g_mulligan_max_cards = oraclemin(max_cards, MULLIGAN_HARD_CAP);
} // mulligan_set_max_cards

void mulligan_reset_max_cards(void)
{ g_mulligan_max_cards = MULLIGAN_DEFAULT_MAX_CARDS;
} // mulligan_reset_max_cards

void strat_lib_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ float minpower;
  uint8_t card_with_lowest_power;

  while(gstate->hand[player].size > 7)
  { minpower = 100.0;
    card_with_lowest_power = 0;

    for(uint8_t i = 0; i < gstate->hand[player].size; i++)
    { uint8_t card_idx = gstate->hand[player].cards[i];
      if(fullDeck[card_idx].power < minpower)
      { minpower = fullDeck[card_idx].power;
        card_with_lowest_power = card_idx;
      }
    }

    Hand_remove(&gstate->hand[player], card_with_lowest_power);
    Discard_add(&gstate->discard[player], card_with_lowest_power);
  }
} // strat_lib_discard_to_7

void strat_lib_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ uint8_t max_nbr_cards_to_mulligan = mulligan_get_max_cards();
  uint8_t nbr_cards_to_mulligan = 0;

  for(uint8_t i = 0; (i < gstate->hand[player].size) &&
      (nbr_cards_to_mulligan < max_nbr_cards_to_mulligan); i++)
  { uint8_t card_idx = gstate->hand[player].cards[i];
    if(fullDeck[card_idx].power < AVERAGE_POWER_FOR_MULLIGAN)
      nbr_cards_to_mulligan++;
  }

  float minpower;
  uint8_t card_with_lowest_power;
  uint8_t nbr_cards_left_to_mulligan = nbr_cards_to_mulligan;

  while(nbr_cards_left_to_mulligan > 0)
  { minpower = 100.0;
    card_with_lowest_power = 0;

    for(uint8_t i = 0; i < gstate->hand[player].size; i++)
    { uint8_t card_idx = gstate->hand[player].cards[i];
      if(fullDeck[card_idx].power < minpower)
      { minpower = fullDeck[card_idx].power;
        card_with_lowest_power = card_idx;
      }
    }

    Hand_remove(&gstate->hand[player], card_with_lowest_power);
    Discard_add(&gstate->discard[player], card_with_lowest_power);
    nbr_cards_left_to_mulligan--;
  }

  for(uint8_t i = 0; i < nbr_cards_to_mulligan; i++)
    draw_1_card(gstate, player, ctx);
} // strat_lib_mulligan
