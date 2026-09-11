// ai_strat_journeyman.c
// Journeyman gap-filler strategy ("Partial Synthesis" / The Journeyman) --
// see doc/ai_agents.md's gap-2 section.
//
// A2 Combo Threshold ("The Showboat") chases combo bonuses above a
// threshold on attack; A4 Balanced Rules ("Bean Counter") caps its
// defenders at E[Attack] - beta*sigma on defense. The Journeyman splits
// responsibilities by phase rather than blending scores within one: it
// attacks with a *simplified* version of A2's combo-chasing (single
// champions or 2-card combos above a threshold -- no 3-card combos, no
// save-for-lethal holding, no probabilistic decline: those are the parts
// of A2's own identity this agent hasn't mastered yet), and defends with a
// *simplified* version of A4's capped selection (same cap formula, no
// resource-target cash gating -- it isn't tracking a cash ledger like Bean
// Counter, just deciding whether a block is worth it in the moment).
// Neither half is a full synthesis (unlike A7 Hybrid HBT's three-layer
// blend of A4/A5/A6) -- "getting the hang of two techniques without fully
// mastering either" is the point of this personality.
//
// Self-contained: no shared state with the real AI_STRATEGY_COMBO_THRESHOLD
// or AI_STRATEGY_BALANCED agents (see ai_strat_junior.c's design note on
// why gap-filler agents don't reuse another registered agent's per-player
// override hooks).
//
// Not independently calibrated to a target rating -- see doc/ai_agents.md's
// gap-2 section: measured as-is alongside The Auditor and The Impersonator
// first.

#include <math.h>

#include "ai_strat_journeyman.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

#define JM_DRAW_HAND_THRESHOLD 5
#define JM_DRAW_ENERGY_FLOOR 20
#define JM_COST_FLOOR 1.3f

// Combo-bonus threshold and weight for the 2-card override below -- roughly
// A2's own CT_DEFAULTS (combo_bonus_threshold=11, aggression_level=1.3,
// combo_weight=2.3626), collapsed to one un-scaled threshold since there's
// no aggression_level dial here to divide it.
#define JM_COMBO_BONUS_THRESHOLD 9.0f
#define JM_COMBO_WEIGHT 2.0f

// Lower than A4's own identity-safe 1.93 (which A4's own calibration
// comment documents as chosen only to stay "recognizably Bean Counter",
// not because it's inherently the best defense) -- The Journeyman has no
// such identity constraint, so it blocks more readily.
#define JM_DEFENSE_BETA 2.2f

/* ========================================================================
   Attack: A1-style efficiency-ranked 2-card greedy baseline (the "learned
   technique" half), overridden by a 2-card combo if its bonus clears
   JM_COMBO_BONUS_THRESHOLD and it outscores the baseline pair (the "chases
   a good combo" half, borrowed from A2). No 3-card combos, no lethal-hold,
   no cash fallback -- those are the parts of A2's own sophistication this
   agent hasn't picked up yet.
   ======================================================================== */

#define JM_MAX_ATTACK_CARDS 3

static uint8_t rank_by_efficiency(const uint8_t* affordable, uint8_t count, uint8_t* out)
{ uint8_t n = 0;
  bool used[12] = { false };

  for(uint8_t picked = 0; picked < count; picked++)
  { int best_slot = -1;
    float best_score = -1.0f;
    for(uint8_t i = 0; i < count; i++)
    { if(used[i]) continue;
      float score = fullDeck[affordable[i]].expected_attack /
                    (fullDeck[affordable[i]].cost + JM_COST_FLOOR);
      if(best_slot < 0 || score > best_score)
      { best_score = score;
        best_slot = (int)i;
      }
    }
    used[best_slot] = true;
    out[n++] = affordable[best_slot];
  }

  return n;
} // rank_by_efficiency

