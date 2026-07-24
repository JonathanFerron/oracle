/* ========================================================================
   TUI (ncurses) UiIO Backend
   ======================================================================== */

#include <stdio.h>
#include <stdarg.h>

#include "tui_input.h"
#include "../shared/ui_constants.h"
#include "../shared/localization.h"

// Routes by kind: SUCCESS/INFO are narrative action outcomes ("Played N
// champion(s)", draw/recall/cash results) and go to Game Messages; WARNING/
// ERROR are interaction feedback (validation problems) and stay in Console.
static void tui_io_message(UiIO* io, UiMsgKind kind, const char* fmt, ...)
{ TuiIoContext* tctx = (TuiIoContext*)io->ctx;
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if(kind == UI_MSG_ERROR)
    tui_add_message_colored(tctx->screen, TUI_MSG_COLOR_ERROR, "%s", buffer);
  else if(kind == UI_MSG_WARNING)
    tui_add_message_colored(tctx->screen, TUI_MSG_COLOR_WARNING, "%s", buffer);
  else if(kind == UI_MSG_SUCCESS)
    tui_add_game_message_colored(tctx->screen, TUI_MSG_COLOR_SUCCESS, "%s", buffer);
  else
    tui_add_game_message(tctx->screen, "%s", buffer);
}

// Live getch()-based line editor drawn into the command window: echoes as
// the player types, Enter completes, Esc/EOF cancels. A resize mid-edit
// relayouts and redraws the whole board before resuming.
static bool tui_io_read_line(UiIO* io, const char* prompt, char* buf, size_t bufsize)
{ TuiIoContext* tctx = (TuiIoContext*)io->ctx;
  TuiScreen* screen = tctx->screen;
  size_t len = 0;
  buf[0] = 0;

  for(;;)
  { char display[MAX_COMMAND_LEN + 80];
    snprintf(display, sizeof(display), "%s%s", prompt, buf);
    tui_draw_command_line(screen, display, true);

    int ch = tui_get_input();

    if(tui_input_is_resize(ch))
    { tui_layout(screen);
      tui_draw_all(screen, tctx->gstate, tctx->cfg);
    }
    else if(tui_input_is_enter(ch))
    { tui_draw_command_line(screen, "> ", false);
      return true;
    }
    else if(tui_input_is_escape(ch))
    { tui_draw_command_line(screen, "> ", false);
      return false;
    }
    else if(tui_input_is_backspace(ch) && len > 0)
      buf[--len] = 0;
    else if(tui_input_is_printable(ch) && len < bufsize - 1)
    { buf[len++] = (char)ch;
      buf[len] = 0;
    }
  }
}

static void tui_io_show_card_list(UiIO* io, const char* title,
                                  const uint8_t* card_indices, int count,
                                  int suggested_idx)
{ TuiIoContext* tctx = (TuiIoContext*)io->ctx;
  config_t* cfg = tctx->cfg;

  tui_add_message(tctx->screen, "%s", title);

  if(count == 0)
  { tui_add_message(tctx->screen, "  %s",
                    LOCALIZED_STRING_L(cfg->language, "(no champions)",
                                       "(aucun champion)", "(sin campeones)"));
    return;
  }

  for(int i = 0; i < count; i++)
  { char card_buf[24];
    tui_format_card(card_indices[i], card_buf, sizeof(card_buf), cfg);
    if(i == suggested_idx)
      tui_add_message_colored(tctx->screen, TUI_MSG_COLOR_SUCCESS, "  [%d] %s  %s",
                              i + 1, card_buf,
                              LOCALIZED_STRING_L(cfg->language, "<- suggested",
                                                 "<- suggere", "<- sugerido"));
    else
      tui_add_message(tctx->screen, "  [%d] %s", i + 1, card_buf);
  }
}

UiIO tui_io_create(TuiIoContext* tctx)
{ UiIO io;
  io.message = tui_io_message;
  io.read_line = tui_io_read_line;
  io.show_card_list = tui_io_show_card_list;
  io.ctx = tctx;
  return io;
}
