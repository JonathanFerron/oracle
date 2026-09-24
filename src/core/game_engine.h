// game_engine.h
// GameEngine: one pollable implementation of the turn flow that runs until
// the next point a player must decide, then stops and reports who and
// what -- see ideas/9 gui/gui_architecture_synthesis.md section 7. Becomes
// the single owner of turn_phase/player_to_move bookkeeping for the real
// (non-rollout) game loop; stda_auto.c's play_stda_auto_game() drives it.
//
// turn_logic.c's play_turn()/begin_of_turn()/attack_phase()/defense_phase()/
// end_of_turn() are UNCHANGED and stay in permanent use by every search
// agent's rollouts (ai_strat_playout.c and A8-A14) -- see
// ideas/9 gui/gui_architecture_synthesis.md's correction #2 ("play_turn()
// can't be deleted"). This is a second, independent implementation of the
// same turn flow, not a replacement -- their RNG consumption order must
// match (verified by testsrc/test_game_engine.c and the -a -p regression
// check), but nothing here calls into turn_logic.c or vice versa.

#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "game_types.h"
#include "game_context.h"
#include "../actions/player_decision.h"
#include "../ai_strat/ai_strategy.h"
#include "../visibility/game_event.h"

// Internal phase bookkeeping -- not exposed to UIs (they only ever see
// `pending`, below).
typedef enum
{ ENG_SETUP,
  ENG_MULLIGAN_WAIT,
  ENG_BEGIN_TURN,
  ENG_ATTACK_WAIT,
  ENG_DEFENSE_WAIT,
  ENG_COMBAT,
  ENG_END_TURN,
  ENG_DISCARD_WAIT,
  ENG_SWITCH_PLAYER,
  ENG_GAME_OVER
} EnginePhase;

// What the engine is waiting on right now. kind == DECISION_KIND_NONE only
// once phase == ENG_GAME_OVER.
typedef struct
{ DecisionKind kind;
  PlayerID player;
} PendingDecision;

typedef struct
{ struct gamestate state;   // authoritative
  EnginePhase phase;
  PendingDecision pending;
} GameEngine;

// Deals the game (setup_game()) and initializes turn/turn_phase/
// player_to_move exactly as stda_auto.c's play_stda_auto_game() does today
// (setup_game() itself leaves them undefined -- see CLAUDE.md's "known
// architectural gaps"), then leaves `e` sitting at the mulligan decision
// (player B, always offered -- see apply_mulligan()'s own comment on why
// this isn't conditional). Emits EVT_GAME_STARTED.
void engine_init(GameEngine* e, uint16_t initial_cash, GameContext* ctx, EventBuf* out);

// Runs automatic steps (combat resolution, luna collection, turn/player
// switching) until a decision is needed or the game ends, appending every
// GameEvent produced along the way to `out`. Safe to call repeatedly while
// already sitting at a wait state -- it just returns `e->pending` again
// without doing anything. Always returns e->pending.
PendingDecision engine_advance(GameEngine* e, GameContext* ctx, EventBuf* out);

// Validates `d` via decision_is_legal() (also checking `player` really is
// who the engine is waiting on) and applies it if legal, then returns --
// call engine_advance() again afterwards to run the automatic steps that
// follow. Returns false (state and phase untouched) if `player`/`d` isn't
// the engine's actual pending decision or fails decision_is_legal().
bool engine_submit(GameEngine* e, PlayerID player, const PlayerDecision* d,
                   GameContext* ctx, EventBuf* out);

// Lets the pending player's existing (state-mutating) AI strategy hook make
// the decision in place, then applies the same phase transition a human's
// engine_submit() would have. Since a strategy doesn't return a GameMove,
// the resulting event is reconstructed from a before/after diff: exact for
// EVT_MULLIGAN_DONE/EVT_DISCARDED_TO_7 (a hand diff has no ambiguity), an
// approximation for EVT_MOVE_PLAYED (champions committed, or MOVE_PASS if
// none -- draw/recall/cash sub-detail isn't recovered, "good enough for a
// message log line" per the synthesis doc, not a perfect GameMove).
void engine_run_ai(GameEngine* e, const StrategySet* strategies, GameContext* ctx,
                   EventBuf* out);

// Ends the game immediately as a resignation: `loser` concedes, the other
// player wins. Not reachable through engine_advance()'s own flow -- no game
// rule triggers this internally; it exists for a future human quitting
// mid-game (the session layer's CMD_RESIGN, src/roles/stda/stda_session.h).
void engine_resign(GameEngine* e, PlayerID loser, EventBuf* out);

#endif // GAME_ENGINE_H
