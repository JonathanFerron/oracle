// player_selection.h
// Player type selection for CLI mode

#ifndef PLAYER_SELECTION_H
#define PLAYER_SELECTION_H

#include "game_types.h"

// Display player selection menu and get user choice
void display_player_selection_menu(config_t* cfg);

// Get player type selection from user input
int get_player_type_choice(config_t* cfg);

// Validate and apply player type selection
void apply_player_selection(config_t* cfg, int choice);

#endif // PLAYER_SELECTION_H
