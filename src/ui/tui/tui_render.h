// tui_render.h
// ncurses window layout and drawing for stda.tui mode (Milestone 1: display only)

#ifndef TUI_RENDER_H
#define TUI_RENDER_H

#include "../../core/game_types.h"

/* Opaque ncurses window handle -- deliberately not #include <ncurses.h> here.
   ncurses.h #defines COLOR_RED/COLOR_GREEN/etc, which collide with this
   codebase's own ChampionColor enum (COLOR_RED/COLOR_INDIGO/COLOR_ORANGE);
   keeping ncurses.h confined to tui_render.c avoids that collision everywhere
   else this header is included. */
typedef struct _win_st WINDOW;

#define TUI_MIN_ROWS 30
#define TUI_MIN_COLS 100
#define TUI_MAX_MESSAGES 100

typedef struct
{ WINDOW* win_top_status;
  WINDOW* win_bottom_status;
  WINDOW* win_command;
  WINDOW* win_play;
  WINDOW* win_shortcuts;
  WINDOW* win_msgbox;
  WINDOW* win_console;

  char* messages[TUI_MAX_MESSAGES];
  int message_count;

  int last_rows;
  int last_cols;
  bool too_small;
} TuiScreen;

/* Lifecycle: sets up ncurses (colors, input mode) and allocates the screen state.
   Does not lay out windows yet -- call tui_layout() once before the first draw. */
TuiScreen* tui_screen_create(void);
void tui_screen_destroy(TuiScreen* screen);

/* (Re)computes window geometry from the current terminal size and (re)creates the
   ncurses windows. Safe to call again after a KEY_RESIZE. */
void tui_layout(TuiScreen* screen);

/* Draws every panel for the given game state and refreshes the physical screen. */
void tui_draw_all(TuiScreen* screen, struct gamestate* gstate, config_t* cfg);

/* Appends a formatted line to the scrolling console log (oldest dropped past
   TUI_MAX_MESSAGES). */
void tui_add_message(TuiScreen* screen, const char* format, ...);

/* Input helpers -- kept here (rather than callers touching <ncurses.h>
   directly) so ncurses stays confined to tui_render.c. */
int tui_get_input(void);
bool tui_input_is_quit(int ch);
bool tui_input_is_resize(int ch);

#endif // TUI_RENDER_H
