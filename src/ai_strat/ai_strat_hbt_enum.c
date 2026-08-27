// ai_strat_hbt_enum.c
// A7 Hybrid HBT's per-turn state derivation and move enumeration/scoring --
// see ai_strat_hbt_enum.h and ai_strat_hbt.h's header comment for the full
// spec (why A4 enters as a penalty not a filter, the T->H weight coupling,
// the corrected aggression sign, the combo hold ported from A3).

#include <math.h>

#include "ai_strat_hbt_enum.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// Terminal-win/-loss bonus in EnergyAdv -- same role as A5's
// HEUR_LETHAL_BONUS: only job is to dominate every other term once either
// player is at 0 energy.
#define HBT_LETHAL_BONUS 100000.0f

// Hand-power ratio triggers for the aggression formula (A6's own numbers,
// tactical_design_notes.md) -- fixed secondary thresholds, not tunable, same
// status as A6's TAC_HAND_POWER_STRONG_RATIO/TAC_HAND_POWER_WEAK_RATIO.
#define HBT_HAND_POWER_STRONG_RATIO 1.5f
#define HBT_HAND_POWER_WEAK_RATIO 0.7f

// Opponent-power-estimate cash-tier adjustment (A6's own numbers) -- fixed
// secondary noise-reduction constants, same status as A6's
// TAC_OPP_CASH_HIGH/LOW_THRESHOLD/MULT.
#define HBT_OPP_CASH_HIGH_THRESHOLD 35
#define HBT_OPP_CASH_HIGH_MULT 1.1f
#define HBT_OPP_CASH_LOW_THRESHOLD 15
#define HBT_OPP_CASH_LOW_MULT 0.9f

/* ========================================================================
   Layer T: phase classification and aggression factor, ported verbatim
   from A6 (ai_strat_tactical.c) -- see ai_strat_hbt.h's header comment for
   why this agent uses the resulting scalar to modulate weights rather than
   to gate an attacker count.
   ======================================================================== */

typedef enum
{ HBT_PHASE_EARLY,
  HBT_PHASE_MID,
  HBT_PHASE_LATE,
  HBT_PHASE_CRITICAL
} HBTGamePhase;

static HBTGamePhase game_phase(uint8_t energy, const HBTParams* params)
{ if(energy >= params->phase_mid_threshold) return HBT_PHASE_EARLY;
  if(energy >= params->phase_late_threshold) return HBT_PHASE_MID;
  if(energy >= params->phase_critical_threshold) return HBT_PHASE_LATE;
  return HBT_PHASE_CRITICAL;
} // game_phase

static float hand_power_sum(const Hand* hand)
{ float total = 0.0f;
  for(uint8_t i = 0; i < hand->size; i++)
    total += fullDeck[hand->cards[i]].power;
  return total;
} // hand_power_sum

static float estimate_opponent_power(const struct gamestate* gstate, PlayerID opponent)
{ float estimate = (float)gstate->hand[opponent].size * (float)AVERAGE_POWER_FOR_MULLIGAN;

  if(gstate->current_cash_balance[opponent] > HBT_OPP_CASH_HIGH_THRESHOLD)
    estimate *= HBT_OPP_CASH_HIGH_MULT;
  else if(gstate->current_cash_balance[opponent] < HBT_OPP_CASH_LOW_THRESHOLD)
    estimate *= HBT_OPP_CASH_LOW_MULT;

  return estimate;
} // estimate_opponent_power

static float calculate_aggression_factor(uint8_t own_energy, uint8_t opp_energy,
                                         HBTGamePhase my_phase, HBTGamePhase opp_phase,
                                         float my_hand_power, float opp_estimated_power,
                                         uint16_t own_cash, const HBTParams* params)
{ float aggression = 0.5f;

  aggression += ((float)own_energy - (float)opp_energy) * params->aggression_energy_diff_weight;

  if(opp_phase == HBT_PHASE_CRITICAL)
    aggression += params->aggression_opp_critical_bonus;
  else if(opp_phase == HBT_PHASE_LATE)
    aggression += params->aggression_opp_late_bonus;

  if(my_phase == HBT_PHASE_CRITICAL)
    aggression -= params->aggression_self_critical_penalty;
  else if(my_phase == HBT_PHASE_LATE)
    aggression -= params->aggression_self_late_penalty;

  if(my_hand_power > opp_estimated_power * HBT_HAND_POWER_STRONG_RATIO)
    aggression += params->aggression_hand_power_bonus;
  if(my_hand_power < opp_estimated_power * HBT_HAND_POWER_WEAK_RATIO)
    aggression -= params->aggression_hand_power_penalty;

  if(own_cash > params->aggression_cash_surplus_threshold)
    aggression += params->aggression_cash_surplus_bonus;

  if(aggression < 0.0f) aggression = 0.0f;
  if(aggression > 1.0f) aggression = 1.0f;

  return aggression;
} // calculate_aggression_factor

