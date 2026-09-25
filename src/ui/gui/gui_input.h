// gui_input.h
// Click-to-stage input, per
// ideas/9 gui/gui_architecture_synthesis.md section 9.4's table: the GUI
// builds one complete PlayerDecision locally, then submits once. Only the
// viewer's own pending decision is ever staged here -- gui_app.c only calls
// gui_input_handle_click() at all when u->pending.player == u->view.viewer
// (see gui_render.c's "opponent is thinking" label for the other case).
//
// The real legality gate is always decision_is_legal() on the session
// thread (engine_submit() revalidates independently regardless of what this
// file thinks) -- the checks here against SessionUpdate.legal[] are a UX
// convenience (enabling Confirm, deciding what's clickable at all), not a
// security boundary.

#ifndef GUI_INPUT_H
#define GUI_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "../../core/game_types.h"
#include "../../actions/player_decision.h"
#include "../../roles/stda/stda_session.h"

// ATTACK's own sub-flow: picking a draw/recall card branches into a
// Draw-vs-Recall choice, and a cash card branches straight into picking an
// exchange target -- see the synthesis doc's ATTACK row.
typedef enum
{ ATTACK_INPUT_NONE = 0,   // toggling champions, or about to click a special card
  ATTACK_INPUT_SPECIAL,    // a draw/recall card clicked -- Draw/Recall/Cancel shown
  ATTACK_INPUT_CASH_TARGET, // a cash card clicked -- pick exactly 1 hand champion
  ATTACK_INPUT_RECALL_PICK // Recall chosen -- discard overlay, picking exactly choose_num
} AttackInputMode;

typedef struct
{ DecisionKind kind;   // which pending decision this state belongs to
  PlayerID player;

  // MULLIGAN/DISCARD_TO_7: toggled hand cards. ATTACK (mode NONE): toggled
  // champions. ATTACK (mode CASH_TARGET): staged[0] only, the exchange
  // target. DEFENSE: toggled champions.
  uint8_t staged[DECISION_MAX_CARDS];
  uint8_t staged_count;

  AttackInputMode attack_mode;
  uint8_t special_card;      // the draw/recall/cash card clicked (ATTACK only)
  uint8_t recall_staged[3];  // champions toggled in the discard overlay
  uint8_t recall_staged_count;
} GuiInputState;

typedef enum
{ GUI_BTN_NONE = 0,
  GUI_BTN_CONFIRM,
  GUI_BTN_PASS,
  GUI_BTN_DECLINE,
  GUI_BTN_DRAW,
  GUI_BTN_RECALL,
  GUI_BTN_CANCEL
} GuiButtonId;

#define GUI_MAX_BUTTONS 3

// Resets staging for a new pending decision -- call whenever
// SessionUpdate.pending.kind/.player changes, or after a rejected submit.
void gui_input_reset(GuiInputState* st, DecisionKind kind, PlayerID player);

// Fills `out` (capacity GUI_MAX_BUTTONS) with the buttons that should be
// shown right now, left to right; returns how many. `*out_confirm_enabled`
// is set whenever GUI_BTN_CONFIRM is among them (unused otherwise) --
// gui_render.c dims the button when false, and gui_input_handle_click()
// ignores clicks on it. gui_render.c and gui_input.c both call this so
// drawing and hit-testing always agree on the button layout.
uint8_t gui_input_active_buttons(const GuiInputState* st, const SessionUpdate* u,
                                 GuiButtonId out[GUI_MAX_BUTTONS],
                                 bool* out_confirm_enabled);

// Handles a left-click at (x,y) in render-output coordinates (matching
// gui_render_frame()'s own SDL_GetRenderOutputSize()). Toggles a card,
// switches attack_mode, or -- for a terminal action (Confirm/Pass/Decline/
// Draw) -- fills `out_cmd` and returns true, meaning the caller should
// session_client_send() it. Returns false if the click only changed staging
// state (or hit nothing); `out_cmd` is untouched in that case.
bool gui_input_handle_click(GuiInputState* st, const SessionUpdate* u,
                            float x, float y, float win_w, float win_h,
                            SessionCommand* out_cmd);

#endif // GUI_INPUT_H
