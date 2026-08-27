// test_hbt2ply_reply.c
// Unit tests for A9 HBT 2-Ply's surrogate-hand builder
// (src/ai_strat/ai_strat_hbt2ply_reply.c) -- the public-information-only
// stand-in for the opponent's hidden hand used to score the second ply.
// Minimal standalone harness in the style of test_recall.c/
// test_cash_exchange.c: links only game_constants.c (fullDeck) and
// card_collection.c (Hand/Discard/CombatZone) alongside the unit under
// test, no game engine or AI strategy roster needed.

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../src/core/game_constants.h"
#include "../src/structures/card_collection.h"
#include "../src/ai_strat/ai_strat_hbt2ply_reply.h"

#define TEST_PASS "\033[32m\xE2\x9C\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xE2\x9C\x97 FAIL\033[0m"

static int g_passed = 0;
static int g_failed = 0;

static void check(const char* name, bool condition)
{ if(condition)
  { printf("  %s: %s\n", TEST_PASS, name);
    g_passed++;
  }
  else
  { printf("  %s: %s\n", TEST_FAIL, name);
    g_failed++;
  }
} // check

static bool contains(const uint8_t* arr, uint8_t n, uint8_t card)
{ for(uint8_t i = 0; i < n; i++)
    if(arr[i] == card) return true;
  return false;
} // contains

// Picks `count` fullDeck indices of the given type not already in `used`,
// appends them to `used`/`*used_n`, and writes them to `out`.
static void pick_unused(CardType type, uint8_t count, uint8_t* out,
                        uint8_t* used, uint8_t* used_n)
{ uint8_t picked = 0;
  for(int i = 0; i < FULL_DECK_SIZE && picked < count; i++)
  { uint8_t card = (uint8_t)i;
    if(fullDeck[card].card_type != type) continue;
    if(contains(used, *used_n, card)) continue;
    out[picked++] = card;
    used[(*used_n)++] = card;
  }
} // pick_unused

// A realistic-ish position: pov (PLAYER_A) holds a mixed hand, both
// discards and combat zones hold a few cards each, opponent's hand is
// declared only by size (its contents are exactly what must stay hidden).
static void make_base_state(struct gamestate* g, uint8_t* seen, uint8_t* seen_n)
{ memset(g, 0, sizeof(*g));
  *seen_n = 0;

  uint8_t buf[8];
  Hand_init(&g->hand[PLAYER_A]);
  pick_unused(CHAMPION_CARD, 2, buf, seen, seen_n);
  Hand_add(&g->hand[PLAYER_A], buf[0]);
  Hand_add(&g->hand[PLAYER_A], buf[1]);
  pick_unused(DRAW_CARD, 1, buf, seen, seen_n);
  Hand_add(&g->hand[PLAYER_A], buf[0]);

  Discard_init(&g->discard[PLAYER_A]);
  pick_unused(CHAMPION_CARD, 3, buf, seen, seen_n);
  for(int i = 0; i < 3; i++) Discard_add(&g->discard[PLAYER_A], buf[i]);

  Discard_init(&g->discard[PLAYER_B]);
  pick_unused(CASH_CARD, 2, buf, seen, seen_n);
  for(int i = 0; i < 2; i++) Discard_add(&g->discard[PLAYER_B], buf[i]);

  CombatZone_init(&g->combat_zone[PLAYER_A]);
  pick_unused(CHAMPION_CARD, 1, buf, seen, seen_n);
  CombatZone_add(&g->combat_zone[PLAYER_A], buf[0]);

  CombatZone_init(&g->combat_zone[PLAYER_B]);
  pick_unused(CHAMPION_CARD, 1, buf, seen, seen_n);
  CombatZone_add(&g->combat_zone[PLAYER_B], buf[0]);

  g->hand[PLAYER_B].size = 5; // public info; contents deliberately unset
} // make_base_state

static void test_size_matches_public_hand_size(void)
{ struct gamestate g;
  uint8_t seen[16], seen_n;
  make_base_state(&g, seen, &seen_n);

  Hand out;
  build_surrogate_hand(&g, PLAYER_A, 1.0f, &out);
  check("surrogate size matches opponent's public hand size",
        out.size == g.hand[PLAYER_B].size);
} // test_size_matches_public_hand_size

static void test_never_reveals_visible_cards(void)
{ struct gamestate g;
  uint8_t seen[16], seen_n;
  make_base_state(&g, seen, &seen_n);

  Hand out;
  build_surrogate_hand(&g, PLAYER_A, 0.5f, &out);

  bool clean = true;
  for(uint8_t i = 0; i < out.size; i++)
    if(contains(seen, seen_n, out.cards[i])) clean = false;

  check("surrogate never contains a card visible to pov "
        "(own hand, either discard, either combat zone)", clean);
} // test_never_reveals_visible_cards

static void test_champion_fraction_tracks_pool(void)
{ struct gamestate g;
  uint8_t seen[16], seen_n;
  make_base_state(&g, seen, &seen_n);

  int pool_champions = 0, pool_total = 0;
  for(int i = 0; i < FULL_DECK_SIZE; i++)
  { if(contains(seen, seen_n, (uint8_t)i)) continue;
    pool_total++;
    if(fullDeck[i].card_type == CHAMPION_CARD) pool_champions++;
  }
  float pool_fraction = (float)pool_champions / (float)pool_total;

  Hand out;
  build_surrogate_hand(&g, PLAYER_A, 1.0f, &out);
  int surrogate_champions = 0;
  for(uint8_t i = 0; i < out.size; i++)
    if(fullDeck[out.cards[i]].card_type == CHAMPION_CARD) surrogate_champions++;
  float surrogate_fraction = (float)surrogate_champions / (float)out.size;

  check("surrogate champion fraction tracks the unseen pool's own fraction",
        fabsf(surrogate_fraction - pool_fraction) <= (1.0f / (float)out.size) + 0.01f);
} // test_champion_fraction_tracks_pool

