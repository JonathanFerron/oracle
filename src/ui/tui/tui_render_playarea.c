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

/* Must match the PAIR_* enum in tui_render.c exactly, including the unused-
   here entries (PAIR_MSG_*), to keep the ordinal values in lockstep -- color
   pairs are initialized once in tui_setup_colors() there; this file just
   needs the numeric IDs to reference them. */
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
  PAIR_MSG_WARNING,
  PAIR_LUNA,
  PAIR_CARD_GREEN
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

/* Detailed CLI-style formatting -- used for the hand and combat zone
   (roomier areas), mirroring cli_display.c's display_player_hand(): full
   species name + dice/cost for champions, "Draw N (Lc)" for draw cards,
   "Exchange for N lunas (Lc)" for cash cards. The compact tui_format_card()
   (tui_render.c) stays the board form used in the tighter discard grid. */
static void tui_format_card_detailed(uint8_t card_idx, char* buf, size_t bufsize,
                                     config_t* cfg)
{ const struct card* c = &fullDeck[card_idx];

  if(c->card_type == CHAMPION_CARD)
    snprintf(buf, bufsize, "%s (D%u+%u, L%u)",
             CHAMPION_SPECIES_NAMES[c->species], c->defense_dice,
             c->attack_base, c->cost);
  else if(c->card_type == DRAW_CARD)
    snprintf(buf, bufsize, "%s %u (L%u)",
             LOCALIZED_STRING("Draw", "Piocher", "Robar"), c->draw_num, c->cost);
  else
    snprintf(buf, bufsize, "%s %u %s (L%u)",
             LOCALIZED_STRING("Exchange for", "Echanger pour", "Cambiar por"),
             c->exchange_cash, LOCALIZED_STRING("lunas", "lunas", "lunas"),
             c->cost);
}

/* Draws the same text tui_format_card_detailed() would produce, but as
   colored segments: champion name (or draw-card label) in its own color,
   luna cost always cyan -- mirrors the CLI's luna-cost coloring. */
static void tui_draw_card_detailed(WINDOW* win, int y, int x, uint8_t card_idx,
                                   config_t* cfg)
{ const struct card* c = &fullDeck[card_idx];
  char seg[32];
  int cx = x;

  if(c->card_type == CHAMPION_CARD)
  { int pair = tui_color_pair_for_card(card_idx);
    snprintf(seg, sizeof(seg), "%s", CHAMPION_SPECIES_NAMES[c->species]);
    wattron(win, COLOR_PAIR(pair));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(pair));
    cx += (int)strlen(seg);

    snprintf(seg, sizeof(seg), " (D%u+%u, L", c->defense_dice, c->attack_base);
    mvwprintw(win, y, cx, "%s", seg);
    cx += (int)strlen(seg);

    snprintf(seg, sizeof(seg), "%u", c->cost);
    wattron(win, COLOR_PAIR(PAIR_LUNA));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_LUNA));
    cx += (int)strlen(seg);

    mvwprintw(win, y, cx, ")");
  }
  else if(c->card_type == DRAW_CARD)
  { snprintf(seg, sizeof(seg), "%s %u",
             LOCALIZED_STRING("Draw", "Piocher", "Robar"), c->draw_num);
    wattron(win, COLOR_PAIR(PAIR_CARD_GREEN));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_CARD_GREEN));
    cx += (int)strlen(seg);

    mvwprintw(win, y, cx, " (L");
    cx += 3;

    snprintf(seg, sizeof(seg), "%u", c->cost);
    wattron(win, COLOR_PAIR(PAIR_LUNA));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_LUNA));
    cx += (int)strlen(seg);

    mvwprintw(win, y, cx, ")");
  }
  else // CASH_CARD
  { snprintf(seg, sizeof(seg), "%s %u %s",
             LOCALIZED_STRING("Exchange for", "Echanger pour", "Cambiar por"),
             c->exchange_cash, LOCALIZED_STRING("lunas", "lunas", "lunas"));
    wattron(win, A_DIM);
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, A_DIM);
    cx += (int)strlen(seg);

    mvwprintw(win, y, cx, " (L");
    cx += 3;

    snprintf(seg, sizeof(seg), "%u", c->cost);
    wattron(win, COLOR_PAIR(PAIR_LUNA));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_LUNA));
    cx += (int)strlen(seg);

    mvwprintw(win, y, cx, ")");
  }
}

