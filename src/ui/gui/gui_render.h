// gui_render.h
// Draws the whole board from the latest SessionUpdate each frame
// (immediate-mode style, no retained scene graph -- see
// ideas/9 gui/gui_architecture_synthesis.md section 9.3), including the
// action bar/buttons and recall overlay driven by gui_input.c's staging
// state, and the message log (gui_log.c).

#ifndef GUI_RENDER_H
#define GUI_RENDER_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "gui_input.h" // GuiInputState
#include "gui_log.h" // GuiLog
#include "../../core/game_types.h" // ui_language_t
#include "../../structures/card_collection.h" // Discard
#include "../../roles/stda/stda_session.h" // SessionUpdate

typedef struct
{ TTF_Font* title_font;  // M1 hello-window wordmark, kept for the pre-first-update splash
  TTF_Font* status_font; // status bar + per-seat energy/cash/name + message log (ComicNeue-Regular)
  TTF_Font* card_font;   // card text + deck/discard count badges (PatrickHand-Regular)
} GuiFonts;

// English-only DecisionKind label for stderr traces (gui_app.c) -- distinct
// from gui_render_frame()'s own status-bar text, which is localized.
const char* gui_decision_kind_debug_name(DecisionKind kind);

// Filters `d` down to its champion cards only (recall targets are always
// champions), preserving order; returns the count written to `out`
// (capacity 40, Discard's own max). Shared by this file's recall-overlay
// draw and gui_input.c's overlay hit-test so their indices always agree.
uint8_t gui_discard_champions(const Discard* d, uint8_t out[40]);

// `input` is the viewer's own staging state (gui_input.c) -- pass NULL when
// there's nothing for the viewer to stage right now (game over, or it's not
// their move; gui_render_frame() shows a "game over"/"opponent is thinking"
// label in the action bar instead of buttons). `log` is drawn in the
// leftover space between the two combat zones (gui_layout.c's log_panel).
void gui_render_frame(SDL_Renderer* renderer, const GuiFonts* fonts,
                      const SessionUpdate* u, const GuiInputState* input,
                      const GuiLog* log, ui_language_t lang);

#endif // GUI_RENDER_H
