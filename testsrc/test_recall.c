// test_recall.c
// Test suite for the recall mechanic (draw/recall cards) and the shared
// collect_champions() helper it relies on.
//
// Recall is exact and mandatory: choosing recall on a "recall N / draw M"
// card recalls exactly N champions from discard (never fewer, never zero) --
// see doc/game_rules_doc.md and CLAUDE.md.

#include "../src/core/game_constants.h"
#include "../src/core/card_actions.h"
#include "../src/core/game_context.h"
#include "../src/ui/cli/cli_input.h"
#include "../src/ui/cli/cli_constants.h"
#include <stdio.h>

#define TEST_PASS "\033[32m✓ PASS\033[0m"
#define TEST_FAIL "\033[31m✗ FAIL\033[0m"

typedef struct
{ const char* name;
  int passed;
  int failed;
} TestSuite;

void print_test_result(const char* test_name, int expected, int actual)
{ if(expected == actual)
  { printf("  %s: %s (expected %d, got %d)\n",
           TEST_PASS, test_name, expected, actual);
  }
  else
  { printf("  %s: %s (expected %d, got %d)\n",
           TEST_FAIL, test_name, expected, actual);
  }
}

// Find the (skip+1)-th champion card index in fullDeck (0 is a deliberate
// edge case: champion index 0 must be selectable, unlike the AI cash-exchange
// heuristic which historically used 0 as a "not found" sentinel).
static uint8_t find_champion(uint8_t skip)
{ uint8_t found = 0;
  for(int i = 0; i < FULL_DECK_SIZE; i++)
  { if(fullDeck[i].card_type == CHAMPION_CARD)
    { if(found == skip) return (uint8_t)i;
      found++;
    }
  }
  return 0;
}

// Find a draw/recall card with the given choose_num (1 or 2)
static uint8_t find_draw_card(uint8_t choose_num)
{ for(int i = 0; i < FULL_DECK_SIZE; i++)
  { if(fullDeck[i].card_type == DRAW_CARD && fullDeck[i].choose_num == choose_num)
      return (uint8_t)i;
  }
  return 0;
}

void test_collect_champions(TestSuite* suite)
{ printf("\n=== collect_champions() TESTS ===\n");

  uint8_t champ_low = find_champion(0);
  uint8_t champ_high = find_champion(1);
  uint8_t draw_card = find_draw_card(1);

  uint8_t mixed[3] = {draw_card, champ_low, champ_high};
  uint8_t out[3];

  uint8_t count = collect_champions(mixed, 3, out, false);
  print_test_result("Filters out non-champion cards", 2, count);
  suite->passed += (count == 2);
  suite->failed += (count != 2);

  count = collect_champions(mixed, 3, out, true);
  int sorted_desc = (count == 2) && (fullDeck[out[0]].power >= fullDeck[out[1]].power);
  print_test_result("Sorts champions by descending power", 1, sorted_desc);
  suite->passed += sorted_desc;
  suite->failed += !sorted_desc;
}

void test_recall_one_champion(TestSuite* suite, GameContext* ctx, config_t* cfg)
{ printf("\n=== RECALL EXACTLY 1 CHAMPION (index-0 edge case) ===\n");

  uint8_t champ0 = find_champion(0);
  uint8_t recall1_card = find_draw_card(1);

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Discard_init(&gs.discard[PLAYER_A]);
  gs.current_cash_balance[PLAYER_A] = 10;
  Hand_add(&gs.hand[PLAYER_A], recall1_card);
  Discard_add(&gs.discard[PLAYER_A], champ0);

  uint8_t indices[2] = {0};
  int result = validate_and_recall_champions(&gs, PLAYER_A, recall1_card,
                                             indices, 1, ctx, cfg);
  print_test_result("Returns ACTION_TAKEN", ACTION_TAKEN, result);
  suite->passed += (result == ACTION_TAKEN);
  suite->failed += (result != ACTION_TAKEN);

  int champ_in_hand = Hand_contains(&gs.hand[PLAYER_A], champ0);
  print_test_result("Champion 0 moved to hand", 1, champ_in_hand);
  suite->passed += champ_in_hand;
  suite->failed += !champ_in_hand;

  int champ_left_in_discard = Discard_remove(&gs.discard[PLAYER_A], champ0);
  print_test_result("Champion 0 removed from discard", 0, champ_left_in_discard);
  suite->passed += !champ_left_in_discard;
  suite->failed += champ_left_in_discard;

  int cost_paid = (gs.current_cash_balance[PLAYER_A] == 10 - fullDeck[recall1_card].cost);
  print_test_result("Card cost paid", 1, cost_paid);
  suite->passed += cost_paid;
  suite->failed += !cost_paid;

  int card_left_hand = !Hand_contains(&gs.hand[PLAYER_A], recall1_card);
  print_test_result("Draw/recall card left hand", 1, card_left_hand);
  suite->passed += card_left_hand;
  suite->failed += !card_left_hand;
}

void test_recall_two_champions(TestSuite* suite, GameContext* ctx, config_t* cfg)
{ printf("\n=== RECALL EXACTLY 2 CHAMPIONS ===\n");

  uint8_t champ0 = find_champion(0);
  uint8_t champ1 = find_champion(1);
  uint8_t recall2_card = find_draw_card(2);

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_B]);
  Discard_init(&gs.discard[PLAYER_B]);
  gs.current_cash_balance[PLAYER_B] = 10;
  Hand_add(&gs.hand[PLAYER_B], recall2_card);
  Discard_add(&gs.discard[PLAYER_B], champ0);
  Discard_add(&gs.discard[PLAYER_B], champ1);

  uint8_t indices[2] = {0, 1};
  int result = validate_and_recall_champions(&gs, PLAYER_B, recall2_card,
                                             indices, 2, ctx, cfg);
  print_test_result("Returns ACTION_TAKEN", ACTION_TAKEN, result);
  suite->passed += (result == ACTION_TAKEN);
  suite->failed += (result != ACTION_TAKEN);

  int both_recalled = Hand_contains(&gs.hand[PLAYER_B], champ0) &&
                      Hand_contains(&gs.hand[PLAYER_B], champ1);
  print_test_result("Both champions moved to hand", 1, both_recalled);
  suite->passed += both_recalled;
  suite->failed += !both_recalled;

  int only_card_left = (gs.discard[PLAYER_B].size == 1 &&
                        gs.discard[PLAYER_B].cards[0] == recall2_card);
  print_test_result("Only the draw/recall card left in discard", 1, only_card_left);
  suite->passed += only_card_left;
  suite->failed += !only_card_left;
}

int main(void)
{ TestSuite suite = {"Recall Mechanic Tests", 0, 0};

  printf("\n=== ORACLE RECALL MECHANIC TEST SUITE ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 1234;
  GameContext* ctx = create_game_context(&cfg);

  test_collect_champions(&suite);
  test_recall_one_champion(&suite, ctx, &cfg);
  test_recall_two_champions(&suite, ctx, &cfg);

  destroy_game_context(ctx);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
