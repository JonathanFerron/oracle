// test_combo_bonus.c
// Test suite for combo bonus calculator
// Updated for refactored source structure

#include "../src/core/combo_bonus.h"
#include "../src/core/game_constants.h"
#include <stdio.h>
#include <string.h>

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

void test_random_distribution(TestSuite *suite)
{ printf("\n=== RANDOM DISTRIBUTION TESTS ===\n");
  CombatCard cards[3];
  int bonus;

  // Test 1: Two same species
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  bonus = calc_random_bonus(cards, 2);
  print_test_result("Two same species", 10, bonus);
  suite->passed += (bonus == 10);
  suite->failed += (bonus != 10);

  // Test 2: Three same species
  cards[0] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  bonus = calc_random_bonus(cards, 3);
  print_test_result("Three same species", 16, bonus);
  suite->passed += (bonus == 16);
  suite->failed += (bonus != 16);

  // Test 3: Two same species + third same order (different species)
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_ELF, COLOR_INDIGO, ORDER_A
  };
  bonus = calc_random_bonus(cards, 3);
  print_test_result("Two species + third same order", 14, bonus);
  suite->passed += (bonus == 14);
  suite->failed += (bonus != 14);

  // Test 4: Two same species + third same color
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_HOBBIT, COLOR_RED, ORDER_B
  };
  bonus = calc_random_bonus(cards, 3);
  print_test_result("Two species + third same color", 13, bonus);
  suite->passed += (bonus == 13);
  suite->failed += (bonus != 13);

  // Test 5: Two same order (no species match)
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_ELF, COLOR_INDIGO, ORDER_A
  };
  bonus = calc_random_bonus(cards, 2);
  print_test_result("Two same order", 7, bonus);
  suite->passed += (bonus == 7);
  suite->failed += (bonus != 7);

  // Test 6: Three same order
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_ELF, COLOR_INDIGO, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_DWARF, COLOR_ORANGE, ORDER_A
  };
  bonus = calc_random_bonus(cards, 3);
  print_test_result("Three same order", 11, bonus);
  suite->passed += (bonus == 11);
  suite->failed += (bonus != 11);

  // Test 7: Two same order + third same color
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_ELF, COLOR_INDIGO, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_HOBBIT, COLOR_RED, ORDER_B
  };
  bonus = calc_random_bonus(cards, 3);
  print_test_result("Two order + third same color", 9, bonus);
  suite->passed += (bonus == 9);
  suite->failed += (bonus != 9);

  // Test 8: Two same color only
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_GOBLIN, COLOR_RED, ORDER_C
  };
  bonus = calc_random_bonus(cards, 2);
  print_test_result("Two same color", 5, bonus);
  suite->passed += (bonus == 5);
  suite->failed += (bonus != 5);

  // Test 9: Three same color (species/orders must all differ, or this
  // accidentally also matches on order -- see Human+Dwarf both ORDER_A)
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_ORANGE, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_GOBLIN, COLOR_ORANGE, ORDER_C
  };
  cards[2] = (CombatCard)
  { SPECIES_HOBBIT, COLOR_ORANGE, ORDER_B
  };
  bonus = calc_random_bonus(cards, 3);
  print_test_result("Three same color", 8, bonus);
  suite->passed += (bonus == 8);
  suite->failed += (bonus != 8);

  // Test 10: No combo
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_GOBLIN, COLOR_INDIGO, ORDER_C
  };
  bonus = calc_random_bonus(cards, 2);
  print_test_result("No combo", 0, bonus);
  suite->passed += (bonus == 0);
  suite->failed += (bonus != 0);
}

