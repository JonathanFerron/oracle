// game_event.h
// GameEvent: what actually happened this step, as data rather than a
// callback fired from inside the engine (a callback would run on the future
// session thread, where the GUI can't safely act) -- see
// ideas/9 gui/gui_architecture_synthesis.md section 6. Events are produced
// by the step driver (Step 5) by comparing state before/after each engine
// step, using the diff helpers below, rather than threading callbacks
// through every primitive in card_actions.c/combat.c.
//
// Header-only dependency on GameMove/CombatDetails (their structs, not
// their implementations) -- see this directory's own "game_types.h + libc
// only" precedent in visible_state.h; nothing here links core/combat.c or
// actions/move_gen.c.

#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#include "../core/game_types.h"
#include "../core/combat.h"
#include "../actions/game_move.h"

typedef enum
{ EVT_GAME_STARTED,
  EVT_MULLIGAN_DONE,     // player, u.cards (discarded -- public, they go to discard)
  EVT_TURN_BEGAN,        // player (the attacker), u.turn
  EVT_CARD_DRAWN,        // player, u.card -- redacted (EVT_CARD_REDACTED) for the non-owner
  EVT_DECK_RESHUFFLED,   // player (whose discard -> deck)
  EVT_MOVE_PLAYED,       // player, u.move -- champions/draw/recall/cash card and any
  // recalled champion identities are all public (discard is face-up)
  EVT_COMBAT_RESOLVED,   // u.combat (attacker is gstate->current_player)
  EVT_LUNA_COLLECTED,    // player
  EVT_DISCARDED_TO_7,    // player, u.cards (public -- discard pile is face-up)
  EVT_GAME_OVER          // player = winner
} GameEventType;

// Sentinel for u.card when event_filter_for_viewer() has redacted it.
#define EVT_CARD_REDACTED ((uint8_t)0xFF)

typedef struct
{ GameEventType type;
  PlayerID player;
  union
  { GameMove move;          // EVT_MOVE_PLAYED
    CombatDetails combat;   // EVT_COMBAT_RESOLVED
    Hand cards;             // EVT_MULLIGAN_DONE / EVT_DISCARDED_TO_7 -- reuses Hand rather
    // than a bespoke count+array pair; both events describe a
    // face-up (discard-bound) card list, sized correctly (12) for free.
    uint8_t card;           // EVT_CARD_DRAWN
    uint16_t turn;          // EVT_TURN_BEGAN
  } u;
} GameEvent;

// Redacts what `viewer` isn't allowed to see, in place. Today that's just
// EVT_CARD_DRAWN's card identity for anyone but the drawing player -- every
// other event type is already public information (see the enum comments
// above). Pass VIEWER_SPECTATOR (visible_state.h) for a spectator view.
void event_filter_for_viewer(GameEvent* e, PlayerID viewer);

// Cards present in `after` (n_after of them) but not in `before` (n_before)
// -- e.g. a hand after a draw/recall, or a combat zone after champions were
// committed. Writes up to max_out entries to `out` (in `after`'s scan
// order) and returns the count written. Works on any card array (Hand,
// Discard, CombatZone all share the cards[]+size shape) -- same "raw
// pointer + count" convention card_actions.c's collect_champions() uses.
uint8_t cards_added(const uint8_t* before, uint8_t n_before,
                    const uint8_t* after, uint8_t n_after,
                    uint8_t* out, uint8_t max_out);

// Cards present in `before` but not `after` -- e.g. champions that left a
// hand, or champions pulled out of a discard by a recall.
uint8_t cards_removed(const uint8_t* before, uint8_t n_before,
                      const uint8_t* after, uint8_t n_after,
                      uint8_t* out, uint8_t max_out);

// Fixed-capacity event log for one engine_advance()/engine_submit()/
// engine_run_ai() call (game_engine.h, Step 5) -- 32 matches the synthesis
// doc's own proposed size, comfortably above the handful of events any one
// step actually produces (at most: reshuffle + card drawn + turn began, or
// move played + combat resolved + luna collected).
#define EVENT_BUF_CAP 32

typedef struct
{ GameEvent ev[EVENT_BUF_CAP];
  uint8_t count;
} EventBuf;

// Appends `e` if there's room; silently drops it otherwise (a caller
// producing more than EVENT_BUF_CAP events in one driver step is a bug
// worth finding via a shrunk buffer in a test, not a runtime crash).
void event_buf_push(EventBuf* buf, GameEvent e);

#endif // GAME_EVENT_H
