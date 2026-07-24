// tui_render.c
// ncurses window layout and drawing for stda.tui mode (Milestone 1: display only)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "tui_render.h"
#include "../../core/game_constants.h"
#include "../shared/localization.h"

/* ncurses.h must come after game_types.h (pulled in via game_constants.h
   above): it #defines COLOR_RED as a macro (=1), which would otherwise
   shadow every later textual use of this codebase's ChampionColor::COLOR_RED
   enum constant (=0). #undef it right back so the rest of this file's
   ChampionColor comparisons resolve to the real enum again; the ncurses
   numeric color IDs used for init_pair() are given Oracle-specific names
   (NC_RED etc.) below instead. */
#include <ncurses.h>
#undef COLOR_RED

/* Standard POSIX curses color numbers (0-7), named locally instead of using
   ncurses' COLOR_* macros: those collide with this codebase's own
   ChampionColor enum (COLOR_RED/COLOR_INDIGO/COLOR_ORANGE, see game_types.h),
   which every other module already relies on. Keeping Oracle-specific names
   on this (ncurses-facing) side avoids touching that established enum. */
#define NC_BLACK   0
#define NC_RED     1
#define NC_GREEN   2
#define NC_YELLOW  3
#define NC_BLUE    4
#define NC_MAGENTA 5
#define NC_CYAN    6
#define NC_WHITE   7

/* PAIR_MSG_ERROR/SUCCESS/WARNING must land on 10/11/12 -- tui_render.h
   exposes those same numbers as TUI_MSG_COLOR_* so callers outside this
   file (e.g. ui/tui/tui_input.c) can tag messages without needing
   <ncurses.h> themselves. */
enum
{ PAIR_DEFAULT = 1,
  PAIR_STATUS_B,
  PAIR_STATUS_A,
  PAIR_BORDER_SHORTCUTS,
  PAIR_BORDER_MSGBOX,
  PAIR_BORDER_CONSOLE,
  PAIR_CARD_RED,
  PAIR_CARD_INDIGO,
  PAIR_CARD_ORANGE,
  PAIR_MSG_ERROR,
  PAIR_MSG_SUCCESS,
  PAIR_MSG_WARNING
};

#define TUI_INFO_COL_MIN 24
#define TUI_INFO_COL_MAX 50
#define TUI_SHORTCUTS_MIN_H 4
#define TUI_MSGBOX_MIN_H 4
#define TUI_CONSOLE_MIN_H 5

static int clampi(int v, int lo, int hi)
{ if(v < lo) return lo;
  if(v > hi) return hi;
  return v;
}

static void tui_setup_colors(void)
{ if(!has_colors()) return;
  start_color();
  init_pair(PAIR_DEFAULT, NC_WHITE, NC_BLACK);
  init_pair(PAIR_STATUS_B, NC_RED, NC_BLACK);
  init_pair(PAIR_STATUS_A, NC_GREEN, NC_BLACK);
  init_pair(PAIR_BORDER_SHORTCUTS, NC_YELLOW, NC_BLACK);
  init_pair(PAIR_BORDER_MSGBOX, NC_MAGENTA, NC_BLACK);
  init_pair(PAIR_BORDER_CONSOLE, NC_GREEN, NC_BLACK);
  init_pair(PAIR_CARD_RED, NC_RED, NC_BLACK);
  init_pair(PAIR_CARD_INDIGO, NC_BLUE, NC_BLACK);
  init_pair(PAIR_CARD_ORANGE, NC_YELLOW, NC_BLACK);
  init_pair(PAIR_MSG_ERROR, NC_RED, NC_BLACK);
  init_pair(PAIR_MSG_SUCCESS, NC_GREEN, NC_BLACK);
  init_pair(PAIR_MSG_WARNING, NC_YELLOW, NC_BLACK);
}