/* Draws the compact tui_format_card() text (tui_render.c) as two-tone
   segments instead of one flat color -- luna cost always cyan, with the
   champion-color/draw-green/dim-cash treatment applied to the rest of the
   text -- matching the CLI's luna-cost coloring in this denser discard-grid
   form. Fixed-width discard columns (TUI_DISCARD_COL_WIDTH) already comfortably
   fit every card type's text, so unlike the detailed form above this doesn't
   need a companion measuring function. */
static void tui_draw_card_compact(WINDOW* win, int y, int x, uint8_t card_idx,
                                  config_t* cfg)
{ const struct card* c = &fullDeck[card_idx];
  char seg[16];
  int cx = x;

  if(c->card_type == CHAMPION_CARD)
  { snprintf(seg, sizeof(seg), "d%u+%u ", c->defense_dice, c->attack_base);
    mvwprintw(win, y, cx, "%s", seg);
    cx += (int)strlen(seg);

    snprintf(seg, sizeof(seg), "%u ", c->cost);
    wattron(win, COLOR_PAIR(PAIR_LUNA));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_LUNA));
    cx += (int)strlen(seg);

    int pair = tui_color_pair_for_card(card_idx);
    wattron(win, COLOR_PAIR(pair));
    mvwprintw(win, y, cx, "%s", CHAMPION_SPECIES_ABBR[c->species]);
    wattroff(win, COLOR_PAIR(pair));
  }
  else if(c->card_type == DRAW_CARD)
  { snprintf(seg, sizeof(seg), "%s%u/%s%u ",
             LOCALIZED_STRING("Dr", "Pio", "Rob"), c->draw_num,
             LOCALIZED_STRING("Rc", "Rap", "Rec"), c->choose_num);
    wattron(win, COLOR_PAIR(PAIR_CARD_GREEN));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_CARD_GREEN));
    cx += (int)strlen(seg);

    snprintf(seg, sizeof(seg), "%u", c->cost);
    wattron(win, COLOR_PAIR(PAIR_LUNA));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_LUNA));
  }
  else // CASH_CARD
  { snprintf(seg, sizeof(seg), "%s%u ", LOCALIZED_STRING("Ex", "Ech", "Cam"),
             c->exchange_cash);
    wattron(win, A_DIM);
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, A_DIM);
    cx += (int)strlen(seg);

    snprintf(seg, sizeof(seg), "%u", c->cost);
    wattron(win, COLOR_PAIR(PAIR_LUNA));
    mvwprintw(win, y, cx, "%s", seg);
    wattroff(win, COLOR_PAIR(PAIR_LUNA));
  }
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

  // No PLAYER_NAMES[player] here -- top/bottom status bars already say whose
  // hand this is, so "Hand PLAYER A (6)" was redundant; just "Hand (6)".
  snprintf(label, sizeof(label), "%s (%d)%s",
           LOCALIZED_STRING("Hand", "Main", "Mano"), hand->size,
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
    tui_format_card(idx, buf, sizeof(buf), cfg);
    snprintf(cells[count].text, sizeof(cells[count].text), "[%d] %s", i + 1, buf);
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

  // No PLAYER_NAMES[player] here -- see tui_draw_hand()'s comment above.
  snprintf(label, sizeof(label), "%s (%d)",
           LOCALIZED_STRING("Hand", "Main", "Mano"), hand->size);
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

  // Detailed CLI-style format (roomy enough here, unlike the discard grid) --
  // measure each "[i] <detail>" entry's plain width first (tui_format_card_
  // detailed()) so the whole column can share one centered x position, then
  // draw the index prefix plain and the card detail as colored segments.
  char details[7][48];
  int widths[7];
  int max_w = 0;

  for(int i = 0; i < n; i++)
  { uint8_t idx = Hand_get(hand, i);
    char card_buf[32];
    tui_format_card_detailed(idx, card_buf, sizeof(card_buf), cfg);
    snprintf(details[i], sizeof(details[i]), "[%d] %s", i + 1, card_buf);
    widths[i] = (int)strlen(details[i]);
    if(widths[i] > max_w) max_w = widths[i];
  }

  int x = tui_center_x(getmaxx(win), max_w);

  for(int i = 0; i < n; i++)
  { uint8_t idx = Hand_get(hand, i);
    char prefix[8];
    snprintf(prefix, sizeof(prefix), "[%d] ", i + 1);
    mvwprintw(win, y + 1 + i, x, "%s", prefix);
    tui_draw_card_detailed(win, y + 1 + i, x + (int)strlen(prefix), idx, cfg);
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
                                    struct gamestate* gstate, config_t* cfg,
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
    tui_draw_card_compact(win, row, col, idx, cfg);

    row += row_step;
    used_in_col++;
    if(used_in_col >= rows_per_col)
    { col += col_step;
      row = row_start;
      used_in_col = 0;
    }
  }
}

