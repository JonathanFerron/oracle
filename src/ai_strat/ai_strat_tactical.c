// ai_strat_tactical.c
// A6 Tactical strategy ("Pressure Cooker") -- see ai_strat_tactical.h for
// the full spec (phase/aggression formulas, the two design-sketch gaps
// filled in, and why combo-awareness is unconditionally on here unlike A4).
// Calibrated -- see aicalibsrc/tactical/ and TACTICAL_DEFAULTS's own comment
// below.

#include <math.h>

#include "ai_strat_tactical.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// Denominator floor in both attack- and defense-efficiency ratios -- same
// role and starting value as A1's VB_COST_FLOOR / A4's BR_COST_FLOOR; not a
// TacticalParams field since nothing has calibrated whether this agent
// needs its own value.
#define TAC_COST_FLOOR 1.3f

// Hand-power ratio triggers for the aggression formula's bonus/penalty
// terms (tactical_design_notes.md's own numbers) -- fixed secondary
// thresholds, not tunable dials; the resulting bonus/penalty *magnitudes*
// (TacticalParams fields) are the actual strategic surface.
#define TAC_HAND_POWER_STRONG_RATIO 1.5f
#define TAC_HAND_POWER_WEAK_RATIO 0.7f

// Opponent-power-estimate cash-tier adjustment (tactical_design_notes.md's
// own numbers) -- secondary noise-reduction constants, fixed for the same
// reason as the ratios above.
#define TAC_OPP_CASH_HIGH_THRESHOLD 35
#define TAC_OPP_CASH_HIGH_MULT 1.1f
#define TAC_OPP_CASH_LOW_THRESHOLD 15
#define TAC_OPP_CASH_LOW_MULT 0.9f

// Calibrated via aicalibsrc/tactical/calibrate_tactical.py's unconstrained
// `optimize --opponent borealis` (16 free parameters, delta-free -- unlike
// A5, nothing here is pinned for scale-invariance). A univariate sweep at
// spec defaults first showed every individual parameter's effect was small
// and mostly flat vs `borealis` (win rate stuck around 12-15%, a floor
// effect) -- the real gains came from the joint search. Result:
// check_personality_flags() (calibrate_tactical.py) reported no flags --
// phase ordering stayed intact (18 < 41 < 67) and the aggression_factor
// range across a synthetic StrategicState battery stayed well above the
// 0.15 collapse threshold, so this agent is still reading the position
// rather than degrading into "a worse Heuristic" (about.md's named failure
// mode) -- no --identity-safe run was needed.
//
// defense_damage_weight landed very small (0.042) relative to
// defense_cash_weight (1.623), meaning declining a block is comparatively
// cheap here and this agent leans toward trading energy for cash more than
// the spec's 1.5/1.0 ratio suggested. Playtracing (turn-by-turn energy
// deltas via `-sa -p` in a debug build, `borealis` on both sides) confirmed
// this is a real risk-tolerant defensive posture, not a "never defend"
// degenerate pattern -- both sides traded damage in a visibly competitive
// exchange, consistent with the measured near-50/50 result below.
//
// Also found and fixed during implementation, before this calibration run:
// the original decide_num_attackers() formula (round(aggression *
// max_playable)) put max_playable=1's sole decision boundary exactly on
// aggression's neutral baseline (0.5), so the agent passed on its only
// affordable champion far more often than intended -- see
// ai_strat_tactical.h's header comment and desired_attacker_count() below
// for the fixed-band replacement.
//
// Measured (validated, both seats): vs `borealis` -> 53.56% [53.07%,
// 54.05%] (40,000 games); vs `rand` -> 98.68% [98.50%, 98.83%] (18,000
// games); vs `value` -> 77.81% [77.20%, 78.41%] (18,000 games); vs `combo`
// -> 70.19% [69.52%, 70.85%] (18,000 games); vs `balanced` -> 70.43%
// [69.76%, 71.10%] (18,000 games); vs `heuristic` -> 39.30% [38.59%,
// 40.02%] (18,000 games) -- the one agent this measures below, consistent
// with `heuristic`'s own rating (60) being the highest measured so far.
// The vs-`borealis` result lands this agent's Bradley-Terry rating above
// the anchor (above 50) -- run --stda.rating for the roster-wide fit before
// trusting this pairwise estimate as the shipped rating.
#define TACTICAL_DEFAULTS \
  { .phase_mid_threshold = 67, \
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
    .defense_damage_weight = 0.0420395f, \
    .defense_cash_weight = 1.6231302f, \
    .defense_conservative_stdev_mult = 1.2332415f, \
    .draw_min_hand_size = 5 }

