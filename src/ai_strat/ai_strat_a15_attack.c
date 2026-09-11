// ai_strat_a15_attack.c
// See ai_strat_a15_attack.h.

#include "ai_strat_a15_attack.h"
#include "ai_strat_a15_cards.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// ---- R6: immediate species-3 combo ----

// First affordable complete 3-same-species combo found in hand (no RNG,
// deterministic first-enumerated match -- R6 is "play it right away", not
// "play the best one", so there is nothing to rank among candidates that
// all qualify equally as "a full species-3 combo").
static bool find_species3_combo(const struct gamestate* gstate, PlayerID player, uint8_t* out)
{ uint8_t champions[12];
  const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t n = collect_champions(hand->cards, hand->size, champions, false);

  for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = i + 1; j < n; j++)
    { if(fullDeck[champions[j]].species != fullDeck[champions[i]].species) continue;
      for(uint8_t k = j + 1; k < n; k++)
      { if(fullDeck[champions[k]].species != fullDeck[champions[i]].species) continue;
        uint16_t cost = fullDeck[champions[i]].cost + fullDeck[champions[j]].cost +
                        fullDeck[champions[k]].cost;
        if(cost > budget) continue;
        out[0] = champions[i];
        out[1] = champions[j];
        out[2] = champions[k];
        return true;
      }
    }
  return false;
} // find_species3_combo

// ---- R1: Player A's turn 1 ----

// Cost-1 draw-2 card if held (played now); else the cost-2 draw-3 card
// (held back on the cost-1's own turn per Jonathan's Q1 reasoning -- the
// recall-2 facet is worth more later than spending it now). Neither
// facet's recall option applies here: the discard pile is empty on turn 1.
static bool play_turn1(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t cost1_draw = UINT8_MAX, cost2_draw = UINT8_MAX;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t c = hand->cards[i];
    if(fullDeck[c].card_type != DRAW_CARD || fullDeck[c].cost > budget) continue;
    if(fullDeck[c].cost == 1) cost1_draw = c;
    else if(fullDeck[c].cost == 2) cost2_draw = c;
  }

  if(cost1_draw != UINT8_MAX)
  { play_draw_card(gstate, player, cost1_draw, ctx);
    return true;
  }
  if(cost2_draw != UINT8_MAX)
  { play_draw_card(gstate, player, cost2_draw, ctx);
    return true;
  }
  return false;
} // play_turn1

// ---- R9a: color/order combo exposed to the forced discard ----

// The 7 possible calc_random_bonus() (combo_bonus.c) values, partitioned by
// tier: species {10,13,14,16} (R6's exclusive territory -- a complete
// species-3 already fired above before this is ever reached, so only a
// species-2 pair (10) could still linger here, and stays excluded per the
// doc's literal "color/order combo" wording), order {7,9,11}, color {5,8}.
static bool is_order_or_color_tier(int bonus)
{ switch(bonus)
  { case 5:
    case 7:
    case 8:
    case 9:
    case 11:
      return true;
    default:
      return false;
  }
} // is_order_or_color_tier

// Highest-bonus affordable complete order/color-tier combo in hand, if any.
static bool find_order_or_color_combo(const struct gamestate* gstate, PlayerID player,
                                      uint8_t* out, uint8_t* out_count)
{ uint8_t champions[12];
  const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t n = collect_champions(hand->cards, hand->size, champions, false);

  int best_bonus = 0;
  uint8_t best[3] = {0};
  uint8_t best_count = 0;

  for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = i + 1; j < n; j++)
    { uint16_t cost2 = fullDeck[champions[i]].cost + fullDeck[champions[j]].cost;
      if(cost2 <= budget)
      { uint8_t pair[2] = { champions[i], champions[j] };
        int b = combo_bonus_for_selection(pair, 2);
        if(is_order_or_color_tier(b) && b > best_bonus)
        { best_bonus = b;
          best[0] = pair[0];
          best[1] = pair[1];
          best_count = 2;
        }
      }
      for(uint8_t k = j + 1; k < n; k++)
      { uint16_t cost3 = cost2 + fullDeck[champions[k]].cost;
        if(cost3 > budget) continue;
        uint8_t triple[3] = { champions[i], champions[j], champions[k] };
        int b = combo_bonus_for_selection(triple, 3);
        if(is_order_or_color_tier(b) && b > best_bonus)
        { best_bonus = b;
          best[0] = triple[0];
          best[1] = triple[1];
          best[2] = triple[2];
          best_count = 3;
        }
      }
    }

  if(best_count == 0) return false;
  for(uint8_t i = 0; i < best_count; i++) out[i] = best[i];
  *out_count = best_count;
  return true;
} // find_order_or_color_combo

