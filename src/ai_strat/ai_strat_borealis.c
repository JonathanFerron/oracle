// ai_strat_borealis.c
// A3 Borealis strategy ("the benchmark") -- see ai_strat_borealis.h and
// ideas/A3 ai agent greedy power (borealis)/greedy_power_borealis_handout.md.
// Attack/defense orchestration and parameter management; candidate
// enumeration and scoring live in ai_strat_borealis_enum.c (Sec.10's
// file-length guidance). The draw-card fallback (Sec.9, try_play_draw_card())
// and the cash-card fallback (Sec.9, try_play_cash_fallback() -- promoted to
// ai_strat_common.{c,h} once A4 Balanced Rules needed the identical logic)
// are placeholder heuristics shared across agents; the discard-to-7/mulligan
// overrides below (Sec.7) are Borealis-specific and clearly marked -- the
// first things a stronger agent should replace.

#include "ai_strat_borealis.h"
#include "ai_strat_borealis_enum.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// Calibrated 2026-08-23 via aicalibsrc/borealis/calibrate_borealis.py's
// `optimize` (differential evolution, all six parameters free, vs `combo`
// -- vs `rand` is ceiling-effected like A1/A2 were, and vs `value` was
// already near parity, so `combo` had the most headroom: at the handout's
// untuned defaults Borealis actually *lost* to it, 43.6%). A preliminary
// manual sweep (this file's calibration log) found the real luna_value
// optimum sits far above the handout's own default guess (0.5) or its
// Sec.13 sweep grid's max (2.0) -- win rate vs `combo` climbs from ~36.5%
// at lambda=0 to a peak around lambda=4.0-4.5 before declining again past
// 6.0, which is why BOUNDS["luna_value"] in the driver was widened to
// (0.0, 6.0). The optimizer's winner landed inside that peak and was
// re-checked for Sec.8's unimodality property (quadratic fit of a lambda
// re-sweep with the other five parameters held at these values):
// concave-down, R^2=0.908, implied optimum 4.066 -- consistent with the
// value shipped below, so this remains usable as a Bradley-Terry anchor.
//
// lethal_combo_bonus and min_hand_size_target both landed at their search
// bounds (24 and 6 respectively) -- unlike A2's aggression_level/
// combo_bonus_threshold, these aren't identity-defining traits (handout
// Sec.9 explicitly calls the draw-card heuristic a placeholder), so this
// wasn't treated as a personality-drift rejection, but a follow-up
// calibration pass with wider bounds on those two may find further gains.
// An A/B (hold_lethal_combos true vs false, otherwise identical) measured
// an exact 50.00% self-play tie at these values -- lethal_combo_bonus=24
// is high enough that holding rarely triggers either way, so it was left
// at its optimizer-found `true` rather than forced off.
//
// Measured (validated, both seats): vs `combo` 43.63% -> 69.13%
// [68.67%, 69.58%] (40,000 games); vs `value` 49.63% -> 74.19% [73.51%,
// 74.87%] (16,000 games); vs `rand` 93.13% -> 99.33% [99.19%, 99.45%]
// (16,000 games). Single source of truth for g_params[]'s initializer and
// borealis_get_default_params() (same pattern as A2's CT_DEFAULTS,
// ai_strat_combo_threshold.c).
#define BOREALIS_DEFAULTS \
  { .luna_value = 4.5846f, \
    .tiebreak_epsilon = 0.3444f, \
    .hold_lethal_combos = true, \
    .lethal_combo_bonus = 24, \
    .lethal_hold_ceiling = 38, \
    .min_hand_size_target = 6 }

#define BOREALIS_DRAW_ENERGY_FLOOR 20 // handout Sec.9

static BorealisParams g_params[2] = { BOREALIS_DEFAULTS, BOREALIS_DEFAULTS };

BorealisParams borealis_get_default_params(void)
{ BorealisParams defaults = BOREALIS_DEFAULTS;
  return defaults;
} // borealis_get_default_params

void borealis_set_params(PlayerID player, const BorealisParams* params)
{ g_params[player] = *params;
} // borealis_set_params

void borealis_reset_params(void)
{ BorealisParams defaults = BOREALIS_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // borealis_reset_params

void borealis_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  const BorealisParams* params = &g_params[player];

  if(try_play_draw_card(gstate, player, (uint8_t)params->min_hand_size_target,
                        BOREALIS_DRAW_ENERGY_FLOOR, ctx))
    return;

  uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, player,
                                             gstate->current_cash_balance[player],
                                             affordable);

  uint8_t chosen[3];
  uint8_t chosen_count;
  borealis_best_champion_set(affordable, count, gstate, player,
                             borealis_expected_attack_of, -1.0f, true, params,
                             ctx, chosen, &chosen_count);

  if(chosen_count == 0)
  { try_play_cash_fallback(gstate, player, count, ctx);
    return;
  }

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, player, chosen[i], ctx);
} // borealis_attack_strategy

void borealis_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  const BorealisParams* params = &g_params[defender];

  uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, defender,
                                             gstate->current_cash_balance[defender],
                                             affordable);

  float incoming = expected_incoming_attack(gstate, defender);

  uint8_t chosen[3];
  uint8_t chosen_count;
  borealis_best_champion_set(affordable, count, gstate, defender,
                             borealis_expected_defense_of, incoming, false, params,
                             ctx, chosen, &chosen_count);

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, defender, chosen[i], ctx);
} // borealis_defense_strategy

