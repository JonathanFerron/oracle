// strategy.c
// Strategy function pointer framework implementation

#include <string.h>
#include <stdlib.h>

#include "ai_strategy.h"
#include "ai_strat_random.h"
#include "ai_strat_valuebased.h"
#include "ai_strat_combo_threshold.h"
#include "ai_strat_borealis.h"
#include "ai_strat_balanced_rules.h"
#include "ai_strat_heuristic.h"
#include "ai_strat_tactical.h"
#include "ai_strat_hbt.h"
#include "ai_strat_hbt2ply.h"
#include "ai_strat_simplemc1.h"
#include "ai_strat_clairvoyant1.h"
#include "ai_strat_lib_heuristics.h"

StrategySet* create_strategy_set(void)
{ StrategySet* strat = (StrategySet*)malloc(sizeof(StrategySet));
  if(strat == NULL) return NULL;

  memset(strat, 0, sizeof(StrategySet));
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

// Registry entry, indexed by AIStrategyType. {NULL, NULL, ..., ...} means
// "not yet implemented" -- ai_strategy_is_implemented() and
// set_player_strategy_by_type() both key off the attack/defense pair. Keep
// in sync with AIStrategyType (game_types.h) and with
// display_ai_strategy_menu()/AI_STRATEGY_SHORTHANDS (player_config.c).
// mulligan/discard are optional even for an implemented agent: leaving
// either NULL means "use the shared power-based default"
// (ai_strat_lib_heuristics.h), which set_player_strategy_by_type() fills in
// below -- so Random and every agent that doesn't override them keeps
// exactly today's behaviour.
typedef struct
{ AttackStrategyFunc attack;
  DefenseStrategyFunc defense;
  MulliganStrategyFunc mulligan; // NULL = strat_lib_mulligan default
  DiscardStrategyFunc discard;   // NULL = strat_lib_discard_to_7 default
} StrategyRegistryEntry;

static const StrategyRegistryEntry STRATEGY_REGISTRY[AI_STRATEGY_COUNT] =
{ [AI_STRATEGY_RANDOM]           = { random_attack_strategy, random_defense_strategy },
  [AI_STRATEGY_VALUE_BASED]      = { value_based_attack_strategy, value_based_defense_strategy },
  [AI_STRATEGY_COMBO_THRESHOLD]  = { combo_threshold_attack_strategy, combo_threshold_defense_strategy },
  [AI_STRATEGY_BOREALIS]         = { borealis_attack_strategy, borealis_defense_strategy,
    borealis_mulligan, borealis_discard_to_7
  },
  [AI_STRATEGY_BALANCED]         = { balanced_rules_attack_strategy,
    balanced_rules_defense_strategy
  },
  [AI_STRATEGY_HEURISTIC]        = { heuristic_attack_strategy,
    heuristic_defense_strategy
  },
  [AI_STRATEGY_TACTICAL]         = { tactical_attack_strategy,
    tactical_defense_strategy
  },
  [AI_STRATEGY_HYBRID_HBT]       = { hbt_attack_strategy, hbt_defense_strategy,
    hbt_mulligan, hbt_discard_to_7
  },
  [AI_STRATEGY_SIMPLE_MC]        = { simplemc_attack_strategy,
    simplemc_defense_strategy
  },
  [AI_STRATEGY_HBT_2PLY]        = { hbt2ply_attack_strategy,
    hbt2ply_defense_strategy
  },
  [AI_STRATEGY_CLAIRVOYANT]      = { clairvoyant_attack_strategy,
    clairvoyant_defense_strategy
  },
  // All other entries default to {NULL, NULL, NULL, NULL} -- not yet implemented.
};

bool ai_strategy_is_implemented(AIStrategyType type)
{ if(type < 0 || type >= AI_STRATEGY_COUNT) return false;

  return STRATEGY_REGISTRY[type].attack != NULL &&
         STRATEGY_REGISTRY[type].defense != NULL;
}

void set_player_strategy_by_type(StrategySet* strat, PlayerID player,
                                 AIStrategyType type)
{ if(!ai_strategy_is_implemented(type))
    type = AI_STRATEGY_RANDOM;

  set_player_strategy(strat, player,
                      STRATEGY_REGISTRY[type].attack,
                      STRATEGY_REGISTRY[type].defense);

  strat->mulligan_strategy[player] = STRATEGY_REGISTRY[type].mulligan ?
                                     STRATEGY_REGISTRY[type].mulligan :
                                     strat_lib_mulligan;
  strat->discard_strategy[player] = STRATEGY_REGISTRY[type].discard ?
                                    STRATEGY_REGISTRY[type].discard :
                                    strat_lib_discard_to_7;
}
