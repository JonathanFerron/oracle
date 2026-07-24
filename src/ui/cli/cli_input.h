#ifndef CLI_INPUT_H
#define CLI_INPUT_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"

/* CLI turn-action command dispatch: intercepts CLI-only diagnostic commands
   (gmst/shod/help) then delegates to the shared grammar in
   ui/interactive/game_commands.h for cham/draw/cash/pass/exit. */
int process_attack_command(char* input_buffer, struct gamestate* gstate,
                           PlayerID player, GameContext* ctx, config_t* cfg);

int process_defense_command(char* input_buffer, struct gamestate* gstate,
                            PlayerID player, GameContext* ctx, config_t* cfg);

#endif // CLI_INPUT_H
