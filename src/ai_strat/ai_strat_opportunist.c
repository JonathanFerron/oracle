// ai_strat_opportunist.c
// Opportunist gap-filler strategy ("Tactical Plus" / The Opportunist) --
// see doc/ai_agents.md's gap-3 section.
//
// A6 Tactical's ("Pressure Cooker") exact attack/defense mechanism,
// unchanged, plus one addition: an "opportunistic finisher" override --
// if any affordable 1-3 champion subset would deal lethal damage (meet or
// exceed the opponent's current energy), play it immediately, overriding
// the normal aggression-gated attacker count. A6 itself has no such
// override: its aggression formula caps at 3 attackers via a phase/hand-
// power read that has no special case for "this exact subset wins right
// now" -- so a conservative aggression read can leave a lethal play on
// the table. Defense is untouched.
//
// A first version instead ported A3/A7's lethal-combo HOLD (the opposite
// idea: decline a good combo now to cash it in later) and measured
// parity with or worse than plain Tactical no matter how the threshold
// was tuned (52 at A3/A7's own bonus=24, down to 34 at a much lower
// bonus=12) -- holding turned out to be a strict cost here, the same
// signature this project has already found for A9's reply_trust and
// A13's hplus_trust, and structurally it can never do better than "never
// hold" (i.e. plain Tactical) since holding only ever withholds value.
// Replaced with the finisher override instead, which can only ever add
// value (it fires *only* on subsets that were about to be missed, never
// overrides a play that was already going to happen) -- but a
// lethal-in-one-turn moment turned out too rare on its own to move this
// agent measurably above plain Tactical (51-53, noise-level parity, both
// measured). A flat aggression boost is layered on top for real
// separation: "watches for the finishing blow AND generally presses a
// bit harder" -- two small, additive traits rather than one rare one.
// 0.20 measures identically to 0.45 (54 both times, near-identical win
// counts) -- this lever saturates the aggression-band formula (0.25/0.5/
// 0.75 thresholds) quickly, so there's no reason to ship an arbitrarily
// larger number for the same effect. Matches A6's own calibration history
// (ai_strat_tactical.c's TACTICAL_DEFAULTS comment): individual aggression
// parameters measured "small and mostly flat" in isolation there too.
#define OPP_AGGRESSION_BOOST 0.20f
//
// Self-contained: its own #defines, not a variant sharing A6's own
// per-player g_params[2] (see ai_strat_junior.c's design note on why
// gap-filler agents never reuse another registered agent's per-player
// override hooks).

#include <math.h>

#include "ai_strat_opportunist.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

#define OPP_COST_FLOOR 1.3f

#define OPP_HAND_POWER_STRONG_RATIO 1.5f
#define OPP_HAND_POWER_WEAK_RATIO 0.7f
#define OPP_OPP_CASH_HIGH_THRESHOLD 35
#define OPP_OPP_CASH_HIGH_MULT 1.1f
#define OPP_OPP_CASH_LOW_THRESHOLD 15
#define OPP_OPP_CASH_LOW_MULT 0.9f

// A6's own TACTICAL_DEFAULTS values (ai_strat_tactical.c), reused as-is --
// this agent's attack/defense mechanism is A6's, unmodified.
#define OPP_PHASE_MID_THRESHOLD 67
#define OPP_PHASE_LATE_THRESHOLD 41
#define OPP_PHASE_CRITICAL_THRESHOLD 18
#define OPP_AGGR_ENERGY_DIFF_WEIGHT 0.0008022129f
#define OPP_AGGR_OPP_LATE_BONUS 0.1262423f
#define OPP_AGGR_OPP_CRITICAL_BONUS 0.2819330f
#define OPP_AGGR_SELF_LATE_PENALTY 0.0530097f
#define OPP_AGGR_SELF_CRITICAL_PENALTY 0.1475105f
#define OPP_AGGR_HAND_POWER_BONUS 0.2479543f
#define OPP_AGGR_HAND_POWER_PENALTY 0.1542592f
#define OPP_AGGR_CASH_SURPLUS_THRESHOLD 10
#define OPP_AGGR_CASH_SURPLUS_BONUS 0.2301680f
#define OPP_DEFENSE_DAMAGE_WEIGHT 0.0420395f
#define OPP_DEFENSE_CASH_WEIGHT 1.6231302f
#define OPP_DEFENSE_CONSERVATIVE_STDEV_MULT 1.2332415f
#define OPP_DRAW_MIN_HAND_SIZE 5

/* ========================================================================
   Layer T: aggression factor, ported verbatim from A6.
   ======================================================================== */

typedef enum
{ OPP_PHASE_EARLY,
  OPP_PHASE_MID,
  OPP_PHASE_LATE,
  OPP_PHASE_CRITICAL
} OppPhase;

