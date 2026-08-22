// game_types.h
// All enums, structs, and type definitions for Oracle game

#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "../structures/deckstack.h"
#include "../structures/card_collection.h"

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
  Hand hand[2];
  Discard discard[2];
  CombatZone combat_zone[2];

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
typedef enum
{ LANG_EN = 0,  /* English (default) */
  LANG_FR,      /* French */
  LANG_ES       /* Spanish */
} ui_language_t;

/* Player Type: interactive or AI */
typedef enum
{ INTERACTIVE_PLAYER = 0,
  AI_PLAYER = 1
} PlayerType;

// Available AI strategies. Order matches the ideas/A1-A11 agent roster and
// the canonical ideas/G1.../oracle_ai_agent_names.md roster/rating table
// (ideas/G1 general info and G2 parameter storing/optimization are support
// material, not themselves agents, so they have no entry here).
//
// AI_STRATEGY_GREEDY_POWER and AI_STRATEGY_COMBO_AWARE were renamed to
// AI_STRATEGY_BOREALIS and AI_STRATEGY_COMBO_THRESHOLD (2026-08-21) per the
// names file's "Required renames": Borealis is now the Bradley-Terry
// benchmark, and "Combo Aware" no longer discriminates now that Borealis
// also computes combo bonuses.
//
// Lives here (rather than in ui/shared/player_config.h, where it used to be)
// so src/ai_strat/ can reference it without depending on src/ui/.
typedef enum
{ AI_STRATEGY_RANDOM = 0,
  AI_STRATEGY_VALUE_BASED,     // ideas/A1 ai agent value based (the apprentice)
  AI_STRATEGY_COMBO_THRESHOLD, // ideas/A2 ai agent combo threshold (the showboat)
  AI_STRATEGY_BOREALIS,        // ideas/A3 ai agent greedy power (borealis) -- benchmark agent
  AI_STRATEGY_BALANCED,        // ideas/A4 ai agent balanced rules (bean counter)
  AI_STRATEGY_HEURISTIC,       // ideas/A5 ai agent heuristic (eps-gam-del)
  AI_STRATEGY_TACTICAL,        // ideas/A6 ai agent tactical (pressure cooker)
  AI_STRATEGY_HYBRID_HBT,      // ideas/A7 ai agent hybrid hbt (the grandmaster)
  AI_STRATEGY_SIMPLE_MC,       // ideas/A8 ai agent simple monte carlo (the soothsayer)
  AI_STRATEGY_HBT_2PLY,        // ideas/A9 ai agent hbt 2 ply (grandmaster ii)
  AI_STRATEGY_ISMCTS,          // ideas/A10 ai agent is-mcts (the omniscient)
  AI_STRATEGY_ISMCTS_NN,       // ideas/A11 ai agent is-mcts + nn (alphaoracle prime)
  AI_STRATEGY_COUNT
} AIStrategyType;

/* Configuration structure */
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
  void* player_config;  /* PlayerConfig* - forward declaration avoidance */
  AIStrategyType agent[2];  /* -Aa/-ai.a and -Ab/-ai.b: per-player agent for
                               --stda.auto (see cmdline.c). Index by PlayerID. */
  bool agent_set[2];        /* whether agent[i] was actually given on the CLI,
                               vs. left at its AI_STRATEGY_RANDOM zero-value. */
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
