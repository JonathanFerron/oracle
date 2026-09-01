// ai_strat_a13_belief.h
// A13 Cartographer's closed-form belief over the exact unseen-card pool --
// Layers K (draw valuation + opponent block/attack expectation) and D
// (reshuffle-boundary draw awareness). See ai_strat_a13.h's header comment
// for the full derivation and rationale; this module is the implementation
// of its "Layer K" and "Layer D" sections only. No dependency on A13Params:
// callers pass the two raw scalars this module actually needs
// (reshuffle_trust, block_combo_bonus) directly, so this stays decoupled and
// unit-testable without an A13Params instance.

#ifndef AI_STRAT_A13_BELIEF_H
#define AI_STRAT_A13_BELIEF_H

#include "../core/game_types.h"

// Hand's own structural cap (card_collection.h's Hand.cards[12]) -- the
// largest opponent hand size this module ever needs to size an array for.
#define A13_MAX_HAND_K 12

// Champion-play role weighting: empirically measured (2026-08-31, ~8000
// games across hbt-vs-hbt/hbt-vs-borealis/borealis-vs-borealis/heuristic-vs-
// heuristic, counting champions actually committed via attack_phase() vs
// defense_phase()) -- champions get played on ATTACK far more often than on
// DEFENSE (agents decline blocking much more often than they decline
// attacking): 78.21%/21.79% combined, consistent within a few points across
// every pairing tested (74.8%-82.6%). A card's value for draw-valuation
// purposes is weighted accordingly, rather than assuming a naive 50/50 split
// -- which is what game_constants.h's AVERAGE_POWER_FOR_MULLIGAN (the
// straight average of attack_efficiency/defense_efficiency Jonathan's own
// `power` field encodes) implicitly does. Per Jonathan (2026-08-31):
// attack_efficiency/defense_efficiency/expected_attack/expected_defense/
// power are ALL his own early heuristic guesses, not derived from anything
// authoritative in the ruleset -- this reweighting doesn't make them
// "correct", it just uses them more honestly than a 50/50 assumption would.
#define A13_ATTACK_ROLE_WEIGHT 0.7821f
#define A13_DEFENSE_ROLE_WEIGHT 0.2179f

// Deck-wide mean of A13_ATTACK_ROLE_WEIGHT*attack_efficiency +
// A13_DEFENSE_ROLE_WEIGHT*defense_efficiency across fullDeck[]'s 102
// champion cards -- same derivation method as AVERAGE_POWER_FOR_MULLIGAN (a
// deck-wide mean of a per-card value function), role-weighted instead of
// 50/50. This is `draw_value`'s comparison baseline (ai_strat_a13_enum.c);
// both must move together if the weighting above is ever re-measured.
#define A13_AVERAGE_CARD_VALUE 5.51528f

typedef struct
{ uint8_t pool_n;          // |U|, the unseen-pool size
  float pool_mean_value;  // E[role-weighted value of a card drawn from the
  // pool right now] -- see A13_ATTACK_ROLE_WEIGHT's comment above
  float draw_value;       // pool_mean_value, blended toward the observer's own
  // discard-pile mean near a deck-exhaustion/reshuffle boundary (Layer D)

  uint8_t k_max;           // opponent's current PUBLIC hand size, capped at
  // A13_MAX_HAND_K; p_k[]/e_block_given_k[] are valid for k = 0..k_max
  float p_k[A13_MAX_HAND_K + 1];             // hypergeometric P(K = k), K =
  // number of the opponent's hidden hand cards that are champions
  float e_block_given_k[A13_MAX_HAND_K + 1]; // E[best <=3-champion
  // expected_defense sum | opponent holds exactly k champions]

  float e_opp_block;  // Sum_k p_k[k] * e_block_given_k[k] -- the aggregate
  // mean the Jensen correction in ai_strat_a13_enum.c improves on
  float e_opp_attack; // same aggregate mean, over expected_attack instead of
  // expected_defense -- Layer R's opp_dpt input when race_use_belief_opp
} A13Belief;

// Builds the belief `observer` can compute about `1 - observer`'s hidden
// hand and near-term draws, from public state only -- see this file's own
// header comment above and ai_strat_a13.h's "Layer K"/"Layer D" sections for
// what each field means and the anti-clairvoyance pool-derivation rule.
A13Belief a13_build_belief(const struct gamestate* gstate, PlayerID observer,
                           float reshuffle_trust, float block_combo_bonus);

#endif // AI_STRAT_A13_BELIEF_H
