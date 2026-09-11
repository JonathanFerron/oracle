// ai_strat_a15.c
// A15 Risk Threshold strategy -- see ai_strat_a15.h for the full rule
// statements. Parameter management and the two StrategySet entry points;
// the actual rule chain lives in ai_strat_a15_attack.c (R1/R6/R9),
// ai_strat_a15_defense.c (R4/R7), and ai_strat_a15_endgame.c (R8).

#include "ai_strat_a15.h"
#include "ai_strat_a15_attack.h"
#include "ai_strat_a15_defense.h"
#include "ai_strat_a15_endgame.h"
#include "ai_strat_a15_prob.h"
#include "../core/card_actions.h"

// Calibrated 2026-09-10 via aicalibsrc/daredevil/calibrate_daredevil.py
// (differential_evolution over all 5 continuous dials vs `borealis`,
// validated at 32,000-64,000 games -- see aicalibsrc/daredevil/README.md
// and doc/ai_agents.md's A15 section for the full record) -- AFTER R8's
// trigger was redesigned the same date (ai_strat_a15_endgame.h's header
// comment has the full story): a first calibration pass against the
// original hand-size-derived horizon found the only winning configuration
// was q2/q3/q4 pinned to a near-zero epsilon, which measured well (44.98%
// vs borealis) but meant R8 was never actually gating on confidence --
// diagnosed directly (a 200-game trace of every P(finish) the old
// mechanism evaluated: >90% below p=0.2 regardless of horizon) as the
// hand-size proxy being almost completely decoupled from real finish
// probability. Replacing it with checking all 4 horizons directly (see
// ai_strat_a15_endgame.c's find_triggering_horizon()) and recalibrating
// from scratch produced a materially different, more interpretable answer:
//
// 1. **R8 is LOAD-BEARING, not a refinement** -- true under both the old
//    and the new mechanism. Ablation (endgame_enabled=false, same
//    defense_loss_threshold): 7.06% vs `borealis`, n=32,000. With R8 on and
//    calibrated: 46.56% [46.02%,47.11%], n=32,000. This agent's R1-R9
//    "normal" chain alone is too passive/combo-hoarding to keep pace with
//    an opponent that actually attacks; R8 is what makes it competitive at
//    all, not just what wins the occasional close-out. A real, still-open
//    question this raises (not attempted here): whether R9's own
//    passivity, not just R8's tuning, deserves a second look.
// 2. **q2/q3/q4 are genuine, high confidence bars** (0.78/0.75/0.69) --
//    this is the result that vindicated dropping the hand-size proxy:
//    once the horizon check reflects real finish probability instead of
//    hand size, "wait for a well-justified chance" (the design doc's own
//    framing) measures as well as the old near-zero hack did, while
//    actually meaning what it says.
// 3. **q1 (the 4-attack horizon) is the one exception, and deliberately
//    so**: a small positive epsilon (0.01, avoiding the same exactly-0.0
//    pathology found in the pre-redesign pass -- confirmed still present:
//    q1=0.5 measured 31.38% vs the epsilon's 46.56% on identical q2-q4).
//    P(finish within 4) is the LARGEST of the four cumulative
//    probabilities (monotonic in N) and is checked LAST (only once
//    horizons 1-3, the genuine confidence gates, have all failed) -- so
//    q1's actual role isn't "confidently predict a win in 4 attacks," it's
//    "don't prematurely rule out pursuing one at all." A wide, permissive
//    gate and a set of tight, confidence-gated ones are doing two
//    different jobs, not the same job at four scales.
#define A15_DEFAULTS \
  { .defense_loss_threshold = 0.20f, \
    .endgame_q1 = 0.01f, .endgame_q2 = 0.78f, .endgame_q3 = 0.75f, .endgame_q4 = 0.69f, \
    .endgame_enabled = true }

static A15Params g_params[2] = { A15_DEFAULTS, A15_DEFAULTS };

A15Params a15_get_default_params(void)
{ A15Params defaults = A15_DEFAULTS;
  return defaults;
} // a15_get_default_params

void a15_set_params(PlayerID player, const A15Params* params)
{ g_params[player] = *params;
} // a15_set_params

void a15_reset_params(void)
{ A15Params defaults = A15_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // a15_reset_params

void a15_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  const A15Params* params = &g_params[player];

  if(params->endgame_enabled && a15_try_endgame_attack(gstate, player, params, ctx))
    return;

  a15_play_normal_attack(gstate, player, ctx);
} // a15_attack_strategy

void a15_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  const A15Params* params = &g_params[defender];

  float p_undefended = a15_p_death_undefended(gstate, defender);
  if(p_undefended < params->defense_loss_threshold) return; // R4: decline

  uint8_t chosen[3];
  uint8_t count = a15_choose_defense_subset(gstate, defender,
                                            params->defense_loss_threshold, chosen);
  for(uint8_t i = 0; i < count; i++)
    play_champion(gstate, defender, chosen[i], ctx);
} // a15_defense_strategy
