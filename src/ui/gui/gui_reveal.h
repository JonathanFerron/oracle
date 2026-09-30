// gui_reveal.h
// Combat-reveal pacing for the GUI (SDL-free -- libc + game_event.h only, so
// it is unit-tested with a fake clock: testsrc/test_gui_reveal.c).
//
// The engine resolves combat instantly and events reach the GUI batched per
// human-facing decision, so one SessionUpdate can hold several events with
// one or two EVT_COMBAT_RESOLVED among them. Pure presentation problem: the
// GUI feeds every event through this queue instead of logging them at once,
// and the queue releases them in order -- non-combat events immediately, a
// combat only after its two-step reveal:
//   1. REVEAL_ATTACK: attacker's dice + attack shown, defender's still hidden;
//   2. after `delay_ms` (or a skip): REVEAL_FULL, defender's dice, defense and
//      damage shown, and only now is the combat's log line released.
// The last combat's panel stays on show (REVEAL_FULL) until the player
// acknowledges it or the next combat starts; a following combat first waits `delay_ms` so back-to-back combats
// (own attack, then the AI's after my defense) don't blur together.
// Time is passed in (ms, any monotonic origin); nothing here reads a clock.

#ifndef GUI_REVEAL_H
#define GUI_REVEAL_H

#include <stdbool.h>
#include <stdint.h>
#include "../../visibility/game_event.h"

#define GUI_REVEAL_DELAY_MAX_S 10

typedef enum
{ REVEAL_NONE,   // no combat shown yet
  REVEAL_ATTACK, // attacker's half shown, waiting to show the defender's
  REVEAL_FULL    // everything shown
} RevealStage;

typedef struct
{ GameEvent queue[EVENT_BUF_CAP]; // unreleased events, oldest first at `head`
  uint16_t head, count;
  int64_t delay_ms;
  bool started;          // the head combat has begun its reveal
  bool awaiting_ack;     // a combat finished revealing and the player hasn't continued yet
  RevealStage stage;
  int64_t stage_since_ms;
  CombatDetails shown;   // the combat on display (valid unless stage == REVEAL_NONE)
  PlayerID shown_attacker;
} GuiReveal;

// `delay_seconds` is clamped to 0..GUI_REVEAL_DELAY_MAX_S; 0 reveals everything at once.
void gui_reveal_init(GuiReveal* rv, int delay_seconds);

// Queues every event of an update (drops what doesn't fit, like EventBuf).
void gui_reveal_push(GuiReveal* rv, const EventBuf* events);

// Releases the next event that may now be logged, or returns false if none
// can be yet. Call repeatedly each frame until it returns false.
bool gui_reveal_pop(GuiReveal* rv, int64_t now_ms, GameEvent* out);

// Skips the current wait (attack -> defense, or the hold before the next
// combat): the next gui_reveal_pop() moves on.
void gui_reveal_skip(GuiReveal* rv, int64_t now_ms);

// After a combat has fully revealed the GUI keeps it on screen until the player
// has taken it in (Continue). True from that moment, once nothing is queued any
// more, until gui_reveal_ack(); the combat is then cleared off the table.
bool gui_reveal_needs_ack(const GuiReveal* rv);
void gui_reveal_ack(GuiReveal* rv);

// True while an EVT_GAME_OVER is still queued behind a combat -- the GUI holds
// its "game over" text until the reveal has reached it.
bool gui_reveal_game_over_pending(const GuiReveal* rv);

// True while events are still queued -- the GUI locks input meanwhile.
bool gui_reveal_busy(const GuiReveal* rv);

// The combat to draw; false before the first one. `*attacker` is who attacked.
bool gui_reveal_current(const GuiReveal* rv, const CombatDetails** details,
                        PlayerID* attacker, RevealStage* stage);

// While a queued combat hasn't been fully revealed, its defender's energy is
// shown as the pre-damage value. Returns true and sets `*energy` for `p` if so.
bool gui_reveal_energy_override(const GuiReveal* rv, PlayerID p, uint8_t* energy);

#endif // GUI_REVEAL_H
