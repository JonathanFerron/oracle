/* ============================================================
   cli_constants.h - CLI-specific constants
   ============================================================ */

#ifndef CLI_CONSTANTS_H
#define CLI_CONSTANTS_H

/* Action return codes for CLI command processing */
#define EXIT_SIGNAL -1
#define ACTION_TAKEN 1
#define NO_ACTION 0

/* Input buffer sizes */
#define MAX_COMMAND_LEN 256
#define MAX_INPUT_LEN_SHORT 10   /* For single-digit choices */
#define MAX_INPUT_LEN_MEDIUM 64  /* For names and text input */

#endif /* CLI_CONSTANTS_H */
