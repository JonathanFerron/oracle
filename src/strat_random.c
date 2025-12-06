// strat_random.c
// Random strategy implementation
#include <stdlib.h>

#include "strat_random.h"
#include "card_actions.h"
#include "game_constants.h"
#include "rnd.h"
#include "mtwister.h"

void random_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;

  if(gstate->hand[attacker].size == 0) return;

  // Build list of affordable cards
  uint8_t affordable[gstate->hand[attacker].size];
  uint8_t count = 0;

  bool has_champions = has_champion_in_hand(&gstate->hand[attacker]);

  for(uint8_t i = 0; i < gstate->hand[attacker].size; i++)
  { uint8_t card_idx = gstate->hand[attacker].cards[i];

    if(fullDeck[card_idx].cost <= gstate->current_cash_balance[attacker])
    { // Skip cash cards if no champions available (in the hand)
      if(fullDeck[card_idx].card_type == CASH_CARD && !has_champions)
        continue;
      affordable[count++] = card_idx;
    }
  }

  if(count == 0) return;

  // Play random affordable card
  uint8_t chosen = RND_randn(count, ctx);
  play_card(gstate, attacker, affordable[chosen], ctx);
} // random_attack_strategy

void random_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  if(gstate->hand[defender].size == 0) return;

  // Only defend 47% of the time: this is a parameter of the strategy that could be set more dynamically and tested, the goal being to make the strategy as strong as possible
  if(genRand(&ctx->rng) > 0.47) return; // TODO: move the 0.47 magic number to a better place and build an optimization framework to tune this parameter

  // Build list of affordable champions
  uint8_t affordable[gstate->hand[defender].size];
  uint8_t count = 0;

  for(uint8_t i = 0; i < gstate->hand[defender].size; i++)
  { uint8_t card_idx = gstate->hand[defender].cards[i];

    if(fullDeck[card_idx].card_type == CHAMPION_CARD &&
       fullDeck[card_idx].cost <= gstate->current_cash_balance[defender])
      affordable[count++] = card_idx;
  }

  if(count == 0) return;

  // Play random champion
  uint8_t chosen = RND_randn(count, ctx);
  play_champion(gstate, defender, affordable[chosen], ctx);
} // random_defense_strategy
