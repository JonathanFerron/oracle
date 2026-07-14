// tui_render.c
// ncurses window layout and drawing for stda.tui mode (Milestone 1: display only)

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "tui_render.h"
#include "../../core/game_constants.h"
#include "../../structures/card_collection.h"
#include "../../structures/deckstack.h"
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

enum
{ PAIR_DEFAULT = 1,
  PAIR_STATUS_B,
  PAIR_STATUS_A,
  PAIR_BORDER_SHORTCUTS,
  PAIR_BORDER_MSGBOX,
  PAIR_BORDER_CONSOLE,
  PAIR_CARD_RED,
  PAIR_CARD_INDIGO,
  PAIR_CARD_ORANGE
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
}

TuiScreen* tui_screen_create(void)
{ TuiScreen* screen = calloc(1, sizeof(TuiScreen));
  if(!screen) return NULL;

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  tui_setup_colors();

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

static void tui_format_card(uint8_t card_idx, char* buf, size_t bufsize)
{ const struct card* c = &fullDeck[card_idx];

  if(c->card_type == CHAMPION_CARD)
    snprintf(buf, bufsize, "d%u+%u %u %s", c->defense_dice, c->attack_base,
             c->cost, CHAMPION_SPECIES_ABBR[c->species]);
  else if(c->card_type == DRAW_CARD)
    snprintf(buf, bufsize, "Pig%u/Rap%u %u", c->draw_num, c->choose_num, c->cost);
  else
    snprintf(buf, bufsize, "Echange%u", c->exchange_cash);
}

static int tui_color_pair_for_card(uint8_t card_idx)
{ const struct card* c = &fullDeck[card_idx];

  if(c->card_type != CHAMPION_CARD) return PAIR_DEFAULT;
  if(c->color == COLOR_RED) return PAIR_CARD_RED;
  if(c->color == COLOR_INDIGO) return PAIR_CARD_INDIGO;
  if(c->color == COLOR_ORANGE) return PAIR_CARD_ORANGE;
  return PAIR_DEFAULT;
}

/* ========================================================================
   Layout helpers
   ======================================================================== */

static int tui_center_x(int win_width, int content_width)
{ int x = (win_width - content_width) / 2;
  return (x < 1) ? 1 : x;
}

static void tui_print_centered(WINDOW* win, int y, const char* text)
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

static const char* tui_role_label(struct gamestate* gstate, PlayerID player,
                                  config_t* cfg)
{ bool is_attacker = (gstate->turn_phase == ATTACK) ==
                     (gstate->current_player == player);
  return is_attacker ?
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

/* ========================================================================
   Play area: hand / deck-discard / combat zone
   ======================================================================== */

/* A single formatted, colored card (or index+card) label ready to be laid
   out in a centered row -- see tui_draw_card_row(). */
typedef struct
{ char text[24];
  int color_pair;
} TuiCardCell;

/* Lays out pre-formatted cells centered on each row, wrapping to further rows
   (each independently centered) as needed; the last available row is
   truncated with "..." if cells remain. Returns rows actually used. */
static int tui_draw_card_row(WINDOW* win, int y, int max_rows, int gap,
                             const TuiCardCell* cells, int count)
{ int w = getmaxx(win);
  int i = 0, row = y, rows_used = 0;

  while(i < count && rows_used < max_rows)
  { int total = 0, j = i;
    while(j < count)
    { int need = (int)strlen(cells[j].text) + gap;
      if(total + need > w - 2 && j > i) break;
      total += need;
      j++;
    }

    bool truncated = (rows_used == max_rows - 1) && (j < count);
    int x = tui_center_x(w, total - gap + (truncated ? 4 : 0));

    for(int k = i; k < j; k++)
    { wattron(win, COLOR_PAIR(cells[k].color_pair));
      mvwprintw(win, row, x, "%s", cells[k].text);
      wattroff(win, COLOR_PAIR(cells[k].color_pair));
      x += (int)strlen(cells[k].text) + gap;
    }

    if(truncated) mvwprintw(win, row, x, "...");

    i = j;
    row++;
    rows_used++;
  }

  return rows_used;
}

static int tui_draw_hand(WINDOW* win, int y, int max_rows, PlayerID player,
                         struct gamestate* gstate, config_t* cfg, bool faceup)
{ const Hand* hand = &gstate->hand[player];
  char label[64];

  snprintf(label, sizeof(label), "%s %s (%d)%s",
           LOCALIZED_STRING("Hand", "Main", "Mano"), PLAYER_NAMES[player],
           hand->size,
           faceup ? "" :
           LOCALIZED_STRING(" [Hidden]", " Cachees", " Ocultas"));
  tui_print_centered(win, y, label);
  if(!faceup || max_rows <= 1) return 1;

  /* Hand is capped at 7 cards by discard_to_7_cards() (end_of_turn()), and
     M1 only ever renders after a full turn completes -- so 7 is a real
     bound here, not a defensive margin. */
  TuiCardCell cells[7];
  int count = 0;

  for(uint8_t i = 0; i < hand->size && count < 7; i++)
  { uint8_t idx = Hand_get(hand, i);
    char buf[20];
    tui_format_card(idx, buf, sizeof(buf));
    snprintf(cells[count].text, sizeof(cells[count].text), "[%d]%s", i + 1, buf);
    cells[count].color_pair = tui_color_pair_for_card(idx);
    count++;
  }

  return 1 + tui_draw_card_row(win, y + 1, max_rows - 1, 3, cells, count);
}

/* Player A's hand is shown as a vertical stack (one card per row, matching
   the target layout) instead of the horizontal wrapping row used elsewhere.
   All entries share one x position (centered on the widest entry) so the
   stack reads as a clean column rather than each line being independently
   (and raggedly) centered. Returns rows used (label + card rows shown). */
static int tui_draw_hand_vertical(WINDOW* win, int y, int max_rows,
                                  PlayerID player, struct gamestate* gstate,
                                  config_t* cfg)
{ const Hand* hand = &gstate->hand[player];
  char label[64];

  snprintf(label, sizeof(label), "%s %s (%d)",
           LOCALIZED_STRING("Hand", "Main", "Mano"), PLAYER_NAMES[player],
           hand->size);
  tui_print_centered(win, y, label);

  int rows_avail = max_rows - 1;
  if(rows_avail < 1) return 1;

  /* Hand is capped at 7 cards by discard_to_7_cards() (end_of_turn()), and
     M1 only ever renders after a full turn completes -- so 7 is a real
     bound here, not a defensive margin. */
  int n = hand->size;
  bool truncated = false;
  if(n > 7) n = 7;
  if(n > rows_avail)
  { n = rows_avail;
    truncated = true;
  }

  char entries[7][24];
  int pairs[7];
  int max_w = 0;

  for(int i = 0; i < n; i++)
  { uint8_t idx = Hand_get(hand, i);
    char buf[20];
    tui_format_card(idx, buf, sizeof(buf));
    snprintf(entries[i], sizeof(entries[i]), "[%d]%s", i + 1, buf);
    pairs[i] = tui_color_pair_for_card(idx);
    int len = (int)strlen(entries[i]);
    if(len > max_w) max_w = len;
  }

  int x = tui_center_x(getmaxx(win), max_w);

  for(int i = 0; i < n; i++)
  { wattron(win, COLOR_PAIR(pairs[i]));
    mvwprintw(win, y + 1 + i, x, "%s", entries[i]);
    wattroff(win, COLOR_PAIR(pairs[i]));
  }

  if(truncated) mvwprintw(win, y + n, x + max_w + 1, "...");

  return 1 + n;
}

/* Grows a compact grid of a player's discard pile from one corner of the
   table toward the vertical middle, one card per row; once a column fills
   up, a new column starts alongside it (moving toward the horizontal
   center for Player B, away from it for Player A). `row_step`/`col_step`
   carry the direction (+1/-1), so this one function serves both mirrored
   layouts. Stops once a new column would cross `col_limit_*` rather than
   overlap the centered hand/deck/combat-zone content in the middle. */
static void tui_draw_discard_column(WINDOW* win, PlayerID player,
                                    struct gamestate* gstate,
                                    int row_start, int row_step,
                                    int rows_per_col, int col_start,
                                    int col_step, int col_limit_min,
                                    int col_limit_max)
{ if(rows_per_col < 1) return;

  const Discard* discard = &gstate->discard[player];
  int row = row_start, col = col_start, used_in_col = 0;

  for(uint8_t i = 0; i < discard->size; i++)
  { if(col < col_limit_min || col > col_limit_max) break;

    uint8_t idx = Discard_get(discard, i);
    char buf[20];
    tui_format_card(idx, buf, sizeof(buf));
    int pair = tui_color_pair_for_card(idx);
    wattron(win, COLOR_PAIR(pair));
    mvwprintw(win, row, col, "%s", buf);
    wattroff(win, COLOR_PAIR(pair));

    row += row_step;
    used_in_col++;
    if(used_in_col >= rows_per_col)
    { col += col_step;
      row = row_start;
      used_in_col = 0;
    }
  }
}

static void tui_draw_deck_discard(WINDOW* win, int y, PlayerID player,
                                  struct gamestate* gstate, config_t* cfg)
{ int deck_size = gstate->deck[player].top + 1;
  int discard_size = gstate->discard[player].size;
  char label[64];

  snprintf(label, sizeof(label), "%s (%d)   %s (%d)",
           LOCALIZED_STRING("Deck", "Paquet", "Mazo"), deck_size,
           LOCALIZED_STRING("Discard", "Discarte", "Descarte"), discard_size);
  tui_print_centered(win, y, label);
}

static int tui_draw_combat_zone(WINDOW* win, int y, int max_rows,
                                PlayerID player, struct gamestate* gstate,
                                config_t* cfg)
{ const CombatZone* zone = &gstate->combat_zone[player];
  char label[64];

  snprintf(label, sizeof(label), "%s %s (%d):",
           LOCALIZED_STRING("Combat zone", "Zone combat", "Zona combate"),
           PLAYER_NAMES[player], zone->size);
  tui_print_centered(win, y, label);
  if(zone->size == 0 || max_rows <= 1) return 1;

  TuiCardCell cells[3];
  int count = 0;

  for(uint8_t i = 0; i < zone->size; i++)
  { uint8_t idx = CombatZone_get(zone, i);
    tui_format_card(idx, cells[count].text, sizeof(cells[count].text));
    cells[count].color_pair = tui_color_pair_for_card(idx);
    count++;
  }

  return 1 + tui_draw_card_row(win, y + 1, max_rows - 1, 2, cells, count);
}

#define TUI_DISCARD_COL_WIDTH 13

/* Player B's hand/deck/discard sit right under its status bar (with a blank
   separator row); its combat zone is pushed down to sit right above the
   "-- combat zone --" divider, so both players' combat zones cluster near
   the screen's vertical middle -- mirroring Player A's block below. Its full
   discard pile grows as a column from the top-left corner downward, adding
   further columns to the right as each fills, independent of (and never
   crossing into) that centered content. */
static void tui_draw_player_b_block(WINDOW* win, int half_h,
                                    struct gamestate* gstate, config_t* cfg)
{ int top_blank = 1;
  int hand_rows = tui_draw_hand(win, top_blank, 2, PLAYER_B, gstate, cfg, false);
  int deck_row = top_blank + hand_rows;
  tui_draw_deck_discard(win, deck_row, PLAYER_B, gstate, cfg);

  int combat_reserved = 2; /* label + up to 1 card row */
  int combat_top = half_h - combat_reserved;
  int min_combat_top = deck_row + 2; /* leave a gap even on short screens */
  if(combat_top < min_combat_top) combat_top = min_combat_top;
  tui_draw_combat_zone(win, combat_top, half_h - combat_top,
                       PLAYER_B, gstate, cfg);

  int pane_w = getmaxx(win);
  tui_draw_discard_column(win, PLAYER_B, gstate,
                          top_blank, +1, half_h - top_blank,
                          1, TUI_DISCARD_COL_WIDTH,
                          1, pane_w / 2 - TUI_DISCARD_COL_WIDTH);
}

/* Mirror of the above: combat zone sits right below the divider (near the
   middle); deck/discard + hand are pushed down to sit right above Player A's
   status bar, with a blank separator row between hand and that bar. Hand's
   row budget is computed dynamically (vs. combat zone's minimum need) since
   the vertical stack can take many more rows than a single wrapped line
   would. Its discard pile mirrors Player B's: grows from the bottom-right
   corner upward, adding columns to the left as each fills. */
static void tui_draw_player_a_block(WINDOW* win, int half_h, int total_h,
                                    struct gamestate* gstate, config_t* cfg)
{ int bottom_fixed = 2; /* deck/discard row + blank row above the status bar */
  int min_combat_rows = 2;
  int hand_rows_wanted = gstate->hand[PLAYER_A].size + 1; /* label + 1/card */
  int avail_for_hand = (total_h - bottom_fixed) - (half_h + min_combat_rows);
  int hand_rows = (hand_rows_wanted < avail_for_hand) ?
                  hand_rows_wanted : avail_for_hand;
  if(hand_rows < 1) hand_rows = 1;

  int bottom_reserved = bottom_fixed + hand_rows;
  int combat_max_rows = (total_h - bottom_reserved) - half_h;
  if(combat_max_rows < 1) combat_max_rows = 1;
  tui_draw_combat_zone(win, half_h, combat_max_rows, PLAYER_A, gstate, cfg);

  int deck_row = total_h - bottom_reserved;
  tui_draw_deck_discard(win, deck_row, PLAYER_A, gstate, cfg);
  tui_draw_hand_vertical(win, deck_row + 1, hand_rows, PLAYER_A, gstate, cfg);
  /* row (total_h - 1) is intentionally left blank, just above the status bar */

  int pane_w = getmaxx(win);
  int row_start = total_h - 2; /* one row above the blank separator */
  tui_draw_discard_column(win, PLAYER_A, gstate,
                          row_start, -1, row_start - (half_h + 1) + 1,
                          pane_w - 1 - TUI_DISCARD_COL_WIDTH, -TUI_DISCARD_COL_WIDTH,
                          pane_w / 2 + TUI_DISCARD_COL_WIDTH, pane_w - 2);
}

static void tui_draw_play_area(TuiScreen* screen, struct gamestate* gstate,
                               config_t* cfg)
{ WINDOW* win = screen->win_play;
  werase(win);

  int h = getmaxy(win);
  int half = h / 2;

  tui_draw_player_b_block(win, half, gstate, cfg);

  char divider[32];
  snprintf(divider, sizeof(divider), "-- %s --",
           LOCALIZED_STRING("combat zone", "zone combat", "zona de combate"));
  tui_print_centered(win, half, divider);

  tui_draw_player_a_block(win, half + 1, h, gstate, cfg);
}

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

static void tui_draw_info_column(TuiScreen* screen, config_t* cfg)
{ werase(screen->win_shortcuts);
  wattron(screen->win_shortcuts, COLOR_PAIR(PAIR_BORDER_SHORTCUTS));
  box(screen->win_shortcuts, 0, 0);
  mvwprintw(screen->win_shortcuts, 0, 2, " %s ",
            LOCALIZED_STRING("Shortcuts", "Raccourcis", "Atajos"));
  wattroff(screen->win_shortcuts, COLOR_PAIR(PAIR_BORDER_SHORTCUTS));

  char shortcuts_text[128];
  snprintf(shortcuts_text, sizeof(shortcuts_text), "%s %s",
           LOCALIZED_STRING("(context sensitive - M2)",
                            "(sensible au contexte - M2)",
                            "(sensible al contexto - M2)"),
           LOCALIZED_STRING("TAB to toggle play/command modes (not yet active)",
                            "TAB pour basculer jeu/commande (pas encore actif)",
                            "TAB para alternar jugar/comando (aun no activo)"));
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
    mvwprintw(screen->win_console, row++, 1, "%.*s",
              segs[s].len, msg + segs[s].offset);
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
  tui_draw_info_column(screen, cfg);

  wrefresh(screen->win_top_status);
  wrefresh(screen->win_play);
  wrefresh(screen->win_shortcuts);
  wrefresh(screen->win_msgbox);
  wrefresh(screen->win_console);
  wrefresh(screen->win_bottom_status);
  wrefresh(screen->win_command);
}

void tui_add_message(TuiScreen* screen, const char* format, ...)
{ char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if(screen->message_count >= TUI_MAX_MESSAGES)
  { free(screen->messages[0]);
    memmove(screen->messages, screen->messages + 1,
            (TUI_MAX_MESSAGES - 1) * sizeof(char*));
    screen->message_count--;
  }

  screen->messages[screen->message_count++] = strdup(buffer);
}

int tui_get_input(void)
{ return getch();
}

bool tui_input_is_quit(int ch)
{ return ch == 'q' || ch == 'Q';
}

bool tui_input_is_resize(int ch)
{ return ch == KEY_RESIZE;
}
