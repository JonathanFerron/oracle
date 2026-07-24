/* ========================================================================
   CLI Input Processing
   Thin CLI-only wrappers: intercepts the CLI's diagnostic commands (gmst,
   shod, help -- full-board/discard/help dumps with no TUI equivalent yet),
   then delegates the actual attack/defense turn-action grammar (cham/draw/
   cash/pass/exit) to the shared, UI-agnostic game_commands.c so it isn't
   duplicated between the CLI and TUI.
   ======================================================================== */

#include <string.h>

#include "cli_input.h"
#include "cli_display.h"
#include "cli_io.h"
#include "../shared/ui_constants.h"
#include "../interactive/game_commands.h"

int process_attack_command(char* input_buffer, struct gamestate* gstate,
                           PlayerID player, GameContext* ctx, config_t* cfg)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strcmp(input_buffer, "gmst") == 0)
  { display_game_status(gstate, cfg);
    return NO_ACTION;
  }
  else if(strcmp(input_buffer, "shod") == 0)
  { display_player_discard_detailed(PLAYER_A, gstate, cfg);
    display_player_discard_detailed(PLAYER_B, gstate, cfg);
    return NO_ACTION;
  }
  else if(strcmp(input_buffer, "help") == 0)
  { display_cli_help(0, cfg);
    return NO_ACTION;
  }

  UiIO io = cli_io_create(cfg);
  return game_process_attack_command(input_buffer, gstate, player, ctx, cfg, &io);
}

int process_defense_command(char* input_buffer, struct gamestate* gstate,
                            PlayerID player, GameContext* ctx, config_t* cfg)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strcmp(input_buffer, "help") == 0)
  { display_cli_help(1, cfg);
    return NO_ACTION;
  }

  UiIO io = cli_io_create(cfg);
  return game_process_defense_command(input_buffer, gstate, player, ctx, cfg, &io);
}
