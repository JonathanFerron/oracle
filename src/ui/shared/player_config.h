// player_config.h
// Extended player configuration with names and strategies

#ifndef PLAYER_CONFIG_H
#define PLAYER_CONFIG_H

#include <stddef.h>

#include "../../core/game_types.h"
#include "../../core/game_context.h"

#define MAX_PLAYER_NAME_LEN 32
#define MAX_STRATEGY_NAME_LEN 32
// "AI - <flavour>" (format_player_label()) -- flavour names top out around
// "IS-MCTS + Neural Network"'s localized variants, well under
// MAX_STRATEGY_NAME_LEN; +8 covers "AI - "/"IA - " plus the nul terminator.
#define MAX_PLAYER_LABEL_LEN (MAX_STRATEGY_NAME_LEN + 8)

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

// Writes the label used in place of a player's raw name wherever the CLI
// (or TUI, if it adopts this too) wants to show at a glance whether a side
// is AI-controlled and which agent: the interactive player's own name
// unchanged, or "AI - <flavour>" (localized, e.g. "AI - Bean Counter") for
// an AI-controlled position -- the human's custom/default name for that
// slot (e.g. "Player2") is intentionally dropped in the AI case, since the
// flavour name is more informative and every other per-turn display already
// carries the position letter separately. `buf` must hold at least
// MAX_PLAYER_LABEL_LEN bytes.
void format_player_label(PlayerID player, PlayerConfig* pconfig, ui_language_t lang,
                         char* buf, size_t n);

// AI agent shorthand lookup for the `-A`/`--ai` CLI option (lowercase
// letters/digits, <=10 chars each; see doc/changelog.md for the full list).
// Returns AI_STRATEGY_COUNT if `shorthand` matches none.
AIStrategyType parse_ai_strategy_shorthand(const char* shorthand);
void print_ai_agent_shorthand_list(config_t* cfg);

// Inverse of parse_ai_strategy_shorthand(). NULL if `strategy` has no
// shorthand (AI_STRATEGY_COUNT or otherwise out of range).
const char* get_ai_strategy_shorthand(AIStrategyType strategy);

// Borealis-scale rating (1-99) for `strategy` -- see AI_STRATEGY_RATINGS's
// comment in player_config.c. *is_measured is set to true for a real
// --stda.rating fit, false for a design-intent estimate; may be NULL.
// Returns -1 (and *is_measured = false) for an out-of-range strategy.
int8_t get_ai_strategy_rating(AIStrategyType strategy, bool* is_measured);

// Bare shorthand codes only, one per line, no localized names/header
// (shell completion; see tools/oracle-completion.bash).
void print_ai_agent_shorthand_codes(void);

#endif // PLAYER_CONFIG_H
