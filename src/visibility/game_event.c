// game_event.c
// event_filter_for_viewer() and the cards_added()/cards_removed() diff
// helpers -- see game_event.h for the design rationale.

#include "game_event.h"

void event_filter_for_viewer(GameEvent* e, PlayerID viewer)
{ if(e->type == EVT_CARD_DRAWN && e->player != viewer)
    e->u.card = EVT_CARD_REDACTED;
} // event_filter_for_viewer

static bool contains(const uint8_t* arr, uint8_t n, uint8_t val)
{ for(uint8_t i = 0; i < n; i++)
    if(arr[i] == val) return true;
  return false;
} // contains

uint8_t cards_added(const uint8_t* before, uint8_t n_before,
                    const uint8_t* after, uint8_t n_after,
                    uint8_t* out, uint8_t max_out)
{ uint8_t n = 0;
  for(uint8_t i = 0; i < n_after && n < max_out; i++)
    if(!contains(before, n_before, after[i])) out[n++] = after[i];
  return n;
} // cards_added

uint8_t cards_removed(const uint8_t* before, uint8_t n_before,
                      const uint8_t* after, uint8_t n_after,
                      uint8_t* out, uint8_t max_out)
{ return cards_added(after, n_after, before, n_before, out, max_out);
} // cards_removed

void event_buf_push(EventBuf* buf, GameEvent e)
{ if(buf->count >= EVENT_BUF_CAP) return;
  buf->ev[buf->count++] = e;
} // event_buf_push

void event_buf_append(EventBuf* dst, const EventBuf* src)
{ for(uint16_t i = 0; i < src->count; i++)
    event_buf_push(dst, src->ev[i]);
} // event_buf_append
