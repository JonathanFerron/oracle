// game_constants.h
// Game constants and lookup tables

#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

// Constants
#define FULL_DECK_SIZE 120
#define MAX_NUMBER_OF_TURNS 500
#define MAX_NUMBER_OF_SIM 10000
// cmdline.c defaults cfg->numsim to this same value; kept as its own name
// (rather than reusing MAX_NUMBER_OF_SIM) so raising the cap above doesn't
// silently raise the no-"-n" default too -- see run_mode_stda_auto()'s
// numsim<=0 fallback in stda_auto.c, which must track this, not the cap.
#define DEFAULT_NUMBER_OF_SIM 1000
#define DEBUG_NUMBER_OF_SIM 1
#define AVERAGE_POWER_FOR_MULLIGAN 4.98
#define INITIAL_CASH_DEFAULT 30
#define INITIAL_ENERGY_DEFAULT 99
#define INITAL_HAND_SIZE_DEFAULT 6
// add here a define for the max hand size for the end of turn discard
#define M_TWISTER_SEED 1337UL

#define oraclemin(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a < _b ? _a : _b; })
#define oraclemax(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })

#include "game_types.h"

// Full deck array (defined in game_constants.c)
extern const struct card fullDeck[FULL_DECK_SIZE];

// Species to Order mapping (defined in game_constants.c)
//extern const ChampionOrder SPECIES_TO_ORDER[];

// String name arrays (defined in game_constants.c)
extern const char* const PLAYER_NAMES[];
extern const char* const GAME_STATE_NAMES[];
extern const char* const TURN_PHASE_NAMES[];
extern const char* const CARD_TYPE_SHORT_NAMES[];
extern const char* const CHAMPION_COLOR_NAMES[];
extern const char* const CHAMPION_SPECIES_NAMES[];
extern const char* const CHAMPION_SPECIES_ABBR[];

#endif // GAME_CONSTANTS_H