// Greedy knapsack over the efficiency ranking, same shape as A1's
// play_attack_selection() (ai_strat_valuebased.c): walk the ranking, play
// each card that fits the remaining budget, until JM_MAX_ATTACK_CARDS is
// reached. Writes chosen indices to out and returns both the count and the
// combined raw expected_attack (for comparison against the combo override).
static uint8_t select_baseline_attack(const struct gamestate* gstate, PlayerID player,
                                      const uint8_t* affordable, uint8_t count,
                                      uint8_t* out, float* out_score)
{ uint8_t ranked[12];
  uint8_t ranked_n = rank_by_efficiency(affordable, count, ranked);

  uint16_t remaining_budget = gstate->current_cash_balance[player];
  uint8_t played = 0;
  float score = 0.0f;

  for(uint8_t i = 0; i < ranked_n && played < JM_MAX_ATTACK_CARDS; i++)
  { uint8_t card_idx = ranked[i];
    if(fullDeck[card_idx].cost > remaining_budget) continue;

    out[played++] = card_idx;
    score += fullDeck[card_idx].expected_attack;
    remaining_budget -= fullDeck[card_idx].cost;
  }

  *out_score = score;
  return played;
} // select_baseline_attack

// Best-scoring 2-card combo whose bonus clears JM_COMBO_BONUS_THRESHOLD,
// affordable within `cash`. Writes both card indices to out[0]/out[1] and
// returns the combined score, or returns -1.0f (out untouched) if none
// clears the threshold.
static float best_pair_combo(const uint8_t* affordable, uint8_t count, uint16_t cash,
                             uint8_t* out)
{ float best_score = -1.0f;

  for(uint8_t i = 0; i < count; i++)
    for(uint8_t j = i + 1; j < count; j++)
    { uint8_t cards[2] = { affordable[i], affordable[j] };
      uint16_t total_cost = fullDeck[cards[0]].cost + fullDeck[cards[1]].cost;
      if(total_cost > cash) continue;

      int bonus = combo_bonus_for_selection(cards, 2);
      if((float)bonus < JM_COMBO_BONUS_THRESHOLD) continue;

      float score = fullDeck[cards[0]].expected_attack +
                    fullDeck[cards[1]].expected_attack +
                    JM_COMBO_WEIGHT * (float)bonus;
      if(score > best_score)
      { best_score = score;
        out[0] = cards[0];
        out[1] = cards[1];
      }
    }

  return best_score;
} // best_pair_combo

// Same idea, 3 cards -- unlike A2's eval_three_card_combos(), no
// save-for-lethal exclusion (that holding-back sophistication is one of
// the things this agent hasn't picked up).
static float best_triple_combo(const uint8_t* affordable, uint8_t count, uint16_t cash,
                               uint8_t* out)
{ float best_score = -1.0f;

  for(uint8_t i = 0; i < count; i++)
    for(uint8_t j = i + 1; j < count; j++)
      for(uint8_t k = j + 1; k < count; k++)
      { uint8_t cards[3] = { affordable[i], affordable[j], affordable[k] };
        uint16_t total_cost = fullDeck[cards[0]].cost + fullDeck[cards[1]].cost +
                              fullDeck[cards[2]].cost;
        if(total_cost > cash) continue;

        int bonus = combo_bonus_for_selection(cards, 3);
        if((float)bonus < JM_COMBO_BONUS_THRESHOLD) continue;

        float score = fullDeck[cards[0]].expected_attack +
                      fullDeck[cards[1]].expected_attack +
                      fullDeck[cards[2]].expected_attack +
                      JM_COMBO_WEIGHT * (float)bonus;
        if(score > best_score)
        { best_score = score;
          out[0] = cards[0];
          out[1] = cards[1];
          out[2] = cards[2];
        }
      }

  return best_score;
} // best_triple_combo