/* ========================================================================
   Layer B: resource targets, ported verbatim from A4
   (ai_strat_balanced_rules.c's resource_targets()), then aggression-scaled
   with the corrected sign (ai_strat_hbt.h's header comment).
   ======================================================================== */

static void resource_targets(uint8_t opp_energy, const HBTParams* params,
                             float* out_target_cash, float* out_target_cards)
{ float e = (float)((int)opp_energy - 8);

  float target_cash = params->target_cash_slope * e + params->target_cash_intercept;
  float target_cards = params->target_cards_slope * e + params->target_cards_intercept;

  if(target_cash < 0.0f) target_cash = 0.0f;
  if(target_cards < 0.0f) target_cards = 0.0f;

  if((int)opp_energy <= params->lethal_horizon && params->late_game_aggro > 0.0f)
  { target_cash /= params->late_game_aggro;
    target_cards /= params->late_game_aggro;
  }

  *out_target_cash = target_cash;
  *out_target_cards = target_cards;
} // resource_targets

/* ========================================================================
   Per-turn state: run T then B, coupling both into the weights/targets
   Layer H's advantage function reads.
   ======================================================================== */

HBTState hbt_evaluate_state(struct gamestate* gstate, PlayerID player,
                            const HBTParams* params)
{ PlayerID opp = 1 - player;
  uint8_t own_energy = gstate->current_energy[player];
  uint8_t opp_energy = gstate->current_energy[opp];

  HBTGamePhase my_phase = game_phase(own_energy, params);
  HBTGamePhase opp_phase = game_phase(opp_energy, params);

  float my_hand_power = hand_power_sum(&gstate->hand[player]);
  float opp_estimated_power = estimate_opponent_power(gstate, opp);

  float aggression = calculate_aggression_factor(own_energy, opp_energy, my_phase, opp_phase,
                                                 my_hand_power, opp_estimated_power,
                                                 gstate->current_cash_balance[player], params);

  float centered = 2.0f * (aggression - 0.5f);

  HBTState state;
  state.eps_eff = params->weight_energy_advantage * (1.0f + params->aggr_energy_gain * centered);
  if(opp_phase == HBT_PHASE_CRITICAL) state.eps_eff *= params->critical_epsilon_mult;

  float resource_fade = 1.0f - params->aggr_resource_fade * centered;
  state.gamma_eff = params->weight_cards_advantage * resource_fade;
  state.delta_eff = params->weight_cash_advantage * resource_fade;

  float target_cash, target_cards;
  resource_targets(opp_energy, params, &target_cash, &target_cards);

  float target_scale_cash = 1.0f - params->target_aggr_cash_scale * centered;
  float target_scale_cards = 1.0f - params->target_aggr_cards_scale * centered;
  state.target_cash = target_cash * target_scale_cash;
  state.target_cards = target_cards * target_scale_cards;
  if(state.target_cash < 0.0f) state.target_cash = 0.0f;
  if(state.target_cards < 0.0f) state.target_cards = 0.0f;

  return state;
} // hbt_evaluate_state

/* ========================================================================
   Layer H: the advantage function, A5's shape (heuristic_advantage())
   re-parameterised on the state's effective weights, with Layer B's
   resource-shortfall penalty subtracted.
   ======================================================================== */

