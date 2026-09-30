// gui_layout.c -- see gui_layout.h.

#include "gui_layout.h"
#include "../../visibility/visible_state.h" // VIEWER_SPECTATOR

#define GUI_STATUS_BAR_H 40.0f
#define GUI_INFO_H 30.0f
#define GUI_MARGIN 12.0f
#define GUI_TOP_INFO_LIFT 9.0f      // opponent status text sits ~one x-height closer to the top
#define GUI_BOTTOM_INFO_DROP 14.0f  // own status text sits ~one ascender closer to the bottom edge
#define GUI_DICE_ROW_H 56.0f  // n-gon die + base/total text, between hand and combat zone
#define GUI_COMBAT_CARDS_W (3 * GUI_CARD_WIDTH + 2 * GUI_CARD_GAP) // widest combat row
#define GUI_BUTTON_W 110.0f
#define GUI_BUTTON_H 28.0f
#define GUI_BUTTON_GAP 8.0f
#define GUI_SEED_W 170.0f     // room reserved for the "Seed n" tag at the status bar's right end
#define GUI_COMBAT_TUCK_GAP 4.0f // vertical gap between the two seats' combat-card rows
#define GUI_SIDE_W GUI_CARD_WIDTH // deck with the discard pile stacked against it
#define GUI_TOGGLE_W 56.0f
#define GUI_TOGGLE_H 28.0f
#define GUI_LOG_FRACTION 0.19f // log side panel width as a fraction of the window...
#define GUI_LOG_MIN_W 220.0f   // ...clamped to this range
#define GUI_LOG_MAX_W 300.0f

// seat[1] (top/opponent): info label, then hand + deck/discard on the same
// row, then that seat's combat-zone row just below it.
static void layout_top_seat(float win_w, float y, GuiLayout* out)
{ out->seat[1].info = (SDL_FRect)
  { GUI_MARGIN, y, win_w - 2 * GUI_MARGIN, GUI_INFO_H
  };
  y += GUI_INFO_H;

  float hand_w = win_w - 2 * GUI_MARGIN - GUI_SIDE_W - GUI_MARGIN;
  out->seat[1].hand = (SDL_FRect)
  { GUI_MARGIN, y, hand_w, GUI_CARD_HEIGHT
  };

  float side_x = win_w - GUI_MARGIN - GUI_SIDE_W;
  out->seat[1].deck = (SDL_FRect)
  { side_x, y, GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };
  out->seat[1].discard = (SDL_FRect) // below the deck (toward the table centre)
  { side_x, y + GUI_CARD_HEIGHT, GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };
  y += GUI_CARD_HEIGHT;

  out->dice_row[1] = (SDL_FRect)
  { GUI_MARGIN, y, hand_w, GUI_DICE_ROW_H
  };
  y += GUI_DICE_ROW_H;

  out->combat_zone[1] = (SDL_FRect)
  { GUI_MARGIN, y, hand_w, GUI_CARD_HEIGHT
  };
} // layout_top_seat

// seat[0] (bottom/viewer): mirror of the top seat, built upward from
// `bottom_y` (the window's bottom edge) so both seats share the same
// hand/deck/discard geometry.
static void layout_bottom_seat(float win_w, float bottom_y, GuiLayout* out)
{ float y = bottom_y - GUI_INFO_H;
  out->seat[0].info = (SDL_FRect)
  { GUI_MARGIN, y, win_w - 2 * GUI_MARGIN, GUI_INFO_H
  };

  y -= GUI_CARD_HEIGHT;
  float hand_w = win_w - 2 * GUI_MARGIN - GUI_SIDE_W - GUI_MARGIN;
  out->seat[0].hand = (SDL_FRect)
  { GUI_MARGIN, y, hand_w, GUI_CARD_HEIGHT
  };

  float side_x = win_w - GUI_MARGIN - GUI_SIDE_W;
  out->seat[0].deck = (SDL_FRect)
  { side_x, y, GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };
  out->seat[0].discard = (SDL_FRect) // above the deck (toward the table centre)
  { side_x, y - GUI_CARD_HEIGHT, GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };

  y -= GUI_DICE_ROW_H;
  out->dice_row[0] = (SDL_FRect)
  { GUI_MARGIN, y, hand_w, GUI_DICE_ROW_H
  };
  y -= GUI_CARD_HEIGHT;
  out->combat_zone[0] = (SDL_FRect)
  { GUI_MARGIN, y, hand_w, GUI_CARD_HEIGHT
  };
} // layout_bottom_seat

