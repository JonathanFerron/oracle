// ai_strat_a15.h
// A15 Risk Threshold strategy ("The Daredevil" / "Le Casse-Cou" / "El
// Temerario") -- see doc/ai_agents.md's A15 section for the full design
// record. Source: a Q&A transcription of Jonathan's own real-table decision
// procedure (2026-09-08/09), not a design aimed at a rating target -- see
// that section's opening note for what this means for how the agent is
// judged. Scoped to the random deck distribution specifically
// (doc/game_rules_doc.md's "Random Distribution"); monochrome/custom deck
// play is out of scope (ai_strat_common.h's combo_bonus_for_selection()
// hardcodes COMBO_BONUS_RANDOM, so this agent is deck-type-correct only
// under that scope, not "automatically" for every deck type -- a design-doc
// claim to that effect was corrected when this was implemented).
//
// ==== Priority order, first match wins (a15_attack_strategy()) ====
//   R8  P(finish opponent within N attacks) >= q_N,            -- go all-out:
//       any N in 1-4 (smallest satisfying N wins)                best affordable combo (any
//                                                                tier), up to 3 champions --
//                                                                see ai_strat_a15_endgame.h for
//                                                                the fully stateless
//                                                                reformulation of the design
//                                                                doc's stateful wording, and
//                                                                the 2026-09-10 correction
//                                                                that replaced an earlier
//                                                                hand-size-derived N (measured
//                                                                badly decoupled from actual
//                                                                finish probability) with
//                                                                checking all 4 horizons
//                                                                directly every turn
//   R6  complete affordable 3-same-species combo in hand     -- play it now, no deferral
//   R1  turn 1, Player A                                     -- play the held cost-1
//                                                                draw-2 card if any, else
//                                                                the cost-2 draw-3 card
//   R9a hand > 7 AND a complete order/color combo is exposed
//       to the forced discard                                -- play it now (the one
//                                                                exception to color/order
//                                                                combos getting no general
//                                                                "play now" preference)
//   R9c an affordable draw/recall card is held                -- play it: recall facet if
//                                                                the discard can supply
//                                                                choose_num champions and at
//                                                                least one is zero-cost or
//                                                                extends a combo, else draw
//   R9b hand > 7                                               -- attack with just enough
//                                                                champions to relieve the
//                                                                pressure (excess, capped at
//                                                                3), lowest combo-participation
//                                                                first; zero-cost is fine to
//                                                                play, breaking a held combo
//                                                                is not
//   R9d otherwise                                              -- PASS; build combo potential
//
// R9c is checked BEFORE R9b: growing/filtering the hand via a held draw or
// recall card is what actually maximizes combo potential, so it takes
// priority over a pressure-relief attack (Jonathan's correction to an
// earlier draft of this ordering). One direct consequence: at hand 8 with a
// draw-2 card held, R9c fires and the hand becomes 8-1+2=9, so the
// end-of-turn discard grows from 1 card to 2 -- an intended trade (pay a
// card to see two and keep the best 7 of 9), not an oversight.
//
// R3 (luna budget indifference) is a hard rule threaded through every path
// above, not a branch of its own: Cash cards are never played by this
// agent. They leave the hand only via the discard-to-7 victim ordering
// (ai_strat_a15_cards.c's victim_tier() puts them first).
//
// ==== R4/R7 -- defense (a15_defense_strategy()) ====
// R4: decline to defend unless P(this specific undefended attack kills me)
// >= defense_loss_threshold (the "p%" dial, default 0.30) -- an EXACT
// probability, not a proxy score, via ai_strat_a15_prob.h's dice-sum
// convolution over the attacker's already-committed champions.
// R7: if defending, prefer (in order) not using a card from the best combo
// held in hand, fewest cards, most attack_base==0 cards (the agent's
// namesake rule -- attack_base==0 champions have identical expected attack
// and defense contribution, verified against combat.c's dice formula),
// highest combo bonus, lowest attack_base sum -- see
// ai_strat_a15_defense.h.
//
// ==== The shared combo-participation scorer (ai_strat_a15_cards.h) ====
// a15_combo_participation(hand, card) = the highest combo_bonus_for_selection()
// any 2- or 3-card subset of hand containing `card` achieves. This drives
// R9b's play choice, R1/R5's discard-to-7 victim, and R2's mulligan victim
// with one function, replacing the "protect a single best combo" list A7's
// ai_strat_hbt_cards.c uses -- it generalises "don't break my combo" to
// "don't break any of my combos, weighted by what each is worth." Worked
// example (Jonathan, 2026-09-10): a hand of 2 Dragons + 2 Elves + 2 Humans +
// 1 Dwarf + 1 Aven scores the Aven lowest (no species/order/color partner
// at all) and the Dwarf next (Order A, shared with the Elves and Humans) --
// the correct two sacrifices. Zero-cost protection is DISCARD-SIDE ONLY
// (R1/R2/R5): a zero-cost champion is a perfectly fine card to play (R9b),
// only a poor one to throw away.
//
// ==== R2 -- mulligan (ai_strat_a15_cards.c's a15_mulligan()) ====
// Skip the mulligan (Player B only, per stda_auto.c's apply_mulligan())
// only if the hand already holds an affordable 2-3 card combo of any kind,
// OR mulliganing would break one already in hand (species/order/color,
// complete or partial) -- both gates checked before any card is touched.
// Otherwise mulligan up to mulligan_get_max_cards() cards (the same
// below-AVERAGE_POWER_FOR_MULLIGAN count-determination every other agent
// uses), chosen via the shared victim scorer above.
//
// ==== Cost-discipline rule (read before adding anything to the dispatch
// chain or the enumeration loops in ai_strat_a15_attack.c/_defense.c) ====
// Nothing expensive (a15_combo_participation() over the whole hand,
// a15_build_dice_pmf(), a would_discard_expose() simulation) may be
// recomputed inside a per-candidate enumeration loop. Precompute once per
// decision, pass the result in. This agent is deterministic and
// closed-form -- it has no simulation budget to hide behind, so an O(n)
// helper called O(n^2) times is a real, measurable cost, not just style.

