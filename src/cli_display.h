#ifndef CLI_DISPLAY_H
#define CLI_DISPLAY_H

#include "game_types.h"

// Status and state display
void display_player_prompt(PlayerID player, struct gamestate* gstate,
                           int is_defense, config_t* cfg);
void display_player_hand(PlayerID player, struct gamestate* gstate,
                         config_t* cfg);
void display_attack_state(struct gamestate* gstate, config_t* cfg);
void display_game_status(struct gamestate* gstate, config_t* cfg);
void display_cli_help(int is_defense, config_t* cfg);

// New extracted functions
void display_turn_header(PlayerID player, PlayerID opponent,
                        struct gamestate* gstate, config_t* cfg);
void display_game_summary(struct gamestate* gstate, config_t* cfg);

#endif // CLI_DISPLAY_H