// True if the real discard-to-7 selection (a15_pick_victim(), run on a
// scratch copy of gstate so nothing here mutates real state) would give up
// any of combo_cards before hand size settles at 7.
static bool would_discard_expose(const struct gamestate* gstate, PlayerID player,
                                 const uint8_t* combo_cards, uint8_t combo_count)
{ struct gamestate sim = *gstate;

  while(sim.hand[player].size > 7)
  { uint8_t victim = a15_pick_victim(&sim, player);
    if(victim == UINT8_MAX) break;
    for(uint8_t i = 0; i < combo_count; i++)
      if(combo_cards[i] == victim) return true;
    Hand_remove(&sim.hand[player], victim);
    Discard_add(&sim.discard[player], victim);
  }
  return false;
} // would_discard_expose

static bool try_play_exposed_combo(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ if(gstate->hand[player].size <= 7) return false;

  uint8_t combo[3];
  uint8_t count;
  if(!find_order_or_color_combo(gstate, player, combo, &count)) return false;
  if(!would_discard_expose(gstate, player, combo, count)) return false;

  for(uint8_t i = 0; i < count; i++)
    play_champion(gstate, player, combo[i], ctx);
  return true;
} // try_play_exposed_combo

// ---- R9c: draw/recall ----

// Best 2/3-card combo bonus a discard-pile champion would create if
// recalled into player's CURRENT hand (the candidate isn't in hand yet, so
// this can't reuse a15_combo_participation() as-is -- same MARGINAL-
// contribution idea though: bonus(subset with candidate) minus bonus(subset
// without it), not the raw subset bonus, or a candidate that's just
// groupable with an already-complete pair (contributing nothing of its own)
// would misread as a valuable recall target -- see
// ai_strat_a15_cards.c's a15_combo_participation() for the same fix and why
// it matters (caught by testsrc/test_a15_combo.c, 2026-09-10).
static int recall_combo_value(const struct gamestate* gstate, PlayerID player,
                              uint8_t candidate)
{ uint8_t champions[12];
  uint8_t n = collect_champions(gstate->hand[player].cards, gstate->hand[player].size,
                                champions, false);
  int best = 0;

  for(uint8_t i = 0; i < n; i++)
  { uint8_t pair[2] = { candidate, champions[i] };
    int bonus2 = combo_bonus_for_selection(pair, 2);
    if(bonus2 > best) best = bonus2;

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t triple[3] = { candidate, champions[i], champions[j] };
      uint8_t rest[2] = { champions[i], champions[j] };
      int marginal = combo_bonus_for_selection(triple, 3) - combo_bonus_for_selection(rest, 2);
      if(marginal > best) best = marginal;
    }
  }
  return best;
} // recall_combo_value

typedef struct
{ uint8_t card_idx;
  bool worthwhile; // zero-cost, or completes/extends a combo with current hand
  int combo_value;
  float expected_attack;
} A15RecallCandidate;

static bool recall_ranks_before(const A15RecallCandidate* a, const A15RecallCandidate* b)
{ if(a->worthwhile != b->worthwhile) return a->worthwhile;
  if(a->combo_value != b->combo_value) return a->combo_value > b->combo_value;
  return a->expected_attack > b->expected_attack;
} // recall_ranks_before

// Every champion in player's discard, ranked descending by recall
// desirability. `out` must hold Discard.cards' bound (40).
static uint8_t rank_recall_candidates(const struct gamestate* gstate, PlayerID player,
                                      A15RecallCandidate* out)
{ uint8_t champions[40];
  const Discard* discard = &gstate->discard[player];
  uint8_t n = collect_champions(discard->cards, discard->size, champions, false);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c = champions[i];
    int value = recall_combo_value(gstate, player, c);
    out[i].card_idx = c;
    out[i].combo_value = value;
    out[i].worthwhile = (fullDeck[c].cost == 0) || (value > 0);
    out[i].expected_attack = fullDeck[c].expected_attack;
  }

  for(uint8_t i = 1; i < n; i++)
  { A15RecallCandidate key = out[i];
    int j = i - 1;
    while(j >= 0 && recall_ranks_before(&key, &out[j]))
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }
  return n;
} // rank_recall_candidates

// For one held draw/recall card: if the discard pile can supply choose_num
// champions and at least one of the top-ranked choose_num is worthwhile,
// scores the recall option (sum of their combo_value); else -1 (recall not
// offered for this card).
static int score_recall_option(const struct gamestate* gstate, PlayerID player,
                               uint8_t card_idx, uint8_t* out_targets)
{ uint8_t choose_num = fullDeck[card_idx].choose_num;
  if(choose_num == 0) return -1;

  A15RecallCandidate ranked[40];
  uint8_t n = rank_recall_candidates(gstate, player, ranked);
  if(n < choose_num) return -1;

  bool any_worthwhile = false;
  int score = 0;
  for(uint8_t i = 0; i < choose_num; i++)
  { if(ranked[i].worthwhile) any_worthwhile = true;
    score += ranked[i].combo_value;
    out_targets[i] = ranked[i].card_idx;
  }
  return any_worthwhile ? score : -1;
} // score_recall_option

