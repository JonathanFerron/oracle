// ai_strat_hbt.c
// A7 Hybrid HBT strategy ("The Grandmaster") -- see ai_strat_hbt.h for the
// full spec (the three-layer synthesis, why A4 enters as a penalty not a
// filter, the corrected aggression sign, the T->H weight coupling, the
// combo hold ported from A3). Attack/defense orchestration and parameter
// management; per-turn state derivation and move enumeration/scoring live
// in ai_strat_hbt_enum.c (same file-length split as A3 Borealis's
// ai_strat_borealis/ai_strat_borealis_enum).

#include "ai_strat_hbt.h"
#include "ai_strat_hbt_enum.h"
#include "../core/card_actions.h"

// Calibrated 2026-08-25 via aicalibsrc/hbt/calibrate_hbt.py's `optimize`,
// staged per the plan in doc/changelog.md (33 free params in one search was
// judged too large a space to trust a single unconstrained pass on, so the
// H/T/B mechanisms inherited from A3/A4/A5/A6 were calibrated once already
// and are not re-derived here):
//
// Stage 1 freed only the eight parameters new to this agent -- the T->H
// weight coupling (aggr_energy_gain, aggr_resource_fade,
// critical_epsilon_mult), the B->H penalty coupling (penalty_cash_weight,
// penalty_cards_weight), the two target-aggression scales
// (target_aggr_cash_scale, target_aggr_cards_scale), and defense_stdev_mult
// -- against `borealis`, with every H/T/B/combo-hold field pinned at its
// SOURCE AGENT'S OWN shipped default (A5's HEURISTIC_DEFAULTS, A6's
// TACTICAL_DEFAULTS, A4's BALANCED_DEFAULTS, A3's combo-hold trio).
// check_personality_flags() reported no flags (T's aggression range stayed
// healthy, none of B's penalty weights or target slopes eroded toward
// their BOUNDS floor) -- no --identity-safe run was needed, the same
// no-flags outcome A6 had. Validated: 60.96% [60.48%, 61.43%] vs `borealis`
// (40,000 games) -- already above A5's own 58.99% vs `borealis`, the
// highest of this agent's three ingredients, using none of A5's weights
// but its own.
//
// Stage 2 additionally freed H's own four non-pinned weights (twelve free
// total), re-deriving them jointly with the eight coupling terms rather
// than reusing A5's shipped values. It found a statistically
// indistinguishable result (61.36% [60.88%, 61.83%] vs `borealis`,
// 40,000 games; a direct stage-1-vs-stage-2 head-to-head measured 49.11%
// [48.56%, 49.66%], a tie within noise) at the cost of weight_cards_advantage
// drifting to 10.72 (vs A5's own shipped 1.96) and opp_card_discount to 2.75
// (near its 3.0 search ceiling) -- no measurable win for abandoning "H ranks
// with A5's own tuned advantage function", so stage 1 shipped originally.
// Stage 3 (also freeing T's twelve aggression/phase fields) was skipped at
// the time: about.md's framing is a synthesis of three ALREADY-CALIBRATED
// agents, and stage 1 already cleared every one of them (see
// doc/changelog.md's win-rate table) without touching their own tuned
// values.
//
// RE-CALIBRATED 2026-08-28 after the 2026-08-27 A5/A7 defense PASS-dominance
// fix (see doc/changelog.md's 2026-08-27 entry and the
// project_a5_a7_defense_pass_dominance memory): that fix made
// defense_stdev_mult -- previously dead weight, since PASS strictly
// dominated every block before the fix -- a live dial for the first time,
// and A7 regressed 62->58 while A5's identical fix improved 60->64. Root
// cause, confirmed by a univariate sweep of defense_stdev_mult against
// `borealis` post-fix (monotonic 60.96% at -2.0 down to 57.53% at +2.0,
// aicalibsrc/hbt/results/sweep_defense_stdev_mult.csv): the shipped +0.711
// (an A6-style inflation of the incoming-attack estimate) now biases toward
// over-blocking, which costs more than it saves. Re-ran stage 1's own
// 12-free-param optimize() (the 8 stage-1 coupling/defense fields plus H's
// four weights, i.e. stage 2's shape) three ways:
//   - unconstrained BOUNDS: 64.15% [63.68%, 64.62%] (40,000 games), no
//     personality flags, but weight_cards_advantage drifted to 12.68 (worse
//     than stage 2's original 10.72 drift) -- the same known failure mode,
//     re-appearing.
//   - --identity-safe over all its bounded fields (not just these 12):
//     63.49% [63.02%, 63.96%] -- discarded, since BOUNDS_IDENTITY_SAFE has
//     no entries for target_aggr_cash_scale/target_aggr_cards_scale, so
//     this silently re-opened T's whole aggression/phase battery instead of
//     giving a tamer version of the SAME 12-param search -- a scope
//     mismatch, not a fair comparison.
//   - --identity-safe restricted to the 10 of those 12 fields
//     BOUNDS_IDENTITY_SAFE actually covers (target_aggr_cash_scale/
//     target_aggr_cards_scale necessarily excluded, held at their existing
//     values): **64.62% [64.15%, 65.09%] (40,000 games), no personality
//     flags, and weight_cards_advantage landed at 1.95 -- essentially
//     A5's own 1.96, no drift at all.** Best win rate of the three AND the
//     only one with no drift; also won the direct three-way selfplay
//     round-robin (53.11% BT win rate vs the unconstrained candidate's
//     48.89% pairwise share and defaults' 45.00%,
//     aicalibsrc/hbt/results/selfplay_named.csv). Shipped.
// Only 10 of the 34 fields changed from the original stage-1 calibration:
// weight_energy_advantage, weight_cards_advantage, weight_taper_exponent,
// opp_card_discount, aggr_energy_gain, aggr_resource_fade,
// critical_epsilon_mult, penalty_cash_weight, penalty_cards_weight,
// defense_stdev_mult (sign-flipped from +0.711 to +0.216 -- still net
// inflating, just far less). Every other field (T's whole aggression/phase
// battery, B's resource targets, the combo-hold trio) is untouched from
// stage 1's original 2026-08-25 fit.
//
// Measured (both seats, this recalibration's shipped values, `-a -p
// --ai.a=hbt --ai.b=borealis`, n=8000 total): vs `borealis` see
// doc/changelog.md's 2026-08-28 entry for the final figure and the honest
// note that the pre-recalibration "58" reference point was itself a single
// n=4000 sample that undershot a much larger re-measurement (~59.9-60.2%
// over 8,000-32,000 games) of the SAME unrecalibrated code -- both figures
// are in that entry.
#define HBT_DEFAULTS \
  { .weight_energy_advantage = 0.30140110f, \
    .weight_cards_advantage = 1.94846331f, \
    .weight_cash_advantage = 1.0f, \
    .weight_taper_exponent = 0.08984036f, \
    .opp_card_discount = 1.34949438f, \
    .phase_mid_threshold = 67, \
    .phase_late_threshold = 41, \
    .phase_critical_threshold = 18, \
    .aggression_energy_diff_weight = 0.0008022129f, \
    .aggression_opp_late_bonus = 0.1262423f, \
    .aggression_opp_critical_bonus = 0.2819330f, \
    .aggression_self_late_penalty = 0.0530097f, \
    .aggression_self_critical_penalty = 0.1475105f, \
    .aggression_hand_power_bonus = 0.2479543f, \
    .aggression_hand_power_penalty = 0.1542592f, \
    .aggression_cash_surplus_threshold = 10, \
    .aggression_cash_surplus_bonus = 0.2301680f, \
    .aggr_energy_gain = 0.11170919f, \
    .aggr_resource_fade = 0.10308390f, \
    .critical_epsilon_mult = 1.91039995f, \
    .target_cash_slope = 0.08096868f, \
    .target_cash_intercept = -2.72849536f, \
    .target_cards_slope = 0.03572451f, \
    .target_cards_intercept = -0.99130504f, \
    .late_game_aggro = 2.09102475f, \
    .lethal_horizon = 9, \
    .target_aggr_cash_scale = 0.09638681f, \
    .target_aggr_cards_scale = 0.20156727f, \
    .penalty_cash_weight = 0.85077871f, \
    .penalty_cards_weight = 0.30673896f, \
    .hold_lethal_combos = true, \
    .lethal_combo_bonus = 24, \
    .lethal_hold_ceiling = 38, \
    .defense_stdev_mult = 0.21564917f }

