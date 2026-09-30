// gui_discard.c -- see gui_discard.h.

#include <stdbool.h>

#include "gui_discard.h"
#include "../../core/game_constants.h" // fullDeck[]

uint8_t gui_discard_select(const Discard* d, uint8_t capacity, uint8_t out[GUI_DISCARD_MAX])
{ bool keep[GUI_DISCARD_MAX] = { false };
  uint8_t picked = 0;

  // Pass 0: champions, newest first. Pass 1: whatever else fits in the spare cells.
  for(int pass = 0; pass < 2; pass++)
  { for(int i = d->size - 1; i >= 0 && picked < capacity; i--)
    { bool champion = fullDeck[d->cards[i]].card_type == CHAMPION_CARD;
      if(!keep[i] && champion == (pass == 0))
      { keep[i] = true;
        picked++;
      }
    }
  }

  uint8_t n = 0;
  for(int i = d->size - 1; i >= 0; i--)
    if(keep[i])
      out[n++] = d->cards[i];
  return n;
} // gui_discard_select
