// move_gen.c
// Legal-move enumeration for search-based AI agents (A8 Simple Monte Carlo
// and later). Attack phase enumerates the full move space this game
// supports in one turn (pass, 1-3 champion subsets, draw, recall, cash);
// defense phase enumerates pass/champion-subsets only -- matching what
// defense_phase() ever calls a defense strategy to decide (turn_logic.c).
//
// Candidate-count budget: hand is capped at 12 cards (Hand struct), so the
// champion-subset space alone is C(12,1)+C(12,2)+C(12,3) = 298 in the
// worst case (compare ai_strat_borealis_enum.c's identical count for its
// own 1-3 subset enumeration). In practice a legal hand mixes card types
// and affordability prunes hard, so doc/ai_agents.md's A8 section
// (the soothsayer)/about.md's "max ~93 moves" figure is typical, not
// worst-case. Recall's C(discard, choose_num) is the real worst-case
// blowup (discard can hold up to 40 cards) -- MoveGenLimits.max_recall_variants
// exists specifically to bound it.

#include "move_gen.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"
#include "../ai_strat/ai_strat_common.h"

// Local pool size before pairing champions for a choose_num == 2 recall card
// -- keeps that enumeration bounded regardless of discard size.
#define RECALL_POOL_CAP 6

static uint8_t append_move(GameMove* out, uint8_t out_n, uint8_t max_out, GameMove m)
{ if(out_n >= max_out) return out_n;
  out[out_n] = m;
  return out_n + 1;
} // append_move

static GameMove make_champions_move(const uint8_t* cards, uint8_t n)
{ GameMove m = { .type = MOVE_CHAMPIONS, .count = n };
  for(uint8_t i = 0; i < n; i++) m.cards[i] = cards[i];
  return m;
} // make_champions_move

// Every 1-3 subset of affordable[0..count) whose *cumulative* cost still
// fits budget -- per-card affordability (build_affordable_champions()) does
// not imply subset affordability, so this rechecks cost per candidate, the
// same cumulative recheck ai_strat_borealis_enum.c's emit_candidate() does.
static uint8_t gen_champion_moves(const uint8_t* affordable, uint8_t count, uint16_t budget,
                                  GameMove* out, uint8_t n, uint8_t max_out)
{ for(uint8_t i = 0; i < count; i++)
  { uint16_t cost1 = fullDeck[affordable[i]].cost;
    if(cost1 <= budget)
      n = append_move(out, n, max_out, make_champions_move(&affordable[i], 1));

    for(uint8_t j = i + 1; j < count; j++)
    { uint16_t cost2 = cost1 + fullDeck[affordable[j]].cost;
      uint8_t pair[2] = { affordable[i], affordable[j] };
      if(cost2 <= budget)
        n = append_move(out, n, max_out, make_champions_move(pair, 2));

      for(uint8_t k = j + 1; k < count; k++)
      { uint16_t cost3 = cost2 + fullDeck[affordable[k]].cost;
        uint8_t triple[3] = { affordable[i], affordable[j], affordable[k] };
        if(cost3 <= budget)
          n = append_move(out, n, max_out, make_champions_move(triple, 3));
      }
    }
  }
  return n;
} // gen_champion_moves

static uint8_t gen_draw_moves(const struct gamestate* gstate, PlayerID player,
                              GameMove* out, uint8_t n, uint8_t max_out)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type != DRAW_CARD) continue;
    if(fullDeck[card_idx].cost > budget) continue;

    GameMove m = { .type = MOVE_DRAW, .card = card_idx };
    n = append_move(out, n, max_out, m);
  }
  return n;
} // gen_draw_moves

// Champions recallable from discard, capped to RECALL_POOL_CAP and sorted
// descending by power (collect_champions()'s sort_desc=true) so the
// highest-power champions are always the ones a capped variant count keeps.
static uint8_t recall_pool(const struct gamestate* gstate, PlayerID player, uint8_t* pool)
{ uint8_t all[40];
  uint8_t all_n = collect_champions(gstate->discard[player].cards,
                                    gstate->discard[player].size, all, true);
  uint8_t pool_n = (uint8_t)oraclemin(all_n, RECALL_POOL_CAP);

  for(uint8_t i = 0; i < pool_n; i++) pool[i] = all[i];
  return pool_n;
} // recall_pool

