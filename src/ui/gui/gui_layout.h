// gui_layout.h
// Computes rectangles for every screen region from the window size, the way
// tui_render.c does for the ncurses TUI -- see
// ideas/9 gui/gui_architecture_synthesis.md section 9.5. Only ever indexed
// by seat slot (0 = the viewer's own seat, "bottom"; 1 = the next seat
// clockwise, "top" with NUM_PLAYERS == 2 today), never by a hardcoded
// PlayerID/"top"-"bottom" pair directly, so a future 3-4 player rework adds
// seats here rather than touching every caller.

#ifndef GUI_LAYOUT_H
#define GUI_LAYOUT_H

#include <SDL3/SDL.h>
#include "../../core/game_types.h"

#define GUI_CARD_WIDTH 96.0f
#define GUI_CARD_HEIGHT 145.0f
#define GUI_CARD_GAP 8.0f

typedef struct
{ SDL_FRect info;   // name/energy/cash label
  SDL_FRect hand;   // bounding row -- gui_layout_card_slot() subdivides it
  SDL_FRect deck;   // single face-down slot + count label
  SDL_FRect discard; // single face-up slot (top card) + count label
} GuiSeatLayout;

typedef struct
{ SDL_FRect status_bar;             // full width, top: turn/phase/pending
  SDL_FRect action_bar;             // full width, just below status_bar:
  // input buttons (gui_input.c) when it's the viewer's move, an "opponent
  // is thinking" label otherwise
  GuiSeatLayout seat[NUM_PLAYERS];
  SDL_FRect combat_zone[NUM_PLAYERS]; // center panel, one bounding row per
  // seat (seat[0]'s row nearer the bottom, seat[1]'s nearer the top) --
  // gui_layout_card_slot() subdivides each into up to 3 card slots
  SDL_FRect log_panel; // between the two combat zones -- gui_log.c
} GuiLayout;

// Recomputes every rect for the current window size (call once per frame --
// cheap, and keeps the GUI responsive to live resizing like the TUI is).
void gui_layout_compute(float win_w, float win_h, GuiLayout* out);

// Maps `player` to a seat slot (0 = viewer's own seat) for the given
// `viewer`. VIEWER_SPECTATOR (visible_state.h) has no seat-0 "own hand", so
// callers should just index seats by raw PlayerID in that case -- this
// function is for the common viewer != spectator case.
uint8_t gui_layout_seat_for_player(PlayerID player, PlayerID viewer);

// Subdivides `row` into `count` evenly spaced, horizontally centered
// GUI_CARD_WIDTH x GUI_CARD_HEIGHT slots; `index` selects which one
// (0-based). Used for hand rows and the up-to-3-card combat zone rows alike.
SDL_FRect gui_layout_card_slot(SDL_FRect row, uint8_t index, uint8_t count);

// Divides `bar` into `total` equal-width, left-aligned buttons with a small
// gap between them; `index` selects which one (0-based). gui_input.c and
// gui_render.c both call this against the same `total`/order (from
// gui_input_active_buttons()), so hit-testing and drawing always agree.
SDL_FRect gui_layout_button_rect(SDL_FRect bar, uint8_t index, uint8_t total);

// A large centered panel for the recall discard-picker overlay.
SDL_FRect gui_layout_overlay_area(float win_w, float win_h);

// Wraps `count` GUI_CARD_WIDTH x GUI_CARD_HEIGHT slots left-to-right,
// top-to-bottom within `area` (as many columns as fit `area`'s width);
// `index` selects which one. Used for the recall overlay's discard grid,
// which -- unlike a hand/combat-zone row -- can hold more champions than
// fit on one line.
SDL_FRect gui_layout_grid_slot(SDL_FRect area, uint8_t index, uint8_t count);

#endif // GUI_LAYOUT_H
