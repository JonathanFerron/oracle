// player_config.h
// Extended player configuration with names and strategies

#ifndef PLAYER_CONFIG_H
#define PLAYER_CONFIG_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"

#define MAX_PLAYER_NAME_LEN 32
#define MAX_STRATEGY_NAME_LEN 32

// Available AI strategies. Order matches the ideas/A1-A11 agent roster and
// the canonical ideas/G1.../oracle_ai_agent_names.md roster/rating table
// (ideas/G1 general info and G2 parameter storing/optimization are support
// material, not themselves agents, so they have no entry here).
//
// AI_STRATEGY_GREEDY_POWER and AI_STRATEGY_COMBO_AWARE were renamed to
// AI_STRATEGY_BOREALIS and AI_STRATEGY_COMBO_THRESHOLD (2026-08-21) per the
// names file's "Required renames": Borealis is now the Bradley-Terry
// benchmark, and "Combo Aware" no longer discriminates now that Borealis
// also computes combo bonuses. `borealis`/`greedy` and `showboat`/`combo`
// remain valid CLI shorthand aliases -- see AI_STRATEGY_SHORTHANDS in
// player_config.c.
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
