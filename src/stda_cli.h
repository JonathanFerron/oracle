#ifndef STDA_CLI_H
#define STDA_CLI_H

#include "game_types.h"
#include "game_context.h"

/* Run mode functions */
void display_player_prompt(PlayerID player, struct gamestate* gstate, int is_defense, config_t* cfg);
void display_player_hand(PlayerID player, struct gamestate* gstate, config_t* cfg);
void display_attack_state(struct gamestate* gstate, config_t* cfg);
int parse_champion_indices(char* input, uint8_t* indices, int max_count, int hand_size, config_t* cfg);
int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count, GameContext* ctx, config_t* cfg);
int handle_draw_command(struct gamestate* gstate, PlayerID player, char* input, GameContext* ctx, config_t* cfg);
int handle_cash_command(struct gamestate* gstate, PlayerID player, char* input, GameContext* ctx, config_t* cfg);
void display_game_status(struct gamestate* gstate, config_t* cfg);
void display_cli_help(int is_defense, config_t* cfg);

int run_mode_stda_cli(config_t* cfg);

#endif
