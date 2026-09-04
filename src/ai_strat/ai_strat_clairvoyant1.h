// ai_strat_clairvoyant1.h
// A12 Clairvoyant ("The Clairvoyant") -- see
// doc/ai_agents.md's A12 section.
//
// A8 Simple Monte Carlo's sibling: the identical progressive-pruning search
// (ai_strat_simplemc_search.h, SimpleMcParams reused as-is -- the
// budget/pruning shape has no reason to differ between the two) with one
// change, targeting the exact bias A8's own changelog entry (2026-08-25)
// diagnosed: A8's rollouts model *both* seats as AI_STRATEGY_RANDOM, so its
// win-probability estimates are calibrated against `rand` specifically and
// systematically biased against a real strategic opponent. This agent's
// rollouts keep its own future moves random (still "a simple MC approach,
// no tree" -- the search doesn't change) but give the *opponent's* simulated
// replies a cheap heuristic instead: no move enumeration, no per-subset
// search, no call into A5's or any other agent's real mechanism -- pick the
// top up to 3 affordable champions by `power` (a single fixed candidate, not
// a search over candidates) and commit them only if a small closed-form
// score (expected damage/defense + combo bonus + an energy-differential
// nudge) clears a threshold, else pass/decline. Deliberately not "as smart
// as A9/A10/A11" -- this is a rollout-policy patch, not a new search
// paradigm.
//
// Status (2026-08-25): implemented and lightly calibrated. Two rounds of
// playtracing found and fixed real defects in the rollout-policy formula
// itself (an attack score that never weighed cost, always positive --
// committed on 100% of decisions; then a defense formula that, once cost
// *was* weighed the same way, over-corrected to declining ~89% of the
// time) -- see doc/changelog.md's 2026-08-25 entry for both traces. A
// cost_weight sweep (both seats, n=700/point, vs borealis) then replaced
// the borrowed Borealis-lambda starting estimate with an empirically
// better value; SimpleMcParams itself was left at A8's own shipped
// defaults throughout (see aicalibsrc/simplemc/ for why that struct isn't
// a traditional decision-weight calibration target). Measured rating: 31
// (n=1500, both seats, vs borealis) -- close to but consistently a few
// points below A8's own 35, not a clear improvement. Shipped as-is; the
// gap wasn't chased further (see doc/changelog.md for why).

#ifndef AI_STRAT_CLAIRVOYANT1_H
#define AI_STRAT_CLAIRVOYANT1_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_simplemc1.h" // SimpleMcParams, reused as-is

void clairvoyant_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void clairvoyant_defense_strategy(struct gamestate* gstate, GameContext* ctx);

SimpleMcParams clairvoyant_get_default_params(void);

// Calibration-only override hook, same pattern as simplemc_set_params() --
// not part of the general strategy framework.
void clairvoyant_set_params(PlayerID player, const SimpleMcParams* params);
void clairvoyant_reset_params(void);

// Sweep/calibration-only override for the cheap opponent-rollout-policy's
// cost weight (see ai_strat_clairvoyant1.c's header comment on
// g_cost_weight for the sweep that picked this value and why it isn't a
// compile-time constant). Not player-specific and not part of
// SimpleMcParams -- it belongs to the rollout policy, not the search.
// Default (matching normal play) is 3.0, picked by a sweep against
// Borealis (2026-08-25) over Borealis's own calibrated luna_value
// (4.5846) as the starting estimate.
void clairvoyant_set_cost_weight(float weight);
float clairvoyant_get_cost_weight(void);

#endif // AI_STRAT_CLAIRVOYANT1_H