static OppPhase game_phase(uint8_t energy)
{ if(energy >= OPP_PHASE_MID_THRESHOLD) return OPP_PHASE_EARLY;
  if(energy >= OPP_PHASE_LATE_THRESHOLD) return OPP_PHASE_MID;
  if(energy >= OPP_PHASE_CRITICAL_THRESHOLD) return OPP_PHASE_LATE;
  return OPP_PHASE_CRITICAL;
} // game_phase

static float hand_power_sum(const Hand* hand)
{ float total = 0.0f;
  for(uint8_t i = 0; i < hand->size; i++)
    total += fullDeck[hand->cards[i]].power;
  return total;
} // hand_power_sum

static float estimate_opponent_power(const struct gamestate* gstate, PlayerID opponent)
{ float estimate = (float)gstate->hand[opponent].size * (float)AVERAGE_POWER_FOR_MULLIGAN;

  if(gstate->current_cash_balance[opponent] > OPP_OPP_CASH_HIGH_THRESHOLD)
    estimate *= OPP_OPP_CASH_HIGH_MULT;
  else if(gstate->current_cash_balance[opponent] < OPP_OPP_CASH_LOW_THRESHOLD)
    estimate *= OPP_OPP_CASH_LOW_MULT;

  return estimate;
} // estimate_opponent_power

static float evaluate_aggression_factor(const struct gamestate* gstate, PlayerID player)
{ PlayerID opp = 1 - player;
  uint8_t own_energy = gstate->current_energy[player];
  uint8_t opp_energy = gstate->current_energy[opp];

  OppPhase my_phase = game_phase(own_energy);
  OppPhase opp_phase = game_phase(opp_energy);
  float my_hand_power = hand_power_sum(&gstate->hand[player]);
  float opp_estimated_power = estimate_opponent_power(gstate, opp);

  float aggression = 0.5f;
  aggression += ((float)own_energy - (float)opp_energy) * OPP_AGGR_ENERGY_DIFF_WEIGHT;

  if(opp_phase == OPP_PHASE_CRITICAL) aggression += OPP_AGGR_OPP_CRITICAL_BONUS;
  else if(opp_phase == OPP_PHASE_LATE) aggression += OPP_AGGR_OPP_LATE_BONUS;

  if(my_phase == OPP_PHASE_CRITICAL) aggression -= OPP_AGGR_SELF_CRITICAL_PENALTY;
  else if(my_phase == OPP_PHASE_LATE) aggression -= OPP_AGGR_SELF_LATE_PENALTY;

  if(my_hand_power > opp_estimated_power * OPP_HAND_POWER_STRONG_RATIO)
    aggression += OPP_AGGR_HAND_POWER_BONUS;
  if(my_hand_power < opp_estimated_power * OPP_HAND_POWER_WEAK_RATIO)
    aggression -= OPP_AGGR_HAND_POWER_PENALTY;

  if(gstate->current_cash_balance[player] > OPP_AGGR_CASH_SURPLUS_THRESHOLD)
    aggression += OPP_AGGR_CASH_SURPLUS_BONUS;

  aggression += OPP_AGGRESSION_BOOST;

  if(aggression < 0.0f) aggression = 0.0f;
  if(aggression > 1.0f) aggression = 1.0f;
  return aggression;
} // evaluate_aggression_factor

/* ========================================================================
   Attack: A6's exact draw-trigger + aggression-driven attacker count +
   greedy combo-aware selection, then the one new gate: hold back if the
   selected group is a big, not-yet-lethal combo against a still-healthy
   opponent.
   ======================================================================== */

static float attack_selection_score(uint8_t card_idx, const uint8_t* already_selected,
                                    uint8_t selected_count)
{ float base = fullDeck[card_idx].expected_attack / (fullDeck[card_idx].cost + OPP_COST_FLOOR);

  uint8_t trial[3];
  for(uint8_t i = 0; i < selected_count; i++) trial[i] = already_selected[i];
  trial[selected_count] = card_idx;

  int with_bonus = combo_bonus_for_selection(trial, (uint8_t)(selected_count + 1));
  int without_bonus = (selected_count >= 2) ?
                      combo_bonus_for_selection(already_selected, selected_count) : 0;

  return base + (float)(with_bonus - without_bonus);
} // attack_selection_score

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
      if(card_idx == UINT8_MAX) continue;
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
    candidates[best_slot] = UINT8_MAX;
  }

  return chosen_count;
} // select_best_attackers

static uint8_t desired_attacker_count(float aggression)
{ if(aggression >= 0.75f) return 3;
  if(aggression >= 0.5f) return 2;
  if(aggression >= 0.25f) return 1;
  return 0;
} // desired_attacker_count

