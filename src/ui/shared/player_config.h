// player_config.h
// Extended player configuration with names and strategies

#ifndef PLAYER_CONFIG_H
#define PLAYER_CONFIG_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"

#define MAX_PLAYER_NAME_LEN 32
#define MAX_STRATEGY_NAME_LEN 32

// AIStrategyType now lives in ../../core/game_types.h (included above) so
// src/ai_strat/ can reference it without depending on src/ui/. See that
// header for the roster/rename comment.
//
// CLI shorthand aliases: as of the one-shorthand-per-agent cleanup, every
// AIStrategyType has exactly one CLI shorthand -- see AI_STRATEGY_SHORTHANDS
// in player_config.c. The `greedy` and `showboat` aliases (retired pre-rename
// tech name and flavour name, respectively) were dropped.

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

// AI agent shorthand lookup for the `-A`/`--ai` CLI option (lowercase
// letters/digits, <=10 chars each; see doc/changelog.md for the full list).
// Returns AI_STRATEGY_COUNT if `shorthand` matches none.
AIStrategyType parse_ai_strategy_shorthand(const char* shorthand);
void print_ai_agent_shorthand_list(config_t* cfg);

// Bare shorthand codes only, one per line, no localized names/header
// (shell completion; see tools/oracle-completion.bash).
void print_ai_agent_shorthand_codes(void);

#endif // PLAYER_CONFIG_H
