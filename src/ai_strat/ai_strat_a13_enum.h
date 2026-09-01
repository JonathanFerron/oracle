// ai_strat_a13_enum.h
// A13 Cartographer's move enumeration/scoring -- see ai_strat_a13.c and
// ai_strat_a13.h for the full spec. Internal to the A13 agent family, same
// role as ai_strat_hbt_enum.h is to A7/A9.

#ifndef AI_STRAT_A13_ENUM_H
#define AI_STRAT_A13_ENUM_H

#include "../core/game_types.h"
#include "ai_strat_a13.h"
#include "ai_strat_a13_belief.h"
#include "ai_strat_a13_state.h"
#include "ai_strat_hbt_enum.h" // HBTBestMove/HBTMoveType

// A5/A7-shaped exhaustive enumeration (every affordable 1-3 champion
// subset, every affordable draw card, every affordable cash card, plus
// pass), reusing A7's own hbt_advantage()/predicted_damage()/
// is_held_combo()/build_affordable_champions() verbatim, plus Layer K
// (draw)/Layer K (block) on top -- ai_strat_a13.h has the full derivation.
// `belief` may be a zeroed/unbuilt A13Belief when every belief-consuming
// dial in `params` is neutral (ai_strat_a13.c's a13_belief_needed()); every
// addition below is itself guarded so this stays exact-identity to A7 in
// that configuration.
HBTBestMove a13_best_attack_move(struct gamestate* gstate, PlayerID player,
                                 const A13Params* params, const A13State* state,
                                 const A13Belief* belief);

// Same enumeration shape restricted to champion subsets (0-3, decline is
// size 0), against Layer R's race-aware variance estimate (state->stdev_eff)
// instead of A7's fixed params->base.defense_stdev_mult. Reuses A7's own
// evaluate_defense_subset() (ai_strat_hbt_enum.h) verbatim -- no A7 file is
// touched by this addition, since that function already takes the incoming-
// attack estimate as a plain float parameter.
HBTBestMove a13_best_defense_move(const struct gamestate* gstate, PlayerID defender,
                                  const A13Params* params, const A13State* state);

#endif // AI_STRAT_A13_ENUM_H
