// ai_strat_a15_cards.c
// See ai_strat_a15_cards.h.

#include "ai_strat_a15_cards.h"
#include "ai_strat_common.h"
#include "ai_strat_lib_heuristics.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// combo_bonus_for_selection() scores the BEST match within a group, not a
// per-card contribution: calc_random_bonus() (combo_bonus.c) falls through
// to the bare pair tier (e.g. 10 for species) when a 3-card group's third
// member matches nothing, so a completely unrelated card riding along with
// a real pair would otherwise read as "participating" in that pair's bonus
// it did nothing to earn. a15_combo_participation() must measure MARGINAL
// contribution instead: bonus(subset) - bonus(subset without `card`). For a
// 2-card subset this reduces to the pair's bonus outright (a single card
// always scores 0, calculate_combo_bonus()'s own range guard) -- only the
// 3-card case needs the subtraction. Caught by testsrc/test_a15_combo.c
// (2026-09-10): a Lycan with no species/order/color match to a Human pair
// was scoring participation 10 purely by being groupable with that pair.
int a15_combo_participation(const struct gamestate* gstate, PlayerID player, uint8_t card)
{ if(fullDeck[card].card_type != CHAMPION_CARD) return 0;

  const Hand* hand = &gstate->hand[player];
  uint8_t champions[12];
  uint8_t n = collect_champions(hand->cards, hand->size, champions, false);

  int best = 0;
  for(uint8_t i = 0; i < n; i++)
  { if(champions[i] == card) continue;
    uint8_t pair[2] = { card, champions[i] };
    int bonus2 = combo_bonus_for_selection(pair, 2);
    if(bonus2 > best) best = bonus2;

    for(uint8_t j = i + 1; j < n; j++)
    { if(champions[j] == card) continue;
      uint8_t triple[3] = { card, champions[i], champions[j] };
      uint8_t rest[2] = { champions[i], champions[j] };
      int marginal = combo_bonus_for_selection(triple, 3) - combo_bonus_for_selection(rest, 2);
      if(marginal > best) best = marginal;
    }
  }
  return best;
} // a15_combo_participation

// Discard-preference tiers, LOWEST value = discarded first. Mirrors R3
// (cash cards leave via discard, never played) and R1/R2/R5 (zero-cost
// champions and held draw/recall cards are protected).
typedef enum
{ A15_VICTIM_CASH = 0,
  A15_VICTIM_CHAMPION_PAID,
  A15_VICTIM_CHAMPION_FREE,
  A15_VICTIM_DRAW
} A15VictimTier;

static A15VictimTier victim_tier(uint8_t card_idx)
{ const struct card* c = &fullDeck[card_idx];
  if(c->card_type == CASH_CARD) return A15_VICTIM_CASH;
  if(c->card_type == CHAMPION_CARD)
    return (c->cost == 0) ? A15_VICTIM_CHAMPION_FREE : A15_VICTIM_CHAMPION_PAID;
  return A15_VICTIM_DRAW;
} // victim_tier

float a15_attack_luna_ratio(uint8_t card_idx)
{ return fullDeck[card_idx].expected_attack / (fullDeck[card_idx].cost + A15_COST_FLOOR);
} // a15_attack_luna_ratio

typedef struct
{ uint8_t card_idx;
  A15VictimTier tier;
  int participation;
  float ratio;
} A15RankedVictim;

static bool ranks_worse(const A15RankedVictim* a, const A15RankedVictim* b)
{ if(a->tier != b->tier) return a->tier > b->tier;
  if(a->participation != b->participation) return a->participation > b->participation;
  return a->ratio > b->ratio;
} // ranks_worse -- true if `a` should be discarded AFTER `b`

// Full discard-priority ranking of player's current hand (ascending: index 0
// is the next card to give up). Shared by a15_pick_victim() (real,
// progressive selection -- call again after each removal) and R2's
// mulligan-would-break-combo pre-check (a one-shot approximation over the
// ORIGINAL hand, not progressively re-ranked -- acceptable for a binary
// go/no-go gate). Returns the hand size (== cards written to out, capped at
// hand's own 12-slot bound).
static uint8_t rank_victims(const struct gamestate* gstate, PlayerID player,
                            A15RankedVictim* out)
{ const Hand* hand = &gstate->hand[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t c = hand->cards[i];
    out[i].card_idx = c;
    out[i].tier = victim_tier(c);
    out[i].participation = a15_combo_participation(gstate, player, c);
    out[i].ratio = (fullDeck[c].card_type == CHAMPION_CARD) ? a15_attack_luna_ratio(c) : 0.0f;
  }

  for(uint8_t i = 1; i < hand->size; i++)
  { A15RankedVictim key = out[i];
    int j = i - 1;
    while(j >= 0 && ranks_worse(&out[j], &key))
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }

  return hand->size;
} // rank_victims

