// strategy.c
// Strategy function pointer framework implementation

#include "ai_strategy.h"
#include <stdlib.h>

StrategySet* create_strategy_set(void)
{ StrategySet* strat = (StrategySet*)malloc(sizeof(StrategySet));
  return strat;
}

void set_player_strategy(StrategySet* strat, PlayerID player,
                         AttackStrategyFunc att_func,
                         DefenseStrategyFunc def_func)
{ strat->attack_strategy[player] = att_func;
  strat->defense_strategy[player] = def_func;
}

void free_strategy_set(StrategySet* strat)
{ if(strat != NULL)
    free(strat);
}
