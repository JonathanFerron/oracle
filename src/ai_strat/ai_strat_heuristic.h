// ai_strat_heuristic.h
// A5 Heuristic strategy ("Eps-Gam-Del") -- see
// doc/ai_agents.md's A5 section and this agent's only
// written spec, preserved below and in ai_strat_heuristic1.c's original
// design-comment stub (deleted; its prose lives on here).
//
// Reduces the whole position to one weighted advantage function and picks
// the legal move (1-move lookahead, no opponent-response simulation) that
// maximises it after being applied:
//
//   taper     = (opp_energy / INITIAL_ENERGY_DEFAULT) ^ weight_taper_exponent
//   EnergyAdv = own_energy - opp_energy, +/- HEUR_LETHAL_BONUS at 0 energy
//   CardsAdv  = own_hand_size - opp_hand_size * opp_card_discount
//   CashAdv   = own_cash - opp_cash
//   Advantage = epsilon*EnergyAdv + taper*gamma*CardsAdv + taper*delta*CashAdv
//
// "Effective cards": the own side is the raw hand count, not an A4-style
// effective-hand-size estimate -- the move enumerator already models a draw
// card's real effect (hand -1 +draw_num) as a distinct candidate move, so
// stacking an effective-hand estimate on top would double-count it. The
// opponent side is discounted by opp_card_discount because their hand is
// hidden -- exactly the stub's own "as we don't know the details of the
// opponent's hand" parenthetical.
//
// Information hiding: this agent may read gstate->hand[opponent].size,
// current_cash_balance[opponent], current_energy[opponent], and
// combat_zone[opponent] (all public in a real game of this), and must never
// read which cards are in gstate->hand[opponent] -- enforced by review, not
// by the compiler.
//
// No opponent block is modelled when scoring an attack: predicted_damage()
// clamps Sigma(expected_attack)+combo_bonus at opp_energy and nothing more.
// Any constant-fraction block model (1-f)*E[attack] is a positive rescaling
// of the attack term and is therefore already absorbed into epsilon, so
// adding one would be a redundant, unidentifiable parameter, not a real
// degree of freedom. Known risk this carries: the agent can claim lethal
// optimistically since no block is subtracted before the clamp. If
// calibration playtracing finds this a real problem (A4's bootstrap-trap
// precedent, doc/changelog.md 2026-08-24), the fix is a parameter-free block
// estimate from public information, not a new dial.
//
// weight_cash_advantage (delta) is pinned at its default during calibration:
// the argmax of a weighted sum of three terms is invariant to a positive
// rescaling of all three weights, so one is redundant -- the same
// conclusion ideas/G2 .../calibration_example.txt reaches ("keep delta
// fixed at 1.0"). It stays a struct field for readability and because the
// about.md-stated identity is "its three weights", not two.
//
// Two about.md-vs-design-docs tensions resolved here, not left ambiguous:
//  - about.md excludes "dynamic/adaptive weights" as A6 Tactical's territory,
//    but the stub, ideas/G1 .../balanced_tactical_hbt_comparison.md's sketch,
//    and ideas/G2 .../ai_params_guide.md all call for gamma/delta to taper
//    with opponent energy. Read as: about.md's exclusion targets A6's
//    game-phase *state machine*, not a smooth function of one public scalar
//    -- so the taper ships as the single weight_taper_exponent dial (0.0
//    recovers strictly fixed weights; 1.0 reproduces the G1 sketch's linear
//    opp_energy/99).
//  - about.md lists "subset enumeration ... as primary logic" out of scope,
//    but that targets A3's *decision rule* (maximise raw subset value).
//    Here enumeration is only the move generator the stub itself demands
//    ("among all the possible moves"); the decision rule is the weighted
//    sum above.
// The stub's further proposal of a hand-power / probability-weighted
// combo-potential term is deferred entirely (doc/oracle_todo.md tracks it as
// a follow-up), since about.md calls it an open question and out of scope
// as primary logic.

#ifndef AI_STRAT_HEURISTIC_H
#define AI_STRAT_HEURISTIC_H

#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ float weight_energy_advantage; // 0.349 -- epsilon; calibrated, see ai_strat_heuristic.c
  float weight_cards_advantage;  // 1.962 -- gamma; calibrated -- far above the spec's
  // illustrative 0.15 (ai_strat_heuristic.c's HEURISTIC_DEFAULTS comment
  // explains why this is a legitimate calibration finding, not identity
  // erosion: it's still the same single weighted-sum mechanism)
  float weight_cash_advantage;   // 1.0   -- delta; PINNED during calibration, see above
  float weight_taper_exponent;   // 0.101 -- calibrated; taper exponent on gamma/delta,
  // 0 = fixed weights
  float opp_card_discount;       // 0.987 -- calibrated; the stub's "Effective Adjustment"
  // on opponent hand size
} HeuristicParams;

void heuristic_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void heuristic_defense_strategy(struct gamestate* gstate, GameContext* ctx);

HeuristicParams heuristic_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/heuristic/), settable per
// player so two different parameter sets can play each other in one
// process/game -- same pattern as balanced_rules_set_params()/
// borealis_set_params()/combo_threshold_set_params(). Not part of the
// general strategy framework: normal play always uses the compiled
// defaults, since nothing else calls these.
void heuristic_set_params(PlayerID player, const HeuristicParams* params);
void heuristic_reset_params(void);

#endif // AI_STRAT_HEURISTIC_H
