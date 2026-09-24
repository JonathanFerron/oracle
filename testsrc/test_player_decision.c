// test_player_decision.c
// Test suite for src/actions/player_decision.c's decision_is_legal() -- GUI
// step 3 (ideas/9 gui/gui_architecture_synthesis.md section 4). Hand-crafted
// game states, same style/fullDeck[] constants as test_moves.c.
//
// fullDeck[] indices used below (see game_constants.c's "Full deck
// definition" comment): CHAMP_A/B/C = 0/1/2 (cost 0), CHAMP_D = 3 (cost 1),
// CHAMP_E..CHAMP_N = 4..13 (all champions, cost/power unchecked -- used only
// as discard filler for the >RECALL_POOL_CAP test), DRAW2 = 102 (cost 1,
// draw_num 2, choose_num 1), DRAW3 = 111 (cost 2, draw_num 3, choose_num 2),
// CASH = 117 (cost 0, exchange_cash 5).

#include <stdio.h>

#include "../src/actions/player_decision.h"
#include "../src/ai_strat/ai_strat_lib_heuristics.h"

#define TEST_PASS "\033[32m\xe2\x9c\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xe2\x9c\x97 FAIL\033[0m"

#define CHAMP_A 0
#define CHAMP_B 1
#define CHAMP_C 2
#define CHAMP_D 3
#define DRAW2   102
#define DRAW3   111
#define CASH    117

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

static struct gamestate blank_state(PlayerID player_to_move, uint16_t budget, TurnPhase phase)
{ struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Hand_init(&gs.hand[PLAYER_B]);
  Discard_init(&gs.discard[PLAYER_A]);
  Discard_init(&gs.discard[PLAYER_B]);
  gs.current_cash_balance[player_to_move] = budget;
  gs.turn_phase = phase;
  gs.player_to_move = player_to_move;
  gs.current_player = player_to_move;
  return gs;
} // blank_state

/* ========================================================================
   ATTACK / DEFENSE: MOVE_PASS, wrong kind/player/phase gating
   ======================================================================== */

static void test_pass_and_gating(TestSuite* suite)
{ printf("\n=== ATTACK/DEFENSE gating: kind, player, phase ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  PlayerDecision pass = { .kind = DECISION_KIND_ATTACK, .move = { .type = MOVE_PASS } };

  check(suite, "MOVE_PASS legal for the player to move",
        decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &pass));
  check(suite, "wrong expected kind rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_DEFENSE, &pass));
  check(suite, "wrong player rejected (not player_to_move)",
        !decision_is_legal(&gs, PLAYER_B, DECISION_KIND_ATTACK, &pass));

  gs.turn_phase = DEFENSE;
  check(suite, "ATTACK decision rejected once phase is DEFENSE",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &pass));
} // test_pass_and_gating

/* ========================================================================
   MOVE_CHAMPIONS
   ======================================================================== */

