/* ============================================================
   ui_constants.h - shared interactive-UI constants (command return
   codes, input buffer sizes). Used across ui/cli, ui/shared and
   ui/interactive -- moved here (was ui/cli/cli_constants.h) once
   player_config.c/player_selection.c and the shared command grammar
   in ui/interactive/game_commands.c needed it too.
   ============================================================ */

#ifndef UI_CONSTANTS_H
#define UI_CONSTANTS_H

/* Action return codes for interactive command processing */
#define EXIT_SIGNAL -1
#define ACTION_TAKEN 1
#define NO_ACTION 0

/* Input buffer sizes */
#define MAX_COMMAND_LEN 256
#define MAX_INPUT_LEN_SHORT 10   /* For single-digit choices */
#define MAX_INPUT_LEN_MEDIUM 64  /* For names and text input */

#endif /* UI_CONSTANTS_H */
