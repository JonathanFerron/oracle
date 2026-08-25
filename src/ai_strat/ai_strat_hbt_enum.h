// ai_strat_hbt_enum.h
// A7 Hybrid HBT's per-turn state derivation (Layers T and B) and move
// enumeration/scoring (Layer H) -- split out of ai_strat_hbt.c/.h once the
// combined file passed ~400 lines, same file-length reasoning as A3
// Borealis's ai_strat_borealis/ai_strat_borealis_enum split
// (ai_strat_borealis_enum.c's own header comment). Internal to the A7 agent:
// nothing outside ai_strat_hbt.c should include this.

#ifndef AI_STRAT_HBT_ENUM_H
#define AI_STRAT_HBT_ENUM_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_hbt.h"

// Per-turn derived state, computed once per call into hbt_attack_strategy()/
// hbt_defense_strategy() and threaded through move scoring -- Layer T's
// aggression-modulated H weights and Layer B's aggression-scaled resource
// targets (ai_strat_hbt.h's header comment has the full formulas).
typedef struct
{ float eps_eff;
  float gamma_eff;
  float delta_eff;
  float target_cash;
  float target_cards;
} HBTState;

// player's own aggression/phase state, evaluated against 1-player (its
// opponent) -- same direction as A6's evaluate_aggression_factor(). Call
// once per turn with `player` = the acting side (attacker on attack,
// defender on defense; ai_strat_hbt.c's two entry points pass the right one).
HBTState hbt_evaluate_state(struct gamestate* gstate, PlayerID player,
                            const HBTParams* params);

// Internal accessor for the live per-player params (g_params, private to
// ai_strat_hbt.c) -- used by ai_strat_hbt_cards.c's mulligan/discard
// overrides, which need the same calibrated values attack/defense use
// (hbt_set_params()'s per-player override included). Not part of the public
// ai_strat_hbt.h API.
const HBTParams* hbt_live_params(PlayerID player);

typedef enum
{ HBT_MOVE_PASS = 0,
  HBT_MOVE_CHAMPIONS,
  HBT_MOVE_DRAW,
  HBT_MOVE_CASH
} HBTMoveType;

typedef struct
{ float advantage;
  HBTMoveType type;
  uint8_t cards[3];
  uint8_t count;
} HBTBestMove;

// A5-shaped exhaustive enumeration (every affordable 1-3 champion subset,
// every affordable draw card, every affordable cash card, plus pass),
// scored by the state-modulated advantage function with A3's combo hold
// excluding held-back subsets. Deterministic argmax, first-enumerated wins
// ties (no RNG).
HBTBestMove hbt_best_attack_move(struct gamestate* gstate, PlayerID player,
                                 const HBTParams* params, const HBTState* state);

// Same exhaustive-enumeration shape restricted to champion subsets (0-3,
// decline is size 0), scored against a variance-aware incoming-attack
// estimate (ai_strat_hbt.h's defense_stdev_mult).
HBTBestMove hbt_best_defense_move(const struct gamestate* gstate, PlayerID defender,
                                  const HBTParams* params, const HBTState* state);

#endif // AI_STRAT_HBT_ENUM_H