uint8_t a15_pick_victim(const struct gamestate* gstate, PlayerID player)
{ if(gstate->hand[player].size == 0) return UINT8_MAX;

  A15RankedVictim ranked[12];
  rank_victims(gstate, player, ranked);
  return ranked[0].card_idx;
} // a15_pick_victim

// R2's first skip gate ("hand is very strong with combos on hand"): true if
// an affordable 2-card (species/order/color) match already exists in hand.
// Any complete 3-card combo's constituent pairs already clear this (species
// matches on all 3 pairs, order/color likewise), so checking pairs alone is
// sufficient -- no separate triple scan needed.
static bool has_achievable_combo(const struct gamestate* gstate, PlayerID player)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t champions[12];
  uint8_t n = collect_champions(hand->cards, hand->size, champions, false);

  for(uint8_t i = 0; i < n; i++)
  { if(fullDeck[champions[i]].cost > budget) continue;
    for(uint8_t j = i + 1; j < n; j++)
    { uint16_t pair_cost = fullDeck[champions[i]].cost + fullDeck[champions[j]].cost;
      if(pair_cost > budget) continue;
      uint8_t pair[2] = { champions[i], champions[j] };
      if(combo_bonus_for_selection(pair, 2) > 0) return true;
    }
  }
  return false;
} // has_achievable_combo

// R2's second skip gate ("don't break any 2 or 3 card in hand combo of any
// kind via a mulligan"): true if any of the `count` cards this mulligan
// would give up (per rank_victims()'s one-shot ranking) has nonzero combo
// participation.
static bool mulligan_would_break_combo(const struct gamestate* gstate, PlayerID player,
                                       uint8_t count)
{ A15RankedVictim ranked[12];
  uint8_t n = rank_victims(gstate, player, ranked);
  uint8_t limit = (count < n) ? count : n;

  for(uint8_t i = 0; i < limit; i++)
    if(ranked[i].participation > 0) return true;
  return false;
} // mulligan_would_break_combo

// The card-COUNT decision (how many below-average-power cards to give up,
// capped at mulligan_get_max_cards()) matches strat_lib_mulligan()/A3's
// borealis_mulligan() precedent -- only WHICH cards (via a15_pick_victim(),
// combo-aware) and WHETHER to mulligan at all (the two skip gates above)
// are new here.
void a15_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ if(has_achievable_combo(gstate, player)) return; // R2 skip gate 1

  uint8_t max_cards = mulligan_get_max_cards();
  uint8_t count = 0;
  for(uint8_t i = 0; i < gstate->hand[player].size && count < max_cards; i++)
    if(fullDeck[gstate->hand[player].cards[i]].power < AVERAGE_POWER_FOR_MULLIGAN)
      count++;
  if(count == 0) return;

  if(mulligan_would_break_combo(gstate, player, count)) return; // R2 skip gate 2

  for(uint8_t i = 0; i < count; i++)
  { uint8_t victim = a15_pick_victim(gstate, player);
    if(victim == UINT8_MAX) break;
    Hand_remove(&gstate->hand[player], victim);
    Discard_add(&gstate->discard[player], victim);
  }
  for(uint8_t i = 0; i < count; i++)
    draw_1_card(gstate, player, ctx);
} // a15_mulligan

// R1/R3/R5: cash cards and zero-participation champions leave first (see
// victim_tier()); zero-cost champions and held draw/recall cards are
// protected until nothing else is left.
void a15_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ (void)ctx;
  while(gstate->hand[player].size > 7)
  { uint8_t victim = a15_pick_victim(gstate, player);
    if(victim == UINT8_MAX) break; // defensive -- hand shouldn't be empty here
    Hand_remove(&gstate->hand[player], victim);
    Discard_add(&gstate->discard[player], victim);
  }
} // a15_discard_to_7
