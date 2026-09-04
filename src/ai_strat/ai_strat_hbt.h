// ai_strat_hbt.h
// A7 Hybrid HBT strategy ("The Grandmaster") -- see doc/ai_agents.md's A7
// section for the full design record and rationale. A
// fixed three-layer synthesis of three already-implemented agents, in this
// order and no other: A4 Balanced Rules **weights** the advantage function
// via a soft resource-shortfall penalty (see below for why this is a
// penalty, not doc/ai_agents.md's A7 section's literal "filter"), A6
// Tactical **weights** the same advantage function dynamically by game
// phase/aggression, and A5 Heuristic **ranks** every enumerated move by the
// resulting weighted advantage. Named "The Grandmaster" as synthesis of the
// three approaches below it on the ladder.
//
// Move generation and the advantage function itself are A5's verbatim
// mechanism (ai_strat_heuristic.h/.c): closed-form 1-move lookahead, argmax
// over {pass, every affordable 1-3 champion subset, every affordable draw
// card, every affordable cash card}. No clone_gamestate()/apply_move() --
// cloning would pull draw_1_card() from the shared GameContext RNG stream
// and perturb every downstream game, the same constraint A5's own header
// documents. Fully deterministic: no A3-style epsilon tie-break.
//
// ==== Deliberate deviation from the original design sketch: A4 enters as
// a SOFT PENALTY, not a hard filter ====
// The original design intent was "A4 Balanced Rules filters viable moves by
// resource constraints" -- drop any move whose cost exceeds a resource-derived budget before ranking. That
// is A4's own mechanism, and A4's own calibration comment
// (ai_strat_balanced_rules.c's BALANCED_DEFAULTS) documents why it is
// risky: a traced game under A4's original spec-derived slope showed 4 of 5
// early turns passing outright because the filter left no legal move, and
// two independent free `optimize` searches drove both target slopes toward
// 0 trying to escape that exact starvation. A4 is also the weakest of the
// three source agents (measured rating 36, versus A5's 60 and A6's 52) --
// bolting its hard-filter form onto A5's enumerator is the single likeliest
// way this agent would land below all three of its own ingredients, which
// would fail this agent's whole reason for existing. Reading "filter" as
// "shape the ranking toward the target, without ever deleting a legal move
// the ranking would otherwise pick" keeps A4's actual contribution --
// resource-target awareness -- while removing its one demonstrated failure
// mode. A penalty of 0 (both penalty_*_weight fields at 0) recovers exactly
// A4's target formula having no effect at all, i.e. pure A5+A6; nothing
// about this choice removes A4's formula, only how it enters scoring.
//
// ==== Corrected sign versus ideas/G1 .../balanced_tactical_hbt_comparison.md ====
// That file's sketch scales target_cash by `(1 + (aggression - 0.5) * 0.4)`
// -- i.e. HIGHER aggression means a LARGER cash reserve target, spending
// LESS while aggressive. That contradicts A4's own late_game_aggro, which
// DIVIDES targets down (spends MORE) as a kill becomes reachable, and
// contradicts doc/ai_agents.md's A7 section's own prose two paragraphs later ("delta
// decreases as opponent energy drops -- spend cash to finish them"). This
// agent ships the opposite sign: higher aggression scales targets DOWN
// (target_aggr_cash_scale/target_aggr_cards_scale enter as `(1 - scale *
// (aggression - 0.5))`), matching both A4's own late-game behaviour and the
// design notes' own stated intent. See doc/changelog.md for this correction.
//
// ==== Layer T: aggression, ported verbatim from A6 ====
// game_phase(), hand_power_sum(), estimate_opponent_power(),
// calculate_aggression_factor() are A6's own formulas
// (ai_strat_tactical.c), computed once per turn into an aggression scalar
// a in [0,1]. Unlike A6, this agent does not use aggression to gate an
// attacker COUNT -- A5's enumerator already considers every subset size, so
// there is no count decision to gate. Instead, aggression modulates A5's
// weights:
//
//   eps_eff   = eps   * (1 + aggr_energy_gain   * 2*(a - 0.5))
//   gamma_eff = gamma * (1 - aggr_resource_fade * 2*(a - 0.5))
//   delta_eff = delta * (1 - aggr_resource_fade * 2*(a - 0.5))
//   if opp_phase == PHASE_CRITICAL: eps_eff *= critical_epsilon_mult
//
// matching doc/ai_agents.md's A7 section's own "epsilon increases in critical phases;
// gamma/delta decrease as opponent energy drops" -- except driven by the
// aggression scalar (a richer, multi-factor signal) rather than opp_energy
// alone, and by a dedicated critical-phase multiplier for the phase
// transition specifically. A5's own taper (weight_taper_exponent, a smooth
// function of opp_energy) is kept unchanged alongside this -- it is A5's
// mechanism, distinct from the aggression fade (a multi-factor scalar), and
// calibration (not an a-priori merge) resolves whether the two overlap.
//
// ==== Layer B: resource targets, ported from A4, entering as a penalty ====
// resource_targets() is A4's formula verbatim (ai_strat_balanced_rules.c),
// then aggression-scaled per the corrected sign above. The penalty applies
// to the RAW post-move hand size/cash (not A4's effective_hand_and_cash()
// estimate) -- A5's enumerator already models a draw card's real effect as
// a distinct candidate move, so stacking an effective-hand estimate on top
// would double-count it (the same argument ai_strat_heuristic.h makes for
// why A5 itself uses raw hand size):
//
//   penalty = penalty_cash_weight  * max(0, target_cash  - post_cash)
//           + penalty_cards_weight * max(0, target_cards - post_hand)
//   advantage = heuristic_advantage(..., eps_eff, gamma_eff, delta_eff) - penalty
//
// ==== Combo hold, ported from A3 ====
// A5 already scores the REALIZED combo bonus inside predicted_damage()/
// predicted_block() for every enumerated subset -- that is genuine
// combo-aware selection, inherited for free. What A5 cannot do on its own is
// recognise a combo worth holding back for a finishing blow, since its
// 1-move lookahead has no concept of "not yet". This agent ports A3
// Borealis's is_held_combo() rule verbatim (ai_strat_borealis_enum.c):
// exclude an attack subset from consideration when hold_lethal_combos is
// set, it has >= 2 cards, its combo bonus clears lethal_combo_bonus, the
// opponent's energy is still above lethal_hold_ceiling, and the subset
// wouldn't finish them off right now. Attack only -- holding back a
// defensive play makes no sense. This, plus the mulligan/discard overrides
// below, is this agent's answer to "should feel combo-aware, not just
// combo-lucky" next to a human opponent.
//
// ==== Defense: A5's enumeration, A4/A6's threat estimate unified into one
// signed dial ====
// Full 0-3 subset enumeration exactly like A5's best_defense_move(), scored
// by the same aggression-modulated advantage, but against a variance-aware
// incoming-attack estimate instead of the raw expected value:
//
//   incoming = expected_incoming_attack(...) +
//              defense_stdev_mult * sqrt(Sum champion_variance(...))
//
// defense_stdev_mult < 0 reproduces A4's E[Attack] - beta*sigma deflation
// (defend against less than the mean, on the theory that dice variance cuts
// both ways); > 0 reproduces A6's inflation (defend against more than the
// mean, conservatively); 0 recovers A5's plain expected value. One sign
// spans both conventions rather than forcing a choice a priori --
// calibration decides which this agent's identity needs.
//
// ==== Mulligan / discard-to-7 ====
// hbt_mulligan()/hbt_discard_to_7() are a LOCAL port of A3's
// borealis_mulligan()/borealis_discard_to_7() shape (find the best held
// combo, protect it, discard/mulligan the lowest-value unprotected card,
// with a two-pass fallback so protection can never stall) -- not a call
// into A3's own functions. Two reasons: (1) borealis_discard_to_7() reads
// Borealis's own g_params[player], so calling it here would make this
// agent's discard behaviour depend on Borealis's calibration and would
// break under calib_hbt --opponent borealis (which sets Borealis's params
// per seat for the match); (2) A3 is the Bradley-Terry rating anchor for
// every agent measured so far -- a refactor of it for reuse is pure
// downside risk to every rating already shipped. Victim valuation here uses
// the A4/A6 efficiency ratio expected_attack/(cost + HBT_COST_FLOOR) rather
// than A3's expected_attack - lambda*cost, so this agent introduces no new
// lambda dial; CLAUDE.md's "manual/duplicated code is preferred over
// macro-magic abstractions" covers the duplication.
//
// Information hiding: same constraint as A5 -- may read
// gstate->hand[opponent].size, current_cash_balance[opponent],
// current_energy[opponent], and combat_zone[opponent] (all public), and
// must never read which cards are in gstate->hand[opponent].