float hbt_advantage(float own_energy, float opp_energy, float own_hand,
                    float opp_hand, float own_cash, float opp_cash,
                    const HBTParams* params, const HBTState* state)
{ float energy_adv = own_energy - opp_energy;
  if(opp_energy <= 0.0f) energy_adv += HBT_LETHAL_BONUS;
  if(own_energy <= 0.0f) energy_adv -= HBT_LETHAL_BONUS;

  float taper = powf(opp_energy / (float)INITIAL_ENERGY_DEFAULT,
                     params->weight_taper_exponent);

  float cards_adv = own_hand - opp_hand * params->opp_card_discount;
  float cash_adv = own_cash - opp_cash;

  float advantage = state->eps_eff * energy_adv +
                    taper * state->gamma_eff * cards_adv +
                    taper * state->delta_eff * cash_adv;

  float cash_shortfall = state->target_cash - own_cash;
  if(cash_shortfall > 0.0f) advantage -= params->penalty_cash_weight * cash_shortfall;

  float cards_shortfall = state->target_cards - own_hand;
  if(cards_shortfall > 0.0f) advantage -= params->penalty_cards_weight * cards_shortfall;

  return advantage;
} // hbt_advantage

// Sigma(expected_attack) + combo bonus, clamped to opp_energy -- identical
// to A5's predicted_damage() (ai_strat_heuristic.c), same "no opponent block
// modelled" rationale (ai_strat_heuristic.h's header comment). A9 HBT 2-Ply
// reuses this verbatim for its own ply-1 damage estimate before subtracting
// the opponent's predicted block -- see ai_strat_hbt_enum.h.
float predicted_damage(const uint8_t* cards, uint8_t count, float opp_energy)
{ float total = 0.0f;
  for(uint8_t i = 0; i < count; i++)
    total += fullDeck[cards[i]].expected_attack;
  total += (float)combo_bonus_for_selection(cards, count);

  if(total > opp_energy) total = opp_energy;
  if(total < 0.0f) total = 0.0f;
  return total;
} // predicted_damage

// Sigma(expected_defense) + combo bonus -- identical to A5's predicted_block().
// A9 HBT 2-Ply reuses this to score the opponent's simulated reply subset.
float predicted_block(const uint8_t* cards, uint8_t count)
{ float total = 0.0f;
  for(uint8_t i = 0; i < count; i++)
    total += fullDeck[cards[i]].expected_defense;
  total += (float)combo_bonus_for_selection(cards, count);
  return total;
} // predicted_block

/* ========================================================================
   Combo hold, ported verbatim from A3's is_held_combo()
   (ai_strat_borealis_enum.c) -- attack only.
   ======================================================================== */

bool is_held_combo(const uint8_t* cards, uint8_t count, float raw_damage,
                   PlayerID opponent, const struct gamestate* gstate,
                   const HBTParams* params)
{ if(!params->hold_lethal_combos || count < 2) return false;

  int bonus = combo_bonus_for_selection(cards, count);
  if(bonus < params->lethal_combo_bonus) return false;

  uint8_t opp_energy = gstate->current_energy[opponent];
  if((int)opp_energy <= params->lethal_hold_ceiling) return false;
  if(raw_damage >= (float)opp_energy) return false;

  return true;
} // is_held_combo

/* ========================================================================
   Move bookkeeping shared by both phases -- same shape as A5's
   BestMove/consider_move().
   ======================================================================== */

static void consider_move(HBTBestMove* best, float advantage, HBTMoveType type,
                          const uint8_t* cards, uint8_t count)
{ if(advantage <= best->advantage) return;

  best->advantage = advantage;
  best->type = type;
  best->count = count;
  for(uint8_t i = 0; i < count; i++) best->cards[i] = cards[i];
} // consider_move

/* ========================================================================
   Attack: pass / every 1-3 affordable-champion subset (combo-hold excluded
   subsets never reach consider_move) / every affordable draw card / every
   affordable cash card.
   ======================================================================== */

static void evaluate_attack_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                   float opp_energy, float own_hand, float opp_hand,
                                   float own_cash, float opp_cash, PlayerID opponent,
                                   struct gamestate* gstate,
                                   const HBTParams* params, const HBTState* state,
                                   HBTBestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float dmg = predicted_damage(cards, count, opp_energy);
  if(is_held_combo(cards, count, dmg, opponent, gstate, params)) return;

  float adv = hbt_advantage(own_energy, opp_energy - dmg, own_hand - (float)count,
                            opp_hand, own_cash - cost, opp_cash, params, state);
  consider_move(best, adv, HBT_MOVE_CHAMPIONS, cards, count);
} // evaluate_attack_subset

