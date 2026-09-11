// ai_strat_a15_defense.c
// See ai_strat_a15_defense.h.

#include "ai_strat_a15_defense.h"
#include "ai_strat_a15_prob.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// The single highest-scoring 2- or 3-card combo currently in hand (any
// tier) -- what R7's "never break a combo held for next turn's attack"
// protects, independent of cost (protection is about what's worth keeping,
// not what's currently playable).
static uint8_t find_best_held_combo(const struct gamestate* gstate, PlayerID player,
                                    uint8_t* out)
{ uint8_t champions[12];
  const Hand* hand = &gstate->hand[player];
  uint8_t n = collect_champions(hand->cards, hand->size, champions, false);

  int best_bonus = 0;
  uint8_t best[3] = {0};
  uint8_t best_count = 0;

  for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t pair[2] = { champions[i], champions[j] };
      int b2 = combo_bonus_for_selection(pair, 2);
      if(b2 > best_bonus)
      { best_bonus = b2;
        best[0] = pair[0];
        best[1] = pair[1];
        best_count = 2;
      }
      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t triple[3] = { champions[i], champions[j], champions[k] };
        int b3 = combo_bonus_for_selection(triple, 3);
        if(b3 > best_bonus)
        { best_bonus = b3;
          best[0] = triple[0];
          best[1] = triple[1];
          best[2] = triple[2];
          best_count = 3;
        }
      }
    }

  for(uint8_t i = 0; i < best_count; i++) out[i] = best[i];
  return best_count;
} // find_best_held_combo

typedef struct
{ uint8_t cards[3];
  uint8_t count;
  float p_death;
  bool clears;
  bool uses_protected;
  uint8_t zero_count;    // cards with attack_base == 0
  int combo_bonus;
  int attack_base_sum;
} A15DefenseCandidate;

// R7's priority order, encoded directly: clearing beats non-clearing; among
// clearing candidates, avoiding the protected combo beats using it, then
// fewest cards, most +0 cards, highest combo bonus, lowest attack_base sum;
// among non-clearing candidates (only reached when nothing clears), lowest
// P(death) wins outright.
static bool defense_ranks_before(const A15DefenseCandidate* a, const A15DefenseCandidate* b)
{ if(a->clears != b->clears) return a->clears;
  if(!a->clears) return a->p_death < b->p_death;

  if(a->uses_protected != b->uses_protected) return !a->uses_protected;
  if(a->count != b->count) return a->count < b->count;
  if(a->zero_count != b->zero_count) return a->zero_count > b->zero_count;
  if(a->combo_bonus != b->combo_bonus) return a->combo_bonus > b->combo_bonus;
  return a->attack_base_sum < b->attack_base_sum;
} // defense_ranks_before

static void consider_defense_candidate(const struct gamestate* gstate, PlayerID defender,
                                       const A15DicePMF* attacker_pmf, float threshold,
                                       const uint8_t* protected_cards, uint8_t protected_count,
                                       const uint8_t* cards, uint8_t count,
                                       A15DefenseCandidate* best, bool* have_best)
{ uint16_t budget = gstate->current_cash_balance[defender];
  uint16_t cost = 0;
  for(uint8_t i = 0; i < count; i++) cost += fullDeck[cards[i]].cost;
  if(cost > budget) return;

  A15DefenseCandidate cand = {0};
  cand.count = count;
  for(uint8_t i = 0; i < count; i++) cand.cards[i] = cards[i];
  cand.p_death = a15_p_death_with_defense(gstate, defender, attacker_pmf, cards, count);
  cand.clears = cand.p_death < threshold;

  for(uint8_t i = 0; i < count; i++)
  { if(fullDeck[cards[i]].attack_base == 0) cand.zero_count++;
    cand.attack_base_sum += fullDeck[cards[i]].attack_base;
    for(uint8_t p = 0; p < protected_count; p++)
      if(cards[i] == protected_cards[p]) cand.uses_protected = true;
  }
  cand.combo_bonus = combo_bonus_for_selection(cards, count);

  if(!*have_best || defense_ranks_before(&cand, best))
  { *best = cand;
    *have_best = true;
  }
} // consider_defense_candidate

uint8_t a15_choose_defense_subset(const struct gamestate* gstate, PlayerID defender,
                                  float threshold, uint8_t* out)
{ PlayerID attacker = 1 - defender;
  A15DicePMF attacker_pmf;
  a15_build_dice_pmf(gstate->combat_zone[attacker].cards, gstate->combat_zone[attacker].size,
                     &attacker_pmf);

  uint8_t protected_cards[3];
  uint8_t protected_count = find_best_held_combo(gstate, defender, protected_cards);

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, defender,
                                         gstate->current_cash_balance[defender], affordable);

  A15DefenseCandidate best;
  bool have_best = false;

  for(uint8_t i = 0; i < n; i++)
  { consider_defense_candidate(gstate, defender, &attacker_pmf, threshold, protected_cards,
                               protected_count, &affordable[i], 1, &best, &have_best);
    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      consider_defense_candidate(gstate, defender, &attacker_pmf, threshold, protected_cards,
                                 protected_count, c2, 2, &best, &have_best);
      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        consider_defense_candidate(gstate, defender, &attacker_pmf, threshold, protected_cards,
                                   protected_count, c3, 3, &best, &have_best);
      }
    }
  }

  if(!have_best) return 0; // no affordable champion at all -- forced decline
  for(uint8_t i = 0; i < best.count; i++) out[i] = best.cards[i];
  return best.count;
} // a15_choose_defense_subset
