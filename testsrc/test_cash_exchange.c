// test_cash_exchange.c
// Test suite for the interactive cash-exchange path (play_cash_card_interactive).
//
// The human player picks any champion to exchange, including champion card
// index 0. select_champion_for_cash_exchange() (the separate AI heuristic)
// used to conflate index 0 with its "not found" sentinel; that sentinel is
// now UINT8_MAX. play_cash_card_interactive never called that heuristic in
// the first place, but this test still pins down that champion index 0 works
// correctly through the interactive path.

#include "../src/core/game_constants.h"
#include "../src/core/card_actions.h"
#include "../src/core/game_context.h"
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

static uint8_t find_cash_card(void)
{ for(int i = 0; i < FULL_DECK_SIZE; i++)
  { if(fullDeck[i].card_type == CASH_CARD)
      return (uint8_t)i;
  }
  return 0;
}

void test_exchange_champion_index_0(TestSuite* suite, GameContext* ctx)
{ printf("\n=== EXCHANGE CHAMPION INDEX 0 (edge case) ===\n");

  uint8_t champ0 = find_champion(0);
  uint8_t cash_card = find_cash_card();

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Discard_init(&gs.discard[PLAYER_A]);
  gs.current_cash_balance[PLAYER_A] = 10;
  Hand_add(&gs.hand[PLAYER_A], cash_card);
  Hand_add(&gs.hand[PLAYER_A], champ0);

  play_cash_card_interactive(&gs, PLAYER_A, cash_card, champ0, ctx);

  int champ_gone_from_hand = !Hand_contains(&gs.hand[PLAYER_A], champ0);
  print_test_result("Champion 0 removed from hand", 1, champ_gone_from_hand);
  suite->passed += champ_gone_from_hand;
  suite->failed += !champ_gone_from_hand;

  int champ_in_discard = Discard_remove(&gs.discard[PLAYER_A], champ0);
  print_test_result("Champion 0 moved to discard", 1, champ_in_discard);
  suite->passed += champ_in_discard;
  suite->failed += !champ_in_discard;

  int cash_gained = (gs.current_cash_balance[PLAYER_A] ==
                     10 - fullDeck[cash_card].cost + fullDeck[cash_card].exchange_cash);
  print_test_result("5 lunas received", 1, cash_gained);
  suite->passed += cash_gained;
  suite->failed += !cash_gained;

  int cash_card_gone = !Hand_contains(&gs.hand[PLAYER_A], cash_card);
  print_test_result("Cash card left hand", 1, cash_card_gone);
  suite->passed += cash_card_gone;
  suite->failed += !cash_card_gone;
}

void test_exchange_non_zero_champion(TestSuite* suite, GameContext* ctx)
{ printf("\n=== EXCHANGE A NON-ZERO CHAMPION (sanity check) ===\n");

  uint8_t champ1 = find_champion(1);
  uint8_t cash_card = find_cash_card();

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_B]);
  Discard_init(&gs.discard[PLAYER_B]);
  gs.current_cash_balance[PLAYER_B] = 10;
  Hand_add(&gs.hand[PLAYER_B], cash_card);
  Hand_add(&gs.hand[PLAYER_B], champ1);

  play_cash_card_interactive(&gs, PLAYER_B, cash_card, champ1, ctx);

  int champ_in_discard = Discard_remove(&gs.discard[PLAYER_B], champ1);
  print_test_result("Champion moved to discard", 1, champ_in_discard);
  suite->passed += champ_in_discard;
  suite->failed += !champ_in_discard;

  int cash_gained = (gs.current_cash_balance[PLAYER_B] ==
                     10 - fullDeck[cash_card].cost + fullDeck[cash_card].exchange_cash);
  print_test_result("5 lunas received", 1, cash_gained);
  suite->passed += cash_gained;
  suite->failed += !cash_gained;
}

int main(void)
{ TestSuite suite = {"Cash Exchange Tests", 0, 0};

  printf("\n=== ORACLE CASH EXCHANGE TEST SUITE ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 1234;
  GameContext* ctx = create_game_context(&cfg);

  test_exchange_champion_index_0(&suite, ctx);
  test_exchange_non_zero_champion(&suite, ctx);

  destroy_game_context(ctx);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