static TacticalParams g_params[2] = { TACTICAL_DEFAULTS, TACTICAL_DEFAULTS };

TacticalParams tactical_get_default_params(void)
{ TacticalParams defaults = TACTICAL_DEFAULTS;
  return defaults;
} // tactical_get_default_params

void tactical_set_params(PlayerID player, const TacticalParams* params)
{ g_params[player] = *params;
} // tactical_set_params

void tactical_reset_params(void)
{ TacticalParams defaults = TACTICAL_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // tactical_reset_params

/* ========================================================================
   Phase classification and the aggression factor (ai_strat_tactical.h's
   header comment has the full formula).
   ======================================================================== */

typedef enum
{ PHASE_EARLY,
  PHASE_MID,
  PHASE_LATE,
  PHASE_CRITICAL
} GamePhase;

static GamePhase game_phase(uint8_t energy, const TacticalParams* params)
{ if(energy >= params->phase_mid_threshold) return PHASE_EARLY;
  if(energy >= params->phase_late_threshold) return PHASE_MID;
  if(energy >= params->phase_critical_threshold) return PHASE_LATE;
  return PHASE_CRITICAL;
} // game_phase

static float hand_power_sum(const Hand* hand)
{ float total = 0.0f;
  for(uint8_t i = 0; i < hand->size; i++)
    total += fullDeck[hand->cards[i]].power;
  return total;
} // hand_power_sum

// opp_hand_size * AVERAGE_POWER_FOR_MULLIGAN, adjusted by a fixed cash-tier
// multiplier -- only reads opponent hand SIZE and cash (both public), never
// card identities.
static float estimate_opponent_power(const struct gamestate* gstate, PlayerID opponent)
{ float estimate = (float)gstate->hand[opponent].size * (float)AVERAGE_POWER_FOR_MULLIGAN;

  if(gstate->current_cash_balance[opponent] > TAC_OPP_CASH_HIGH_THRESHOLD)
    estimate *= TAC_OPP_CASH_HIGH_MULT;
  else if(gstate->current_cash_balance[opponent] < TAC_OPP_CASH_LOW_THRESHOLD)
    estimate *= TAC_OPP_CASH_LOW_MULT;

  return estimate;
} // estimate_opponent_power

static float calculate_aggression_factor(uint8_t own_energy, uint8_t opp_energy,
                                         GamePhase my_phase, GamePhase opp_phase,
                                         float my_hand_power, float opp_estimated_power,
                                         uint16_t own_cash, const TacticalParams* params)
{ float aggression = 0.5f;

  aggression += ((float)own_energy - (float)opp_energy) * params->aggression_energy_diff_weight;

  if(opp_phase == PHASE_CRITICAL)
    aggression += params->aggression_opp_critical_bonus;
  else if(opp_phase == PHASE_LATE)
    aggression += params->aggression_opp_late_bonus;

  if(my_phase == PHASE_CRITICAL)
    aggression -= params->aggression_self_critical_penalty;
  else if(my_phase == PHASE_LATE)
    aggression -= params->aggression_self_late_penalty;

  if(my_hand_power > opp_estimated_power * TAC_HAND_POWER_STRONG_RATIO)
    aggression += params->aggression_hand_power_bonus;
  if(my_hand_power < opp_estimated_power * TAC_HAND_POWER_WEAK_RATIO)
    aggression -= params->aggression_hand_power_penalty;

  if(own_cash > params->aggression_cash_surplus_threshold)
    aggression += params->aggression_cash_surplus_bonus;

  if(aggression < 0.0f) aggression = 0.0f;
  if(aggression > 1.0f) aggression = 1.0f;

  return aggression;
} // calculate_aggression_factor

static float evaluate_aggression_factor(const struct gamestate* gstate, PlayerID player,
                                        const TacticalParams* params)
{ PlayerID opp = 1 - player;
  uint8_t own_energy = gstate->current_energy[player];
  uint8_t opp_energy = gstate->current_energy[opp];

  GamePhase my_phase = game_phase(own_energy, params);
  GamePhase opp_phase = game_phase(opp_energy, params);

  float my_hand_power = hand_power_sum(&gstate->hand[player]);
  float opp_estimated_power = estimate_opponent_power(gstate, opp);

  return calculate_aggression_factor(own_energy, opp_energy, my_phase, opp_phase,
                                     my_hand_power, opp_estimated_power,
                                     gstate->current_cash_balance[player], params);
} // evaluate_aggression_factor

/* ========================================================================
   Attack: draw-card trigger, then num_attackers = round(aggression *
   min(3, affordable)), then greedy combo-aware selection (combo-awareness
   is unconditionally on here, unlike A4's optional combo_weight).
   ======================================================================== */

static float attack_selection_score(uint8_t card_idx, const uint8_t* already_selected,
                                    uint8_t selected_count)
{ float base = fullDeck[card_idx].expected_attack / (fullDeck[card_idx].cost + TAC_COST_FLOOR);

  uint8_t trial[3];
  for(uint8_t i = 0; i < selected_count; i++) trial[i] = already_selected[i];
  trial[selected_count] = card_idx;

  int with_bonus = combo_bonus_for_selection(trial, (uint8_t)(selected_count + 1));
  int without_bonus = (selected_count >= 2) ?
                      combo_bonus_for_selection(already_selected, selected_count) : 0;

  return base + (float)(with_bonus - without_bonus);
} // attack_selection_score

// Greedily plays up to max_count champions from the caller's own
// build_affordable_champions() result, re-ranking after each pick (so the
// combo term sees the actually-chosen set), staying within the player's
// full current cash balance (no target-derived cap -- see
// ai_strat_tactical.h on why A6 has no resource-target formula).
static uint8_t select_best_attackers(const struct gamestate* gstate, PlayerID player,
                                     const uint8_t* affordable, uint8_t affordable_count,
                                     uint8_t max_count, uint8_t* out)
{ uint8_t candidates[12];
  for(uint8_t i = 0; i < affordable_count; i++) candidates[i] = affordable[i];

  uint8_t chosen_count = 0;
  int32_t budget_left = gstate->current_cash_balance[player];

  while(chosen_count < max_count)
  { int8_t best_slot = -1;
    float best_score = -1.0f;

    for(uint8_t i = 0; i < affordable_count; i++)
    { uint8_t card_idx = candidates[i];
      if(card_idx == UINT8_MAX) continue; // already chosen
      if(fullDeck[card_idx].cost > budget_left) continue;

      float score = attack_selection_score(card_idx, out, chosen_count);
      if(best_slot < 0 || score > best_score)
      { best_score = score;
        best_slot = (int8_t)i;
      }
    }

    if(best_slot < 0) break;

    uint8_t card_idx = candidates[best_slot];
    out[chosen_count++] = card_idx;
    budget_left -= fullDeck[card_idx].cost;
    candidates[best_slot] = UINT8_MAX; // consumed
  }

  return chosen_count;
} // select_best_attackers

// Fixed aggression bands rather than round(aggression * max_playable):
// found by playtracing that proportional rounding puts max_playable=1's
// SOLE decision boundary exactly at aggression's neutral baseline (0.5) --
// since routine negative signals (e.g. the hand-power penalty) push
// aggression just below 0.5 often, the agent was passing on its only
// affordable champion far more often than any other implemented agent ever
// declines to attack (measured losing to Random, the only implemented agent
// to do so). Fixed bands keep the same 4-level "turn up the heat" escalation
// but the neutral baseline now lands in the >=0.25 band (still commits 1),
// removing the pathological coin-flip. max_playable=2/3's boundaries were
// never at 0.5 under the old formula either, so this only changes behaviour
// at max_playable=1.
static uint8_t desired_attacker_count(float aggression)
{ if(aggression >= 0.75f) return 3;
  if(aggression >= 0.5f) return 2;
  if(aggression >= 0.25f) return 1;
  return 0;
} // desired_attacker_count

void tactical_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  const TacticalParams* params = &g_params[player];

  if(try_play_draw_card(gstate, player, params->draw_min_hand_size,
                        params->phase_critical_threshold, ctx))
    return; // playing a draw card consumes the turn's one action

  uint8_t affordable[12];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t affordable_count = build_affordable_champions(gstate, player, budget, affordable);

  float aggression = evaluate_aggression_factor(gstate, player, params);

  uint8_t max_playable = (uint8_t)oraclemin(3, affordable_count);
  uint8_t num_attackers = (uint8_t)oraclemin(desired_attacker_count(aggression), max_playable);

  if(num_attackers == 0)
  { try_play_cash_fallback(gstate, player, affordable_count, ctx);
    return;
  }

  uint8_t chosen[3];
  uint8_t chosen_count = select_best_attackers(gstate, player, affordable, affordable_count,
                                               num_attackers, chosen);

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, player, chosen[i], ctx);
} // tactical_attack_strategy

