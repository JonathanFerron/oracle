// ui_io.h
// Minimal I/O seam so the shared interactive command grammar/rules
// (src/ui/interactive/game_commands.c) can be driven by either the CLI
// (stdio) or the TUI (ncurses) without duplicating the rules themselves.
// Board/state rendering is NOT part of this seam -- each UI renders its own
// way (cli_display.c vs tui_render.c); this only covers the three points
// where the shared command logic used to touch stdio directly: transient
// feedback messages, blocking line reads (top-level command line and
// nested sub-prompts like recall/cash-exchange), and "show this titled list
// of champions" presentation.

#ifndef UI_IO_H
#define UI_IO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum
{ UI_MSG_INFO,
  UI_MSG_SUCCESS,
  UI_MSG_WARNING,
  UI_MSG_ERROR
} UiMsgKind;

typedef struct UiIO
{ // Feedback line (printf-style; each call is one logical message/line).
  void (*message)(struct UiIO* io, UiMsgKind kind, const char* fmt, ...);

  // Blocking read of one line of input, preceded by the given prompt text
  // (prompt is printed/shown verbatim, no trailing newline added -- callers
  // include any ": "/"> " decoration themselves, matching prior CLI text).
  // Returns false on EOF/cancel; on success buf holds the line with any
  // trailing \n/\r stripped.
  bool (*read_line)(struct UiIO* io, const char* prompt, char* buf, size_t bufsize);

  // Presents a titled list of champion cards (recall candidates, exchange
  // candidates, etc.). suggested_idx is an index into card_indices (marks
  // e.g. the lowest-power/recommended one), or -1 for none.
  void (*show_card_list)(struct UiIO* io, const char* title,
                         const uint8_t* card_indices, int count,
                         int suggested_idx);

  void* ctx; // implementation-specific (CLI: config_t*; TUI: TuiScreen*+more)
} UiIO;

#endif // UI_IO_H
