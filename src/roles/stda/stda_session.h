// stda_session.h
// SessionClient: the opaque handle a GUI (or a headless test harness) talks
// to instead of touching the engine/GameContext directly -- see
// ideas/9 gui/gui_architecture_synthesis.md section 8. A local session owns
// its GameEngine + GameContext exclusively on its own thread (C11
// <threads.h>, not SDL -- the plan file's Step 6 threading decision: this
// keeps stda_session.c buildable/testable without SDL3, consistent with
// every other file in this directory, and Windows/MSYS2 portability is no
// longer a project goal so <threads.h>'s weaker support there is moot). A
// future NetSession would implement the same interface over sockets,
// leaving GUI code unchanged.
//
// Poll, not push: unlike the synthesis doc's own SDL_PushEvent()-based
// wake-up, session_client_poll() is plain non-blocking polling, meant to be
// called once per GUI frame -- the GUI already redraws every frame (no
// animation, render-from-state design), so a poll adds at most one frame of
// latency and keeps this file (and its headless test) completely SDL-free.
//
// Publish cadence (a deliberate refinement on the synthesis doc's own §8.2
// pseudocode, which publishes after every single engine_advance() step):
// the session thread only actually PUBLISHES -- makes a new SessionUpdate
// visible to session_client_poll() -- once it reaches a decision pending on
// a human seat, or the game ends. A single-slot "latest update" would
// otherwise lose events fired during a silent AI-only stretch (e.g. an AI
// defender's move, the combat resolution, luna collection, all before the
// human's next decision) if the client doesn't happen to poll in between;
// every event from such a stretch is batched into the one publish that
// follows instead.

#ifndef STDA_SESSION_H
#define STDA_SESSION_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"
#include "../../core/game_engine.h"
#include "../../actions/player_decision.h"
#include "../../actions/move_gen.h" // MOVE_GEN_MAX_MOVES
#include "../../visibility/visible_state.h"
#include "../../visibility/game_event.h"

typedef struct
{ VisibleGameState view;
  PendingDecision pending;
  uint8_t legal_count;              // valid when pending.kind is ATTACK/DEFENSE
  GameMove legal[MOVE_GEN_MAX_MOVES];
  uint8_t min_cards, max_cards;     // valid when pending.kind is MULLIGAN/DISCARD_TO_7
  EventBuf events;                  // every event since the previous publish, filtered
  // for this client's viewer (event_filter_for_viewer())
  bool rejected;                    // the last CMD_SUBMIT was illegal; pending unchanged
} SessionUpdate;

typedef enum
{ CMD_SUBMIT,
  CMD_RESIGN,
  CMD_QUIT
} SessionCommandType;

typedef struct
{ SessionCommandType type;
  PlayerDecision decision; // CMD_SUBMIT only
} SessionCommand;

typedef struct SessionClient SessionClient; // opaque; see stda_session.c

// Starts a local session on its own thread and returns immediately.
// player_types[] says which seat(s) are AI (the session drives them itself
// via the real strategy hooks in `strategies`) vs. interactive/human (the
// session waits for a SessionCommand); the client's own viewer is the first
// INTERACTIVE_PLAYER seat found, or a spectator if none are (hot-seat, two
// interactive seats sharing one client, is a later UX question -- see
// gui_architecture_synthesis.md section 8.5). `strategies` is copied by
// value; `ctx` is NOT copied -- the session thread owns it exclusively from
// this call onward, matching "GameContext/RNG... touched only by the
// session thread" (synthesis section 8.3). Returns NULL on allocation/
// thread-creation failure.
SessionClient* session_local_start(const PlayerType player_types[NUM_PLAYERS],
                                   const StrategySet* strategies,
                                   uint16_t initial_cash, GameContext* ctx);

// Non-blocking. Copies the latest published update into `out` and returns
// true if it's newer than the last poll; returns false (`out` untouched)
// otherwise. Safe to call every frame.
bool session_client_poll(SessionClient* c, SessionUpdate* out);

// Non-blocking: hands `cmd` to the session thread. Meaningful only while
// the most recently polled update's `pending` is what `cmd` responds to --
// the session revalidates independently regardless (decision_is_legal()),
// so a stale/bogus send is simply rejected (reflected in the next update's
// `rejected` flag for CMD_SUBMIT), never a correctness risk.
void session_client_send(SessionClient* c, const SessionCommand* cmd);

// Sends CMD_QUIT, blocks until the session thread exits, then frees `c`.
void session_client_close(SessionClient* c);

#endif // STDA_SESSION_H
