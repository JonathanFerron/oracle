// ai_strat_a13.h
// A13 Cartographer strategy ("The Cartographer") -- see
// doc/ai_agents.md's A13 section for the full
// design record and rationale index. Design-only status as of 2026-08-31;
// this header is the approved design, written before ai_strat_a13.c.
//
// A7 Hybrid HBT's exact three-layer synthesis (ai_strat_hbt.h/.c,
// ai_strat_hbt_enum.h/.c), inherited verbatim as `.base` below, plus four new
// deterministic layers. The unifying idea across all four: model hidden
// information as a distribution computed in closed form, never as a point
// estimate, and never simulate the opponent's decision -- a direct answer to
// A9 HBT 2-Ply's failure (see "The A9 precedent" below).
//
// ==== Ship gates: two, not one ====
// A Borealis-scale rating above A7's 65 is necessary but NOT sufficient to
// ship this agent. A7 itself rated above A5 in the roster-wide fit while
// losing 26.0% head-to-head to A5 specifically; A9 rated 59 (above A6's 52)
// while measuring only 47.2% head-to-head against A7, the exact opponent its
// own added ply targeted (see both agents' about.md). A roster-wide fit and a
// specific pairwise matchup can and do come apart. This agent's actual ship
// gate (see aicalibsrc/carto/'s calibration plan) is therefore BOTH: (1) a
// --stda.rating fit above 65, and (2) a 40,000-game, both-seats head-to-head
// against `hbt` with a Wilson 95% CI lower bound strictly above 50%. Gate (2)
// is the one that actually establishes this agent is stronger than the agent
// it's built on top of.
//
// ==== The A9 precedent (why nothing here simulates a decision) ====
// A9 added a deterministic one-ply opponent reply against a single fabricated
// "surrogate hand" (ai_strat_hbt2ply_reply.c's build_surrogate_hand()),
// blended by reply_trust. Win rate vs A7 declined MONOTONICALLY with trust --
// 47.6/43.4/39.4/37.3/31.2% at trust 0/0.25/0.5/0.75/1.0 -- so reply_trust=0,
// which provably recovers A7 exactly, was the optimum on the entire curve
// (ai_strat_hbt2ply.h). A8/A12's uniform/cheap rollout policies (measured 35,
// 31) showed the same class of defect from a different angle: an estimator
// with a systematic bias that more computation cannot correct.
//
// This agent's structural answer: never fabricate one hidden hand and score a
// decision against it. Every belief quantity below is a closed-form
// expectation over the exact unseen-card pool, derived once per decision by
// subtraction from the known 120-card fullDeck[] -- the same pool
// ai_strat_playout.c's mc_determinize() (A8's determinization) re-deals from
// by sampling. This agent computes analytically what that function samples.
// See ai_strat_common.h's strat_common_unseen_pool() for the pool itself.
//
// ==== Anti-clairvoyance rule ====
// The unseen pool is 120 - own_hand - both_discards - both_combat_zones. It
// deliberately does NOT subtract the observer's own deck -- a player does not
// know their own deck's contents either (doc/game_rules_doc.md), and
// strat_common_unseen_pool()'s own header comment states this explicitly.
// Never read deck[*].card_indices directly, and never read
// gstate->hand[opponent].cards -- same information-hiding constraint A5/A7/A9
// inherit (see below), enforced by review, not by the compiler.
//
// ==== Layer R: race arithmetic (state-dependent variance) ====
// Deterministic turns-to-kill both ways, using a CONTINUOUS ratio (never
// ceil() -- ceil() is discontinuous and would make the dial jumpy exactly
// when my_dpt is small, i.e. early game):
//
//   my_dpt  = best-affordable-subset predicted_damage() right now,
//             minus belief_opp_block_trust * (this turn's expected block)
//   opp_dpt = race_use_belief_opp ? (belief-derived expected opponent attack)
//                                 : an estimate_opponent_power()-style proxy
//   ttk_me  = clamp(opp_energy / max(my_dpt,  0.5), 1, 30)
//   ttk_opp = clamp(own_energy / max(opp_dpt, 0.5), 1, 30)
//   race    = ttk_opp - ttk_me + (I am the current attacker ? +0.5 : -0.5)
//   u       = clamp(race / race_scale, -1, +1)   -- only when race_scale > 0
//
//   stdev_eff = base.defense_stdev_mult
//             + race_stdev_ahead  * max(u, 0)
//             + race_stdev_behind * min(u, 0)
//   eps_race  = 1 + race_eps_gain * u
//
// SIGN CONVENTION (read this before touching either race_stdev_* dial):
// base.defense_stdev_mult > 0 INFLATES the threat estimate, which makes the
// agent BLOCK MORE (ai_strat_hbt.h's "Implementation note" on this exact
// dial). Blocking is the LOW-variance choice -- it spends cards to compress
// the damage spread. So "ahead in the race => avoid variance" means
// race_stdev_ahead > 0 (block more when ahead) and race_stdev_behind < 0
// (block less, seek variance, when behind). A sweep with the wrong sign will
// look like a dead mechanism and is not -- this is the exact bug class
// ai_strat_hbt.h documents for balanced_tactical_hbt_comparison.md's original
// sketch. This is the direct completion of A7's own signed-dial unification
// of A4's deflation and A6's inflation into one field -- and it requires NO
// change to any A7 file: `incoming` is already a float parameter to
// evaluate_defense_subset() (ai_strat_hbt_enum.h), so this agent computes
// stdev_eff itself and passes the resulting float straight in. base.* is
// never mutated.
//
// ==== Layer K (draw): deck-aware draw valuation, plus Layer D (reshuffle
// boundary) folded into the same belief pass ====
// A5/A7's gamma treats every card in a DRAW candidate's yield as equally
// likely; the true expected value of "the next card(s) I draw" is the mean of
// the live unseen pool, which drifts (usually upward, since low-value cards
// clear the pool disproportionately as champions get played and discarded)
// as the game progresses. A DRAW candidate's score in the enumeration gains:
//
//   belief_draw_weight * draw_num * (draw_value - A13_AVERAGE_CARD_VALUE)
//
// where draw_value is normally the pool mean, but is blended toward the mean
// of the OBSERVER'S OWN DISCARD PILE (visible to them exactly, card by card)
// in proportion to belief_reshuffle_trust and how much of the draw would come
// from a reshuffle -- deck[player].top + 1 is public pile height, and on
// exhaustion a player's own discard reshuffles back into their own deck
// (card_actions.c). Near that boundary the true expected draw is not the
// diffuse pool mean but a sharp, exactly-known number.
//
// CORRECTION during Stage 2 calibration (2026-08-31): the per-card value
// function was originally just fullDeck[].power, averaged over the WHOLE
// pool including non-champion cards, compared against the deck-wide
// AVERAGE_POWER_FOR_MULLIGAN. Two compounding bugs: (1) draw/cash cards
// carry much lower power than champions, so averaging over the whole pool
// (not champions only) diluted draw_value below its comparison baseline by a
// large, mostly-constant amount unrelated to real pool depletion -- this
// alone made only strongly-negative belief_draw_weight look good and
// positive weight look catastrophic in an early sweep (34.7% win rate at
// +2.0!), a bug signature, not a real finding; (2) even after restricting to
// champions, `power` itself (per Jonathan, 2026-08-31) is just his own early
// heuristic guess -- specifically the STRAIGHT 50/50 average of
// attack_efficiency/defense_efficiency -- with no basis for assuming a card
// is equally likely to be played on attack vs defense. Measured directly
// instead (ai_strat_a13_belief.h's A13_ATTACK_ROLE_WEIGHT comment): counting
// actual champion commitments across ~8000 games (hbt/borealis/heuristic
// pairings) gives 78.21% attack / 21.79% defense, consistent within a few
// points across every pairing tested. ai_strat_a13_belief.h's
// pool_mean_value()/A13_AVERAGE_CARD_VALUE now use this weighting on
// attack_efficiency/defense_efficiency directly, both fixes shipped
// together. See doc/changelog.md and the project_a13_cartographer memory
// for the full calibration record.
//
// CORRECTION during implementation: the design plan originally called for a
// third opponent-power dial here (belief_opp_power_trust), blending A6-style
// estimate_opponent_power() toward the pool's mean card value for the SAME
// opp_dpt quantity race_use_belief_opp already selects between. Building
// ai_strat_a13_state.c surfaced two problems with that: (1) it would have
// been a redundant THIRD tier on a quantity race_use_belief_opp already
// makes a clean two-tier choice for (belief-independent fallback vs full
// belief), and (2) a genuine units mismatch -- the fallback proxy is
// naturally expressed in expected_attack units (it feeds opp_dpt directly),
// while the pool's mean card value is a separate currency (Layer K-draw's
// own, now role-weighted per the correction above), and blending the two
// directly would have been dimensionally wrong. Dropped entirely rather
// than shipped broken; race_use_belief_opp is this agent's only opponent-
// capability-estimate toggle. See
// ai_strat_a13_state.c's opp_dpt_fallback().
//
// ==== Layer K (block): Jensen-corrected expected block ====
// The version that does NOT replay A9's failure. A naive
// max(raw(S) - E[block], 0) is a monotone transform of raw(S), because
// E[block] does not depend on which subset S is chosen -- it only
// discourages attacking uniformly relative to pass/draw/cash, which is
// exactly A9's documented failure signature, and is already foreclosed by
// ai_strat_heuristic.h's own header ("any constant-fraction block model ...
// is a positive rescaling of the attack term and is therefore already
// absorbed into epsilon"). The corrected, non-degenerate version conditions
// on K = the number of affordable champions in the opponent's hand, a
// genuinely hypergeometric random variable with real spread over 0..h:
//
//   E[net(S)] = Sum_{k=0..h} P(K=k) * min(max(raw(S) - E[block|k], 0), opp_energy)
//   E[block|k] = Sum_{i=1..min(k,3)} E[X_(i)] + hplus_block_combo * max(min(k,3)-1, 0)
//
// where X_(i) is the i-th largest expected_defense order statistic over the
// opponent's (hypothetical) affordable champions. max(.,0) is convex, so
// Jensen's gap E[max(raw-B,0)] - max(raw-E[B],0) is strictly positive and
// SHRINKS as raw(S) grows -- it penalises small, easily-absorbed attacks more
// than committed 2-3 champion subsets. Subset-dependent, and the opposite
// sign to A9's mechanism (which could only ever discourage committing
// champions). This is the highest-risk layer -- see about.md's risk ranking.
// hplus_trust blends this corrected net-damage term against A5/A7's
// undefended predicted_damage(); 0 recovers A7's attack scoring exactly.
//
// ==== Cost discipline (read before adding anything to the belief) ====
// Nothing inside the enumeration loop may touch the belief. A13Belief is
// computed ONCE per call into a13_attack_strategy()/a13_defense_strategy(),
// immediately before enumeration -- the same lifetime as A7's own
// hbt_evaluate_state() (ai_strat_hbt.c). Every candidate subset then reads
// only pre-computed floats out of it. Violating this is the one way this
// agent accidentally becomes an A8-class agent by cost.
//
// No cross-call cache, no static, no turn stamp: that would be hidden mutable
// global state (against CLAUDE.md's GameContext-threading rule) and would be
// wrong under calibration, where two seats carry two different parameter
// sets in one process.
//
// ==== The superset guarantee ====
// There exists one parameter vector -- every dial below at its listed neutral
// value -- under which this agent is A7 BIT-FOR-BIT: same move, same
// tie-breaks, same mulligan, same discard. Enforced by SHORT-CIRCUIT
// (a13_belief_needed(), skipping the entire belief computation and cost when
// every belief-consuming dial is neutral), not by multiplying contributions
// by zero -- this makes recovery exact regardless of any degenerate-pool
// arithmetic (e.g. h=0, or an empty pool) and collapses this agent's cost to
// A7's in the neutral configuration, which is what makes the Stage 0 timing
// check in aicalibsrc/carto/ meaningful. This is the same property that made
// A9's own failure cleanly diagnosable, and it gives every one of the four
// layers above an independent, safe fallback: a layer that does not
// calibrate is pinned at its neutral value and the agent degrades toward A7,
// never toward something worse.
//
// ==== Mulligan / discard-to-7 ====
// Inherited from A7 exactly, via hbt_mulligan_with()/hbt_discard_to_7_with()
// (ai_strat_hbt.h) called with &g_params[player].base -- no third local port
// of A3's protect-the-combo shape. This agent introduces no new mulligan/
// discard behaviour: none of the four new layers above have anything to say
// about which cards to keep.
//
// Information hiding: same constraint as A5/A7/A9 -- may read
// gstate->hand[opponent].size, current_cash_balance[opponent],
// current_energy[opponent], and combat_zone[opponent] (all public), and must
// never read which cards are in gstate->hand[opponent] or in either deck.

