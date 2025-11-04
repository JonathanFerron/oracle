// game_types.h
// All enums, structs, and type definitions for Oracle game

#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "deckstack.h"
#include "hdcll.h"

// Player identification
typedef enum
{ PLAYER_A = 0,
  PLAYER_B = 1
} PlayerID;

// Game state enum
typedef enum
{ PLAYER_A_WINS = 0,
  PLAYER_B_WINS = 1,
  DRAW = 2,
  ACTIVE = 3
} GameStateEnum;

// Turn phase enum
typedef enum
{ ATTACK,
  DEFENSE
} TurnPhase;

// Card type enum
typedef enum
{ CHAMPION_CARD,
  DRAW_CARD,
  CASH_CARD
} CardType;

// Champion color enum
typedef enum
{ COLOR_RED,
  COLOR_INDIGO,
  COLOR_ORANGE,
  COLOR_NOT_APPLICABLE,
  COLOR_COUNT = COLOR_NOT_APPLICABLE
} ChampionColor;

// Champion species enum
typedef enum
{ SPECIES_HUMAN,
  SPECIES_ELF,
  SPECIES_DWARF,
  SPECIES_ORC,
  SPECIES_GOBLIN,
  SPECIES_DRAGON,
  SPECIES_HOBBIT,
  SPECIES_CENTAUR,
  SPECIES_MINOTAUR,
  SPECIES_AVEN,
  SPECIES_CYCLOPS,
  SPECIES_FAUN,
  SPECIES_FAIRY,
  SPECIES_KOATL,
  SPECIES_LYCAN,
  SPECIES_NOT_APPLICABLE,
  SPECIES_COUNT = SPECIES_NOT_APPLICABLE
} ChampionSpecies;

// Champion order enum (The Five Orders of Arcadia)
typedef enum
{ ORDER_A,  // Human, Elf, Dwarf - "Dawn Light"
  ORDER_B,  // Hobbit, Faun, Centaur - "Verdant Light"
  ORDER_C,  // Orc, Goblin, Minotaur - "Ember Light"
  ORDER_D,  // Dragon, Cyclops, Fairy - "Eternal Light"
  ORDER_E,  // Aven, Koatl, Lycan - "Moonlight"
  ORDER_NOT_APPLICABLE,
  ORDER_COUNT = ORDER_NOT_APPLICABLE
} ChampionOrder;

// Deck type enum
typedef enum
{ DECK_RANDOM,
  DECK_MONOCHROME,
  DECK_CUSTOM
} DeckType;

// Card structure
struct card
{ CardType card_type;
  uint8_t cost;

  // Champion card fields
  uint8_t champion_id;
  uint8_t defense_dice;
  uint8_t attack_base;
  ChampionColor color;
  ChampionSpecies species;
  ChampionOrder order;

  // Draw card fields
  uint8_t draw_num;
  uint8_t choose_num;

  // Calculated fields
  float expected_attack;
  float expected_defense;
  float attack_efficiency;
  float defense_efficiency;
  float power;

  // Cash card fields
  uint8_t exchange_cash;
}; // card

// Game state structure
struct gamestate
{ PlayerID current_player;
  uint16_t current_cash_balance[2];
  uint8_t current_energy[2];
  bool someone_has_zero_energy;

  struct deck_stack deck[2];
  struct HDCLList hand[2];
  struct HDCLList discard[2];
  struct HDCLList combat_zone[2];

  uint16_t turn;
  GameStateEnum game_state;
  TurnPhase turn_phase;
  PlayerID player_to_move;
}; // gamestate

/* Game mode enumeration */
typedef enum
{ MODE_NONE = 0,
  MODE_STDA_AUTO,    /* Standalone automated */
  MODE_STDA_SIM,     /* Standalone simulation */
  MODE_STDA_CLI,     /* Standalone text command line interface */
  MODE_STDA_TUI,     /* Standalone text UI */
  MODE_STDA_GUI,     /* Standalone graphical UI */
  MODE_SERVER,       /* Server mode */
  MODE_CLIENT_SIM,   /* Client simulation */
  MODE_CLIENT_CLI,   /* Client text command line interface */
  MODE_CLIENT_TUI,   /* Client text UI */
  MODE_CLIENT_GUI,   /* Client graphical UI */
  MODE_CLIENT_AI     /* AI agent client */
} game_mode_t;

/* UI language codes */
typedef enum {
    LANG_EN = 0,  /* English (default) */
    LANG_FR,      /* French */
    LANG_ES       /* Spanish */
} ui_language_t;

/* Player Type: interactive or AI */
typedef enum
{
  INTERACTIVE_PLAYER = 0,
  AI_PLAYER = 1
} PlayerType;

/* Configuration structure */
// TODO: consider whether this struct should be moved to the game_context.h source file, or to cmdline.h as it's primarily populated in the cmldline.c file.
typedef struct
{ game_mode_t mode;
  bool verbose;
  int numsim;
  char* input_file;
  char* output_file;
  char* ai_agent;
  ui_language_t language;
  uint32_t prng_seed;
  bool use_random_seed;
  PlayerType player_types[2]; // first value is for playerA, second value is for playerB
} config_t;

#include "game_constants.h"

// Game statistics structure
struct gamestats
{ uint16_t cumul_player_wins[2];
  uint16_t cumul_number_of_draws;
  uint16_t game_end_turn_number[MAX_NUMBER_OF_SIM];  // look into dynamically allocating space for this as we don't need 1000 entries for interactive modes (we just need one)
  uint16_t simnum;
}; // gamestats

#endif // GAME_TYPES_H