static bool try_play_draw_or_recall(struct gamestate* gstate, PlayerID player,
                                    GameContext* ctx)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];

  uint8_t best_recall_card = UINT8_MAX, best_recall_targets[3] = {0}, best_recall_count = 0;
  int best_recall_score = -1;
  uint8_t cheapest_draw_card = UINT8_MAX, cheapest_cost = UINT8_MAX;

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t c = hand->cards[i];
    if(fullDeck[c].card_type != DRAW_CARD || fullDeck[c].cost > budget) continue;
    if(fullDeck[c].cost < cheapest_cost)
    { cheapest_draw_card = c;
      cheapest_cost = fullDeck[c].cost;
    }

    uint8_t targets[3];
    int score = score_recall_option(gstate, player, c, targets);
    if(score > best_recall_score)
    { best_recall_score = score;
      best_recall_card = c;
      best_recall_count = fullDeck[c].choose_num;
      for(uint8_t k = 0; k < best_recall_count; k++) best_recall_targets[k] = targets[k];
    }
  }

  if(best_recall_card != UINT8_MAX)
  { play_recall_card(gstate, player, best_recall_card, best_recall_targets,
                     best_recall_count, ctx);
    return true;
  }
  if(cheapest_draw_card != UINT8_MAX)
  { play_draw_card(gstate, player, cheapest_draw_card, ctx);
    return true;
  }
  return false;
} // try_play_draw_or_recall

// ---- R9b: pressure attack ----

typedef struct
{ uint8_t card_idx;
  int participation;
  float ratio;
} A15AttackCandidate;

static bool ranks_worse_to_play(const A15AttackCandidate* a, const A15AttackCandidate* b)
{ if(a->participation != b->participation) return a->participation > b->participation;
  return a->ratio > b->ratio;
} // ranks_worse_to_play -- true if `a` should be sacrificed AFTER `b`

// Up to `to_play` champions, chosen lowest-combo-participation first (never
// breaking a held combo unless every remaining card is equally or more
// attached to one -- the ranking degrades gracefully rather than needing a
// hard "protected" gate), affordable cumulatively within budget.
static uint8_t pick_pressure_attackers(const struct gamestate* gstate, PlayerID player,
                                       uint8_t to_play, uint8_t* out)
{ uint8_t champions[12];
  const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t n = collect_champions(hand->cards, hand->size, champions, false);

  A15AttackCandidate ranked[12];
  for(uint8_t i = 0; i < n; i++)
  { ranked[i].card_idx = champions[i];
    ranked[i].participation = a15_combo_participation(gstate, player, champions[i]);
    ranked[i].ratio = a15_attack_luna_ratio(champions[i]);
  }
  for(uint8_t i = 1; i < n; i++)
  { A15AttackCandidate key = ranked[i];
    int j = i - 1;
    while(j >= 0 && ranks_worse_to_play(&ranked[j], &key))
    { ranked[j + 1] = ranked[j];
      j--;
    }
    ranked[j + 1] = key;
  }

  uint8_t count = 0;
  uint16_t spent = 0;
  for(uint8_t i = 0; i < n && count < to_play; i++)
  { uint8_t c = ranked[i].card_idx;
    if(fullDeck[c].cost + spent > budget) continue;
    out[count++] = c;
    spent += fullDeck[c].cost;
  }
  return count;
} // pick_pressure_attackers

// ---- Dispatch ----

void a15_play_normal_attack(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ uint8_t species_combo[3];
  if(find_species3_combo(gstate, player, species_combo)) // R6
  { for(uint8_t i = 0; i < 3; i++) play_champion(gstate, player, species_combo[i], ctx);
    return;
  }

  if(gstate->turn == 1 && player == PLAYER_A && play_turn1(gstate, player, ctx)) // R1
    return;

  if(try_play_exposed_combo(gstate, player, ctx)) return;    // R9a
  if(try_play_draw_or_recall(gstate, player, ctx)) return;   // R9c

  if(gstate->hand[player].size > 7)                          // R9b
  { uint8_t excess = gstate->hand[player].size - 7;
    uint8_t to_play = (excess < 3) ? excess : 3;
    uint8_t attackers[3];
    uint8_t count = pick_pressure_attackers(gstate, player, to_play, attackers);
    for(uint8_t i = 0; i < count; i++)
      play_champion(gstate, player, attackers[i], ctx);
    if(count > 0) return;
  }

  // R9d: PASS -- build combo potential for next turn.
} // a15_play_normal_attack