/* Prints a short label flush to the left or right edge of the window --
   used to tuck the Deck/Discard counts into opposite corners of a row
   (rather than centering them, which is now reserved for the Hand label
   sharing that same row). */
static void tui_print_corner(WINDOW* win, int y, bool left_aligned,
                             const char* text)
{ if(left_aligned)
    mvwprintw(win, y, 1, "%s", text);
  else
  { int x = getmaxx(win) - (int)strlen(text) - 1;
    if(x < 1) x = 1;
    mvwprintw(win, y, x, "%s", text);
  }
}

static void tui_draw_deck_label(WINDOW* win, int y, bool left_aligned,
                                PlayerID player, struct gamestate* gstate,
                                config_t* cfg)
{ int deck_size = gstate->deck[player].top + 1;
  char label[32];
  snprintf(label, sizeof(label), "%s (%d)",
           LOCALIZED_STRING("Deck", "Paquet", "Mazo"), deck_size);
  tui_print_corner(win, y, left_aligned, label);
}

static void tui_draw_discard_label(WINDOW* win, int y, bool left_aligned,
                                   PlayerID player, struct gamestate* gstate,
                                   config_t* cfg)
{ int discard_size = gstate->discard[player].size;
  char label[32];
  snprintf(label, sizeof(label), "%s (%d)",
           LOCALIZED_STRING("Discard", "Discarte", "Descarte"), discard_size);
  tui_print_corner(win, y, left_aligned, label);
}

/* Draws up to `max_rows` combat-zone champions stacked vertically, one per
   row, tucked right against the "-- combat zone --" divider and growing
   further away from it (toward the owning player's hand) as more champions
   are added. No more "Combat zone PLAYER x (n):" label -- the table side and
   the divider already make whose zone this is obvious. `direction` is -1 for
   Player B (grows upward, toward B's hand above the divider) and +1 for
   Player A (grows downward, toward A's hand below it). */
static int tui_draw_combat_zone_stack(WINDOW* win, int edge_row, int direction,
                                      int max_rows, PlayerID player,
                                      struct gamestate* gstate, config_t* cfg)
{ const CombatZone* zone = &gstate->combat_zone[player];
  int n = zone->size;
  if(n > max_rows) n = max_rows;
  if(n > 3) n = 3; /* CombatZone cap; guard even if max_rows allowed more */

  for(int i = 0; i < n; i++)
  { uint8_t idx = CombatZone_get(zone, i);
    char buf[40];
    tui_format_card_detailed(idx, buf, sizeof(buf), cfg);
    int row = edge_row + i * direction;
    int x = tui_center_x(getmaxx(win), (int)strlen(buf));
    tui_draw_card_detailed(win, row, x, idx, cfg);
  }

