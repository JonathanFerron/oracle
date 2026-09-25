// gui_layout.c -- see gui_layout.h.

#include "gui_layout.h"
#include "../../visibility/visible_state.h" // VIEWER_SPECTATOR

#define GUI_STATUS_BAR_H 40.0f
#define GUI_ACTION_BAR_H 44.0f
#define GUI_INFO_H 24.0f
#define GUI_MARGIN 12.0f
#define GUI_BUTTON_W 120.0f
#define GUI_SIDE_W (2 * GUI_CARD_WIDTH + GUI_CARD_GAP) // deck + discard, side by side

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
  out->seat[1].discard = (SDL_FRect)
  { side_x + GUI_CARD_WIDTH + GUI_CARD_GAP, y,
    GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };
  y += GUI_CARD_HEIGHT + GUI_CARD_GAP;

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
  out->seat[0].discard = (SDL_FRect)
  { side_x + GUI_CARD_WIDTH + GUI_CARD_GAP, y,
    GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };

  y -= GUI_CARD_GAP + GUI_CARD_HEIGHT;
  out->combat_zone[0] = (SDL_FRect)
  { GUI_MARGIN, y, hand_w, GUI_CARD_HEIGHT
  };
} // layout_bottom_seat

void gui_layout_compute(float win_w, float win_h, GuiLayout* out)
{ out->status_bar = (SDL_FRect)
  { 0, 0, win_w, GUI_STATUS_BAR_H
  };
  out->action_bar = (SDL_FRect)
  { 0, GUI_STATUS_BAR_H, win_w, GUI_ACTION_BAR_H
  };

  layout_top_seat(win_w, GUI_STATUS_BAR_H + GUI_ACTION_BAR_H + GUI_MARGIN, out);
  layout_bottom_seat(win_w, win_h - GUI_MARGIN, out);

  // The message log fills whatever's left between the two combat zones --
  // derived from their already-computed rects rather than a separate
  // hardcoded budget, so it grows for free on a taller window and shrinks
  // gracefully (toward nothing) on a short one, the same as every other
  // region here.
  float log_top = out->combat_zone[1].y + GUI_CARD_HEIGHT + GUI_MARGIN;
  float log_bottom = out->combat_zone[0].y - GUI_MARGIN;
  out->log_panel = (SDL_FRect)
  { GUI_MARGIN, log_top, win_w - 2 * GUI_MARGIN, log_bottom - log_top
  };
} // gui_layout_compute

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

  float total_w = count * GUI_CARD_WIDTH + (count - 1) * GUI_CARD_GAP;
  float start_x = row.x + (row.w - total_w) / 2.0f;
  float y = row.y + (row.h - GUI_CARD_HEIGHT) / 2.0f;

  return (SDL_FRect)
  { start_x + index * (GUI_CARD_WIDTH + GUI_CARD_GAP), y, GUI_CARD_WIDTH, GUI_CARD_HEIGHT
  };
} // gui_layout_card_slot

SDL_FRect gui_layout_button_rect(SDL_FRect bar, uint8_t index, uint8_t total)
{ if(total == 0)
    return (SDL_FRect)
  { bar.x, bar.y, 0, 0
  };

  float y = bar.y + (bar.h - (GUI_ACTION_BAR_H - 2 * GUI_MARGIN)) / 2.0f;
  float h = GUI_ACTION_BAR_H - 2 * GUI_MARGIN;
  return (SDL_FRect)
  { bar.x + GUI_MARGIN + index * (GUI_BUTTON_W + GUI_MARGIN), y, GUI_BUTTON_W, h
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