static HBTParams g_params[2] = { HBT_DEFAULTS, HBT_DEFAULTS };

HBTParams hbt_get_default_params(void)
{ HBTParams defaults = HBT_DEFAULTS;
  return defaults;
} // hbt_get_default_params

void hbt_set_params(PlayerID player, const HBTParams* params)
{ g_params[player] = *params;
} // hbt_set_params

void hbt_reset_params(void)
{ HBTParams defaults = HBT_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // hbt_reset_params

// Internal accessor for ai_strat_hbt_cards.c -- see ai_strat_hbt_enum.h's
// declaration comment.
const HBTParams* hbt_live_params(PlayerID player)
{ return &g_params[player];
} // hbt_live_params

void hbt_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  const HBTParams* params = &g_params[player];

  HBTState state = hbt_evaluate_state(gstate, player, params);
  HBTBestMove move = hbt_best_attack_move(gstate, player, params, &state);

  switch(move.type)
  { case HBT_MOVE_CHAMPIONS:
      for(uint8_t i = 0; i < move.count; i++)
        play_champion(gstate, player, move.cards[i], ctx);
      return;
    case HBT_MOVE_DRAW:
      play_draw_card(gstate, player, move.cards[0], ctx);
      return;
    case HBT_MOVE_CASH:
      play_cash_card_ai(gstate, player, move.cards[0], ctx);
      return;
    case HBT_MOVE_PASS:
    default:
      return;
  }
} // hbt_attack_strategy

void hbt_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  const HBTParams* params = &g_params[defender];

  HBTState state = hbt_evaluate_state(gstate, defender, params);
  HBTBestMove move = hbt_best_defense_move(gstate, defender, params, &state);
  if(move.type != HBT_MOVE_CHAMPIONS) return; // decline

  for(uint8_t i = 0; i < move.count; i++)
    play_champion(gstate, defender, move.cards[i], ctx);
} // hbt_defense_strategy
