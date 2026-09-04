// ai_strat_ismcts_flat.h
// A10 IS-MCTS's flat-rollout scoring for mulligan / discard-to-7 -- see
// doc/ai_agents.md's A10 section. Both are one-shot
// choices with no opponent reply to search (mulligan happens before either
// player has moved; discard-to-7 has already resolved combat for the turn),
// so a UCT tree buys nothing here -- each candidate subset is instead scored
// by averaging `limit_flat_iterations / n` determinized rollouts
// (mc_playout_from_turn_boundary(), ai_strat_playout.h), argmax over the
// candidates. Registered as AI_STRATEGY_ISMCTS's mulligan_strategy/
// discard_strategy overrides in ai_strategy.c's STRATEGY_REGISTRY[],
// replacing the shared strat_lib_mulligan/strat_lib_discard_to_7 default
// every other un-overridden agent still falls back to.

#ifndef AI_STRAT_ISMCTS_FLAT_H
#define AI_STRAT_ISMCTS_FLAT_H

#include "../core/game_types.h"
#include "../core/game_context.h"

void ismcts_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void ismcts_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);

#endif // AI_STRAT_ISMCTS_FLAT_H