// Any 1-3 affordable-champion subset whose predicted damage (raw expected
// attack + combo bonus, uncapped) meets or exceeds the opponent's current
// energy. Exhaustive over `affordable` (<=12 cards, so <=299 subsets,
// same bound as A3's own enumeration) -- this is a rare, situational
// check, not a replacement for A6's own greedy selection, so no pruning.
// Returns the subset size (0 = none found) and writes it to `out`
// (>=3 slots), preferring the CHEAPEST winning subset found (so this
// agent doesn't spend more cash than it needs to finish the game).
static uint8_t find_lethal_subset(const struct gamestate* gstate, PlayerID player,
                                  const uint8_t* affordable, uint8_t count,
                                  uint8_t opp_energy, uint8_t* out)
{ uint16_t budget = gstate->current_cash_balance[player];
  uint8_t best_count = 0;
  uint16_t best_cost = UINT16_MAX;

  for(uint8_t i = 0; i < count; i++)
    for(uint8_t j = i; j < count; j++)
      for(uint8_t k = j; k < count; k++)
      { uint8_t cards[3];
        uint8_t n = 0;
        cards[n++] = affordable[i];
        if(j != i) cards[n++] = affordable[j];
        if(k != j) cards[n++] = affordable[k];

        uint16_t cost = 0;
        float damage = (float)combo_bonus_for_selection(cards, n);
        for(uint8_t c = 0; c < n; c++)
        { cost += fullDeck[cards[c]].cost;
          damage += fullDeck[cards[c]].expected_attack;
        }
        if(cost > budget || damage < (float)opp_energy) continue;

        if(cost < best_cost)
        { best_cost = cost;
          best_count = n;
          for(uint8_t c = 0; c < n; c++) out[c] = cards[c];
        }
      }

  return best_count;
} // find_lethal_subset

void opportunist_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  PlayerID opp = 1 - player;

  if(try_play_draw_card(gstate, player, OPP_DRAW_MIN_HAND_SIZE,
                        OPP_PHASE_CRITICAL_THRESHOLD, ctx))
    return;

  uint8_t affordable[12];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t affordable_count = build_affordable_champions(gstate, player, budget, affordable);

  uint8_t lethal[3];
  uint8_t lethal_count = find_lethal_subset(gstate, player, affordable, affordable_count,
                                            gstate->current_energy[opp], lethal);
  if(lethal_count > 0)
  { for(uint8_t i = 0; i < lethal_count; i++)
      play_champion(gstate, player, lethal[i], ctx);
    return;
  }

  float aggression = evaluate_aggression_factor(gstate, player);

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
} // opportunist_attack_strategy

/* ========================================================================
   Defense: A6's exact mechanism, unchanged -- holding applies to attack
   only.
   ======================================================================== */

typedef struct
{ uint8_t card_index;
  float defense_score;
  float attack_score;
} OppDefenseCandidate;

static bool defense_ranks_before(const OppDefenseCandidate* a, const OppDefenseCandidate* b)
{ if(a->defense_score != b->defense_score) return a->defense_score > b->defense_score;
  if(a->attack_score != b->attack_score) return a->attack_score < b->attack_score;
  return a->card_index < b->card_index;
} // defense_ranks_before

static uint8_t build_ranked_defenders(const struct gamestate* gstate, PlayerID defender,
                                      OppDefenseCandidate* out)
{ uint8_t candidates[12];
  uint16_t budget = gstate->current_cash_balance[defender];
  uint8_t count = build_affordable_champions(gstate, defender, budget, candidates);

  for(uint8_t i = 0; i < count; i++)
  { uint8_t card_idx = candidates[i];
    out[i] = (OppDefenseCandidate)
    { .card_index = card_idx,
        .defense_score = fullDeck[card_idx].expected_defense /
                         (fullDeck[card_idx].cost + OPP_COST_FLOOR),
                         .attack_score = fullDeck[card_idx].expected_attack /
                                         (fullDeck[card_idx].cost + OPP_COST_FLOOR)
    };
  }

  for(uint8_t i = 1; i < count; i++)
  { OppDefenseCandidate key = out[i];
    int j = i - 1;
    while(j >= 0 && defense_ranks_before(&key, &out[j]))
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }

  return count;
} // build_ranked_defenders

void opportunist_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  PlayerID attacker = gstate->current_player;

  float expected_attack = expected_incoming_attack(gstate, defender);

  float attack_variance = 0.0f;
  const CombatZone* zone = &gstate->combat_zone[attacker];
  for(uint8_t i = 0; i < zone->size; i++)
    attack_variance += champion_variance(zone->cards[i]);

  float attack_estimate = expected_attack +
                          OPP_DEFENSE_CONSERVATIVE_STDEV_MULT * sqrtf(attack_variance);

  OppDefenseCandidate ranked[12];
  uint8_t count = build_ranked_defenders(gstate, defender, ranked);

  float best_value = -attack_estimate * OPP_DEFENSE_DAMAGE_WEIGHT;
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

    float value = -(damage * OPP_DEFENSE_DAMAGE_WEIGHT + running_cost * OPP_DEFENSE_CASH_WEIGHT);

    if(value > best_value)
    { best_value = value;
      best_num = num;
    }
  }

  for(uint8_t i = 0; i < best_num; i++)
    play_champion(gstate, defender, prefix[i], ctx);
} // opportunist_defense_strategy
