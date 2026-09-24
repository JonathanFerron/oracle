// test_game_event.c
// Test suite for src/visibility/game_event.c's event_filter_for_viewer()
// and the cards_added()/cards_removed() diff helpers -- GUI step 4
// (ideas/9 gui/gui_architecture_synthesis.md section 6). No GameContext/RNG
// dependency, same "game_types.h + libc only" style as test_visibility.c;
// diff helpers are tested on plain local arrays, not real Hand/Discard
// objects, since they only ever need a pointer+count.

#include <stdio.h>

#include "../src/visibility/game_event.h"
#include "../src/visibility/visible_state.h" // VIEWER_SPECTATOR

#define TEST_PASS "\033[32m\xe2\x9c\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xe2\x9c\x97 FAIL\033[0m"

typedef struct
{ int passed;
  int failed;
} TestSuite;

static void check(TestSuite* suite, const char* name, bool ok)
{ if(ok)
  { printf("  %s: %s\n", TEST_PASS, name);
    suite->passed++;
  }
  else
  { printf("  %s: %s\n", TEST_FAIL, name);
    suite->failed++;
  }
} // check

/* ========================================================================
   cards_added / cards_removed
   ======================================================================== */

static void test_cards_added_basic(TestSuite* suite)
{ printf("\n=== cards_added: one new card ===\n");

  uint8_t before[] = {1, 2, 3};
  uint8_t after[] = {1, 2, 3, 4};
  uint8_t out[8];

  uint8_t n = cards_added(before, 3, after, 4, out, 8);
  check(suite, "exactly 1 added", n == 1);
  check(suite, "the new card is 4", out[0] == 4);
} // test_cards_added_basic

static void test_cards_added_multi_and_order(TestSuite* suite)
{ printf("\n=== cards_added: several new cards, out follows after's scan order ===\n");

  uint8_t before[] = {1, 2};
  uint8_t after[] = {3, 1, 4, 2};
  uint8_t out[8];

  uint8_t n = cards_added(before, 2, after, 4, out, 8);
  check(suite, "exactly 2 added", n == 2);
  check(suite, "first new card is 3 (after's scan order)", out[0] == 3);
  check(suite, "second new card is 4", out[1] == 4);
} // test_cards_added_multi_and_order

static void test_cards_removed_basic(TestSuite* suite)
{ printf("\n=== cards_removed: one card left ===\n");

  uint8_t before[] = {1, 2, 3};
  uint8_t after[] = {1, 3};
  uint8_t out[8];

  uint8_t n = cards_removed(before, 3, after, 2, out, 8);
  check(suite, "exactly 1 removed", n == 1);
  check(suite, "the removed card is 2", out[0] == 2);
} // test_cards_removed_basic

static void test_diff_no_change(TestSuite* suite)
{ printf("\n=== cards_added/removed: identical arrays -> 0 ===\n");

  uint8_t both[] = {5, 6, 7};
  uint8_t out[8];

  check(suite, "0 added", cards_added(both, 3, both, 3, out, 8) == 0);
  check(suite, "0 removed", cards_removed(both, 3, both, 3, out, 8) == 0);
} // test_diff_no_change

static void test_diff_max_out_truncation(TestSuite* suite)
{ printf("\n=== cards_added: max_out truncation ===\n");

  uint8_t before[] = {0};
  uint8_t after[] = {1, 2, 3, 4};
  uint8_t out[2];

  uint8_t n = cards_added(before, 1, after, 4, out, 2);
  check(suite, "capped to max_out", n == 2);
} // test_diff_max_out_truncation

/* ========================================================================
   event_filter_for_viewer
   ======================================================================== */

static void test_card_drawn_redacted_for_non_owner(TestSuite* suite)
{ printf("\n=== event_filter_for_viewer: EVT_CARD_DRAWN ===\n");

  GameEvent e = { .type = EVT_CARD_DRAWN, .player = PLAYER_A, .u.card = 42 };

  GameEvent for_owner = e;
  event_filter_for_viewer(&for_owner, PLAYER_A);
  check(suite, "owner sees the real card", for_owner.u.card == 42);

  GameEvent for_opponent = e;
  event_filter_for_viewer(&for_opponent, PLAYER_B);
  check(suite, "non-owner sees EVT_CARD_REDACTED", for_opponent.u.card == EVT_CARD_REDACTED);

  GameEvent for_spectator = e;
  event_filter_for_viewer(&for_spectator, VIEWER_SPECTATOR);
  check(suite, "spectator sees EVT_CARD_REDACTED", for_spectator.u.card == EVT_CARD_REDACTED);
} // test_card_drawn_redacted_for_non_owner

static void test_other_events_untouched(TestSuite* suite)
{ printf("\n=== event_filter_for_viewer: every other event type is already public ===\n");

  GameEvent move = { .type = EVT_MOVE_PLAYED, .player = PLAYER_A,
                     .u.move = { .type = MOVE_DRAW, .card = 102 }
                   };
  event_filter_for_viewer(&move, PLAYER_B);
  check(suite, "EVT_MOVE_PLAYED's move untouched for the non-mover",
        move.u.move.type == MOVE_DRAW && move.u.move.card == 102);

  GameEvent turn = { .type = EVT_TURN_BEGAN, .player = PLAYER_A, .u.turn = 7 };
  event_filter_for_viewer(&turn, PLAYER_B);
  check(suite, "EVT_TURN_BEGAN's turn number untouched", turn.u.turn == 7);
} // test_other_events_untouched

int main(void)
{ TestSuite suite = {0, 0};

  printf("Running game_event tests...\n");

  test_cards_added_basic(&suite);
  test_cards_added_multi_and_order(&suite);
  test_cards_removed_basic(&suite);
  test_diff_no_change(&suite);
  test_diff_max_out_truncation(&suite);
  test_card_drawn_redacted_for_non_owner(&suite);
  test_other_events_untouched(&suite);

  printf("\n%d passed, %d failed\n", suite.passed, suite.failed);
  return suite.failed == 0 ? 0 : 1;
} // main
