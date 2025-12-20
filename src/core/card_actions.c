// card_actions.c
// Implementation of card playing and game actions
#include <stdio.h>
#include <stdlib.h>

#include "card_actions.h"
#include "game_constants.h"
#include "../util/rnd.h"
#include "../util/debug.h"

int has_champion_in_hand(Hand* hand)
{ for(uint8_t i = 0; i < hand->size; i++)
  { if(fullDeck[hand->cards[i]].card_type == CHAMPION_CARD)
      return true;
  }
  return false;
}

// TODO: this code should be moved to the strategy, similar to how the mulligan function should be moved there as well: this implementation is based on the power heuristic
uint8_t select_champion_for_cash_exchange(Hand* hand)
{ float min_power = 100.0;
  uint8_t champion_to_exchange = 0;

  for(uint8_t i = 0; i < hand->size; i++)
  { if(fullDeck[hand->cards[i]].card_type == CHAMPION_CARD)
    { if(fullDeck[hand->cards[i]].power < min_power)
      { min_power = fullDeck[hand->cards[i]].power;
        champion_to_exchange = hand->cards[i];
      }
    }
  }

  return champion_to_exchange;
}


void play_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx)
{ CardType type = fullDeck[card_idx].card_type;

  if(type == CHAMPION_CARD)
    play_champion(gstate, player, card_idx, ctx);
  else if(type == DRAW_CARD)
    play_draw_card(gstate, player, card_idx, ctx);
  else if(type == CASH_CARD)
    play_cash_card(gstate, player, card_idx, ctx);
}

void play_champion(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx)
{ // Add to combat zone
  CombatZone_add(&gstate->combat_zone[player], card_idx);

  // Remove from hand
  Hand_remove(&gstate->hand[player], card_idx);

  // Pay cost
  gstate->current_cash_balance[player] -= fullDeck[card_idx].cost;

  DEBUG_PRINT(" Played champion card index %u\n", card_idx);
}

void play_draw_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx)
{ // Remove from hand
  Hand_remove(&gstate->hand[player], card_idx);

  // Pay cost
  gstate->current_cash_balance[player] -= fullDeck[card_idx].cost;

  // Draw cards
  uint8_t n = fullDeck[card_idx].draw_num;
  DEBUG_PRINT(" Playing draw card %u, drawing %u cards\n", card_idx, n);

  for(uint8_t i = 0; i < n; i++)
    draw_1_card(gstate, player, ctx);

  // Move the draw card to discard
  Discard_add(&gstate->discard[player], card_idx);
}


void play_cash_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx)
{ // Remove cash card from hand
  Hand_remove(&gstate->hand[player], card_idx);

  // Pay cost (0 for cash cards)
  gstate->current_cash_balance[player] -= fullDeck[card_idx].cost;  // we could consider removing this line of code as long as we can always assume that the cost will be 0

  // Select champion to exchange
  uint8_t champion_to_exchange = select_champion_for_cash_exchange(&gstate->hand[player]);

  if(champion_to_exchange != 0)
  { // Remove champion from hand and place in discard
    Hand_remove(&gstate->hand[player], champion_to_exchange);
    Discard_add(&gstate->discard[player], champion_to_exchange);

    // Collect cash
    uint8_t cash_received = fullDeck[card_idx].exchange_cash;
    gstate->current_cash_balance[player] += cash_received;


    DEBUG_PRINT(" Exchanged champion card %u for %u lunas\n", champion_to_exchange, cash_received);

  }

  // Move cash card to discard
  Discard_add(&gstate->discard[player], card_idx);
}

void draw_1_card(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ if(DeckStk_isEmpty(&gstate->deck[player]))
  { shuffle_discard_and_form_deck(&gstate->discard[player], &gstate->deck[player], ctx);
    DEBUG_PRINT(" Reshuffled deck for player %u\n", player);
  }

  uint8_t cardindex = DeckStk_pop(&gstate->deck[player]);
  Hand_add(&gstate->hand[player], cardindex);

  DEBUG_PRINT(" Drew card index %u from player %u deck\n", cardindex, player);
}

void shuffle_discard_and_form_deck(Discard* discard, struct deck_stack* deck, GameContext* ctx)
{ uint8_t n = discard->size;

  DEBUG_PRINT(" Discard size: %u\n", n);

  // Shuffle the card indices directly in the discard array
  RND_partial_shuffle(discard->cards, n, n, ctx);

  // Push to deck
  for(uint8_t i = 0; i < n; i++)
    DeckStk_push(deck, discard->cards[i]);

  // Empty the discard
  Discard_clear(discard);
}

// TODO: this function should be moved to the strategy code as it is an AI agent method that could vary from one AI implementation to another. This implementation uses power heuristics that would
// likely be good enough for the random, balanced, value based, combo aware (Borealis benchmark agent), greedy power and heuristic AI agent, and could be improved upon for the 
// more advanced AI agents (Monte Carlo based ones and perhaps also the 2 ply HBT one)
void discard_to_7_cards(struct gamestate* gstate, GameContext* ctx)
{ if(gstate->hand[gstate->current_player].size <= 7) return;

  float minpower;
  uint8_t card_with_lowest_power;

  while(gstate->hand[gstate->current_player].size > 7)
  { // Find card with lowest power
    minpower = 100.0;
    card_with_lowest_power = 0;

    for(uint8_t i = 0; i < gstate->hand[gstate->current_player].size; i++)
    { uint8_t card_idx = gstate->hand[gstate->current_player].cards[i];
      if(fullDeck[card_idx].power < minpower)
      { minpower = fullDeck[card_idx].power;
        card_with_lowest_power = card_idx;
      }
    }

    // Discard it
    Hand_remove(&gstate->hand[gstate->current_player],
                            card_with_lowest_power);
    Discard_add(&gstate->discard[gstate->current_player],
                                card_with_lowest_power);
  }
}

