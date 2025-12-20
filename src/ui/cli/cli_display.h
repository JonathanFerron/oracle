#ifndef CLI_DISPLAY_H
#define CLI_DISPLAY_H

#include "../../core/game_types.h"

/* ANSI color codes */
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define GRAY    "\033[38;2;128;128;128m"
#define BOLD_WHITE   "\033[1;37m"
#define COLOR_P1     "\033[1;36m"
#define COLOR_P2     "\033[1;33m"
#define COLOR_ENERGY MAGENTA
#define COLOR_LUNA   CYAN

/* Visual indicators */
#define ICON_PROMPT ">"
#define ICON_SUCCESS "[OK]"

// Status and state display
void display_player_prompt(PlayerID player, struct gamestate* gstate,
                           int is_defense, config_t* cfg);
void display_player_hand(PlayerID player, struct gamestate* gstate,
                         config_t* cfg);
void display_attack_state(struct gamestate* gstate, config_t* cfg);
void display_game_status(struct gamestate* gstate, config_t* cfg);
void display_cli_help(int is_defense, config_t* cfg);

void display_turn_header(PlayerID player, PlayerID opponent,
                        struct gamestate* gstate, config_t* cfg);
void display_game_summary(struct gamestate* gstate, config_t* cfg);

// Card display with power values
void display_card_with_power(uint8_t card_idx, int display_num,
                             int show_power, config_t* cfg);

// Mulligan and discard prompts
void display_mulligan_prompt(struct gamestate* gstate,
                             PlayerID player, config_t* cfg);

void display_discard_prompt(struct gamestate* gstate,
                            PlayerID player, config_t* cfg);

#endif // CLI_DISPLAY_H