static uint8_t gen_recall_variants_1(const uint8_t* pool, uint8_t pool_n, uint8_t card_idx,
                                     uint8_t max_variants, GameMove* out, uint8_t n, uint8_t max_out)
{ uint8_t emitted = 0;

  for(uint8_t i = 0; i < pool_n && emitted < max_variants; i++, emitted++)
  { GameMove m = { .type = MOVE_RECALL, .card = card_idx, .count = 1 };
    m.recall[0] = pool[i];
    n = append_move(out, n, max_out, m);
  }
  return n;
} // gen_recall_variants_1

static uint8_t gen_recall_variants_2(const uint8_t* pool, uint8_t pool_n, uint8_t card_idx,
                                     uint8_t max_variants, GameMove* out, uint8_t n, uint8_t max_out)
{ uint8_t emitted = 0;

  for(uint8_t i = 0; i < pool_n && emitted < max_variants; i++)
    for(uint8_t j = i + 1; j < pool_n && emitted < max_variants; j++, emitted++)
    { GameMove m = { .type = MOVE_RECALL, .card = card_idx, .count = 2 };
      m.recall[0] = pool[i];
      m.recall[1] = pool[j];
      n = append_move(out, n, max_out, m);
    }
  return n;
} // gen_recall_variants_2

static uint8_t gen_recall_moves(const struct gamestate* gstate, PlayerID player,
                                uint8_t max_variants, GameMove* out, uint8_t n, uint8_t max_out)
{ if(max_variants == 0) return n;

  const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];

  uint8_t pool[RECALL_POOL_CAP];
  uint8_t pool_n = recall_pool(gstate, player, pool);

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type != DRAW_CARD) continue;
    if(fullDeck[card_idx].cost > budget) continue;

    uint8_t choose_num = fullDeck[card_idx].choose_num;
    if(choose_num < 1 || choose_num > 2 || pool_n < choose_num) continue;

    n = (choose_num == 1)
        ? gen_recall_variants_1(pool, pool_n, card_idx, max_variants, out, n, max_out)
        : gen_recall_variants_2(pool, pool_n, card_idx, max_variants, out, n, max_out);
  }
  return n;
} // gen_recall_moves

// One MOVE_CASH per (affordable cash card x exchange-target variant), the
// lowest-power hand champions first -- matches
// select_champion_for_cash_exchange()'s own minimum-power default when
// max_variants caps down to 1. No champion in hand at all means no cash
// move is legal, matching ai_strat_random.c's own cash-card guard.
static uint8_t gen_cash_moves(const struct gamestate* gstate, PlayerID player,
                              uint8_t max_variants, GameMove* out, uint8_t n, uint8_t max_out)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];

  uint8_t champs[12];
  uint8_t champ_n = collect_champions(hand->cards, hand->size, champs, true); // desc by power
  if(champ_n == 0) return n;

  uint8_t variants = (uint8_t)oraclemin(max_variants == 0 ? 1 : max_variants, champ_n);

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type != CASH_CARD) continue;
    if(fullDeck[card_idx].cost > budget) continue;

    for(uint8_t v = 0; v < variants; v++)
    { GameMove m = { .type = MOVE_CASH, .card = card_idx, .count = 1 };
      m.cards[0] = champs[champ_n - 1 - v]; // lowest power first (tail of desc sort)
      n = append_move(out, n, max_out, m);
    }
  }
  return n;
} // gen_cash_moves

uint8_t get_available_moves(const struct gamestate* gstate, PlayerID player,
                            const MoveGenLimits* limits, GameMove* out, uint8_t max_out)
{ uint8_t n = 0;
  GameMove pass = { .type = MOVE_PASS };
  n = append_move(out, n, max_out, pass);

  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t affordable[12];
  uint8_t affordable_count = build_affordable_champions(gstate, player, budget, affordable);
  n = gen_champion_moves(affordable, affordable_count, budget, out, n, max_out);

  if(gstate->turn_phase == DEFENSE) return n; // defender only ever places 0-3 champions

  n = gen_draw_moves(gstate, player, out, n, max_out);
  n = gen_recall_moves(gstate, player, limits->max_recall_variants, out, n, max_out);
  n = gen_cash_moves(gstate, player, limits->max_cash_variants, out, n, max_out);

  return n;
} // get_available_moves