// Pulls the two seats' dice-row + combat-zone blocks together, centred in the
// space between the hands (top block: dice row over cards; bottom block: cards
// over dice row), so the two rows of combat cards sit nearly against each other.
// Skipped when the window is too short to hold both blocks.
static void layout_tuck_combat(GuiLayout* out)
{ float top = out->seat[1].hand.y + out->seat[1].hand.h;
  float avail = out->seat[0].hand.y - top;
  float block_h = 2 * (GUI_DICE_ROW_H + GUI_CARD_HEIGHT) + GUI_COMBAT_TUCK_GAP;
  if(avail <= block_h)
    return;

  float y = top + (avail - block_h) / 2.0f;
  out->dice_row[1].y = y;
  y += GUI_DICE_ROW_H;
  out->combat_zone[1].y = y;
  y += GUI_CARD_HEIGHT + GUI_COMBAT_TUCK_GAP;
  out->combat_zone[0].y = y;
  y += GUI_CARD_HEIGHT;
  out->dice_row[0].y = y;
} // layout_tuck_combat

// Info area to the right of the combat cards, spanning dice row + zone.
static void layout_combat_info(GuiLayout* out)
{ for(int s = 0; s < NUM_PLAYERS; s++)
  { SDL_FRect z = out->combat_zone[s], d = out->dice_row[s];
    float x = z.x + (z.w + GUI_COMBAT_CARDS_W) / 2.0f + GUI_CARD_GAP * 2;
    float top = SDL_min(z.y, d.y), bottom = SDL_max(z.y + z.h, d.y + d.h);
    out->combat_info[s] = (SDL_FRect)
    { x, top, SDL_max(0.0f, z.x + z.w - x), bottom - top
    };
  }
} // layout_combat_info

void gui_layout_compute(float win_w, float win_h, bool log_open, GuiLayout* out)
{ float log_w = win_w * GUI_LOG_FRACTION;
  if(log_w < GUI_LOG_MIN_W)
    log_w = GUI_LOG_MIN_W;
  if(log_w > GUI_LOG_MAX_W)
    log_w = GUI_LOG_MAX_W;
  float board_w = log_open ? win_w - log_w : win_w;

  out->status_bar = (SDL_FRect)
  { 0, 0, board_w, GUI_STATUS_BAR_H
  };

  // Status bar, right to left: [log tab when closed] seed tag, then the input
  // buttons (or a status label) just left of the seed.
  float right = board_w - (log_open ? 10.0f : GUI_TOGGLE_W + GUI_MARGIN);
  out->seed_tag = (SDL_FRect)
  { right - GUI_SEED_W, 0, GUI_SEED_W, GUI_STATUS_BAR_H
  };
  float buttons_w = GUI_MAX_BUTTON_SLOTS * (GUI_BUTTON_W + GUI_BUTTON_GAP) - GUI_BUTTON_GAP;
  out->buttons = (SDL_FRect)
  { out->seed_tag.x - GUI_MARGIN - buttons_w, 0, buttons_w, GUI_STATUS_BAR_H
  };

  layout_top_seat(board_w, GUI_STATUS_BAR_H + GUI_MARGIN - GUI_TOP_INFO_LIFT, out);
  layout_bottom_seat(board_w, win_h - GUI_MARGIN + GUI_BOTTOM_INFO_DROP, out);
  layout_tuck_combat(out);
  layout_combat_info(out);

  // The message log fills whatever's left between the two combat zones --
  // derived from their already-computed rects rather than a separate
  // hardcoded budget, so it grows for free on a taller window and shrinks
  // gracefully (toward nothing) on a short one, the same as every other
  // region here.
  out->log_open = log_open;
  if(log_open)
  { out->log_panel = (SDL_FRect)
    { board_w, 0, win_w - board_w, win_h
    };
    out->log_toggle = (SDL_FRect)
    { win_w - GUI_MARGIN / 2 - GUI_TOGGLE_H, GUI_MARGIN / 2, GUI_TOGGLE_H, GUI_TOGGLE_H
    };
  }
  else
  { out->log_panel = (SDL_FRect)
    { win_w, 0, 0, win_h
    };
    out->log_toggle = (SDL_FRect)
    { win_w - GUI_MARGIN / 2 - GUI_TOGGLE_W, GUI_MARGIN / 2, GUI_TOGGLE_W, GUI_TOGGLE_H
    };
  }
} // gui_layout_compute