HBTBestMove hbt_best_attack_move(struct gamestate* gstate, PlayerID player,
                                 const HBTParams* params, const HBTState* state)
{ PlayerID opp = 1 - player;
  float own_energy = (float)gstate->current_energy[player];
  float opp_energy = (float)gstate->current_energy[opp];
  float own_hand = (float)gstate->hand[player].size;
  float opp_hand = (float)gstate->hand[opp].size;
  float own_cash = (float)gstate->current_cash_balance[player];
  float opp_cash = (float)gstate->current_cash_balance[opp];

  HBTBestMove best =
  { .advantage = hbt_advantage(own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, params, state),
    .type = HBT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, player,
                                         gstate->current_cash_balance[player], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_attack_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                           own_cash, opp_cash, opp, gstate, params, state, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_attack_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                             own_cash, opp_cash, opp, gstate, params, state, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_attack_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, opp, gstate, params, state, &best);
      }
    }
  }

  bool has_champion = has_champion_in_hand(&gstate->hand[player]);
  const Hand* hand = &gstate->hand[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].cost > gstate->current_cash_balance[player]) continue;

    if(fullDeck[card_idx].card_type == DRAW_CARD)
    { float new_hand = own_hand - 1.0f + (float)fullDeck[card_idx].draw_num;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost;
      float adv = hbt_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                new_cash, opp_cash, params, state);
      consider_move(&best, adv, HBT_MOVE_DRAW, &card_idx, 1);
    }
    else if(fullDeck[card_idx].card_type == CASH_CARD && has_champion)
    { float new_hand = own_hand - 2.0f;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost +
                       (float)fullDeck[card_idx].exchange_cash;
      float adv = hbt_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                new_cash, opp_cash, params, state);
      consider_move(&best, adv, HBT_MOVE_CASH, &card_idx, 1);
    }
  }

  return best;
} // hbt_best_attack_move

/* ========================================================================
   Defense: pass (decline) / every 0-3 affordable-champion subset, against a
   variance-aware incoming-attack estimate (ai_strat_hbt.h's
   defense_stdev_mult).
   ======================================================================== */

float variance_aware_incoming(const struct gamestate* gstate, PlayerID defender,
                              PlayerID attacker, const HBTParams* params)
{ float expected = expected_incoming_attack(gstate, defender);

  float variance = 0.0f;
  const CombatZone* zone = &gstate->combat_zone[attacker];
  for(uint8_t i = 0; i < zone->size; i++)
    variance += champion_variance(zone->cards[i]);

  float incoming = expected + params->defense_stdev_mult * sqrtf(variance);
  return (incoming < 0.0f) ? 0.0f : incoming;
} // variance_aware_incoming

void evaluate_defense_subset(const uint8_t* cards, uint8_t count, float own_energy,
                             float opp_energy, float own_hand, float opp_hand,
                             float own_cash, float opp_cash, float incoming,
                             const HBTParams* params, const HBTState* state,
                             HBTBestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float damage = incoming - predicted_block(cards, count);
  if(damage < 0.0f) damage = 0.0f;

  float new_energy = own_energy - damage;
  if(new_energy < 0.0f) new_energy = 0.0f;

  float adv = hbt_advantage(new_energy, opp_energy, own_hand - (float)count,
                            opp_hand, own_cash - cost, opp_cash, params, state);
  consider_move(best, adv, HBT_MOVE_CHAMPIONS, cards, count);
} // evaluate_defense_subset

HBTBestMove hbt_best_defense_move(const struct gamestate* gstate, PlayerID defender,
                                  const HBTParams* params, const HBTState* state)
{ PlayerID attacker = 1 - defender;
  float own_energy = (float)gstate->current_energy[defender];
  float opp_energy = (float)gstate->current_energy[attacker];
  float own_hand = (float)gstate->hand[defender].size;
  float opp_hand = (float)gstate->hand[attacker].size;
  float own_cash = (float)gstate->current_cash_balance[defender];
  float opp_cash = (float)gstate->current_cash_balance[attacker];
  float incoming = variance_aware_incoming(gstate, defender, attacker, params);

  HBTBestMove best =
  { .advantage = hbt_advantage(own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, params, state),
    .type = HBT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, defender,
                                         gstate->current_cash_balance[defender], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_defense_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                            own_cash, opp_cash, incoming, params, state, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_defense_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, incoming, params, state, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_defense_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                                own_cash, opp_cash, incoming, params, state, &best);
      }
    }
  }

  return best;
} // hbt_best_defense_move
