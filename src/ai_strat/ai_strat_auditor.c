// ai_strat_auditor.c
// Auditor gap-filler strategy ("Corrected Ledger" / The Auditor) -- see
// doc/ai_agents.md's gap-2 section.
//
// A4 Balanced Rules ("Bean Counter") was designed for rating ~62 but
// measured 36 -- its own shipped calibration comment
// (ai_strat_balanced_rules.c's BALANCED_DEFAULTS) documents why: an
// unconstrained parameter search found real strength only by abandoning
// resource discipline entirely (cash/card slopes toward 0 -- spend
// everything regardless of opponent energy -- and a defense_beta past 2.0
// that almost never blocks), but that erodes "Bean Counter" into a
// different, dumber agent, so A4 ships at the identity-safe optimum
// instead (defense_beta capped at 2.0, non-degenerate slopes).
//
// The Auditor picks a point a bit further along that same spectrum than
// A4's identity-safe optimum -- looser resource tracking, a bit more
// reluctant to block -- without going all the way to the degenerate
// extreme. It is a self-contained fork of A4's shape (resource-target
// formula, greedy efficiency-ranked selection), not a variant that shares
// A4's own per-player g_params[2] -- see ai_strat_junior.c's design note:
// two registered agents sharing one static params array indexed by
// PlayerID would corrupt each other's behavior in any round-robin that
// plays both in the same process.
//
// Not independently calibrated to a target rating -- see doc/ai_agents.md's
// gap-2 section: The Auditor, The Impersonator, and The Journeyman are
// measured as-is first; The Inconsistent's mixture weights (a separate
// task) are then tuned to fill whichever of {38, 41, 43, 46} the three of
// them leave uncovered.

#include <math.h>

#include "ai_strat_auditor.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

#define AUD_COST_FLOOR 1.3f

// Looser than A4's identity-safe BALANCED_DEFAULTS (target_cash_slope
// 0.081, defense_beta 1.93): smaller slopes mean the resource targets track
// opponent energy more weakly (more willing to spend early), and a
// defense_beta above A4's own 2.0 identity-safe ceiling demands a bigger
// safety margin before committing a blocker (defends less often).
#define AUD_CASH_SLOPE 0.02f
#define AUD_CASH_INTERCEPT -1.0f
#define AUD_CARDS_SLOPE 0.01f
#define AUD_CARDS_INTERCEPT -0.5f
#define AUD_DEFENSE_BETA 2.3f
#define AUD_LATE_GAME_AGGRO 2.0f
#define AUD_LETHAL_HORIZON 9
#define AUD_DRAW2_HAND_THRESHOLD 6
#define AUD_DRAW3_HAND_THRESHOLD 6

// target = slope*(opp_energy - 8) + intercept, clamped at >= 0, divided by
// AUD_LATE_GAME_AGGRO once opp_energy drops to/below AUD_LETHAL_HORIZON --
// same shape as A4's resource_targets() (ai_strat_balanced_rules.c), looser
// constants.
static void resource_targets(uint8_t opp_energy, float* out_target_cash,
                             float* out_target_cards)
{ float e = (float)((int)opp_energy - 8);

  float target_cash = AUD_CASH_SLOPE * e + AUD_CASH_INTERCEPT;
  float target_cards = AUD_CARDS_SLOPE * e + AUD_CARDS_INTERCEPT;

  if(target_cash < 0.0f) target_cash = 0.0f;
  if(target_cards < 0.0f) target_cards = 0.0f;

  if((int)opp_energy <= AUD_LETHAL_HORIZON)
  { target_cash /= AUD_LATE_GAME_AGGRO;
    target_cards /= AUD_LATE_GAME_AGGRO;
  }

  *out_target_cash = target_cash;
  *out_target_cards = target_cards;
} // resource_targets

static uint8_t find_affordable_draw_card(const struct gamestate* gstate, PlayerID player,
                                         uint8_t draw_num)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type == DRAW_CARD &&
       fullDeck[card_idx].draw_num == draw_num &&
       fullDeck[card_idx].cost <= budget)
      return card_idx;
  }
  return UINT8_MAX;
} // find_affordable_draw_card

static bool try_play_draw_step(struct gamestate* gstate, PlayerID player,
                               uint8_t opp_energy, GameContext* ctx)
{ if((int)opp_energy <= AUD_LETHAL_HORIZON) return false;

  uint8_t hand_size = gstate->hand[player].size;

  if(hand_size < AUD_DRAW3_HAND_THRESHOLD)
  { uint8_t card_idx = find_affordable_draw_card(gstate, player, 3);
    if(card_idx != UINT8_MAX)
    { play_draw_card(gstate, player, card_idx, ctx);
      return true;
    }
  }

  if(hand_size < AUD_DRAW2_HAND_THRESHOLD)
  { uint8_t card_idx = find_affordable_draw_card(gstate, player, 2);
    if(card_idx != UINT8_MAX)
    { play_draw_card(gstate, player, card_idx, ctx);
      return true;
    }
  }

  return false;
} // try_play_draw_step