/* ========================================================================
   Defense: rank by defense efficiency (tie-break: worst attack efficiency
   first, inherited from A4's convention), then walk prefixes of length
   1..3 of that single ranking, keeping whichever prefix length maximises
   value = -(expected_damage*damage_weight + cost*cash_weight) against a
   CONSERVATIVE (inflated) attack estimate -- see ai_strat_tactical.h on the
   sign, the opposite of A4's E[Attack] - beta*sigma cap.
   ======================================================================== */

typedef struct
{ uint8_t card_index;
  float defense_score;
  float attack_score; // tie-break only, ascending
} TacticalDefenseCandidate;

static bool defense_ranks_before(const TacticalDefenseCandidate* a,
                                 const TacticalDefenseCandidate* b)
{ if(a->defense_score != b->defense_score) return a->defense_score > b->defense_score;
  if(a->attack_score != b->attack_score) return a->attack_score < b->attack_score;
  return a->card_index < b->card_index;
} // defense_ranks_before

static uint8_t build_ranked_defenders(const struct gamestate* gstate, PlayerID defender,
                                      TacticalDefenseCandidate* out)
{ uint8_t candidates[12];
  uint16_t budget = gstate->current_cash_balance[defender];
  uint8_t count = build_affordable_champions(gstate, defender, budget, candidates);

  for(uint8_t i = 0; i < count; i++)
  { uint8_t card_idx = candidates[i];
    out[i] = (TacticalDefenseCandidate)
    { .card_index = card_idx,
        .defense_score = fullDeck[card_idx].expected_defense /
                         (fullDeck[card_idx].cost + TAC_COST_FLOOR),
                         .attack_score = fullDeck[card_idx].expected_attack /
                                         (fullDeck[card_idx].cost + TAC_COST_FLOOR)
    };
  }

  for(uint8_t i = 1; i < count; i++)
  { TacticalDefenseCandidate key = out[i];
    int j = i - 1;
    while(j >= 0 && defense_ranks_before(&key, &out[j]))
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }

  return count;
} // build_ranked_defenders

