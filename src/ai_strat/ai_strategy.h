// strategy.h
// Strategy function pointer framework

#ifndef STRATEGY_H
#define STRATEGY_H

#include "../core/game_types.h"
#include "../core/game_context.h"

// Strategy function pointer types
typedef void (*AttackStrategyFunc)(struct gamestate* gstate, GameContext* ctx);
typedef void (*DefenseStrategyFunc)(struct gamestate* gstate, GameContext* ctx);

// Optional per-player hooks for the mulligan/discard-to-7 decision (which
// cards to give up), as distinct from the attack/defense turn strategy.
// set_player_strategy_by_type() fills these with the shared power-based
// default (ai_strat_lib_heuristics.h) for any agent that doesn't register
// its own in ai_strategy.c's STRATEGY_REGISTRY[] -- so Random and every
// non-overriding agent keep exactly today's behaviour.
typedef void (*MulliganStrategyFunc)(struct gamestate* gstate, PlayerID player,
                                     GameContext* ctx);
typedef void (*DiscardStrategyFunc)(struct gamestate* gstate, PlayerID player,
                                    GameContext* ctx);

// Strategy set for both players
typedef struct
{ AttackStrategyFunc attack_strategy[2];     // One for each player
  DefenseStrategyFunc defense_strategy[2];   // One for each player
  MulliganStrategyFunc mulligan_strategy[2]; // One for each player
  DiscardStrategyFunc discard_strategy[2];   // One for each player
  // TODO: Add here a string with a description of the strategy (e.g. "RANDOM")
} StrategySet;

// Strategy set management
StrategySet* create_strategy_set(void);
void set_player_strategy(StrategySet* strat, PlayerID player,
                         AttackStrategyFunc att_func,
                         DefenseStrategyFunc def_func);
void free_strategy_set(StrategySet* strat);

// Strategy registry: the single AIStrategyType -> function pointer dispatch
// point, consulted by every mode (stda.auto, CLI, TUI) instead of each
// hardcoding random_attack_strategy/random_defense_strategy. An agent is
// "implemented" once both its attack and defense functions are registered
// in ai_strategy.c's internal table; unimplemented agents register as
// {NULL, NULL} and fall back to Random.
bool ai_strategy_is_implemented(AIStrategyType type);
void set_player_strategy_by_type(StrategySet* strat, PlayerID player,
                                 AIStrategyType type);

#endif // STRATEGY_H