TuiScreen* tui_screen_create(void)
{ TuiScreen* screen = calloc(1, sizeof(TuiScreen));
  if(!screen) return NULL;

  /* TUI Milestone 2 now runs the player-configuration menu (plain
     printf/fgets) before this is ever called, so flush it first --
     initscr() does no implicit flush of prior stdio output. */
  fflush(stdout);

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  tui_setup_colors();

  /* Every panel here is an independent newwin() (not a derwin()/subwin() of
     stdscr), refreshed only via wrefresh() -- stdscr itself is never drawn
     to or refreshed. Without one plain refresh() first, ncurses' physical/
     virtual screen sync is never seeded and every subsequent wrefresh() on
     those windows computes as a no-op: verified via a tmux-driven session
     where tui_draw_all() completed (every wrefresh() returned OK) but
     nothing ever reached the terminal until this call was added. */
  refresh();

  screen->message_count = 0;
  screen->too_small = false;
  return screen;
}

static void tui_destroy_windows(TuiScreen* screen)
{ WINDOW** wins[] = { &screen->win_top_status, &screen->win_bottom_status,
                      &screen->win_command, &screen->win_play, &screen->win_shortcuts,
                      &screen->win_msgbox, &screen->win_console
                    };

  for(size_t i = 0; i < sizeof(wins) / sizeof(wins[0]); i++)
  { if(*wins[i])
    { delwin(*wins[i]);
      *wins[i] = NULL;
    }
  }
}

void tui_screen_destroy(TuiScreen* screen)
{ if(!screen) return;

  for(int i = 0; i < screen->message_count; i++)
    free(screen->messages[i]);

  tui_destroy_windows(screen);
  endwin();
  free(screen);
}

void tui_layout(TuiScreen* screen)
{ int rows, cols;
  getmaxyx(stdscr, rows, cols);

  tui_destroy_windows(screen);
  screen->last_rows = rows;
  screen->last_cols = cols;

  if(rows < TUI_MIN_ROWS || cols < TUI_MIN_COLS)
  { screen->too_small = true;
    return;
  }
  screen->too_small = false;

  int info_w = clampi(cols / 3, TUI_INFO_COL_MIN, TUI_INFO_COL_MAX);
  int play_w = cols - info_w;
  int top_h = 1, bottom_h = 1, command_h = 1;

  int body_rows = rows - top_h - bottom_h - command_h;
  int shortcuts_h = clampi(body_rows / 5, TUI_SHORTCUTS_MIN_H, 6);
  int msgbox_h = clampi(body_rows / 5, TUI_MSGBOX_MIN_H, 6);
  int console_h = body_rows - shortcuts_h - msgbox_h;

  if(console_h < TUI_CONSOLE_MIN_H)
  { console_h = TUI_CONSOLE_MIN_H;
    shortcuts_h = TUI_SHORTCUTS_MIN_H;
    msgbox_h = body_rows - console_h - shortcuts_h;
  }

  screen->win_top_status = newwin(top_h, cols, 0, 0);
  screen->win_play = newwin(body_rows, play_w, top_h, 0);
  screen->win_shortcuts = newwin(shortcuts_h, info_w, top_h, play_w);
  screen->win_msgbox = newwin(msgbox_h, info_w, top_h + shortcuts_h, play_w);
  screen->win_console = newwin(console_h, info_w,
                               top_h + shortcuts_h + msgbox_h, play_w);
  screen->win_bottom_status = newwin(bottom_h, cols, top_h + body_rows, 0);
  screen->win_command = newwin(command_h, cols,
                               top_h + body_rows + bottom_h, 0);

  scrollok(screen->win_console, TRUE);
}

/* ========================================================================
   Card formatting / coloring
   ======================================================================== */

void tui_format_card(uint8_t card_idx, char* buf, size_t bufsize)
{ const struct card* c = &fullDeck[card_idx];

  if(c->card_type == CHAMPION_CARD)
    snprintf(buf, bufsize, "d%u+%u %u %s", c->defense_dice, c->attack_base,
             c->cost, CHAMPION_SPECIES_ABBR[c->species]);
  else if(c->card_type == DRAW_CARD)
    snprintf(buf, bufsize, "Pig%u/Rap%u %u", c->draw_num, c->choose_num, c->cost);
  else
    snprintf(buf, bufsize, "Echange%u", c->exchange_cash);
}

/* ========================================================================
   Layout helpers
   ======================================================================== */

/* Not static: shared with tui_render_playarea.c (forward-declared there,
   not part of the public tui_render.h API). */
