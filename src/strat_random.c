// strat_random.c
// Random strategy implementation

#include "strat_random.h"
#include "card_actions.h"
#include "game_constants.h"
#include "rnd.h"
#include "mtwister.h"
#include <stdlib.h>

extern MTRand MTwister_rand_struct;

void random_attack_strategy(struct gamestate* gstate)
{ PlayerID attacker = gstate->current_player;

  if(gstate->hand[attacker].size == 0) return;

  // Build list of affordable cards
  uint8_t affordable[gstate->hand[attacker].size];
  uint8_t count = 0;

  int has_champions = has_champion_in_hand(&gstate->hand[attacker]);
  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[attacker]);  // this allocates memory on the heap for array hand_array, make sure to free() it

  for(uint8_t i = 0; i < gstate->hand[attacker].size; i++)
  { uint8_t card_idx = hand_array[i];

    if(fullDeck[card_idx].cost <= gstate->current_cash_balance[attacker])
    { // Skip cash cards if no champions available
      if(fullDeck[card_idx].card_type == CASH_CARD && !has_champions)
        continue;
      affordable[count++] = card_idx;
    }
  }
  free(hand_array);

  if(count == 0) return;

  // Play random affordable card
  uint8_t chosen = RND_randn(count);
  play_card(gstate, attacker, affordable[chosen]);
}

void random_defense_strategy(struct gamestate* gstate)
{ PlayerID defender = 1 - gstate->current_player;

  if(gstate->hand[defender].size == 0) return;

  // Only defend 47% of the time
  if(genRand(&MTwister_rand_struct) > 0.47) return;

  // Build list of affordable champions
  uint8_t affordable[gstate->hand[defender].size];
  uint8_t count = 0;

  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[defender]); // this allocates memory on the heap for array hand_array, make sure to free() it

  for(uint8_t i = 0; i < gstate->hand[defender].size; i++)
  { uint8_t card_idx = hand_array[i];

    if(fullDeck[card_idx].card_type == CHAMPION_CARD &&
       fullDeck[card_idx].cost <= gstate->current_cash_balance[defender])
      affordable[count++] = card_idx;
  }
  free(hand_array);

  if(count == 0) return;

  // Play random champion
  uint8_t chosen = RND_randn(count);
  play_champion(gstate, defender, affordable[chosen]);
}