GuiSeatRects gui_layout_seat_rects(const GuiLayout* layout, uint8_t seat)
{ return (GuiSeatRects)
  { layout->combat_zone[seat], layout->dice_row[seat], layout->combat_info[seat]
  };
} // gui_layout_seat_rects

uint8_t gui_layout_seat_for_player(PlayerID player, PlayerID viewer)
{ if(viewer == VIEWER_SPECTATOR)
    return (uint8_t)player;
  return (uint8_t)(((int)player - (int)viewer + NUM_PLAYERS) % NUM_PLAYERS);
} // gui_layout_seat_for_player

SDL_FRect gui_layout_card_slot(SDL_FRect row, uint8_t index, uint8_t count)
{ if(count == 0)
    return (SDL_FRect)
  { row.x, row.y, 0, 0
  };

  // Natural spacing, tightened (cards overlap, later ones on top) once the
  // row is too narrow to hold `count` cards side by side.
  float step = GUI_CARD_WIDTH + GUI_CARD_GAP;
  if(count > 1 && (count - 1) * step + GUI_CARD_WIDTH > row.w)
    step = (row.w - GUI_CARD_WIDTH) / (count - 1);
  float total_w = (count - 1) * step + GUI_CARD_WIDTH;
  float start_x = row.x + (row.w - total_w) / 2.0f;
  float y = row.y + (row.h - GUI_CARD_HEIGHT) / 2.0f;

  return (SDL_FRect)
  { start_x + index * step, y, GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };
} // gui_layout_card_slot

SDL_FRect gui_layout_button_rect(SDL_FRect area, uint8_t index, uint8_t total)
{ if(total == 0)
    return (SDL_FRect)
  { area.x, area.y, 0, 0
  };

  // Right-aligned: the last button ends at the area's right edge.
  uint8_t from_right = (uint8_t)(total - 1 - index);
  float x = area.x + area.w - GUI_BUTTON_W - from_right * (GUI_BUTTON_W + GUI_BUTTON_GAP);
  return (SDL_FRect)
  { x, area.y + (area.h - GUI_BUTTON_H) / 2.0f, GUI_BUTTON_W, GUI_BUTTON_H
  };
} // gui_layout_button_rect

SDL_FRect gui_layout_overlay_area(float win_w, float win_h)
{ float w = win_w * 0.8f;
  float h = win_h * 0.6f;
  return (SDL_FRect)
  { (win_w - w) / 2.0f, (win_h - h) / 2.0f, w, h
  };
} // gui_layout_overlay_area

SDL_FRect gui_layout_grid_slot(SDL_FRect area, uint8_t index, uint8_t count)
{ (void)count;
  int cols = (int)((area.w + GUI_CARD_GAP) / (GUI_CARD_WIDTH + GUI_CARD_GAP));
  if(cols < 1)
    cols = 1;

  int col = index % cols;
  int row = index / cols;
  return (SDL_FRect)
  { area.x + col * (GUI_CARD_WIDTH + GUI_CARD_GAP),
    area.y + row * (GUI_CARD_HEIGHT + GUI_CARD_GAP),
    GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };
} // gui_layout_grid_slot

SDL_FRect gui_layout_discard_slot(SDL_FRect area, uint8_t index)
{ float w = area.w / 2.0f, h = area.h / 2.0f;
  return (SDL_FRect)
  { area.x + (index % 2) * w, area.y + (index / 2) * h, w, h
  };
} // gui_layout_discard_slot