void test_prebuilt_distribution(TestSuite *suite)
{ printf("\n=== PRE-BUILT/MONOCHROME TESTS ===\n");
  CombatCard cards[3];
  int bonus;

  // Test 1: Two same species
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  bonus = calc_prebuilt_bonus(cards, 2);
  print_test_result("Two same species", 7, bonus);
  suite->passed += (bonus == 7);
  suite->failed += (bonus != 7);

  // Test 2: Three same species
  cards[0] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  bonus = calc_prebuilt_bonus(cards, 3);
  print_test_result("Three same species", 12, bonus);
  suite->passed += (bonus == 12);
  suite->failed += (bonus != 12);

  // Test 3: Two same species + third same order
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  bonus = calc_prebuilt_bonus(cards, 3);
  print_test_result("Two species + third same order", 9, bonus);
  suite->passed += (bonus == 9);
  suite->failed += (bonus != 9);

  // Test 4: Two same order (no species match)
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  bonus = calc_prebuilt_bonus(cards, 2);
  print_test_result("Two same order", 4, bonus);
  suite->passed += (bonus == 4);
  suite->failed += (bonus != 4);

  // Test 5: Three same order
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_ELF, COLOR_RED, ORDER_A
  };
  cards[2] = (CombatCard)
  { SPECIES_DWARF, COLOR_RED, ORDER_A
  };
  bonus = calc_prebuilt_bonus(cards, 3);
  print_test_result("Three same order", 6, bonus);
  suite->passed += (bonus == 6);
  suite->failed += (bonus != 6);

  // Test 6: No combo
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_GOBLIN, COLOR_RED, ORDER_C
  };
  bonus = calc_prebuilt_bonus(cards, 2);
  print_test_result("No combo", 0, bonus);
  suite->passed += (bonus == 0);
  suite->failed += (bonus != 0);
}

void test_main_function(TestSuite *suite)
{ printf("\n=== MAIN FUNCTION TESTS ===\n");
  CombatCard cards[3];
  int bonus;

  // Test with RANDOM deck type
  cards[0] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  cards[1] = (CombatCard)
  { SPECIES_HUMAN, COLOR_RED, ORDER_A
  };
  bonus = calculate_combo_bonus(cards, 2, DECK_RANDOM);
  print_test_result("DECK_RANDOM routing", 10, bonus);
  suite->passed += (bonus == 10);
  suite->failed += (bonus != 10);

  // Test with MONOCHROME deck type
  bonus = calculate_combo_bonus(cards, 2, DECK_MONOCHROME);
  print_test_result("DECK_MONOCHROME routing", 7, bonus);
  suite->passed += (bonus == 7);
  suite->failed += (bonus != 7);

  // Test with CUSTOM deck type
  bonus = calculate_combo_bonus(cards, 2, DECK_CUSTOM);
  print_test_result("DECK_CUSTOM routing", 7, bonus);
  suite->passed += (bonus == 7);
  suite->failed += (bonus != 7);

  // Test edge case: 1 card
  bonus = calculate_combo_bonus(cards, 1, DECK_RANDOM);
  print_test_result("Single card (no combo)", 0, bonus);
  suite->passed += (bonus == 0);
  suite->failed += (bonus != 0);
}

int main(void)
{ TestSuite suite = {"Combo Bonus Tests", 0, 0};

  printf("\n");
  printf("╔════════════════════════════════════════════╗\n");
  printf("║  ORACLE COMBO BONUS CALCULATOR TEST SUITE ║\n");
  printf("╠════════════════════════════════════════════╣\n");
  printf("║  The Five Orders of Arcadia               ║\n");
  printf("║  - ORDER_A (Dawn Light)                    ║\n");
  printf("║  - ORDER_B (Verdant Light)                 ║\n");
  printf("║  - ORDER_C (Ember Light)                   ║\n");
  printf("║  - ORDER_D (Eternal Light)                 ║\n");
  printf("║  - ORDER_E (Moonlight)                     ║\n");
  printf("╚════════════════════════════════════════════╝\n");

  test_random_distribution(&suite);
  test_prebuilt_distribution(&suite);
  test_main_function(&suite);

  printf("\n");
  printf("╔════════════════════════════════════════════╗\n");
  printf("║  TEST SUMMARY                              ║\n");
  printf("╠════════════════════════════════════════════╣\n");
  printf("║  Passed: %-3d                               ║\n", suite.passed);
  printf("║  Failed: %-3d                               ║\n", suite.failed);
  printf("║  Total:  %-3d                               ║\n",
         suite.passed + suite.failed);
  printf("╚════════════════════════════════════════════╝\n");

  return suite.failed > 0 ? 1 : 0;
}
