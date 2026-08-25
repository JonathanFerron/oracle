// ai_strat_hbt_cards.c
// A7 Hybrid HBT's mulligan/discard-to-7 overrides -- a LOCAL port of A3
// Borealis's borealis_mulligan()/borealis_discard_to_7() shape
// (ai_strat_borealis.c), not a call into A3's own functions. See
// ai_strat_hbt.h's header comment ("Mulligan / discard-to-7") for why: A3's
// versions read Borealis's own g_params[player], so calling them here would
// make this agent's discard behaviour depend on Borealis's calibration and
// would break under calib_hbt --opponent borealis (which sets Borealis's
// params per seat for the match); and A3 is the Bradley-Terry rating anchor
// for every agent measured so far, so refactoring it for reuse is pure
// downside risk to every rating already shipped.
//
// Protects the best held 2-3 card combo clearing lethal_combo_bonus from the
// shared lowest-power discard heuristic, which would otherwise actively
// sabotage hold_lethal_combos by throwing away exactly the cards a held
// combo needs -- same rationale as A3's Sec.7 note (ai_strat_borealis.h).
// Victim valuation uses the A4/A6 efficiency ratio
// expected_attack/(cost + HBT_COST_FLOOR) rather than A3's
// expected_attack - lambda*cost, so this introduces no new lambda dial.

#include "ai_strat_hbt.h"
#include "ai_strat_hbt_enum.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"
#include "../structures/card_collection.h"

// Denominator floor, same role and value as A4's BR_COST_FLOOR/A6's
// TAC_COST_FLOOR -- not a HBTParams field since nothing has calibrated
// whether this agent needs its own value.
#define HBT_COST_FLOOR 1.3f

static bool is_protected(uint8_t card_idx, const uint8_t* protected_cards,
                         uint8_t protected_count)
{ for(uint8_t i = 0; i < protected_count; i++)
    if(protected_cards[i] == card_idx) return true;
  return false;
} // is_protected

// Finds the highest-bonus 2- or 3-card champion combo in hand that clears
// lethal_combo_bonus -- the same threshold hbt_best_attack_move()'s combo
// hold (ai_strat_hbt_enum.c's is_held_combo()) would hold back. Returns the
// subset size (0-3, 0 = nothing worth protecting) and writes its card
// indices to out (>= 3 slots). Considers the whole hand, not just affordable
// champions -- discard/mulligan decisions aren't cash-budget-gated.
static uint8_t find_protected_combo(const Hand* hand, const HBTParams* params, uint8_t* out)
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

// Lowest expected_attack/(cost + HBT_COST_FLOOR) among hand cards, skipping
// protected ones unless every card in hand is protected -- "never let
// holding produce a stall", same fallback as A3's pick_discard_victim().
static uint8_t pick_discard_victim(const Hand* hand, const uint8_t* protected_cards,
                                   uint8_t protected_count)
{ uint8_t victim = UINT8_MAX;
  float victim_value = 1e9f;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(is_protected(card_idx, protected_cards, protected_count)) continue;

    float value = fullDeck[card_idx].expected_attack / (fullDeck[card_idx].cost + HBT_COST_FLOOR);
    if(value < victim_value)
    { victim_value = value;
      victim = card_idx;
    }
  }
  if(victim != UINT8_MAX) return victim;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    float value = fullDeck[card_idx].expected_attack / (fullDeck[card_idx].cost + HBT_COST_FLOOR);
    if(value < victim_value)
    { victim_value = value;
      victim = card_idx;
    }
  }

  return victim;
} // pick_discard_victim

void hbt_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ (void)ctx;
  const HBTParams* params = hbt_live_params(player);
  uint8_t protected_cards[3];
  uint8_t protected_count = find_protected_combo(&gstate->hand[player], params,
                                                 protected_cards);

  while(gstate->hand[player].size > 7)
  { uint8_t victim = pick_discard_victim(&gstate->hand[player], protected_cards,
                                         protected_count);
    Hand_remove(&gstate->hand[player], victim);
    Discard_add(&gstate->discard[player], victim);
  }
} // hbt_discard_to_7

// The card-COUNT decision (how many below-average cards to give up, capped
// at 2) is unchanged from strat_lib_mulligan() (ai_strat_lib_heuristics.c) /
// A3's borealis_mulligan(); only which specific cards are chosen differs,
// via pick_discard_victim().
void hbt_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const HBTParams* params = hbt_live_params(player);
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
                                         protected_count);
    Hand_remove(&gstate->hand[player], victim);
    Discard_add(&gstate->discard[player], victim);
  }

  for(uint8_t i = 0; i < nbr_cards_to_mulligan; i++)
    draw_1_card(gstate, player, ctx);
} // hbt_mulligan
