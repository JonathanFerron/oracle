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

void gui_reveal_push(GuiReveal* rv, const EventBuf* events)
{ for(uint16_t i = 0; i < events->count; i++)
  { if(rv->count >= EVENT_BUF_CAP)
      return;
    rv->queue[(rv->head + rv->count) % EVENT_BUF_CAP] = events->ev[i];
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
  rv->started = true;
  rv->stage = REVEAL_ATTACK;
  rv->stage_since_ms = now_ms;
} // start_head_combat

bool gui_reveal_pop(GuiReveal* rv, int64_t now_ms, GameEvent* out)
{ if(rv->count == 0)
    return false;
  if(rv->queue[rv->head].type != EVT_COMBAT_RESOLVED)
  { release_head(rv, out);
    return true;
  }

  if(!rv->started)
  { // The previous combat is still on show: give it its own beat first.
    if(rv->stage == REVEAL_FULL && now_ms - rv->stage_since_ms < rv->delay_ms)
      return false;
    start_head_combat(rv, now_ms);
  }
  if(rv->stage == REVEAL_ATTACK)
  { if(now_ms - rv->stage_since_ms < rv->delay_ms)
      return false;
    rv->stage = REVEAL_FULL;
    rv->stage_since_ms = now_ms;
  }
  rv->started = false;
  release_head(rv, out);
  return true;
} // gui_reveal_pop

void gui_reveal_skip(GuiReveal* rv, int64_t now_ms)
{ if(rv->started || rv->stage == REVEAL_FULL)
    rv->stage_since_ms = now_ms - rv->delay_ms;
} // gui_reveal_skip

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