int tui_center_x(int win_width, int content_width)
{ int x = (win_width - content_width) / 2;
  return (x < 1) ? 1 : x;
}

void tui_print_centered(WINDOW* win, int y, const char* text)
{ mvwprintw(win, y, tui_center_x(getmaxx(win), (int)strlen(text)), "%s", text);
}

/* Prints a left-aligned / centered / right-aligned trio on one row, all
   positioned relative to `pane_width` rather than the window's own width --
   status bar windows span the full terminal, but the text should only occupy
   the play-area ("table") portion of that row, not spill into the info
   column drawn alongside it. */
static void tui_print_3segment(WINDOW* win, int y, int pane_width,
                               const char* left, const char* center,
                               const char* right)
{ mvwprintw(win, y, 1, "%s", left);
  mvwprintw(win, y, tui_center_x(pane_width, (int)strlen(center)), "%s", center);
  int right_x = pane_width - (int)strlen(right) - 2;
  if(right_x < 1) right_x = 1;
  mvwprintw(win, y, right_x, "%s", right);
}

/* Simple char-width wrapping (no word-break awareness -- fine for the short,
   static reference text this is used for). Prints at most max_rows lines. */
static void tui_print_wrapped(WINDOW* win, int y, int max_rows, int width,
                              const char* text)
{ int len = (int)strlen(text);
  int pos = 0, row = y;

  for(int i = 0; i < max_rows && pos < len; i++, row++)
  { int chunk = (len - pos < width) ? (len - pos) : width;
    mvwprintw(win, row, 1, "%.*s", chunk, text + pos);
    pos += chunk;
  }
}

/* ========================================================================
   Status bars
   ======================================================================== */

static const char* tui_active_label(struct gamestate* gstate, PlayerID player,
                                    config_t* cfg)
{ return (gstate->current_player == player) ?
         LOCALIZED_STRING("Active", "Actif", "Activo") :
         LOCALIZED_STRING("Waiting", "En attente", "Esperando");
}

// current_player is this turn's attacker for its entire duration (attack
// through end_of_turn) -- change_current_player() only runs at the very
// end, so this doesn't need (and must not key off) turn_phase: attack_phase()
// itself flips turn_phase to DEFENSE partway through, which would otherwise
// invert this label once a human is watching mid-turn instead of only ever
// redrawing after a full AI-vs-AI play_turn() (turn_phase always DEFENSE by
// M1's single post-turn draw, so the bug never showed there).
static const char* tui_role_label(struct gamestate* gstate, PlayerID player,
                                  config_t* cfg)
{ return (gstate->current_player == player) ?
         LOCALIZED_STRING("Attacker", "Attaquant", "Atacante") :
         LOCALIZED_STRING("Defender", "Defenseur", "Defensor");
}

/* Top (Player B) and bottom (Player A) bars are laid out as mirror images of
   each other across the screen's horizontal center line: the lunas/energy
   pair always sits on the left of the "table", status/role always on the
   right, name centered -- so both bars line up vertically edge-to-edge. */
static void tui_draw_status_bars(TuiScreen* screen, struct gamestate* gstate,
                                 config_t* cfg)
{ int pane_w = getmaxx(screen->win_play);

  char left_b[32], right_b[32];
  snprintf(left_b, sizeof(left_b), "%u %s | %u %s",
           gstate->current_cash_balance[PLAYER_B],
           LOCALIZED_STRING("lunas", "lunas", "lunas"),
           gstate->current_energy[PLAYER_B],
           LOCALIZED_STRING("energy", "energie", "energia"));
  snprintf(right_b, sizeof(right_b), "%s | %s",
           tui_active_label(gstate, PLAYER_B, cfg),
           tui_role_label(gstate, PLAYER_B, cfg));

  werase(screen->win_top_status);
  wattron(screen->win_top_status, COLOR_PAIR(PAIR_STATUS_B));
  tui_print_3segment(screen->win_top_status, 0, pane_w,
                     left_b, PLAYER_NAMES[PLAYER_B], right_b);
  wattroff(screen->win_top_status, COLOR_PAIR(PAIR_STATUS_B));

  char left_a[32], right_a[32];
  snprintf(left_a, sizeof(left_a), "%u %s | %u %s",
           gstate->current_energy[PLAYER_A],
           LOCALIZED_STRING("energy", "energie", "energia"),
           gstate->current_cash_balance[PLAYER_A],
           LOCALIZED_STRING("lunas", "lunas", "lunas"));
  snprintf(right_a, sizeof(right_a), "%s | %s",
           tui_role_label(gstate, PLAYER_A, cfg),
           tui_active_label(gstate, PLAYER_A, cfg));

  werase(screen->win_bottom_status);
  wattron(screen->win_bottom_status, COLOR_PAIR(PAIR_STATUS_A));
  tui_print_3segment(screen->win_bottom_status, 0, pane_w,
                     left_a, PLAYER_NAMES[PLAYER_A], right_a);
  wattroff(screen->win_bottom_status, COLOR_PAIR(PAIR_STATUS_A));

  werase(screen->win_command);
  mvwprintw(screen->win_command, 0, 0, "> ");
}