void tactical_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  PlayerID attacker = gstate->current_player;
  const TacticalParams* params = &g_params[defender];

  float expected_attack = expected_incoming_attack(gstate, defender);

  float attack_variance = 0.0f;
  const CombatZone* zone = &gstate->combat_zone[attacker];
  for(uint8_t i = 0; i < zone->size; i++)
    attack_variance += champion_variance(zone->cards[i]);

  float attack_estimate = expected_attack +
                          params->defense_conservative_stdev_mult * sqrtf(attack_variance);

  TacticalDefenseCandidate ranked[12];
  uint8_t count = build_ranked_defenders(gstate, defender, ranked);

  float best_value = -attack_estimate * params->defense_damage_weight; // baseline: decline
  uint8_t best_num = 0;

  uint8_t max_num = (uint8_t)oraclemin(3, count);
  float running_defense = 0.0f;
  float running_cost = 0.0f;
  uint8_t prefix[3];

  for(uint8_t num = 1; num <= max_num; num++)
  { uint8_t card_idx = ranked[num - 1].card_index;
    prefix[num - 1] = card_idx;
    running_defense += fullDeck[card_idx].expected_defense;
    running_cost += (float)fullDeck[card_idx].cost;

    float total_defense = running_defense + (float)combo_bonus_for_selection(prefix, num);
    float damage = attack_estimate - total_defense;
    if(damage < 0.0f) damage = 0.0f;

    float value = -(damage * params->defense_damage_weight +
                    running_cost * params->defense_cash_weight);

    if(value > best_value)
    { best_value = value;
      best_num = num;
    }
  }

  for(uint8_t i = 0; i < best_num; i++)
    play_champion(gstate, defender, prefix[i], ctx);
} // tactical_defense_strategy
