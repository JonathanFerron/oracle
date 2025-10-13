// game_constants.h
// Game constants and lookup tables

#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#include "game_types.h"

// Constants
#define FULL_DECK_SIZE 120
#define MAX_NUMBER_OF_TURNS 500
#define MAX_NUMBER_OF_SIM 1000
#define DEBUG_NUMBER_OF_SIM 1
#define AVERAGE_POWER_FOR_MULLIGAN 4.98

// Full deck array (defined in game_constants.c)
extern const struct card fullDeck[FULL_DECK_SIZE];

// Species to Order mapping (defined in game_constants.c)
extern const ChampionOrder SPECIES_TO_ORDER[];

// String name arrays (defined in game_constants.c)
extern const char *const PLAYER_NAMES[];
extern const char *const GAME_STATE_NAMES[];
extern const char *const TURN_PHASE_NAMES[];
extern const char *const CARD_TYPE_SHORT_NAMES[];
extern const char *const CHAMPION_COLOR_NAMES[];

// Helper function to get order from species
ChampionOrder get_order_from_species(ChampionSpecies species);

#endif // GAME_CONSTANTS_H
