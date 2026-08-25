// ai_strat_tactical.h
// A6 Tactical strategy ("Pressure Cooker") -- see
// ideas/A6 ai agent tactical (pressure cooker)/about.md and
// tactical_design_notes.md (this agent's only written spec, a full but
// partially-unfinished code sketch -- some formulas below are direct ports
// of its own numbers, others fill gaps it left open; both are called out
// below).
//
// Classifies the game into a phase (early/mid/late/critical, by energy
// thresholds) and derives a single 0.0-1.0 aggression factor from energy
// difference, hand power, and cash surplus, then scales how many champions
// to commit by that factor -- "turns up the heat as the position sharpens."
//
// GamePhase(energy) = EARLY    if energy >= phase_mid_threshold       (spec 75)
//                    = MID      if phase_late_threshold <= energy < phase_mid_threshold (spec 40)
//                    = LATE     if phase_critical_threshold <= energy < phase_late_threshold (spec 15)
//                    = CRITICAL if energy < phase_critical_threshold
//
// Aggression = 0.5                                                    (neutral baseline)
//            + (own_energy - opp_energy) * aggression_energy_diff_weight
//            + aggression_opp_critical_bonus if opp_phase == CRITICAL,
//              else aggression_opp_late_bonus if opp_phase == LATE
//            - aggression_self_critical_penalty if my_phase == CRITICAL,
//              else aggression_self_late_penalty if my_phase == LATE
//            + aggression_hand_power_bonus if my_hand_power > opp_estimated_power * 1.5
//            - aggression_hand_power_penalty if my_hand_power < opp_estimated_power * 0.7
//            + aggression_cash_surplus_bonus if own_cash > aggression_cash_surplus_threshold
//   clamped to [0.0, 1.0]. opp_estimated_power = opp_hand_size *
//   AVERAGE_POWER_FOR_MULLIGAN, adjusted by a fixed (non-tunable)
//   cash-tier multiplier -- the sketch's own secondary noise-reduction
//   constants, not a primary strategic dial.
//
// Two gaps the design sketch left open, filled in here rather than left
// ambiguous:
//  - The sketch's GamePhase thresholds (75/40/15) and its aggression
//    "smell blood" cutoffs (independently 20/40) are two separate step
//    functions over the same energy axis. Unified here onto one tunable
//    3-threshold set, shared by GamePhase() and the aggression formula --
//    "classify into a phase, then read the position" is one coherent
//    mechanism, not two overlapping ones.
//  - The sketch calls decide_num_attackers() but never implements it. Filled
//    in as aggression-scaled: desired = 3 if aggression>=0.75, 2 if >=0.5,
//    1 if >=0.25, else 0, then num_attackers = min(desired, min(3,
//    affordable_champion_count)) -- the aggression factor governs quantity,
//    not just a yes/no gate, matching "turns up the heat as the position
//    sharpens". Fixed bands rather than round(aggression * max_playable):
//    playtracing found the proportional-rounding version put
//    max_playable=1's SOLE decision boundary exactly at aggression's
//    neutral baseline (0.5), so routine negative signals (e.g. the
//    hand-power penalty) pushed aggression just below 0.5 often enough that
//    the agent passed on its only affordable champion far more than any
//    other implemented agent ever declines to attack (measured losing to
//    Random before this fix, the only implemented agent to do so). Bands
//    keep the same 4-level escalation while landing the neutral baseline in
//    the ">=0.25" tier instead of exactly on a boundary.
//
// Card selection within that count is greedy combo-aware ranking (the same
// shape as A4's attack_selection_score()/select_attack_champions()), not A3
// Borealis's exhaustive subset-value enumeration -- about.md doesn't forbid
// enumeration for A6, but this keeps A6's complexity budget on the
// phase/aggression mechanism (its actual identity). Unlike A4, combo
// awareness here is unconditionally on (the sketch: "Combo Prioritization:
// Always evaluate 2-3 card combinations, not just individual cards") --
// there is no combo_weight field to switch it off, matching the design
// intent that this agent, unlike combo-blind A4, always looks for combos.
//
// No resource-target formula (A4's target_cash/target_cards): cash/hand-size
// decisions flow from the phase-and-aggression-derived attack count and a
// reused draw-card trigger (ai_strat_common.h's try_play_draw_card(), whose
// opp_energy_floor argument is the SAME phase_critical_threshold used
// everywhere else -- drawing is skipped exactly when the opponent is one
// finishing blow away). about.md's "phase/aggression modelling can consume
// those targets but doesn't replace them" is read as describing A7 Hybrid's
// future synthesis, not a requirement on A6 itself.
//
// Defense is a standalone EV comparison, independent of aggression_factor --
// the sketch's defense function never references it. Evaluates the greedy
// defense-efficiency-ranked prefix of length 0..3 that maximises
// value = -(expected_damage * defense_damage_weight + defense_cost * defense_cash_weight),
// where expected_damage is measured against a *conservative* (inflated, not
// capped) attack estimate:
//   attack_estimate = expected_attack + defense_conservative_stdev_mult * stdev(attack)
// Note the sign -- the opposite of A4's E[Attack] - beta*sigma cap. No
// coupling to aggression_factor is added; this is a deliberate fidelity
// choice, not an oversight.
//
// Reuses two helpers promoted from A4 Balanced Rules into ai_strat_common.h
// once this agent needed the identical formulas: champion_variance()
// (V[Dn] = (n*n-1)/12) and effective_hand_and_cash() (used here for its
// hand-size output only).

#ifndef AI_STRAT_TACTICAL_H
#define AI_STRAT_TACTICAL_H

#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ // Phase thresholds -- shared by GamePhase() and the aggression formula
  uint8_t phase_mid_threshold;      // 67 -- energy at/above -> EARLY; calibrated
  uint8_t phase_late_threshold;     // 41 -- energy at/above -> MID; calibrated
  uint8_t phase_critical_threshold; // 18 -- energy at/above -> LATE, below -> CRITICAL;
  // calibrated. Also reused as try_play_draw_card()'s opp_energy_floor.

  // Aggression factor formula -- calibrated, see ai_strat_tactical.c
  float aggression_energy_diff_weight;        // 0.0008 -- per point of (own-opp) energy
  float aggression_opp_late_bonus;            // 0.126
  float aggression_opp_critical_bonus;        // 0.282
  float aggression_self_late_penalty;         // 0.053
  float aggression_self_critical_penalty;     // 0.148
  float aggression_hand_power_bonus;          // 0.248
  float aggression_hand_power_penalty;        // 0.154
  uint16_t aggression_cash_surplus_threshold; // 10
  float aggression_cash_surplus_bonus;        // 0.230

  // Defense EV weights -- calibrated
  float defense_damage_weight;           // 0.042
  float defense_cash_weight;             // 1.623
  float defense_conservative_stdev_mult; // 1.233

  // Draw-card trigger (try_play_draw_card()'s min_hand_size)
  uint8_t draw_min_hand_size; // 5 -- calibrated
} TacticalParams;

void tactical_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void tactical_defense_strategy(struct gamestate* gstate, GameContext* ctx);

TacticalParams tactical_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/tactical/), settable per
// player so two different parameter sets can play each other in one
// process/game -- same pattern as balanced_rules_set_params()/
// heuristic_set_params(). Not part of the general strategy framework: normal
// play always uses the compiled defaults, since nothing else calls these.
void tactical_set_params(PlayerID player, const TacticalParams* params);
void tactical_reset_params(void);

#endif // AI_STRAT_TACTICAL_H
