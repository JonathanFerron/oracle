// player_config.h
// Extended player configuration with names and strategies

#ifndef PLAYER_CONFIG_H
#define PLAYER_CONFIG_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"

#define MAX_PLAYER_NAME_LEN 32
#define MAX_STRATEGY_NAME_LEN 32

// Available AI strategies
typedef enum
{ AI_STRATEGY_RANDOM = 0,
  AI_STRATEGY_BALANCED,
  AI_STRATEGY_HEURISTIC,
  AI_STRATEGY_HYBRID,
  AI_STRATEGY_SIMPLE_MC,
  AI_STRATEGY_ISMCTS,
  AI_STRATEGY_COUNT
} AIStrategyType;

// Player assignment modes
typedef enum
{ ASSIGN_DIRECT = 0,      // Player1 -> A, Player2 -> B
  ASSIGN_INVERTED,        // Player1 -> B, Player2 -> A
  ASSIGN_RANDOM           // Randomly assign who goes first
} PlayerAssignmentMode;

// Player configuration data
typedef struct
{ PlayerType player_types[2];           // Interactive or AI for each position
  char player_names[2][MAX_PLAYER_NAME_LEN];
  AIStrategyType ai_strategies[2];
  PlayerAssignmentMode assignment_mode;
} PlayerConfig;

// Configuration functions
void init_player_config(PlayerConfig* pconfig);
void get_player_names(config_t* cfg, PlayerConfig* pconfig);
void get_ai_strategies(config_t* cfg, PlayerConfig* pconfig);
void get_player_assignment(PlayerConfig* pconfig, config_t* cfg);
void apply_player_assignment(PlayerConfig* pconfig, config_t* cfg,
                             GameContext* ctx);

// Strategy name utilities
const char* get_strategy_display_name(AIStrategyType strategy,
                                      ui_language_t lang);
const char* get_player_display_name(PlayerID player, PlayerConfig* pconfig);

#endif // PLAYER_CONFIG_H
