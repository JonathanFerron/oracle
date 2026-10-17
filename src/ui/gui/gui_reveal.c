// gui_reveal.c -- see gui_reveal.h.

#include <string.h>

#include "gui_reveal.h"

void gui_reveal_init(GuiReveal* rv, int delay_seconds)
{ if(delay_seconds < 0)
    delay_seconds = 0;
  if(delay_seconds > GUI_REVEAL_DELAY_MAX_S)
    delay_seconds = GUI_REVEAL_DELAY_MAX_S;
  memset(rv, 0, sizeof(*rv));
  rv->delay_ms = (int64_t)delay_seconds * 1000;
  rv->stage = REVEAL_NONE;
} // gui_reveal_init

void gui_reveal_push(GuiReveal* rv, const EventBuf* events, const VisibleGameState* views,
                     uint8_t view_count)
{ uint8_t combats = 0;
  for(uint16_t i = 0; i < events->count; i++)
  { if(rv->count >= EVENT_BUF_CAP)
      return;
    uint16_t slot = (uint16_t)((rv->head + rv->count) % EVENT_BUF_CAP);
    rv->queue[slot] = events->ev[i];
    rv->queue_has_view[slot] = false;
    if(events->ev[i].type == EVT_COMBAT_RESOLVED && combats++ < view_count)
    { rv->queue_view[slot] = views[combats - 1];
      rv->queue_has_view[slot] = true;
    }
    rv->count++;
  }
} // gui_reveal_push

static void release_head(GuiReveal* rv, GameEvent* out)
{ *out = rv->queue[rv->head];
  rv->head = (uint16_t)((rv->head + 1) % EVENT_BUF_CAP);
  rv->count--;
} // release_head

static void start_head_combat(GuiReveal* rv, int64_t now_ms)
{ const GameEvent* e = &rv->queue[rv->head];
  rv->shown = e->u.combat;
  rv->shown_attacker = e->player;
  rv->shown_has_view = rv->queue_has_view[rv->head];
  if(rv->shown_has_view)
    rv->shown_view = rv->queue_view[rv->head];
  rv->started = true;
  rv->stage = REVEAL_ATTACK;
  rv->stage_since_ms = now_ms;
} // start_head_combat

// Releases the oldest held-back draw.
static void release_held(GuiReveal* rv, GameEvent* out)
{ *out = rv->held[0];
  rv->held_count--;
  memmove(rv->held, rv->held + 1, rv->held_count * sizeof(GameEvent));
} // release_held

// A draw arriving while a combat awaits its ack goes into `held` instead of
// the log. Returns false (release it now) if `held` is full.
static bool hold_head_draw(GuiReveal* rv)
{ if(!rv->awaiting_ack || rv->queue[rv->head].type != EVT_CARD_DRAWN
     || rv->held_count >= GUI_REVEAL_HELD_CAP)
    return false;
  release_head(rv, &rv->held[rv->held_count++]);
  return true;
} // hold_head_draw

bool gui_reveal_pop(GuiReveal* rv, int64_t now_ms, GameEvent* out)
{ if(rv->held_count > 0 && !rv->awaiting_ack)
  { release_held(rv, out);
    return true;
  }
  while(rv->count > 0 && hold_head_draw(rv))
    ;
  if(rv->count == 0)
    return false;
  if(rv->queue[rv->head].type != EVT_COMBAT_RESOLVED)
  { release_head(rv, out);
    return true;
  }

  if(!rv->started)
  { // The previous combat is still on show: give it its own beat first.
    if(rv->stage == REVEAL_FULL && now_ms - rv->stage_since_ms < rv->delay_ms)
      return false;
    if(rv->held_count > 0) // the previous combat is done with: its draws go first
    { release_held(rv, out);
      return true;
    }
    start_head_combat(rv, now_ms);
  }
  if(rv->stage == REVEAL_ATTACK)
  { if(now_ms - rv->stage_since_ms < rv->delay_ms)
      return false;
    rv->stage = REVEAL_FULL;
    rv->stage_since_ms = now_ms;
    PlayerID defender = (PlayerID)((rv->shown_attacker + 1) % NUM_PLAYERS);
    rv->shown_view.energy[defender] = rv->shown.defender_energy_after;
  }
  rv->started = false;
  rv->awaiting_ack = true;
  release_head(rv, out);
  return true;
} // gui_reveal_pop

void gui_reveal_skip(GuiReveal* rv, int64_t now_ms)
{ if(rv->started || rv->stage == REVEAL_FULL)
    rv->stage_since_ms = now_ms - rv->delay_ms;
} // gui_reveal_skip

bool gui_reveal_needs_ack(const GuiReveal* rv)
{ return rv->awaiting_ack && rv->count == 0;
} // gui_reveal_needs_ack

void gui_reveal_ack(GuiReveal* rv)
{ rv->awaiting_ack = false;
} // gui_reveal_ack

bool gui_reveal_game_over_pending(const GuiReveal* rv)
{ for(uint16_t i = 0; i < rv->count; i++)
    if(rv->queue[(rv->head + i) % EVENT_BUF_CAP].type == EVT_GAME_OVER)
      return true;
  return false;
} // gui_reveal_game_over_pending

bool gui_reveal_busy(const GuiReveal* rv)
{ return rv->count > 0;
} // gui_reveal_busy

bool gui_reveal_current(const GuiReveal* rv, const CombatDetails** details,
                        PlayerID* attacker, RevealStage* stage)
{ if(rv->stage == REVEAL_NONE)
    return false;
  *details = &rv->shown;
  *attacker = rv->shown_attacker;
  *stage = rv->stage;
  return true;
} // gui_reveal_current

bool gui_reveal_energy_override(const GuiReveal* rv, PlayerID p, uint8_t* energy)
{ for(uint16_t i = 0; i < rv->count; i++)
  { const GameEvent* e = &rv->queue[(rv->head + i) % EVENT_BUF_CAP];
    if(e->type == EVT_COMBAT_RESOLVED && (PlayerID)((e->player + 1) % NUM_PLAYERS) == p)
    { *energy = e->u.combat.defender_energy_before;
      return true;
    }
  }
  return false;
} // gui_reveal_energy_override

bool gui_reveal_view(const GuiReveal* rv, const VisibleGameState** view)
{ if(rv->stage == REVEAL_NONE || !rv->shown_has_view)
    return false;
  *view = &rv->shown_view;
  return true;
} // gui_reveal_view
