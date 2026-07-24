// tui_render.h
// ncurses window layout and drawing for stda.tui mode (Milestone 1: display only)

#ifndef TUI_RENDER_H
#define TUI_RENDER_H

#include <stddef.h>
#include "../../core/game_types.h"
#include "../../core/combat.h"

/* Opaque ncurses window handle -- deliberately not #include <ncurses.h> here.
   ncurses.h #defines COLOR_RED/COLOR_GREEN/etc, which collide with this
   codebase's own ChampionColor enum (COLOR_RED/COLOR_INDIGO/COLOR_ORANGE);
   keeping ncurses.h confined to tui_render.c avoids that collision everywhere
   else this header is included. */
typedef struct _win_st WINDOW;

#define TUI_MIN_ROWS 30
#define TUI_MIN_COLS 100
#define TUI_MAX_MESSAGES 100

/* Message color pairs, for tui_add_message_colored() -- exposed as plain
   ints (rather than requiring <ncurses.h>) so callers outside tui_render.c
   (e.g. ui/tui/tui_input.c's UiIO backend) can tag messages by kind. Must
   match the PAIR_MSG_* values in the internal color-pair enum in
   tui_render.c. */
#define TUI_MSG_COLOR_DEFAULT 0
#define TUI_MSG_COLOR_ERROR   10
#define TUI_MSG_COLOR_SUCCESS 11
#define TUI_MSG_COLOR_WARNING 12
#define TUI_MSG_COLOR_PLAYER_A 15
#define TUI_MSG_COLOR_PLAYER_B 16

typedef struct
{ WINDOW* win_top_status;
  WINDOW* win_bottom_status;
  WINDOW* win_command;
  WINDOW* win_play;
  WINDOW* win_shortcuts;
  WINDOW* win_msgbox;
  WINDOW* win_console;

  // Console: interaction (prompts, input echo, validation errors,
  // recall/cash candidate lists).
  char* messages[TUI_MAX_MESSAGES];
  int message_colors[TUI_MAX_MESSAGES];
  int message_count;

  // Game Messages: narrative (turn summaries, combat resolution, damage,
  // energy changes, action outcomes like "Played N champion(s)").
  char* game_messages[TUI_MAX_MESSAGES];
  int game_message_colors[TUI_MAX_MESSAGES];
  int game_message_count;

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

/* Appends a formatted line to the scrolling Console log (oldest dropped past
   TUI_MAX_MESSAGES), in the default color. Console is for interaction:
   prompts, input echo, validation errors, recall/cash candidate lists. */
void tui_add_message(TuiScreen* screen, const char* format, ...);

/* Same, tagged with one of the TUI_MSG_COLOR_* constants above. */
void tui_add_message_colored(TuiScreen* screen, int color_pair, const char* format, ...);

/* Same as tui_add_message()/tui_add_message_colored(), but appends to the
   Game Messages log instead -- narrative: turn summaries, combat resolution,
   damage, energy changes, action outcomes ("Played N champion(s)"). */
void tui_add_game_message(TuiScreen* screen, const char* format, ...);
void tui_add_game_message_colored(TuiScreen* screen, int color_pair, const char* format, ...);

/* Returns TUI_MSG_COLOR_PLAYER_A/B for the given player -- convenience for
   tagging player-attributed Game Messages lines (e.g. "PLAYER x (Attacker):"
   headers, energy-change lines) with the same color as that player's status
   bar. */
int tui_player_msg_color(PlayerID player);

/* Input helpers -- kept here (rather than callers touching <ncurses.h>
   directly) so ncurses stays confined to tui_render.c. */
int tui_get_input(void);
bool tui_input_is_quit(int ch);
bool tui_input_is_resize(int ch);
bool tui_input_is_tab(int ch);
bool tui_input_is_enter(int ch);
bool tui_input_is_escape(int ch);
bool tui_input_is_backspace(int ch);
bool tui_input_is_printable(int ch);

/* Draws literal text into the bottom command-line window (used both as a
   live line editor in COMMAND mode and as a status line in PLAY mode);
   show_cursor controls whether the terminal cursor is shown at end-of-text. */
void tui_draw_command_line(TuiScreen* screen, const char* text, bool show_cursor);

/* Formats a card compactly (e.g. "d5+3 3 RAT") -- shared by the play-area
   card rendering and the UiIO show_card_list()/message-log formatting.
   Localized (draw/recall/cash labels follow cfg->language). */
void tui_format_card(uint8_t card_idx, char* buf, size_t bufsize, config_t* cfg);

/* Renders a resolved combat's per-champion breakdown into the message log
   (mirrors cli_action_display.c's display_combat_details_cli(), TUI-side). */
void tui_show_combat_details(TuiScreen* screen, struct gamestate* gstate,
                             CombatDetails* details, config_t* cfg);

#endif // TUI_RENDER_H