static void test_champions_move(TestSuite* suite)
{ printf("\n=== MOVE_CHAMPIONS ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_D); // cost 1, unaffordable at budget 0

  PlayerDecision d = { .kind = DECISION_KIND_ATTACK,
                       .move = { .type = MOVE_CHAMPIONS, .count = 2, .cards = {CHAMP_B, CHAMP_A} }
                     };
  check(suite, "legal pair, submitted in reverse order (order-insensitive)",
        decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  d.move.cards[0] = CHAMP_A;
  d.move.cards[1] = CHAMP_D;
  check(suite, "unaffordable pair rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  d.move.count = 1;
  d.move.cards[0] = CHAMP_C; // not in hand at all
  check(suite, "card not in hand rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  d.move.count = 2;
  d.move.cards[0] = CHAMP_A;
  d.move.cards[1] = CHAMP_A; // duplicate
  check(suite, "duplicate card in decision rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));
} // test_champions_move

static void test_defense_champions_only(TestSuite* suite)
{ printf("\n=== DEFENSE: MOVE_CHAMPIONS ok, MOVE_DRAW rejected outright ===\n");

  struct gamestate gs = blank_state(PLAYER_B, 0, DEFENSE);
  Hand_add(&gs.hand[PLAYER_B], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_B], DRAW2);

  PlayerDecision champ = { .kind = DECISION_KIND_DEFENSE,
                           .move = { .type = MOVE_CHAMPIONS, .count = 1, .cards = {CHAMP_A} }
                         };
  check(suite, "MOVE_CHAMPIONS legal on defense",
        decision_is_legal(&gs, PLAYER_B, DECISION_KIND_DEFENSE, &champ));

  PlayerDecision draw = { .kind = DECISION_KIND_DEFENSE,
                          .move = { .type = MOVE_DRAW, .card = DRAW2 }
                        };
  check(suite, "MOVE_DRAW rejected outright on defense (not just illegal-in-hand)",
        !decision_is_legal(&gs, PLAYER_B, DECISION_KIND_DEFENSE, &draw));
} // test_defense_champions_only

/* ========================================================================
   MOVE_DRAW
   ======================================================================== */

static void test_draw_move(TestSuite* suite)
{ printf("\n=== MOVE_DRAW ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], DRAW2); // cost 1, unaffordable at budget 0

  PlayerDecision d = { .kind = DECISION_KIND_ATTACK,
                       .move = { .type = MOVE_DRAW, .card = DRAW2 }
                     };
  check(suite, "unaffordable draw card rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  gs.current_cash_balance[PLAYER_A] = 1;
  check(suite, "affordable draw card accepted",
        decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));
} // test_draw_move

/* ========================================================================
   MOVE_RECALL -- including the >RECALL_POOL_CAP (6) case move_gen.c's
   own template enumeration can't sample, which decision_is_legal() must
   still accept via its structural (not membership-only) check.
   ======================================================================== */

static void test_recall_move(TestSuite* suite)
{ printf("\n=== MOVE_RECALL ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], DRAW2); // choose_num 1
  Discard_add(&gs.discard[PLAYER_A], CHAMP_A);
  Discard_add(&gs.discard[PLAYER_A], CHAMP_B);

  PlayerDecision d = { .kind = DECISION_KIND_ATTACK,
                       .move = { .type = MOVE_RECALL, .card = DRAW2, .count = 1, .recall = {CHAMP_B} }
                     };
  check(suite, "legal single recall from discard",
        decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  d.move.count = 2;
  d.move.recall[1] = CHAMP_A;
  check(suite, "wrong count (choose_num is 1) rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  d.move.count = 1;
  d.move.recall[0] = CHAMP_C; // never discarded
  check(suite, "recalling a champion not actually in discard rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  // 10-champion discard, choose_num 2 -- exceeds move_gen.c's RECALL_POOL_CAP
  // (6), so get_available_moves()'s own template enumeration only ever
  // samples pairs within its capped, power-sorted pool of 6. A human may
  // legitimately request any 2 of the real 10; decision_is_legal() must
  // accept a pair outside that capped pool.
  struct gamestate gs10 = blank_state(PLAYER_A, 2, ATTACK);
  Hand_add(&gs10.hand[PLAYER_A], DRAW3); // choose_num 2
  uint8_t champs10[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  for(int i = 0; i < 10; i++) Discard_add(&gs10.discard[PLAYER_A], champs10[i]);

  PlayerDecision d10 = { .kind = DECISION_KIND_ATTACK,
                         .move = { .type = MOVE_RECALL, .card = DRAW3, .count = 2, .recall = {8, 9} }
                       };
  check(suite, "recall pair outside move_gen's capped-6 template pool still legal",
        decision_is_legal(&gs10, PLAYER_A, DECISION_KIND_ATTACK, &d10));

  d10.move.recall[0] = 9;
  d10.move.recall[1] = 9; // duplicate
  check(suite, "duplicate recall entries rejected",
        !decision_is_legal(&gs10, PLAYER_A, DECISION_KIND_ATTACK, &d10));
} // test_recall_move

/* ========================================================================
   MOVE_CASH
   ======================================================================== */

static void test_cash_move(TestSuite* suite)
{ printf("\n=== MOVE_CASH ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], CASH);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);

  PlayerDecision d = { .kind = DECISION_KIND_ATTACK,
                       .move = { .type = MOVE_CASH, .card = CASH, .count = 1, .cards = {CHAMP_B} }
                     };
  check(suite, "legal exchange target (not just the lowest-power one)",
        decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));

  d.move.cards[0] = CHAMP_D; // never in hand
  check(suite, "exchange target not in hand rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_ATTACK, &d));
} // test_cash_move

/* ========================================================================
   MULLIGAN
   ======================================================================== */

static void test_mulligan(TestSuite* suite)
{ printf("\n=== MULLIGAN ===\n");

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Hand_init(&gs.hand[PLAYER_B]);
  Discard_init(&gs.discard[PLAYER_A]);
  Discard_init(&gs.discard[PLAYER_B]);
  Hand_add(&gs.hand[PLAYER_B], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_B], CHAMP_B);
  Hand_add(&gs.hand[PLAYER_B], CHAMP_C);
  gs.turn = 0;

  PlayerDecision d = { .kind = DECISION_KIND_MULLIGAN, .count = 2,
                       .cards = {CHAMP_A, CHAMP_B}
                     };
  check(suite, "legal mulligan (2 of B's own cards, up to the default cap)",
        decision_is_legal(&gs, PLAYER_B, DECISION_KIND_MULLIGAN, &d));
  check(suite, "player A rejected (only B mulligans)",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_MULLIGAN, &d));

  gs.turn = 1;
  check(suite, "rejected once turn != 0 (past the mulligan window)",
        !decision_is_legal(&gs, PLAYER_B, DECISION_KIND_MULLIGAN, &d));
  gs.turn = 0;

  d.count = mulligan_get_max_cards() + 1;
  d.cards[2] = CHAMP_C;
  check(suite, "count above mulligan_get_max_cards() rejected",
        !decision_is_legal(&gs, PLAYER_B, DECISION_KIND_MULLIGAN, &d));

  d.count = 1;
  d.cards[0] = CHAMP_D; // not in B's hand
  check(suite, "card not in hand rejected",
        !decision_is_legal(&gs, PLAYER_B, DECISION_KIND_MULLIGAN, &d));

  d.count = 0;
  check(suite, "count 0 (keep hand) is legal",
        decision_is_legal(&gs, PLAYER_B, DECISION_KIND_MULLIGAN, &d));
} // test_mulligan

/* ========================================================================
   DISCARD_TO_7
   ======================================================================== */

static void test_discard_to_7(TestSuite* suite)
{ printf("\n=== DISCARD_TO_7 ===\n");

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Hand_init(&gs.hand[PLAYER_B]);
  Discard_init(&gs.discard[PLAYER_A]);
  Discard_init(&gs.discard[PLAYER_B]);
  for(uint8_t i = 0; i < 9; i++) Hand_add(&gs.hand[PLAYER_A], i); // 9 cards, discard 2
  gs.current_player = PLAYER_A;

  PlayerDecision d = { .kind = DECISION_KIND_DISCARD_TO_7, .count = 2, .cards = {0, 1} };
  check(suite, "legal exact discard (hand.size - 7)",
        decision_is_legal(&gs, PLAYER_A, DECISION_KIND_DISCARD_TO_7, &d));

  d.count = 1;
  check(suite, "wrong (too few) count rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_DISCARD_TO_7, &d));

  d.count = 2;
  check(suite, "wrong player rejected (not current_player)",
        !decision_is_legal(&gs, PLAYER_B, DECISION_KIND_DISCARD_TO_7, &d));

  Hand_remove(&gs.hand[PLAYER_A], 8);
  Hand_remove(&gs.hand[PLAYER_A], 7); // now 7 cards, no discard needed
  check(suite, "no discard needed (hand.size <= 7) rejected",
        !decision_is_legal(&gs, PLAYER_A, DECISION_KIND_DISCARD_TO_7, &d));
} // test_discard_to_7

int main(void)
{ TestSuite suite = {0, 0};

  printf("Running decision_is_legal() tests...\n");

  test_pass_and_gating(&suite);
  test_champions_move(&suite);
  test_defense_champions_only(&suite);
  test_draw_move(&suite);
  test_recall_move(&suite);
  test_cash_move(&suite);
  test_mulligan(&suite);
  test_discard_to_7(&suite);

  printf("\n%d passed, %d failed\n", suite.passed, suite.failed);
  return suite.failed == 0 ? 0 : 1;
} // main