static float avg_expected_defense(const Hand* hand)
{ float total = 0.0f;
  int n = 0;
  for(uint8_t i = 0; i < hand->size; i++)
  { if(fullDeck[hand->cards[i]].card_type != CHAMPION_CARD) continue;
    total += fullDeck[hand->cards[i]].expected_defense;
    n++;
  }
  return (n > 0) ? total / (float)n : 0.0f;
} // avg_expected_defense

static void test_pessimism_orders_monotonically(void)
{ struct gamestate g;
  uint8_t seen[16], seen_n;
  make_base_state(&g, seen, &seen_n);

  Hand weak, median, strong;
  build_surrogate_hand(&g, PLAYER_A, 0.0f, &weak);
  build_surrogate_hand(&g, PLAYER_A, 0.5f, &median);
  build_surrogate_hand(&g, PLAYER_A, 1.0f, &strong);

  float d_weak = avg_expected_defense(&weak);
  float d_median = avg_expected_defense(&median);
  float d_strong = avg_expected_defense(&strong);

  check("surrogate_pessimism orders monotonically (0.0 <= 0.5 <= 1.0 in "
        "assumed blocking strength)", d_weak <= d_median + 1e-4f &&
        d_median <= d_strong + 1e-4f);
} // test_pessimism_orders_monotonically

static void test_deterministic(void)
{ struct gamestate g;
  uint8_t seen[16], seen_n;
  make_base_state(&g, seen, &seen_n);

  Hand a, b;
  build_surrogate_hand(&g, PLAYER_A, 0.7f, &a);
  build_surrogate_hand(&g, PLAYER_A, 0.7f, &b);

  check("same gstate and pessimism always produce the same surrogate "
        "(no RNG)", a.size == b.size &&
        memcmp(a.cards, b.cards, a.size) == 0);
} // test_deterministic

static void test_never_exceeds_hand_capacity(void)
{ struct gamestate g;
  uint8_t seen[100], seen_n = 0;
  memset(&g, 0, sizeof(g));

  uint8_t buf[40];
  Hand_init(&g.hand[PLAYER_A]);
  pick_unused(CHAMPION_CARD, 6, buf, seen, &seen_n);
  for(int i = 0; i < 6; i++) Hand_add(&g.hand[PLAYER_A], buf[i]);

  Discard_init(&g.discard[PLAYER_A]);
  pick_unused(CHAMPION_CARD, 40, buf, seen, &seen_n);
  for(int i = 0; i < 40; i++) Discard_add(&g.discard[PLAYER_A], buf[i]);

  Discard_init(&g.discard[PLAYER_B]);
  // Champions, not draw cards -- fullDeck holds only 15 draw cards total
  // (vs 102 champions), too few to fill a 40-card discard pile.
  pick_unused(CHAMPION_CARD, 40, buf, seen, &seen_n);
  for(int i = 0; i < 40; i++) Discard_add(&g.discard[PLAYER_B], buf[i]);

  CombatZone_init(&g.combat_zone[PLAYER_A]);
  pick_unused(CHAMPION_CARD, 3, buf, seen, &seen_n);
  for(int i = 0; i < 3; i++) CombatZone_add(&g.combat_zone[PLAYER_A], buf[i]);

  CombatZone_init(&g.combat_zone[PLAYER_B]);
  pick_unused(CHAMPION_CARD, 3, buf, seen, &seen_n);
  for(int i = 0; i < 3; i++) CombatZone_add(&g.combat_zone[PLAYER_B], buf[i]);

  (void)seen_n; // 92 champions consumed above (of 102 total), leaving a
  // real pool of 28 -- still amply above Hand's own 12-card capacity, so
  // this scenario exercises that structural cap, not a pool-exhaustion path
  // (unreachable in real play: max discard(40)+discard(40)+zone(3)+zone(3)
  // +hand(12) = 98 < 120, so the unseen pool can never drop below 22).
  g.hand[PLAYER_B].size = 35; // deliberately unrealistic -- real hands never
  // exceed Hand's own 12-card capacity; this checks build_surrogate_hand
  // degrades safely (via Hand_add's own bound) rather than reading past it

  Hand out;
  build_surrogate_hand(&g, PLAYER_A, 1.0f, &out);
  check("surrogate size never exceeds Hand's structural 12-card capacity, "
        "even given an out-of-range declared opponent hand size",
        out.size == 12);
} // test_never_exceeds_hand_capacity

int main(void)
{ printf("Running A9 HBT 2-Ply surrogate-hand tests...\n\n");

  test_size_matches_public_hand_size();
  test_never_reveals_visible_cards();
  test_champion_fraction_tracks_pool();
  test_pessimism_orders_monotonically();
  test_deterministic();
  test_never_exceeds_hand_capacity();

  printf("\n%d passed, %d failed\n", g_passed, g_failed);
  return (g_failed == 0) ? 0 : 1;
} // main
