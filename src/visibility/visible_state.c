// visible_state.c
// Builds a VisibleGameState (visible_state.h) from the authoritative
// struct gamestate, keeping only what one viewer is allowed to see.

#include "visible_state.h"
#include "../structures/card_collection.h"

// Fields that are public regardless of viewer: energy/cash/hand size/deck
// size/discard/combat zone for every player.
static void fill_public_fields(const struct gamestate* gs, VisibleGameState* out)
{ for(uint8_t p = 0; p < NUM_PLAYERS; p++)
  { out->energy[p] = gs->current_energy[p];
    out->cash[p] = gs->current_cash_balance[p];
    out->hand_count[p] = gs->hand[p].size;
    out->deck_count[p] = (uint8_t)(gs->deck[p].top + 1);
    out->discard[p] = gs->discard[p];
    out->combat_zone[p] = gs->combat_zone[p];
  }
} // fill_public_fields

// Turn/phase bookkeeping and outcome -- also public.
static void fill_flow_fields(const struct gamestate* gs, VisibleGameState* out)
{ out->turn = gs->turn;
  out->turn_phase = gs->turn_phase;
  out->current_player = gs->current_player;
  out->player_to_move = gs->player_to_move;
  out->combo_bonus_table = gs->combo_bonus_table;
  out->game_state = gs->game_state;
  out->game_over = (gs->game_state != ACTIVE);
} // fill_flow_fields

void visibility_filter(const struct gamestate* gs, PlayerID viewer,
                       VisibleGameState* out)
{ out->viewer = viewer;
  fill_public_fields(gs, out);
  fill_flow_fields(gs, out);

  if(viewer == VIEWER_SPECTATOR)
    Hand_init(&out->my_hand);
  else
    out->my_hand = gs->hand[viewer];
} // visibility_filter
