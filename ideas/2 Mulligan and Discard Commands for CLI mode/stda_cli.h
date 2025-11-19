// stda_cli.h - Updated header with new functions

#ifndef STDA_CLI_H
#define STDA_CLI_H

#include "game_types.h"
#include "game_context.h"

/* Display Functions */
void display_player_prompt(PlayerID player, struct gamestate* gstate, int is_defense);
void display_player_hand(PlayerID player, struct gamestate* gstate);
void display_attack_state(struct gamestate* gstate);
void display_game_status(struct gamestate* gstate);
void display_cli_help(int is_defense);

/* Card Display Helpers */
void display_card_with_index(uint8_t card_idx, int display_num, int show_power);

/* Input Parsing Functions */
int parse_champion_indices(char* input, uint8_t* indices, int max_count, int hand_size);
int parse_card_indices_cli(char* input, uint8_t* indices, int max_count, int hand_size);

/* Card Action Handlers */
int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count, GameContext* ctx);
int handle_draw_command(struct gamestate* gstate, PlayerID player, char* input, GameContext* ctx);
int handle_cash_command(struct gamestate* gstate, PlayerID player, char* input, GameContext* ctx);

/* Shared Helper Functions (NEW) */
void discard_and_draw(struct gamestate* gstate, PlayerID player, uint8_t* indices,
                     int count, int draw_replacements, GameContext* ctx);

/* Main Mode Function */
int run_mode_stda_cli(config_t* cfg);

#endif