#ifndef AI_STRAT_HBT_H
#define AI_STRAT_HBT_H

#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ // -- Layer H: A5's advantage weights (ai_strat_heuristic.h) --
  float weight_energy_advantage; // epsilon; see ai_strat_heuristic.h
  float weight_cards_advantage;  // gamma
  float weight_cash_advantage;   // delta; PINNED at 1.0, same scale-invariance
  // redundancy as A5 -- see ai_strat_heuristic.h
  float weight_taper_exponent;   // taper exponent on gamma/delta vs opp_energy
  float opp_card_discount;       // discount on hidden opponent hand size

  // -- Layer T: A6's phase/aggression formula (ai_strat_tactical.h) --
  uint8_t phase_mid_threshold;
  uint8_t phase_late_threshold;
  uint8_t phase_critical_threshold;
  float aggression_energy_diff_weight;
  float aggression_opp_late_bonus;
  float aggression_opp_critical_bonus;
  float aggression_self_late_penalty;
  float aggression_self_critical_penalty;
  float aggression_hand_power_bonus;
  float aggression_hand_power_penalty;
  uint16_t aggression_cash_surplus_threshold;
  float aggression_cash_surplus_bonus;

  // -- T -> H weight coupling (new to this agent) --
  float aggr_energy_gain;      // epsilon gain per unit of (aggression - 0.5), doubled
  float aggr_resource_fade;    // gamma/delta fade per unit of (aggression - 0.5), doubled
  float critical_epsilon_mult; // extra epsilon multiplier when opponent is PHASE_CRITICAL

  // -- Layer B: A4's resource targets (ai_strat_balanced_rules.h) --
  float target_cash_slope;
  float target_cash_intercept;
  float target_cards_slope;
  float target_cards_intercept;
  float late_game_aggro;
  int   lethal_horizon;
  float target_aggr_cash_scale;  // corrected sign vs the G1 sketch -- see header comment
  float target_aggr_cards_scale;

  // -- B -> H penalty coupling (new to this agent) --
  float penalty_cash_weight;  // rho:  penalty per luna short of target_cash
  float penalty_cards_weight; // rho_c: penalty per card short of target_cards

  // -- Combo hold, from A3 (ai_strat_borealis.h) --
  bool  hold_lethal_combos;
  int   lethal_combo_bonus;
  int   lethal_hold_ceiling;

  // -- Defense --
  float defense_stdev_mult; // signed: <0 = A4-style deflation, >0 = A6-style
  // inflation, 0 = A5's plain expected value
} HBTParams;