// Greedily plays up to max_count champions, ranked by plain
// power/(cost+floor) efficiency (no combo nudge -- A4's own combo_weight
// ships at 0.0 too), affordable within cash_cap.
static uint8_t select_attack_champions(const struct gamestate* gstate, PlayerID player,
                                       uint8_t max_count, uint16_t cash_cap, uint8_t* out)
{ uint8_t candidates[12];
  uint8_t candidate_count = build_affordable_champions(gstate, player, cash_cap, candidates);

  uint8_t chosen_count = 0;
  int32_t budget_left = cash_cap;

  while(chosen_count < max_count)
  { int8_t best_slot = -1;
    float best_score = -1.0f;

    for(uint8_t i = 0; i < candidate_count; i++)
    { uint8_t card_idx = candidates[i];
      if(card_idx == UINT8_MAX) continue;
      if(fullDeck[card_idx].cost > budget_left) continue;

      float score = fullDeck[card_idx].expected_attack /
                    (fullDeck[card_idx].cost + AUD_COST_FLOOR);
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
} // select_attack_champions

void auditor_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  uint8_t opp_energy = gstate->current_energy[1 - player];

  if(try_play_draw_step(gstate, player, opp_energy, ctx))
    return;

  float effective_hand, effective_cash;
  effective_hand_and_cash(gstate, player, &effective_hand, &effective_cash);

  float target_cash, target_cards;
  resource_targets(opp_energy, &target_cash, &target_cards);

  long cards_to_play = lroundf(effective_hand - target_cards);
  if(cards_to_play < 0) cards_to_play = 0;
  if(cards_to_play > 3) cards_to_play = 3;

  float cash_surplus = effective_cash - target_cash;
  uint16_t cash_cap = (cash_surplus > 0.0f) ? (uint16_t)cash_surplus : 0;

  uint8_t chosen[3];
  uint8_t chosen_count = select_attack_champions(gstate, player, (uint8_t)cards_to_play,
                                                 cash_cap, chosen);

  if(chosen_count == 0)
  { uint8_t affordable[12];
    uint8_t count = build_affordable_champions(gstate, player, cash_cap, affordable);
    try_play_cash_fallback(gstate, player, count, ctx);
    return;
  }

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, player, chosen[i], ctx);
} // auditor_attack_strategy

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
  { uint8_t card_idx = candidates[i];
    out[i] = (DefenseCandidate)
    { .card_index = card_idx,
        .defense_score = fullDeck[card_idx].expected_defense /
                         (fullDeck[card_idx].cost + AUD_COST_FLOOR)
    };
  }

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

// Walks the defense-efficiency ranking, adding defenders while the running
// total (actual combo bonus included) stays at or below
// E[Attack] - AUD_DEFENSE_BETA*sigma -- same shape as A4's select_defenders()
// (ai_strat_balanced_rules.c), just a bigger beta (demands a wider safety
// margin, so it blocks less often).
static uint8_t select_defenders(const struct gamestate* gstate, PlayerID defender,
                                uint16_t cash_cap, float cap, uint8_t* out)
{ DefenseCandidate ranked[12];
  uint8_t count = build_ranked_defenders(gstate, defender, cash_cap, ranked);

  uint8_t chosen_count = 0;
  float total_defense = 0.0f;
  int32_t budget_left = cash_cap;

  for(uint8_t i = 0; i < count && chosen_count < 3; i++)
  { uint8_t card_idx = ranked[i].card_index;
    if(fullDeck[card_idx].cost > budget_left) continue;

    uint8_t trial[3];
    for(uint8_t j = 0; j < chosen_count; j++) trial[j] = out[j];
    trial[chosen_count] = card_idx;

    float trial_total = total_defense + fullDeck[card_idx].expected_defense +
                        (float)combo_bonus_for_selection(trial, (uint8_t)(chosen_count + 1));
    if(trial_total > cap) continue;

    out[chosen_count++] = card_idx;
    total_defense += fullDeck[card_idx].expected_defense;
    budget_left -= fullDeck[card_idx].cost;
  }

  return chosen_count;
} // select_defenders

void auditor_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  PlayerID attacker = gstate->current_player;

  float expected_attack = expected_incoming_attack(gstate, defender);

  float attack_variance = 0.0f;
  const CombatZone* zone = &gstate->combat_zone[attacker];
  for(uint8_t i = 0; i < zone->size; i++)
    attack_variance += champion_variance(zone->cards[i]);

  float cap = expected_attack - AUD_DEFENSE_BETA * sqrtf(attack_variance);
  if(cap < 0.0f) cap = 0.0f;

  float effective_cash;
  { float unused_hand;
    effective_hand_and_cash(gstate, defender, &unused_hand, &effective_cash);
  }

  uint8_t opp_energy = gstate->current_energy[attacker];
  float target_cash, target_cards_unused;
  resource_targets(opp_energy, &target_cash, &target_cards_unused);

  float cash_surplus = effective_cash - target_cash;
  uint16_t cash_cap = (cash_surplus > 0.0f) ? (uint16_t)cash_surplus : 0;

  uint8_t chosen[3];
  uint8_t chosen_count = select_defenders(gstate, defender, cash_cap, cap, chosen);

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, defender, chosen[i], ctx);
} // auditor_defense_strategy