/* tui_draw_play_area() (hand/deck-discard/combat-zone drawing for both
   players) now lives in tui_render_playarea.c -- forward-declared here
   (not part of the public tui_render.h API) since tui_draw_all() below
   still calls it. */
void tui_draw_play_area(TuiScreen* screen, struct gamestate* gstate, config_t* cfg);

/* ========================================================================
   Info column: shortcuts / message box / console
   ======================================================================== */

/* One wrapped display-line's worth of an original console message: which
   message it came from, and the [offset, offset+len) slice of it to print. */
typedef struct
{ int msg_index;
  int offset;
  int len;
} TuiConsoleSegment;

#define TUI_CONSOLE_SEG_MAX 256
#define TUI_CONSOLE_RECENT_MSGS 40

/* Wraps the most recent messages (bounded lookback, not the whole history --
   plenty for any realistic console height) into segments in chronological
   order; the caller then displays only the tail of this list that fits. */
static int tui_build_console_segments(TuiScreen* screen, int avail_w,
                                      TuiConsoleSegment* segs, int max_segs)
{ int count = 0;
  int start = screen->message_count - TUI_CONSOLE_RECENT_MSGS;
  if(start < 0) start = 0;

  for(int i = start; i < screen->message_count && count < max_segs; i++)
  { const char* msg = screen->messages[i];
    int len = (int)strlen(msg);

    if(len == 0)
    { segs[count++] = (TuiConsoleSegment)
      { i, 0, 0
      };
      continue;
    }

    int pos = 0;
    while(pos < len && count < max_segs)
    { int chunk = (len - pos < avail_w) ? (len - pos) : avail_w;
      segs[count++] = (TuiConsoleSegment)
      { i, pos, chunk
      };
      pos += chunk;
    }
  }

  return count;
}

/* Shortcut hint text depends only on the current turn phase (not on whether
   the phase's actor is human) -- keeps this rendering layer decoupled from
   PlayerConfig, matching the rest of tui_render.c (gstate+cfg only). Showing
   it during an AI turn is harmless; the player just won't use it that turn. */
static void tui_shortcuts_text(struct gamestate* gstate, config_t* cfg,
                               char* buf, size_t bufsize)
{ if(gstate->turn_phase == ATTACK)
    snprintf(buf, bufsize, "%s",
             LOCALIZED_STRING(
               "TAB=command mode | 1-7=toggle champion | Enter=play | Esc=clear | P=pass",
               "TAB=mode commande | 1-7=selectionner champion | Entree=jouer | Echap=effacer | P=passer",
               "TAB=modo comando | 1-7=alternar campeon | Enter=jugar | Esc=borrar | P=pasar"));
  else
    snprintf(buf, bufsize, "%s",
             LOCALIZED_STRING(
               "TAB=command mode | 'cham <i..>' or 'pass' to defend",
               "TAB=mode commande | 'cham <i..>' ou 'pass' pour defendre",
               "TAB=modo comando | 'cham <i..>' o 'pass' para defender"));
}