/* ========================================================================
   Discard-to-7 / mulligan overrides (Sec.7's 2026-08-22 note) -- protect
   the best held combo from the shared lowest-`power` heuristic, which would
   otherwise preferentially throw away exactly the expensive champions a
   held combo needs.
   ======================================================================== */

static bool is_protected(uint8_t card_idx, const uint8_t* protected_cards,
                         uint8_t protected_count)
{ for(uint8_t i = 0; i < protected_count; i++)
    if(protected_cards[i] == card_idx) return true;
  return false;
} // is_protected

// Finds the highest-bonus 2- or 3-card champion combo in hand that clears
// lethal_combo_bonus -- the same threshold attack's is_held_combo() (Sec.7,
// ai_strat_borealis_enum.c) would hold back. Returns the subset size (0-3,
// 0 = nothing worth protecting) and writes its card indices to out (>=3
// slots). Considers the whole hand, not just affordable champions --
// discard/mulligan decisions aren't cash-budget-gated.
static uint8_t find_protected_combo(const Hand* hand, const BorealisParams* params,
                                    uint8_t* out)
{ if(!params->hold_lethal_combos) return 0;

  uint8_t champions[12];
  uint8_t n = collect_champions(hand->cards, hand->size, champions, false);

  int best_bonus = params->lethal_combo_bonus - 1;
  uint8_t best[3] = { 0 };
  uint8_t best_count = 0;

  for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t pair[2] = { champions[i], champions[j] };
      int bonus2 = combo_bonus_for_selection(pair, 2);
      if(bonus2 > best_bonus)
      { best_bonus = bonus2;
        best[0] = pair[0];
        best[1] = pair[1];
        best_count = 2;
      }

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t triple[3] = { champions[i], champions[j], champions[k] };
        int bonus3 = combo_bonus_for_selection(triple, 3);
        if(bonus3 > best_bonus)
        { best_bonus = bonus3;
          best[0] = triple[0];
          best[1] = triple[1];
          best[2] = triple[2];
          best_count = 3;
        }
      }
    }

  for(uint8_t i = 0; i < best_count; i++) out[i] = best[i];
  return best_count;
} // find_protected_combo

// Borealis's own valuation for "which card to give up", matching its
// attack scoring model (Sec.4) rather than the shared power-ratio
// heuristic -- so a discard doesn't throw away a card Borealis itself
// would value highly. Lower is worse (discard first).
static float borealis_card_value(uint8_t card_idx, float lambda)
{ return fullDeck[card_idx].expected_attack - lambda * (float)fullDeck[card_idx].cost;
} // borealis_card_value

// Lowest borealis_card_value() among hand cards, skipping protected ones
// unless every card in hand is protected -- Sec.7: "never let holding
// produce a stall."
static uint8_t pick_discard_victim(const Hand* hand, const uint8_t* protected_cards,
                                   uint8_t protected_count, float lambda)
{ uint8_t victim = UINT8_MAX;
  float victim_value = 1e9f;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(is_protected(card_idx, protected_cards, protected_count)) continue;

    float value = borealis_card_value(card_idx, lambda);
    if(value < victim_value)
    { victim_value = value;
      victim = card_idx;
    }
  }
  if(victim != UINT8_MAX) return victim;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    float value = borealis_card_value(card_idx, lambda);
    if(value < victim_value)
    { victim_value = value;
      victim = card_idx;
    }
  }

  return victim;
} // pick_discard_victim

void borealis_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const BorealisParams* params = &g_params[player];
  uint8_t protected_cards[3];
  uint8_t protected_count = find_protected_combo(&gstate->hand[player], params,
                                                 protected_cards);

  while(gstate->hand[player].size > 7)
  { uint8_t victim = pick_discard_victim(&gstate->hand[player], protected_cards,
                                         protected_count, params->luna_value);
    Hand_remove(&gstate->hand[player], victim);
    Discard_add(&gstate->discard[player], victim);
  }
} // borealis_discard_to_7

// The card-COUNT decision (how many below-average cards to give up, capped
// at 2) is unchanged from strat_lib_mulligan() (ai_strat_lib_heuristics.c);
// only which specific cards are chosen differs, via pick_discard_victim().
void borealis_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const BorealisParams* params = &g_params[player];
  uint8_t max_nbr_cards_to_mulligan = 2;
  uint8_t nbr_cards_to_mulligan = 0;

  for(uint8_t i = 0; (i < gstate->hand[player].size) &&
      (nbr_cards_to_mulligan < max_nbr_cards_to_mulligan); i++)
  { uint8_t card_idx = gstate->hand[player].cards[i];
    if(fullDeck[card_idx].power < AVERAGE_POWER_FOR_MULLIGAN)
      nbr_cards_to_mulligan++;
  }

  uint8_t protected_cards[3];
  uint8_t protected_count = find_protected_combo(&gstate->hand[player], params,
                                                 protected_cards);

  for(uint8_t i = 0; i < nbr_cards_to_mulligan; i++)
  { uint8_t victim = pick_discard_victim(&gstate->hand[player], protected_cards,
                                         protected_count, params->luna_value);
    Hand_remove(&gstate->hand[player], victim);
    Discard_add(&gstate->discard[player], victim);
  }

  for(uint8_t i = 0; i < nbr_cards_to_mulligan; i++)
    draw_1_card(gstate, player, ctx);
} // borealis_mulligan