void journeyman_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;

  if(try_play_draw_card(gstate, player, JM_DRAW_HAND_THRESHOLD,
                        JM_DRAW_ENERGY_FLOOR, ctx))
    return;

  uint16_t cash = gstate->current_cash_balance[player];
  uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, player, cash, affordable);
  if(count == 0) return;

  uint8_t baseline[2];
  float baseline_score;
  uint8_t baseline_count = select_baseline_attack(gstate, player, affordable, count,
                                                  baseline, &baseline_score);

  uint8_t pair[2];
  float pair_score = best_pair_combo(affordable, count, cash, pair);

  uint8_t triple[3];
  float triple_score = best_triple_combo(affordable, count, cash, triple);

  if(triple_score > baseline_score && triple_score > pair_score)
  { for(uint8_t i = 0; i < 3; i++) play_champion(gstate, player, triple[i], ctx);
    return;
  }

  if(pair_score > baseline_score)
  { play_champion(gstate, player, pair[0], ctx);
    play_champion(gstate, player, pair[1], ctx);
    return;
  }

  for(uint8_t i = 0; i < baseline_count; i++)
    play_champion(gstate, player, baseline[i], ctx);
} // journeyman_attack_strategy

/* ========================================================================
   Defense: same capped-selection shape as A4 (ai_strat_balanced_rules.c's
   select_defenders()), ranked by raw expected_defense (no cost-efficiency
   ratio, no resource-target cash gating -- full current cash balance is
   the only budget).
   ======================================================================== */

typedef struct
{ uint8_t card_index;
  float defense_score;
} DefenseCandidate;

static bool defense_ranks_before(const DefenseCandidate* a, const DefenseCandidate* b)
{ if(a->defense_score != b->defense_score) return a->defense_score > b->defense_score;
  return a->card_index < b->card_index;
} // defense_ranks_before

static uint8_t build_ranked_defenders(const struct gamestate* gstate, PlayerID defender,
                                      uint16_t cash_cap, DefenseCandidate* out)
{ uint8_t candidates[12];
  uint8_t count = build_affordable_champions(gstate, defender, cash_cap, candidates);

  for(uint8_t i = 0; i < count; i++)
    out[i] = (DefenseCandidate)
  { .card_index = candidates[i],
      .defense_score = fullDeck[candidates[i]].expected_defense
  };

  for(uint8_t i = 1; i < count; i++)
  { DefenseCandidate key = out[i];
    int j = i - 1;
    while(j >= 0 && defense_ranks_before(&key, &out[j]))
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }

  return count;
} // build_ranked_defenders

void journeyman_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  PlayerID attacker = gstate->current_player;

  float expected_attack = expected_incoming_attack(gstate, defender);

  float attack_variance = 0.0f;
  const CombatZone* zone = &gstate->combat_zone[attacker];
  for(uint8_t i = 0; i < zone->size; i++)
    attack_variance += champion_variance(zone->cards[i]);

  float cap = expected_attack - JM_DEFENSE_BETA * sqrtf(attack_variance);
  if(cap < 0.0f) cap = 0.0f;

  uint16_t cash_cap = gstate->current_cash_balance[defender];

  DefenseCandidate ranked[12];
  uint8_t count = build_ranked_defenders(gstate, defender, cash_cap, ranked);

  uint8_t chosen[3];
  uint8_t chosen_count = 0;
  float total_defense = 0.0f;
  int32_t budget_left = cash_cap;

  for(uint8_t i = 0; i < count && chosen_count < 3; i++)
  { uint8_t card_idx = ranked[i].card_index;
    if(fullDeck[card_idx].cost > budget_left) continue;

    uint8_t trial[3];
    for(uint8_t j = 0; j < chosen_count; j++) trial[j] = chosen[j];
    trial[chosen_count] = card_idx;

    float trial_total = total_defense + fullDeck[card_idx].expected_defense +
                        (float)combo_bonus_for_selection(trial, (uint8_t)(chosen_count + 1));
    if(trial_total > cap) continue;

    chosen[chosen_count++] = card_idx;
    total_defense += fullDeck[card_idx].expected_defense;
    budget_left -= fullDeck[card_idx].cost;
  }

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, defender, chosen[i], ctx);
} // journeyman_defense_strategy
