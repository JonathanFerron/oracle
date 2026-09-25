// gui_log.h
// Scrolling message log fed by SessionUpdate.events -- turns "energy
// changed" into "why": mulligans, draws, moves played, combat results
// (from CombatDetails, already carried by EVT_COMBAT_RESOLVED -- no new
// engine plumbing needed), luna collection, discards, game over. A fixed
// ring buffer (GUI_LOG_CAPACITY lines), formatted once per event as it
// arrives rather than every frame -- see gui_log_append_events()'s own
// comment for why that's safe (the language doesn't change mid-game).

#ifndef GUI_LOG_H
#define GUI_LOG_H

#include <stdint.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../core/game_types.h" // ui_language_t
#include "../../roles/stda/stda_session.h" // SessionUpdate

#define GUI_LOG_CAPACITY 200
#define GUI_LOG_LINE_LEN 96

typedef struct
{ char lines[GUI_LOG_CAPACITY][GUI_LOG_LINE_LEN];
  uint16_t count; // valid entries, caps at GUI_LOG_CAPACITY
  uint16_t next;  // ring-buffer write cursor
} GuiLog;

void gui_log_init(GuiLog* log);

// Formats every event in `u->events` into one localized line each and
// appends them (oldest lines silently drop once GUI_LOG_CAPACITY is
// reached). Formatted once at append time, not redrawn from raw GameEvents
// every frame -- `lang` is fixed for the life of the process (no in-game
// language switch yet), so nothing is lost by not keeping the raw events
// around. `u->view.game_state` (not the EVT_GAME_OVER event's own `.player`,
// which game_engine.c always sets to a real player even on a draw) is what
// decides the game-over line's wording.
void gui_log_append_events(GuiLog* log, const SessionUpdate* u, ui_language_t lang);

// Draws as many of the most recent lines as fit in `area` (oldest at top,
// newest at bottom -- auto-scrolls, no manual scrollback yet), on a panel
// background.
void gui_log_draw(SDL_Renderer* renderer, TTF_Font* font, SDL_FRect area, const GuiLog* log);

#endif // GUI_LOG_H
