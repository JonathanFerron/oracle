// ai_strat_balanced_rules.h
// A4 Balanced Rules strategy ("Bean Counter") -- see
// ideas/A4 ai agent balanced rules (bean counter)/about.md and this agent's
// only written spec, preserved below and in that file: effective hand size,
// effective cash, and priority-ordered attack/defense rules.
//
// Obsessive resource accounting: derives a target cash reserve and a target
// effective hand size directly from the opponent's current energy (linear
// formulas below), spends/holds to hit those targets, and defends by a
// variance-aware rule, E[Total Def] <= E[Total Attack] - beta*sigma, rather
// than a flat threshold. Deliberately combo-blind on selection (combo_weight
// defaults to 0.0 -- see the field comment): combo scoring as a primary
// signal belongs to A2/A3, not this agent. Also deliberately not a
// game-phase state machine (that's A6 Tactical) and not a weighted
// multi-factor advantage function (that's A5 Heuristic) -- one set of
// formulas, not a state machine or a tunable weighted sum.
//
// Effective hand size = actual hand size
//   + 1 * (affordable Draw-2/cost-1 cards held)
//   + 2 * (affordable Draw-3/cost-2 cards held)
// Effective cash = actual cash - total cost of those same held draw cards
// (only while affordable -- an unaffordable draw card is dead weight, not
// future hand size). Both formulas date to ai_strat_balancedrules1.c's
// original spec.
//
// Resource targets: target = slope*(opp_energy - 8) + intercept, clamped at
// >= 0, divided by late_game_aggro once opp_energy drops to/below
// lethal_horizon (spends down harder as a kill becomes reachable). The
// target-cash slope is INITIAL_CASH_DEFAULT/91 -- an earlier draft of this
// agent's spec was anchored to a 19-luna starting stack from an obsolete
// rule set; starting cash is 30 today (game_constants.h), so the ladder was
// re-anchored to hold the actual full starting stack at full opponent
// energy, not a fossil value. The +8/+3 intercepts quoted in some design
// docs (ideas/G1 .../balanced_tactical_hbt_comparison.md,
// ideas/G2 .../ai_params_guide.md) do not reproduce the original spec's own
// numeric tables and were a misreading of its inverse form; both intercepts
// ship at 0.0 here.
//
// Attack: draw a held, affordable draw card (if any) when hand size is
// small and the opponent isn't near-lethal, then STOP -- playing a draw
// card ends the turn's one action (doc/game_rules_doc.md), so it is never
// combined with playing champions in the same turn. Otherwise, play
// round(effective_hand - target_cards) champions (clamped to [0,3]),
// ranked by attack efficiency, as long as doing so keeps effective cash at
// or above target_cash.
//
// Defense: play champions ranked by defense efficiency (tie-break: worst
// attack efficiency first -- spend the cards that are worst on offence),
// while the running total (including the actual combo bonus the selection
// would score) stays at or below
// E[Total Attack] - defense_beta*StdDev[Total Attack]. Declining some
// blocks outright, or stopping short of a full block, is this rule working
// as designed -- doc/game_rules_doc.md's own advice is that taking 5-7
// damage is often better than spending 3 lunas on an unnecessary defender.
// Do not "fix" this by defending more aggressively.
//
// V[Dn] = (n*n - 1)/12 where n is a champion's defense_dice face count
// (combat.c rolls the same die for both attack and defense contributions,
// so one variance formula covers both phases). Computed inline rather than
// precomputed into fullDeck[] -- one multiply, not worth 120 edited rows.

#ifndef AI_STRAT_BALANCED_RULES_H
#define AI_STRAT_BALANCED_RULES_H

#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ float target_cash_slope;      // 0.081 -- calibrated; see BALANCED_DEFAULTS's comment
  float target_cash_intercept;  // -2.73 -- calibrated
  float target_cards_slope;     // 0.036 -- calibrated
  float target_cards_intercept; // -0.99 -- calibrated
  float defense_beta;           // 1.93 -- sigma multiplier in the defense cap; calibrated
  float late_game_aggro;        // 2.09 -- target divisor once opp energy <= lethal_horizon; calibrated
  float combo_weight;           // 0.0 -- SHIPPED BLIND. Nudges attack-selection ranking by
  // the marginal combo bonus a card would add to the champions
  // already chosen this turn; ships at 0.0 (about.md puts combo
  // scoring as a primary signal out of scope for this agent). It
  // exists purely so calibration can measure what combo-awareness
  // is worth to a resource-driven agent -- a shipped non-zero
  // value is a deliberate human call, not an automatic optimizer
  // win (see A2's rejected aggression_level=2.21 precedent,
  // doc/changelog.md). Unrelated to the defense cap's own combo
  // bonus term, which is always computed regardless of this
  // weight -- that one is arithmetic necessity (the true expected
  // total defense the beta*sigma rule compares against), not a
  // selection-priority signal.
  int   lethal_horizon;         // 9 -- calibrated; opp energy at/below which targets relax and
  // the draw step is skipped entirely (the stub's late-game carve-out)
  int   draw2_hand_threshold;   // 6 -- calibrated; play an affordable Draw-2 (cost 1) card below this hand size
  int   draw3_hand_threshold;   // 6 -- calibrated; play an affordable Draw-3 (cost 2) card below this hand size
} BalancedRulesParams;

void balanced_rules_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void balanced_rules_defense_strategy(struct gamestate* gstate, GameContext* ctx);

BalancedRulesParams balanced_rules_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/balanced/), settable per
// player so two different parameter sets can play each other in one
// process/game -- same pattern as value_based_set_params()/
// combo_threshold_set_params()/borealis_set_params(). Not part of the
// general strategy framework: normal play always uses the compiled
// defaults, since nothing else calls these.
void balanced_rules_set_params(PlayerID player, const BalancedRulesParams* params);
void balanced_rules_reset_params(void);

#endif // AI_STRAT_BALANCED_RULES_H
