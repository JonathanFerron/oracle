// strategy.h
// Strategy function pointer framework

#ifndef STRATEGY_H
#define STRATEGY_H

#include "game_types.h"
#include "game_context.h"

// Strategy function pointer types
typedef void (*AttackStrategyFunc)(struct gamestate* gstate, GameContext* ctx);
typedef void (*DefenseStrategyFunc)(struct gamestate* gstate, GameContext* ctx);

// Strategy set for both players
typedef struct
{ AttackStrategyFunc attack_strategy[2];   // One for each player
  DefenseStrategyFunc defense_strategy[2]; // One for each player
} StrategySet;

// Strategy set management
StrategySet* create_strategy_set(void);
void set_player_strategy(StrategySet* strat, PlayerID player,
                         AttackStrategyFunc att_func,
                         DefenseStrategyFunc def_func);
void free_strategy_set(StrategySet* strat);

#endif // STRATEGY_H
