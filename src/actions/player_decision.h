// player_decision.h
// PlayerDecision: wraps GameMove (attack/defense) plus mulligan/discard-to-7
// card lists into the one decision type every player-facing surface (CLI,
// TUI, and eventually the SDL3 GUI's session client) submits to the engine.
// GameMove itself stays untouched -- search agents (A8-A14) hold it in large
// arrays, so widening it to carry a 5+ card discard list would grow every
// tree node. See ideas/9 gui/gui_architecture_synthesis.md section 4.

#ifndef PLAYER_DECISION_H
#define PLAYER_DECISION_H

#include "../core/game_types.h"
#include "game_move.h"

typedef enum
{ DECISION_KIND_NONE = 0,
  DECISION_KIND_MULLIGAN,     // player B, before turn 1: discard 0..mulligan_get_max_cards()
  DECISION_KIND_ATTACK,       // move: turn_phase == ATTACK
  DECISION_KIND_DEFENSE,      // move: MOVE_PASS or MOVE_CHAMPIONS, turn_phase == DEFENSE
  DECISION_KIND_DISCARD_TO_7
} DecisionKind;

#define DECISION_MAX_CARDS 12   // == Hand capacity; discard-to-7 can exceed 3

// move/cards fields are always fullDeck[] indices, never hand positions --
// hand positions shift as cards are removed and differ between the engine's
// state and whatever order a UI displays them in.
typedef struct
{ DecisionKind kind;
  GameMove move;                       // DECISION_KIND_ATTACK / _DEFENSE
  uint8_t count;                       // DECISION_KIND_MULLIGAN / _DISCARD_TO_7
  uint8_t cards[DECISION_MAX_CARDS];   // DECISION_KIND_MULLIGAN / _DISCARD_TO_7
} PlayerDecision;

// Returns true iff `d` is a legal decision of kind `expected` for `player`
// in gstate's current state -- the server-side "never trust the client"
// check every submission path must run before applying a decision.
// ATTACK/DEFENSE moves are checked by canonical (order-insensitive)
// membership against get_available_moves() built with exhaustive-enough
// limits {1,1} (recall/cash variant caps don't affect which champion/draw
// moves are emitted -- only how many recall/cash sub-choice variants); the
// recall/cash sub-choice itself (which champions, which exchange target) is
// then checked structurally against the real discard/hand, since a human
// may legitimately choose any variant, not just the one limits={1,1} sampled.
bool decision_is_legal(const struct gamestate* gstate, PlayerID player,
                       DecisionKind expected, const PlayerDecision* d);

#endif // PLAYER_DECISION_H
