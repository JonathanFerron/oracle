// ai_strat_a13_state.h
// A13 Cartographer's per-turn derived state -- A7's HBTState, unchanged,
// plus Layer R (race arithmetic). See ai_strat_a13.h's "Layer R: race
// arithmetic" section for the full formula and the defense_stdev_mult sign
// convention; this module is that section's implementation.

#ifndef AI_STRAT_A13_STATE_H
#define AI_STRAT_A13_STATE_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_a13.h"
#include "ai_strat_a13_belief.h"
#include "ai_strat_hbt_enum.h"

typedef struct
{ HBTState base;    // A7's per-turn derived state (eps_eff/gamma_eff/
  // delta_eff/target_cash/target_cards), computed via hbt_evaluate_state()
  // and otherwise untouched by this agent

  float stdev_eff; // Layer R's race-aware replacement for
  // params->base.defense_stdev_mult -- equals it exactly when
  // params->race_scale <= 0 (the neutral/off configuration)
  float eps_race;  // Layer R's multiplicative epsilon modulation from race
  // position -- exactly 1.0f when params->race_scale <= 0
} A13State;

// Computes state.base via hbt_evaluate_state() verbatim, then Layer R's
// stdev_eff/eps_race on top -- short-circuited to state.base's own
// defense_stdev_mult / 1.0f with NO belief/TTK arithmetic at all when
// params->race_scale <= 0, per ai_strat_a13.h's cost-discipline rule.
// `belief` may be a zeroed/unbuilt A13Belief when race_use_belief_opp is
// false (this function never dereferences belief's block/attack fields in
// that path) -- see ai_strat_a13.c's a13_belief_needed().
A13State a13_evaluate_state(struct gamestate* gstate, PlayerID player,
                            const A13Params* params, const A13Belief* belief);

#endif // AI_STRAT_A13_STATE_H