void hbt_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void hbt_defense_strategy(struct gamestate* gstate, GameContext* ctx);

HBTParams hbt_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/hbt/), settable per player
// so two different parameter sets can play each other in one process/game --
// same pattern as heuristic_set_params()/tactical_set_params()/
// balanced_rules_set_params()/borealis_set_params(). Not part of the general
// strategy framework: normal play always uses the compiled defaults, since
// nothing else calls these.
void hbt_set_params(PlayerID player, const HBTParams* params);
void hbt_reset_params(void);

// StrategySet mulligan_strategy[]/discard_strategy[] overrides -- see
// ai_strategy.h and this file's header comment on why these are a local
// port of A3's shape rather than a call into A3's own functions.
void hbt_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void hbt_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);

// Same behaviour as hbt_discard_to_7()/hbt_mulligan(), but scored against an
// explicit params pointer instead of hbt_live_params(player) -- lets A13
// inherit this agent's exact mulligan/discard behaviour (via &a13_params->base)
// without a third local port of A3's shape. hbt_discard_to_7()/hbt_mulligan()
// are one-line wrappers calling these with hbt_live_params(player).
void hbt_discard_to_7_with(struct gamestate* gstate, PlayerID player, GameContext* ctx,
                           const HBTParams* params);
void hbt_mulligan_with(struct gamestate* gstate, PlayerID player, GameContext* ctx,
                       const HBTParams* params);

#endif // AI_STRAT_HBT_H
