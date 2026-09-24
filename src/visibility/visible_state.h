// visible_state.h
// VisibleGameState: the filtered view of struct gamestate a UI client (GUI,
// future network client) is allowed to see. Never carries the opponent's
// hand contents, deck order, or RNG state -- see
// ideas/9 gui/gui_architecture_synthesis.md section 5 for the design
// rationale (one render path for standalone and networked play; hidden
// info can't leak by accident because the GUI is never handed the real
// struct gamestate*).

#ifndef VISIBLE_STATE_H
#define VISIBLE_STATE_H

#include "../core/game_types.h"

// A regular player sees their own hand; a spectator (AI-vs-AI viewing, a
// future "watch" feature) sees neither hand, only counts -- see
// gui_architecture_synthesis.md section 5.3.
#define VIEWER_SPECTATOR ((PlayerID)0xFF)

// Per-player arrays are indexed by PlayerID (0/1), never split into
// my_*/opp_* fields, so rendering code can loop over players -- the one
// deliberate exception is my_hand, since only the viewer's own hand is ever
// visible (gui_architecture_synthesis.md section 5.1a).
typedef struct
{ PlayerID viewer;                    // whose eyes this view is filtered for;
  // VIEWER_SPECTATOR for a spectator view

  // Public, per player
  uint8_t energy[NUM_PLAYERS];
  uint16_t cash[NUM_PLAYERS];
  uint8_t hand_count[NUM_PLAYERS];
  uint8_t deck_count[NUM_PLAYERS];    // deck[p].top + 1
  Discard discard[NUM_PLAYERS];       // full contents -- discard piles are
  // face-up (doc/game_rules_doc.md glossary)
  CombatZone combat_zone[NUM_PLAYERS];

  // Private to viewer (empty/zeroed for a spectator)
  Hand my_hand;

  // Flow
  uint16_t turn;
  TurnPhase turn_phase;
  PlayerID current_player;            // attacker this turn
  PlayerID player_to_move;
  ComboBonusTable combo_bonus_table;  // needed to score combo bonuses in the UI
  bool game_over;
  GameStateEnum game_state;           // winner once game_over is true
} VisibleGameState;

// Fills out from gs, keeping only what `viewer` is allowed to see. Pass
// VIEWER_SPECTATOR for a spectator view (both hands as counts only).
void visibility_filter(const struct gamestate* gs, PlayerID viewer,
                       VisibleGameState* out);

#endif // VISIBLE_STATE_H
