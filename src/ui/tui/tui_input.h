// tui_input.h
// TUI (ncurses) implementation of the UiIO seam (src/ui/shared/ui_io.h), so
// the shared interactive command logic in src/ui/interactive/ can drive a
// human turn inside the TUI the same way it drives the CLI. Only calls into
// tui_render.h's safe wrappers -- never touches <ncurses.h> directly.

#ifndef TUI_INPUT_H
#define TUI_INPUT_H

#include "../shared/ui_io.h"
#include "tui_render.h"

// Bundles the pointers the TUI's UiIO callbacks need (message/show_card_list
// only need screen+cfg; read_line also needs gstate to redraw on resize).
// Caller owns this and must keep it alive for as long as the returned UiIO
// is used (typically a stack variable for the duration of one phase handler).
typedef struct
{ TuiScreen* screen;
  struct gamestate* gstate;
  config_t* cfg;
} TuiIoContext;

UiIO tui_io_create(TuiIoContext* tctx);

#endif // TUI_INPUT_H