#ifndef AI_STRAT_A15_H
#define AI_STRAT_A15_H

#include "../core/game_types.h"
#include "../core/game_context.h"

// 5 free calibration dials plus one structural switch. Named scalars, not
// an array, so the calibration harness's positional argv and the Python
// driver's PARAM_NAMES list stay one-to-one with these declarations (the
// established calib_*.c convention -- see aicalibsrc/hbt2ply/calib_hbt2ply.c).
typedef struct
{ float defense_loss_threshold; // R4's p, [0,1], calibrated default 0.20
  float endgame_q1;             // R8, 4-attack horizon, calibrated default
  // 0.01 -- deliberately a small epsilon, NOT a real confidence bar (see
  // ai_strat_a15.c's A15_DEFAULTS comment: this horizon is checked last,
  // after q2-q4 have all failed, so its job is "don't rule out pursuing a
  // win," not "confidently predict one")
  float endgame_q2;             // R8, 3-attack horizon, calibrated default
  // 0.78 -- a genuine confidence bar, unlike q1
  float endgame_q3;             // R8, 2-attack horizon, calibrated default 0.75
  float endgame_q4;             // R8, 1-attack horizon, calibrated default 0.69
  bool endgame_enabled;         // default true -- an ablation switch for
  // calibration (aicalibsrc/daredevil/), not a "feature not built yet"
  // flag: measured LOAD-BEARING (7.06% vs borealis with this off, 46.56%
  // with it on and calibrated -- ai_strat_a15.c's A15_DEFAULTS comment).
} A15Params;

void a15_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void a15_defense_strategy(struct gamestate* gstate, GameContext* ctx);

// StrategySet mulligan_strategy[]/discard_strategy[] overrides -- R2 and
// R1/R3/R5 respectively. Implemented in ai_strat_a15_cards.c (declared
// there too, for that file's own internal use); re-declared here so
// ai_strategy.c's STRATEGY_REGISTRY[] needs only this one header, matching
// every other agent's single-include-point pattern (e.g. ai_strat_a13.h).
void a15_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void a15_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);

A15Params a15_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/daredevil/ once it
// exists), settable per player -- same pattern as every other calibrated
// agent (e.g. combo_threshold_set_params(), ai_strat_combo_threshold.h).
// Not part of the general strategy framework: normal play always uses the
// compiled defaults, since nothing else calls these.
void a15_set_params(PlayerID player, const A15Params* params);
void a15_reset_params(void);

#endif // AI_STRAT_A15_H
