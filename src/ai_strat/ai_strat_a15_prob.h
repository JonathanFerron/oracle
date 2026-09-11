// ai_strat_a15_prob.h
// A15 Risk Threshold's exact combat-probability math -- the one genuinely
// new piece of infrastructure this agent needs. R4's threshold ("decline to
// defend unless P(death this combat) >= p%") is a real probability, not a
// proxy score, because it's computable exactly: RND_dn(n) (src/util/rnd.c)
// is uniform on [1,n], so the dice-sum contribution of up to 3 committed
// champions is a finite discrete distribution -- a 3-step convolution, not
// an estimate. See ai_strat_a15.h's "R4 -- defense threshold" for the rule
// this serves.

#ifndef AI_STRAT_A15_PROB_H
#define AI_STRAT_A15_PROB_H

#include "../core/game_types.h"

// Every champion's defense_dice is d4/d6/d8/d12/d20 (game_rules_doc.md), and
// a combat zone holds at most 3 champions, so the dice-SUM-only
// contribution (no attack_base, no combo bonus -- those are added by the
// caller, since they're already known scalars, not distributions) is
// bounded to [count, count*20], i.e. at most [0,60] over 0-3 dice.
#define A15_MAX_DICE_SUM 60

// A discrete probability distribution over a dice-sum total, dense on
// [lo,hi] (pmf[i] may be 0 for lo<=i<=hi, e.g. an unreachable sum with mixed
// die sizes, but is never touched outside that range by any function below).
typedef struct
{ float pmf[A15_MAX_DICE_SUM + 1];
  uint8_t lo;
  uint8_t hi;
} A15DicePMF;

// Builds the dice-sum-only PMF for a set of champions' defense_dice via
// direct convolution (die by die): pmf[0]=1 with count=0, and each
// additional champion's die is folded in by spreading each existing mass
// point uniformly over its `sides` outcomes. count must be 0-3, matching
// combo_bonus_for_selection()'s own range convention -- a combat zone can
// hold no more.
void a15_build_dice_pmf(const uint8_t* card_indices, uint8_t count, A15DicePMF* out);

// P(this attack's total damage, if left completely undefended, >= my
// current energy) -- exact, over the attacker's already-committed
// gstate->combat_zone[attacker] (public information once committed; see
// ai_strat_common.h's expected_incoming_attack() for the same access
// pattern). This is R4's core quantity. Call only when the attacker's
// combat zone is non-empty (turn_logic.c's defense_phase() already
// guarantees this before calling any DefenseStrategyFunc).
float a15_p_death_undefended(const struct gamestate* gstate, PlayerID defender);

// Same quantity, but with a candidate defense subset committed: exact via a
// joint sum over (attacker dice outcome x defender dice outcome), since the
// two are independent. `attacker_pmf` is the attacker's dice-sum PMF,
// precomputed ONCE per decision by the caller (R7 evaluates many candidate
// defense subsets, and the attacker's own committed cards don't change
// between candidates -- see ai_strat_a15.h's cost-discipline note) via
// a15_build_dice_pmf() over gstate->combat_zone[1-defender]. defense_cards/
// defense_count describe the candidate subset under consideration (0-3
// champions from the defender's hand, not yet played).
float a15_p_death_with_defense(const struct gamestate* gstate, PlayerID defender,
                               const A15DicePMF* attacker_pmf,
                               const uint8_t* defense_cards, uint8_t defense_count);

// R8's core quantity: P(I can finish `player`'s opponent within the next
// n_attacks of player's own attacks), a NORMAL APPROXIMATION -- unlike R4's
// exact convolution above, this depends on unseen future draws for both
// sides and has no closed form. mu/sigma^2 are built from: `player`'s
// currently-held champions (known exactly) plus (n_attacks-1) additional
// own draws (the doc's own turn arithmetic: a window of N attacks includes
// this turn's already-drawn hand plus N-1 more of player's own future
// draws), each valued at the SHARED unseen pool's mean (both sides' future
// draws come from the same pool symmetrically -- see
// ai_strat_common.h's strat_common_unseen_pool(), the design doc's own
// anti-clairvoyance correction: a player doesn't know their own deck either)
// -- minus the opponent's expected block, modeled via
// A15_ENDGAME_OPP_BLOCK_FREQ (ai_strat_a15_prob.c) applied to the opponent's
// visible hand COUNT (their hand's actual composition is unknown, so it's
// valued at the same shared pool's mean too). n_attacks must be 1-4
// (ai_strat_a15_endgame.c's horizon is always in that range).
float a15_p_finish_within(const struct gamestate* gstate, PlayerID player, uint8_t n_attacks);

#endif // AI_STRAT_A15_PROB_H
