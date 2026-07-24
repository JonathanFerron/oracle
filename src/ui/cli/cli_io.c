/* ========================================================================
   CLI (stdio) UiIO Backend
   ======================================================================== */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "cli_io.h"
#include "cli_display.h"
#include "../shared/localization.h"

static void cli_io_message(UiIO* io, UiMsgKind kind, const char* fmt, ...)
{ switch(kind)
  { case UI_MSG_ERROR:
      fputs(RED, stdout);
      break;
    case UI_MSG_SUCCESS:
      fputs(GREEN ICON_SUCCESS " ", stdout);
      break;
    case UI_MSG_WARNING:
      fputs(YELLOW, stdout);
      break;
    case UI_MSG_INFO:
      break;
  }

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  if(kind != UI_MSG_INFO) fputs(RESET, stdout);
  putchar('\n');
}

static bool cli_io_read_line(UiIO* io, const char* prompt, char* buf, size_t bufsize)
{ fputs(prompt, stdout);
  fflush(stdout);

  if(fgets(buf, bufsize, stdin) == NULL)
    return false;

  buf[strcspn(buf, "\n")] = 0;
  return true;
}

static void cli_io_show_card_list(UiIO* io, const char* title,
                                  const uint8_t* card_indices, int count,
                                  int suggested_idx)
{ config_t* cfg = (config_t*)io->ctx;

  printf("\n%s\n", title);

  if(count == 0)
  { printf("  %s\n", LOCALIZED_STRING("(no champions)", "(aucun champion)", "(sin campeones)"));
    return;
  }

  for(int i = 0; i < count; i++)
  { display_card_with_power(card_indices[i], i + 1, 1, cfg);
    if(i == suggested_idx)
      printf("      " GRAY "%s" RESET "\n",
             LOCALIZED_STRING("^ suggested", "^ suggere", "^ sugerido"));
  }
}

UiIO cli_io_create(config_t* cfg)
{ UiIO io;
  io.message = cli_io_message;
  io.read_line = cli_io_read_line;
  io.show_card_list = cli_io_show_card_list;
  io.ctx = cfg;
  return io;
}
