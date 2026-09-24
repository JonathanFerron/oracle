// test_visibility.c
// Test suite for src/visibility/visible_state.c's visibility_filter() --
// the GUI/future-network-client filter that must never leak the opponent's
// hand contents or deck order (ideas/9 gui/gui_architecture_synthesis.md
// section 5). Hand-built gamestate rather than setup_game()'s shuffled
// deal: this layer has no RNG/GameContext dependency at all, matching
// src/rating/'s "game_types.h + libc only" pattern (see makefile's
// TEST_RATING_SRCS comment).

#include <stdio.h>

#include "../src/visibility/visible_state.h"
#include "../src/structures/deckstack.h"

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

// A hand-built gamestate: 3 cards in each hand (values chosen so opponent-
// hand and own-hand contents are easy to tell apart), 2 discarded cards for
// A, an empty discard for B, 1 champion in A's combat zone, a 5-card deck
// for A (top = 4) and an empty deck for B (top = -1).
static void build_fixture(struct gamestate* gs)
{ gs->current_player = PLAYER_A;
  gs->current_energy[PLAYER_A] = 77;
  gs->current_energy[PLAYER_B] = 88;
  gs->current_cash_balance[PLAYER_A] = 3;
  gs->current_cash_balance[PLAYER_B] = 5;
  gs->someone_has_zero_energy = false;
  gs->turn = 12;
  gs->turn_phase = DEFENSE;
  gs->player_to_move = PLAYER_B;
  gs->combo_bonus_table = COMBO_BONUS_RANDOM;
  gs->game_state = ACTIVE;

  Hand_init(&gs->hand[PLAYER_A]);
  Hand_add(&gs->hand[PLAYER_A], 10);
  Hand_add(&gs->hand[PLAYER_A], 11);
  Hand_add(&gs->hand[PLAYER_A], 12);

  Hand_init(&gs->hand[PLAYER_B]);
  Hand_add(&gs->hand[PLAYER_B], 20);
  Hand_add(&gs->hand[PLAYER_B], 21);
  Hand_add(&gs->hand[PLAYER_B], 22);

  Discard_init(&gs->discard[PLAYER_A]);
  Discard_add(&gs->discard[PLAYER_A], 30);
  Discard_add(&gs->discard[PLAYER_A], 31);
  Discard_init(&gs->discard[PLAYER_B]);

  CombatZone_init(&gs->combat_zone[PLAYER_A]);
  CombatZone_add(&gs->combat_zone[PLAYER_A], 10);
  CombatZone_init(&gs->combat_zone[PLAYER_B]);

  gs->deck[PLAYER_A].top = -1;
  for(int i = 0; i < 5; i++)
    DeckStk_push(&gs->deck[PLAYER_A], (uint8_t)(40 + i));
  gs->deck[PLAYER_B].top = -1;
} // build_fixture

static void test_viewer_a_sees_own_hand_not_bs(TestSuite* suite)
{ struct gamestate gs;
  build_fixture(&gs);
  VisibleGameState v;
  visibility_filter(&gs, PLAYER_A, &v);

  check(suite, "viewer field set to PLAYER_A", v.viewer == PLAYER_A);
  check(suite, "my_hand is A's hand (size 3)", v.my_hand.size == 3);
  check(suite, "my_hand card 0 is A's card (10)", v.my_hand.cards[0] == 10);
  check(suite, "hand_count[B] is 3 (count only, no contents field)",
        v.hand_count[PLAYER_B] == 3);
} // test_viewer_a_sees_own_hand_not_bs

static void test_viewer_b_sees_own_hand_not_as(TestSuite* suite)
{ struct gamestate gs;
  build_fixture(&gs);
  VisibleGameState v;
  visibility_filter(&gs, PLAYER_B, &v);

  check(suite, "my_hand is B's hand (size 3)", v.my_hand.size == 3);
  check(suite, "my_hand card 0 is B's card (20)", v.my_hand.cards[0] == 20);
  check(suite, "hand_count[A] is 3 (count only)", v.hand_count[PLAYER_A] == 3);
} // test_viewer_b_sees_own_hand_not_as

static void test_spectator_sees_neither_hand(TestSuite* suite)
{ struct gamestate gs;
  build_fixture(&gs);
  VisibleGameState v;
  visibility_filter(&gs, VIEWER_SPECTATOR, &v);

  check(suite, "spectator viewer field is VIEWER_SPECTATOR",
        v.viewer == VIEWER_SPECTATOR);
  check(suite, "spectator my_hand is empty", v.my_hand.size == 0);
  check(suite, "spectator still sees hand counts",
        v.hand_count[PLAYER_A] == 3 && v.hand_count[PLAYER_B] == 3);
} // test_spectator_sees_neither_hand

static void test_public_fields_copied(TestSuite* suite)
{ struct gamestate gs;
  build_fixture(&gs);
  VisibleGameState v;
  visibility_filter(&gs, PLAYER_A, &v);

  check(suite, "energy copied for both players",
        v.energy[PLAYER_A] == 77 && v.energy[PLAYER_B] == 88);
  check(suite, "cash copied for both players",
        v.cash[PLAYER_A] == 3 && v.cash[PLAYER_B] == 5);
  check(suite, "discard[A] fully copied (2 cards, face-up)",
        v.discard[PLAYER_A].size == 2 &&
        v.discard[PLAYER_A].cards[0] == 30 &&
        v.discard[PLAYER_A].cards[1] == 31);
  check(suite, "combat_zone[A] fully copied", v.combat_zone[PLAYER_A].size == 1 &&
        v.combat_zone[PLAYER_A].cards[0] == 10);
  check(suite, "turn/turn_phase/player_to_move copied",
        v.turn == 12 && v.turn_phase == DEFENSE && v.player_to_move == PLAYER_B);
} // test_public_fields_copied

static void test_deck_count_from_top(TestSuite* suite)
{ struct gamestate gs;
  build_fixture(&gs);
  VisibleGameState v;
  visibility_filter(&gs, PLAYER_A, &v);

  check(suite, "deck_count[A] == top+1 == 5", v.deck_count[PLAYER_A] == 5);
  check(suite, "deck_count[B] == 0 for an empty deck (top == -1)",
        v.deck_count[PLAYER_B] == 0);
} // test_deck_count_from_top

static void test_game_over_flag(TestSuite* suite)
{ struct gamestate active, over;
  build_fixture(&active);
  build_fixture(&over);
  over.game_state = PLAYER_A_WINS;

  VisibleGameState v_active, v_over;
  visibility_filter(&active, PLAYER_A, &v_active);
  visibility_filter(&over, PLAYER_A, &v_over);

  check(suite, "game_over false while game_state == ACTIVE", !v_active.game_over);
  check(suite, "game_over true once game_state != ACTIVE", v_over.game_over);
  check(suite, "game_state itself copied through", v_over.game_state == PLAYER_A_WINS);
} // test_game_over_flag

int main(void)
{ TestSuite suite = {0, 0};

  printf("Running visibility_filter() tests...\n\n");
  test_viewer_a_sees_own_hand_not_bs(&suite);
  test_viewer_b_sees_own_hand_not_as(&suite);
  test_spectator_sees_neither_hand(&suite);
  test_public_fields_copied(&suite);
  test_deck_count_from_top(&suite);
  test_game_over_flag(&suite);

  printf("\n%d passed, %d failed\n", suite.passed, suite.failed);
  return suite.failed == 0 ? 0 : 1;
} // main
