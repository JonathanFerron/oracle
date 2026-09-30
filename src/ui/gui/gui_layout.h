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

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "../../core/game_types.h"

// Sized so the champion art area stays 90x138 px with the wide left column and
// tall name bar of gui_card_face.c (aspect ~0.77, a bit wider than the print).
#define GUI_CARD_WIDTH 139.0f
#define GUI_CARD_HEIGHT 180.0f
#define GUI_CARD_GAP 8.0f
#define GUI_DISCARD_MIN_COLS 2 // the grid is sized from the window within these bounds
#define GUI_DISCARD_MAX_COLS 8
#define GUI_DISCARD_MIN_ROWS 2
#define GUI_DISCARD_MAX_ROWS 4
#define GUI_MAX_BUTTON_SLOTS 3 // widest button list (gui_input.h GUI_MAX_BUTTONS)

typedef struct
{ SDL_FRect info;   // name/energy/cash label
  SDL_FRect hand;   // bounding row -- gui_layout_card_slot() subdivides it
  SDL_FRect deck;   // single face-down slot + count label
  SDL_FRect discard; // mini-card grid (cols x rows of half-size cards) at the table's
  // left edge, under the top hand / over the bottom hand; cell 0 is the count tile
  // (gui_layout_discard_slot())
  uint8_t discard_cols, discard_rows;
} GuiSeatLayout;

typedef struct
{ SDL_FRect status_bar;             // board width, top: turn/phase/pending text at
  // the left, then (right end) the input buttons and the seed tag
  SDL_FRect buttons;                // inside status_bar, left of seed_tag: input buttons
  // (gui_input.c) when it's the viewer's move, an "opponent is thinking" label
  // otherwise; gui_layout_button_rect() right-aligns within it
  SDL_FRect seed_tag;               // inside status_bar: the "Seed n" tag (right-aligned)
  GuiSeatLayout seat[NUM_PLAYERS];
  SDL_FRect combat_zone[NUM_PLAYERS]; // center panel, one bounding row per
  // seat (seat[0]'s row nearer the bottom, seat[1]'s nearer the top) --
  // gui_layout_card_slot() subdivides each into up to 3 card slots
  // Dice results, in a row between each seat's hand and its combat zone (outer
  // side of the zone, like throwing dice on your own side of the table): same
  // x-span as the combat zone, so a die lines up under/over its champion card.
  SDL_FRect dice_row[NUM_PLAYERS];
  // Right of the (up to 3) combat cards, spanning the dice row and the combat
  // zone: combo bonus, side total, damage, energy change.
  SDL_FRect combat_info[NUM_PLAYERS];
  bool log_open;         // side panel shown? (the other regions shrink to make room)
  SDL_FRect log_panel;   // full-height right-hand column (zero width when closed)
  SDL_FRect log_toggle;  // close button in the panel header when open, a "Log" tab
  // in the top-right corner (over the status bar) when closed
} GuiLayout;

// The three rects of a seat's combat presentation: the champion cards, the dice
// row and the info area beside them (seat slot: 0 = the viewer).
typedef struct
{ SDL_FRect zone, dice, info;
} GuiSeatRects;

GuiSeatRects gui_layout_seat_rects(const GuiLayout* layout, uint8_t seat);

// `log_open` reserves the right-hand log column; every other region then lays
// out within the remaining width. Recomputes every rect for the current window size (call once per frame --
// cheap, and keeps the GUI responsive to live resizing like the TUI is).
void gui_layout_compute(float win_w, float win_h, bool log_open, GuiLayout* out);

// Maps `player` to a seat slot (0 = viewer's own seat) for the given
// `viewer`. VIEWER_SPECTATOR (visible_state.h) has no seat-0 "own hand", so
// callers should just index seats by raw PlayerID in that case -- this
// function is for the common viewer != spectator case.
uint8_t gui_layout_seat_for_player(PlayerID player, PlayerID viewer);

// Subdivides `row` into `count` evenly spaced, horizontally centered
// GUI_CARD_WIDTH x GUI_CARD_HEIGHT slots; `index` selects which one
// (0-based). Used for hand rows and the up-to-3-card combat zone rows alike.
SDL_FRect gui_layout_card_slot(SDL_FRect row, uint8_t index, uint8_t count);

// Lays out `total` buttons right-aligned in `area` (GuiLayout.buttons) with a small
// gap between them; `index` selects which one (0-based). gui_input.c and
// gui_render.c both call this against the same `total`/order (from
// gui_input_active_buttons()), so hit-testing and drawing always agree.
SDL_FRect gui_layout_button_rect(SDL_FRect area, uint8_t index, uint8_t total);

// Cell `index` (reading order, `cols` per row) of a discard grid: a half-width,
// half-height card.
SDL_FRect gui_layout_discard_slot(SDL_FRect area, uint8_t cols, uint8_t index);

// A large centered panel for the recall discard-picker overlay.
SDL_FRect gui_layout_overlay_area(float win_w, float win_h);

// Wraps `count` GUI_CARD_WIDTH x GUI_CARD_HEIGHT slots left-to-right,
// top-to-bottom within `area` (as many columns as fit `area`'s width);
// `index` selects which one. Used for the recall overlay's discard grid,
// which -- unlike a hand/combat-zone row -- can hold more champions than
// fit on one line.
SDL_FRect gui_layout_grid_slot(SDL_FRect area, uint8_t index, uint8_t count);

#endif // GUI_LAYOUT_H
