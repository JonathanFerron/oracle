// ai_strat_ismctsnn_state.c
// See ai_strat_ismctsnn_state.h for the full design rationale.

#include <string.h>

#include "ai_strat_ismctsnn_state.h"
#include "../core/game_constants.h"

_Static_assert(sizeof(ISMCTSNNStateVector) == ISMCTSNN_STATE_DIM * sizeof(float),
               "ISMCTSNNStateVector must be a flat array of ISMCTSNN_STATE_DIM floats");

uint8_t ismctsnn_catalog_index(const struct card* card)
{ if(card->card_type == CHAMPION_CARD) return card->champion_id - 1;
  if(card->card_type == DRAW_CARD) return (card->draw_num == 2) ? 102 : 103;
  return 104; // CASH_CARD
} // ismctsnn_catalog_index

static void accumulate_catalog(float* catalog, const uint8_t* card_indices, uint8_t count)
{ for(uint8_t i = 0; i < count; i++)
  { uint8_t type_index = ismctsnn_catalog_index(&fullDeck[card_indices[i]]);
    catalog[type_index] += 1.0f;
  }
} // accumulate_catalog

void ismctsnn_encode_state(const struct gamestate* gstate, PlayerID observer,
                           ISMCTSNNStateVector* out)
{ PlayerID opp = 1 - observer;
  memset(out, 0, sizeof(*out));

  accumulate_catalog(out->own_hand, gstate->hand[observer].cards, gstate->hand[observer].size);
  accumulate_catalog(out->own_discard, gstate->discard[observer].cards,
                     gstate->discard[observer].size);
  accumulate_catalog(out->own_combat_zone, gstate->combat_zone[observer].cards,
                     gstate->combat_zone[observer].size);
  accumulate_catalog(out->opp_discard, gstate->discard[opp].cards, gstate->discard[opp].size);
  accumulate_catalog(out->opp_combat_zone, gstate->combat_zone[opp].cards,
                     gstate->combat_zone[opp].size);

  out->energy_me = (float)gstate->current_energy[observer] / ISMCTSNN_ENERGY_NORM;
  out->energy_opp = (float)gstate->current_energy[opp] / ISMCTSNN_ENERGY_NORM;
  out->cash_me = (float)gstate->current_cash_balance[observer] / ISMCTSNN_CASH_NORM;
  out->cash_opp = (float)gstate->current_cash_balance[opp] / ISMCTSNN_CASH_NORM;
  out->turn = (float)gstate->turn / ISMCTSNN_TURN_NORM;
  out->phase = (gstate->turn_phase == DEFENSE) ? 1.0f : 0.0f;
  out->own_deck_remaining = (float)(gstate->deck[observer].top + 1);
  out->opp_deck_remaining = (float)(gstate->deck[opp].top + 1);
  out->opp_hand_size = (float)gstate->hand[opp].size;
  out->combo_bonus_random = (gstate->combo_bonus_table == COMBO_BONUS_RANDOM) ? 1.0f : 0.0f;
  out->combo_bonus_monochrome = (gstate->combo_bonus_table == COMBO_BONUS_MONOCHROME) ? 1.0f : 0.0f;
  out->combo_bonus_custom = (gstate->combo_bonus_table == COMBO_BONUS_CUSTOM) ? 1.0f : 0.0f;
} // ismctsnn_encode_state