#ifndef AI_STRAT_A13_H
#define AI_STRAT_A13_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_hbt.h"

typedef struct
{ HBTParams base; // A7's 34 fields, frozen at hbt_get_default_params() --
  // this agent re-derives none of them. Only base.defense_stdev_mult is ever
  // re-fit for this agent specifically (Layer R turns it from THE value into
  // a BASELINE of a now state-dependent quantity); the other 33 fields are
  // hard-pinned with no --identity-safe escape hatch, same reasoning as A9's
  // (there is nothing of A7's own tuning to erode).

  // -- Layer R: race arithmetic (5 fields) --
  float race_scale;         // [0,12], neutral 0  -- TTK interpolation width; 0 = off
  float race_stdev_ahead;   // [-2,2], neutral 0  -- expect > 0, see sign convention above
  float race_stdev_behind;  // [-2,2], neutral 0  -- expect < 0, see sign convention above
  float race_eps_gain;      // [-1,1], neutral 0  -- TTK-driven epsilon modulation
  bool race_use_belief_opp; // neutral false      -- opp_dpt source; lets Layer R ship if
  // Layer K's opponent-block belief doesn't calibrate

  // -- Layer K (draw) + Layer D (reshuffle boundary) (2 fields) --
  float belief_draw_weight;     // [-2,2], neutral 0 -- DRAW candidate bonus weight
  float belief_reshuffle_trust; // [0,1],  neutral 0 -- blend toward own-discard mean
  // near a deck-exhaustion boundary

  // -- Layer K (block): Jensen-corrected expected block (3 fields) --
  float belief_opp_block_trust; // [0,1], neutral 0 -- the A13 analogue of A9's
  // reply_trust; sweep it exactly as A9's was (about.md's risk #1/#2)
  float hplus_trust;            // [0,1],  neutral 0 -- blend predicted_damage() with
  // the Jensen-corrected E[net(S)]
  float hplus_block_combo;      // [0,10], neutral 0 -- expected combo bonus per
  // blocker beyond the first, inside E[block|k]
} A13Params;

void a13_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void a13_defense_strategy(struct gamestate* gstate, GameContext* ctx);

A13Params a13_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/carto/), settable per player
// -- same pattern as hbt_set_params() and every other calibrated agent. Not
// part of the general strategy framework: normal play always uses the
// compiled defaults, since nothing else calls these.
void a13_set_params(PlayerID player, const A13Params* params);
void a13_reset_params(void);

// StrategySet mulligan_strategy[]/discard_strategy[] overrides -- thin
// wrappers over hbt_discard_to_7_with()/hbt_mulligan_with() (ai_strat_hbt.h)
// called with &g_params[player].base, per "Mulligan / discard-to-7" above.
void a13_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void a13_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);

#endif // AI_STRAT_A13_H