  return n;
}

#define TUI_DISCARD_COL_WIDTH 13

/* Player B's Discard count is tucked into the pane's top-left corner --
   right where its card grid actually starts, one row below -- and its Deck
   count into the top-right corner, sharing the row with B's centered Hand
   label. Its combat zone is tucked right against the "-- combat zone --"
   divider, growing upward toward B's hand as more champions are added, so
   both players' zones cluster near the screen's vertical middle -- mirroring
   Player A's block below. Its full discard pile grows as a column from the
   top-left corner downward, adding further columns to the right as each
   fills, independent of (and never crossing into) that centered content. */
static void tui_draw_player_b_block(WINDOW* win, int half_h,
                                    struct gamestate* gstate, config_t* cfg)
{ int header_row = 1;
  int hand_rows = tui_draw_hand(win, header_row, 2, PLAYER_B, gstate, cfg, false);
  tui_draw_discard_label(win, header_row, true, PLAYER_B, gstate, cfg);
  tui_draw_deck_label(win, header_row, false, PLAYER_B, gstate, cfg);

  int content_top = header_row + hand_rows;

  int combat_avail = half_h - content_top;
  if(combat_avail > 3) combat_avail = 3;
  if(combat_avail < 0) combat_avail = 0;
  tui_draw_combat_zone_stack(win, half_h - 1, -1, combat_avail, PLAYER_B, gstate, cfg);

  int pane_w = getmaxx(win);
  tui_draw_discard_column(win, PLAYER_B, gstate, cfg,
                          content_top, +1, half_h - content_top,
                          1, TUI_DISCARD_COL_WIDTH,
                          1, pane_w / 2 - TUI_DISCARD_COL_WIDTH);
}

/* Mirror of the above: Discard's count is tucked into the pane's
   bottom-right corner -- right where its card grid actually starts, one row
   above -- and Deck's into the bottom-left corner, on what used to be a
   blank spacer row just above Player A's status bar. Combat zone sits right
   below the divider, growing downward toward A's hand as more champions are
   added; the hand itself is pushed down to sit right above that corner-label
   row. Hand's row budget is computed dynamically (vs. combat zone's minimum
   need) since the vertical stack can take many more rows than a single
   wrapped line would. Its discard pile mirrors Player B's: grows from the
   bottom-right corner upward, adding columns to the left as each fills. */
static void tui_draw_player_a_block(WINDOW* win, int half_h, int total_h,
                                    struct gamestate* gstate, config_t* cfg)
{ int bottom_fixed = 1; /* corner Deck/Discard label row */
  int min_combat_rows = 1;
  int hand_rows_wanted = gstate->hand[PLAYER_A].size + 1; /* label + 1/card */
  int avail_for_hand = (total_h - bottom_fixed) - (half_h + min_combat_rows);
  int hand_rows = (hand_rows_wanted < avail_for_hand) ?
                  hand_rows_wanted : avail_for_hand;
  if(hand_rows < 1) hand_rows = 1;

  int bottom_reserved = bottom_fixed + hand_rows;
  int combat_avail = (total_h - bottom_reserved) - half_h;
  if(combat_avail > 3) combat_avail = 3;
  if(combat_avail < 0) combat_avail = 0;
  tui_draw_combat_zone_stack(win, half_h + 1, +1, combat_avail, PLAYER_A, gstate, cfg);

  int hand_row = total_h - bottom_reserved;
  tui_draw_hand_vertical(win, hand_row, hand_rows, PLAYER_A, gstate, cfg);

  int corner_row = total_h - 1;
  tui_draw_deck_label(win, corner_row, true, PLAYER_A, gstate, cfg);
  tui_draw_discard_label(win, corner_row, false, PLAYER_A, gstate, cfg);

  int pane_w = getmaxx(win);
  int row_start = total_h - 2;
  tui_draw_discard_column(win, PLAYER_A, gstate, cfg,
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
