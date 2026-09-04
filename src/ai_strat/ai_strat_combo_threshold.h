// ai_strat_combo_threshold.h
// A2 Combo Threshold strategy ("The Showboat") -- see
// doc/ai_agents.md's A2 section
//
// A threshold-gated combo chaser: on attack it prunes candidate plays to
// single champions plus pairs/triples whose combo bonus clears a tunable
// threshold, and on defense it declines a fraction of the blocks it
// should take. Both weaknesses are the agent's intended character (handout
// §8), not implementation shortcuts -- see ai_strat_combo_threshold.c's
// header comment before "improving" either one.

#ifndef AI_STRAT_COMBO_THRESHOLD_H
#define AI_STRAT_COMBO_THRESHOLD_H

#include "../core/game_types.h"
#include "../core/game_context.h"

// Tunable parameters (handout §6.3, plus combo_weight/min_play_score_floor --
// referenced in the handout's prose (§6.4 steps 3 and 5, §9.1) but never
// declared as struct fields there). All nine are calibration starting
// guesses, not tuned values -- see aicalibsrc/combo/ once it exists.
typedef struct
{ float aggression_level;           // 1.0-3.0, divides both combo thresholds below
  int   combo_bonus_threshold;      // 7-12, min 2-card bonus to pursue
  int   combo3_bonus_threshold;     // 14+, min 3-card bonus to pursue
  float combo_weight;               // w in: score = Sigma(expected_attack) + w*combo_bonus
  float min_play_score_floor;       // below this, prefer the cash-card fallback
  float defend_probability_base;    // 0.40-0.70
  int   defend_damage_threshold;    // 6-10, ignore attacks below this
  int   min_hand_size_target;       // 3-5, draw-card trigger
  bool  save_big_combos_for_lethal; // hold +16 triples while opponent energy is high
} ComboThresholdParams;

void combo_threshold_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void combo_threshold_defense_strategy(struct gamestate* gstate, GameContext* ctx);

ComboThresholdParams combo_threshold_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/combo/ once it exists),
// settable per player so two different parameter sets can play each other in
// one process/game -- same pattern as value_based_set_params(). Not part of
// the general strategy framework: normal play always uses the compiled
// defaults, since nothing else calls these.
void combo_threshold_set_params(PlayerID player, const ComboThresholdParams* params);
void combo_threshold_reset_params(void);

#endif // AI_STRAT_COMBO_THRESHOLD_H
