// ai_strat_borealis.h
// A3 Borealis strategy ("the benchmark") -- see
// doc/ai_agents.md's A3 section
//
// This is the Bradley-Terry anchor for the entire rating system: its
// strength is fixed at rating 50 by definition, and every other agent and
// human player is measured against it. One-ply, no simulation, no opponent
// modelling: enumerates every legal 0-3 champion subset, scores each by
// expected combat contribution net of luna cost, and plays the best,
// breaking near-ties at random and holding back very large combos for a
// finishing blow (handout Sec.2). luna_value (lambda) is deliberately the
// *only* strength dial -- win rate is unimodal in it (handout Sec.8), which
// is what makes this agent usable as a calibratable, non-memorisable
// benchmark. Do not weaken it by randomly skipping correct plays; that is
// exploitable, not diffuse, suboptimality.

#ifndef AI_STRAT_BOREALIS_H
#define AI_STRAT_BOREALIS_H

#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ float luna_value;           // lambda: damage-units per luna. Default 0.5
  float tiebreak_epsilon;     // score window for random tie-break. Default 0.5
  bool  hold_lethal_combos;   // enable combo holding. Default true
  int   lethal_combo_bonus;   // combo bonus considered "big". Default 16
  int   lethal_hold_ceiling;  // stop holding at/below this opp energy. Default 25
  int   min_hand_size_target; // draw-card trigger. Default 4
} BorealisParams;

void borealis_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void borealis_defense_strategy(struct gamestate* gstate, GameContext* ctx);

BorealisParams borealis_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/borealis/), settable per
// player so two different parameter sets can play each other in one
// process/game -- same pattern as value_based_set_params()/
// combo_threshold_set_params(). The handout's own Sec.3 note flags its
// originally-specified single global setter as superseded by this
// per-player form for exactly that reason. Not part of the general
// strategy framework: normal play always uses the compiled defaults, since
// nothing else calls these.
void borealis_set_params(PlayerID player, const BorealisParams* params);
void borealis_reset_params(void);

// StrategySet mulligan_strategy[]/discard_strategy[] overrides (see
// ai_strategy.h and card_actions.c's discard_to_7_cards()/stda_auto.c's
// apply_mulligan()). Protect the best held combo in hand from being thrown
// away by the shared lowest-`power` heuristic, which would otherwise
// actively sabotage hold_lethal_combos -- handout Sec.7's 2026-08-22 note.
void borealis_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void borealis_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);

#endif // AI_STRAT_BOREALIS_H
