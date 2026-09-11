// ai_strat_impersonator.c
// Impersonator gap-filler strategy ("Uncalibrated Power" / The
// Impersonator) -- see doc/ai_agents.md's gap-2 section.
//
// Reuses A3 Borealis's exact scoring/enumeration engine
// (borealis_best_champion_set(), ai_strat_borealis_enum.{c,h}) -- the same
// exhaustive 0-3 champion subset search, capped-value-minus-lambda-cost
// scoring, and epsilon tie-break -- but with its own, deliberately
// mistuned luna_value (lambda). Borealis's own calibration comment
// (BOREALIS_DEFAULTS, ai_strat_borealis.c) documents that lambda is *the*
// strength dial and that the untuned handout guess (0.5) measured *below*
// A2 Combo Threshold (43.6%) before calibration found the real optimum
// near 4.07-4.58. The Impersonator copies Borealis's whole formula
// faithfully -- same tiebreak_epsilon/hold_lethal_combos/lethal_combo_bonus/
// lethal_hold_ceiling/min_hand_size_target as the real, calibrated
// Borealis -- and gets everything right except the one dial that actually
// matters.
//
// Passes its own local BorealisParams by pointer into the shared
// enumeration engine rather than calling borealis_set_params() -- that
// setter mutates AI_STRATEGY_BOREALIS's own per-player g_params[2], which
// would corrupt the real Borealis's behavior in any round-robin that plays
// both in the same process (see ai_strat_junior.c's design note on why
// gap-filler agents never reuse another registered agent's per-player
// override hooks). borealis_best_champion_set() takes params as a plain
// argument and touches no Borealis-internal state, so this is safe reuse,
// not a boundary violation -- see ai_strat_borealis_enum.h's updated
// header comment.
//
// No mulligan/discard-to-7 override (uses the shared power-based default,
// ai_strat_lib_heuristics.c) -- the impersonation is combat-formula-deep
// only, not a full copy of Borealis's discard/mulligan combo protection.

#include "ai_strat_impersonator.h"
#include "ai_strat_borealis.h"       // BorealisParams
#include "ai_strat_borealis_enum.h"  // borealis_best_champion_set(), contribution funcs
#include "ai_strat_common.h"
#include "../core/card_actions.h"

// Same as the real, calibrated Borealis's BOREALIS_DEFAULTS
// (ai_strat_borealis.c) in every field except luna_value: 4.0-4.58 is
// documented as the real per-lambda optimum (unimodal, "declining again
// past 6.0"). luna_value=2.0 (the old, pre-widening sweep grid's own max,
// per that same comment) was tried first and measured a disappointing
// rating 35 -- still meaningfully below the peak, since the win-rate curve
// is quite peaked around 4.0-4.58 (doc/ai_agents.md's gap-2 section has the
// measurement log). 3.3 sits closer to that peak while still clearly off
// it -- a plausible "almost got the calibration right" guess rather than
// the untuned-handout extreme (0.5, which loses outright to `combo`).
#define IMPERSONATOR_PARAMS \
  { .luna_value = 3.3f, \
    .tiebreak_epsilon = 0.3444f, \
    .hold_lethal_combos = true, \
    .lethal_combo_bonus = 24, \
    .lethal_hold_ceiling = 38, \
    .min_hand_size_target = 6 }

#define IMPERSONATOR_DRAW_ENERGY_FLOOR 20 // same as Borealis's own

static const BorealisParams g_impersonator_params = IMPERSONATOR_PARAMS;

void impersonator_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;

  if(try_play_draw_card(gstate, player,
                        (uint8_t)g_impersonator_params.min_hand_size_target,
                        IMPERSONATOR_DRAW_ENERGY_FLOOR, ctx))
    return;

  uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, player,
                                             gstate->current_cash_balance[player],
                                             affordable);

  uint8_t chosen[3];
  uint8_t chosen_count;
  borealis_best_champion_set(affordable, count, gstate, player,
                             borealis_expected_attack_of, -1.0f, true,
                             &g_impersonator_params, ctx, chosen, &chosen_count);

  if(chosen_count == 0)
  { try_play_cash_fallback(gstate, player, count, ctx);
    return;
  }

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, player, chosen[i], ctx);
} // impersonator_attack_strategy

void impersonator_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, defender,
                                             gstate->current_cash_balance[defender],
                                             affordable);

  float incoming = expected_incoming_attack(gstate, defender);

  uint8_t chosen[3];
  uint8_t chosen_count;
  borealis_best_champion_set(affordable, count, gstate, defender,
                             borealis_expected_defense_of, incoming, false,
                             &g_impersonator_params, ctx, chosen, &chosen_count);

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, defender, chosen[i], ctx);
} // impersonator_defense_strategy
