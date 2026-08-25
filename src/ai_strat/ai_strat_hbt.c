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
// with A5's own tuned advantage function", so stage 1 ships. Stage 3 (also
// freeing T's twelve aggression/phase fields) was skipped: about.md's
// framing is a synthesis of three ALREADY-CALIBRATED agents, and stage 1
// already clears every one of them (see doc/changelog.md's win-rate table)
// without touching their own tuned values.
//
// Measured (both seats, stage 1's shipped values): vs `borealis` 60.96%
// [60.48%, 61.43%] (40,000 games, validated); vs `balanced` 77.6% (18,000
// games); vs `tactical` 60.9% (18,000 games); vs `heuristic` 26.0% (40,000
// games) -- notably a LOSS to the very agent this agent's own ranking layer
// is built on. Not a bug: turn-count histograms (`-sa -p`, avg 7.3 turns,
// max 14) show fast, decisive games on both sides, not a stall -- B's
// penalty and T's weight modulation perturb H's advantage function away
// from A5's own unperturbed optimum, and that perturbation costs more
// against an opponent using that exact unperturbed mechanism than it gains
// elsewhere. --stda.rating's roster-wide Bradley-Terry fit (2026-08-25,
// `-r -p --rating.games=2000`) is what actually answers "stronger than the
// three it combines": rating 62 (Grandmaster) vs 61 (Eps-Gam-Del/A5), 53
// (Pressure Cooker/A6), 34 (Bean Counter/A4) -- the pairwise loss to A5
// above does not stop the synthesis from out-rating all three ingredients
// once weighed against the whole roster, the same way A6's own pairwise
// loss to A5 (39.30%) did not stop A6 from rating above A4. See
// doc/changelog.md for the full table and the decision-gate reasoning.
#define HBT_DEFAULTS \
  { .weight_energy_advantage = 0.34929208f, \
    .weight_cards_advantage = 1.96227051f, \
    .weight_cash_advantage = 1.0f, \
    .weight_taper_exponent = 0.10115113f, \
    .opp_card_discount = 0.98660043f, \
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
    .aggr_energy_gain = 0.16112329f, \
    .aggr_resource_fade = 0.07170858f, \
    .critical_epsilon_mult = 2.46429341f, \
    .target_cash_slope = 0.08096868f, \
    .target_cash_intercept = -2.72849536f, \
    .target_cards_slope = 0.03572451f, \
    .target_cards_intercept = -0.99130504f, \
    .late_game_aggro = 2.09102475f, \
    .lethal_horizon = 9, \
    .target_aggr_cash_scale = 0.09638681f, \
    .target_aggr_cards_scale = 0.20156727f, \
    .penalty_cash_weight = 0.74931590f, \
    .penalty_cards_weight = 0.86105139f, \
    .hold_lethal_combos = true, \
    .lethal_combo_bonus = 24, \
    .lethal_hold_ceiling = 38, \
    .defense_stdev_mult = 0.71117494f }

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