static void tui_draw_info_column(TuiScreen* screen, struct gamestate* gstate,
                                 config_t* cfg)
{ werase(screen->win_shortcuts);
  wattron(screen->win_shortcuts, COLOR_PAIR(PAIR_BORDER_SHORTCUTS));
  box(screen->win_shortcuts, 0, 0);
  mvwprintw(screen->win_shortcuts, 0, 2, " %s ",
            LOCALIZED_STRING("Shortcuts", "Raccourcis", "Atajos"));
  wattroff(screen->win_shortcuts, COLOR_PAIR(PAIR_BORDER_SHORTCUTS));

  char shortcuts_text[128];
  tui_shortcuts_text(gstate, cfg, shortcuts_text, sizeof(shortcuts_text));
  tui_print_wrapped(screen->win_shortcuts, 1, getmaxy(screen->win_shortcuts) - 2,
                    getmaxx(screen->win_shortcuts) - 2, shortcuts_text);

  werase(screen->win_msgbox);
  wattron(screen->win_msgbox, COLOR_PAIR(PAIR_BORDER_MSGBOX));
  box(screen->win_msgbox, 0, 0);
  mvwprintw(screen->win_msgbox, 0, 2, " %s ",
            LOCALIZED_STRING("Game Messages", "Messages du jeu", "Mensajes"));
  wattroff(screen->win_msgbox, COLOR_PAIR(PAIR_BORDER_MSGBOX));

  werase(screen->win_console);
  wattron(screen->win_console, COLOR_PAIR(PAIR_BORDER_CONSOLE));
  box(screen->win_console, 0, 0);
  mvwprintw(screen->win_console, 0, 2, " %s ",
            LOCALIZED_STRING("Console", "Console", "Consola"));
  wattroff(screen->win_console, COLOR_PAIR(PAIR_BORDER_CONSOLE));

  int h, w;
  getmaxyx(screen->win_console, h, w);
  int max_lines = h - 2;
  int avail_w = w - 2;
  if(avail_w < 1) avail_w = 1;

  TuiConsoleSegment segs[TUI_CONSOLE_SEG_MAX];
  int seg_count = tui_build_console_segments(screen, avail_w, segs,
                                             TUI_CONSOLE_SEG_MAX);
  int start_seg = (seg_count > max_lines) ? seg_count - max_lines : 0;
  int row = 1;

  for(int s = start_seg; s < seg_count; s++)
  { const char* msg = screen->messages[segs[s].msg_index];
    int pair = screen->message_colors[segs[s].msg_index];
    if(pair > 0) wattron(screen->win_console, COLOR_PAIR(pair));
    mvwprintw(screen->win_console, row++, 1, "%.*s",
              segs[s].len, msg + segs[s].offset);
    if(pair > 0) wattroff(screen->win_console, COLOR_PAIR(pair));
  }
}

/* ========================================================================
   Public entry points
   ======================================================================== */

static void tui_draw_too_small(TuiScreen* screen, config_t* cfg)
{ clear();
  mvprintw(0, 0, "%s (%dx%d, %s %dx%d)",
           LOCALIZED_STRING("Please enlarge the terminal",
                            "Veuillez agrandir le terminal",
                            "Por favor agrande la terminal"),
           screen->last_cols, screen->last_rows,
           LOCALIZED_STRING("minimum", "minimum", "minimo"),
           TUI_MIN_COLS, TUI_MIN_ROWS);
  refresh();
}

void tui_draw_all(TuiScreen* screen, struct gamestate* gstate, config_t* cfg)
{ if(screen->too_small)
  { tui_draw_too_small(screen, cfg);
    return;
  }

  tui_draw_status_bars(screen, gstate, cfg);
  tui_draw_play_area(screen, gstate, cfg);
  tui_draw_info_column(screen, gstate, cfg);

  wrefresh(screen->win_top_status);
  wrefresh(screen->win_play);
  wrefresh(screen->win_shortcuts);
  wrefresh(screen->win_msgbox);
  wrefresh(screen->win_console);
  wrefresh(screen->win_bottom_status);
  wrefresh(screen->win_command);
}

/* Message log, input predicates, command-line drawing, and combat-details
   rendering now live in tui_render_io.c (declared in tui_render.h). */
