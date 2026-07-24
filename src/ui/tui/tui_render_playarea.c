// tui_render_playarea.c
// Play-area drawing (hand / deck-discard / combat zone for both players) --
// split out of tui_render.c to keep both files under the project's
// file-size guideline (mirrors the cli_display.c / cli_action_display.c
// split: core layout/status vs. the larger board-content drawing).

#include <string.h>
#include <stdio.h>

#include "tui_render.h"
#include "../../core/game_constants.h"
#include "../../structures/card_collection.h"
#include "../../structures/deckstack.h"
#include "../shared/localization.h"

/* Same ncurses.h/COLOR_RED discipline as tui_render.c -- see that file's
   top-of-file comment for why. */
#include <ncurses.h>
#undef COLOR_RED

/* Must match the PAIR_* enum in tui_render.c (color pairs are initialized
   there via tui_setup_colors(); this file only needs the card-color subset). */
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

/* Defined in tui_render.c; shared here for centering hand/discard/combat-
   zone content. Not part of the public tui_render.h API. */
int tui_center_x(int win_width, int content_width);
void tui_print_centered(WINDOW* win, int y, const char* text);

static int tui_color_pair_for_card(uint8_t card_idx)
{ const struct card* c = &fullDeck[card_idx];

  if(c->card_type != CHAMPION_CARD) return PAIR_DEFAULT;
  if(c->color == COLOR_RED) return PAIR_CARD_RED;
  if(c->color == COLOR_INDIGO) return PAIR_CARD_INDIGO;
  if(c->color == COLOR_ORANGE) return PAIR_CARD_ORANGE;
  return PAIR_DEFAULT;
}

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

void tui_draw_play_area(TuiScreen* screen, struct gamestate* gstate,
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
