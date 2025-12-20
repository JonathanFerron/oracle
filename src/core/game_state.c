// game_state.c
// Game state initialization and management implementation
#include <stdio.h>
#include <string.h>

#include "game_state.h"
#include "game_constants.h"
#include "../util/rnd.h"
#include "../structures/deckstack.h"
#include "game_context.h"

void setup_game(uint16_t initial_cash, struct gamestate* gstate, GameContext* ctx)
{ // Initialize game state
  gstate->current_player = PLAYER_A;
  gstate->current_cash_balance[PLAYER_A] = initial_cash;
  gstate->current_cash_balance[PLAYER_B] = initial_cash;
  gstate->current_energy[PLAYER_A] = INITIAL_ENERGY_DEFAULT;
  gstate->current_energy[PLAYER_B] = INITIAL_ENERGY_DEFAULT;
  gstate->someone_has_zero_energy = false;
  gstate->game_state = ACTIVE;

  // Initialize decks
  gstate->deck[PLAYER_A].top = -1;
  gstate->deck[PLAYER_B].top = -1;

  // Randomly distribute cards
  uint8_t rndCardIndex[FULL_DECK_SIZE];
  for(uint8_t i = 0; i < FULL_DECK_SIZE; i++)
    rndCardIndex[i] = i;
  RND_partial_shuffle(rndCardIndex, FULL_DECK_SIZE, 2*MAX_DECK_STACK_SIZE, ctx);

  // Push cards to decks alternately
  uint8_t i = 0;
  while(i < 2*MAX_DECK_STACK_SIZE)
  { DeckStk_push(&gstate->deck[PLAYER_A], rndCardIndex[i++]);
    DeckStk_push(&gstate->deck[PLAYER_B], rndCardIndex[i++]);
  }

  // Initialize hands, combat zones, and discards
  Hand_init(&gstate->hand[PLAYER_A]);
  Hand_init(&gstate->hand[PLAYER_B]);
  Discard_init(&gstate->discard[PLAYER_A]);
  Discard_init(&gstate->discard[PLAYER_B]);
  CombatZone_init(&gstate->combat_zone[PLAYER_A]);
  CombatZone_init(&gstate->combat_zone[PLAYER_B]);

  // Draw initial hands (6 cards each)
  for(i = 0; i < INITAL_HAND_SIZE_DEFAULT; i++)
  { uint8_t cardindex = DeckStk_pop(&gstate->deck[PLAYER_A]);
    Hand_add(&gstate->hand[PLAYER_A], cardindex);
    cardindex = DeckStk_pop(&gstate->deck[PLAYER_B]);
    Hand_add(&gstate->hand[PLAYER_B], cardindex);
  }

} // setup_game

void collect_1_luna(struct gamestate* gstate)
{ gstate->current_cash_balance[gstate->current_player]++;
}

void change_current_player(struct gamestate* gstate)
{ gstate->current_player = 1 - gstate->current_player;
}

