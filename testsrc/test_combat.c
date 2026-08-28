// test_combat.c
// Test suite for combat.c's combo-bonus table selection. Added 2026-08-28
// alongside the ComboBonusTable rename/fix (see doc/oracle_todo.md's Bug
// Tracker and doc/changelog.md's 2026-08-28 entry): combat.c used to pass a
// hardcoded COMBO_BONUS_RANDOM literal to calculate_combo_bonus() regardless
// of gstate->combo_bonus_table. combo_bonus.c's own routing was already
// covered by test_combo_bonus.c; nothing exercised combat.c's integration
// with it, which is exactly why the bug survived unnoticed.

#include "../src/core/combat.h"
#include "../src/core/game_state.h"
#include "../src/core/game_context.h"
#include <stdio.h>

#define TEST_PASS "\033[32m✓ PASS\033[0m"
#define TEST_FAIL "\033[31m✗ FAIL\033[0m"

typedef struct
{ const char* name;
  int passed;
  int failed;
} TestSuite;

static void print_test_result(const char* test_name, int expected, int actual)
{ if(expected == actual)
    printf("  %s: %s (expected %d, got %d)\n", TEST_PASS, test_name, expected, actual);
  else
    printf("  %s: %s (expected %d, got %d)\n", TEST_FAIL, test_name, expected, actual);
}

// fullDeck[0]/[3] are both SPECIES_HUMAN/COLOR_ORANGE/ORDER_A (see
// game_constants.c's "Orange champions" block) -- a same-species 2-card
// combo, which scores differently under the two tables: 10
// (calc_random_bonus) vs 7 (calc_prebuilt_bonus), a delta of 3. Same
// selection test_combo_bonus.c's own DECK_RANDOM/DECK_MONOCHROME routing
// tests already use directly on combo_bonus.c.
#define CHAMP_HUMAN_A 0
#define CHAMP_HUMAN_B 3
#define EXPECTED_COMBO_DELTA 3

void test_setup_game_defaults_to_random(TestSuite* suite)
{ printf("\n=== setup_game() defaults combo_bonus_table to COMBO_BONUS_RANDOM ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 1234;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs;
  setup_game(20, &gs, ctx);

  print_test_result("combo_bonus_table defaults to COMBO_BONUS_RANDOM",
                    COMBO_BONUS_RANDOM, gs.combo_bonus_table);
  suite->passed += (gs.combo_bonus_table == COMBO_BONUS_RANDOM);
  suite->failed += (gs.combo_bonus_table != COMBO_BONUS_RANDOM);

  destroy_game_context(ctx);
} // test_setup_game_defaults_to_random

// The regression this whole test file exists to catch: with the same
// combat-zone contents and identically-seeded (so identically-rolled) dice,
// calculate_total_attack() must actually read gstate->combo_bonus_table
// rather than always resolving as COMBO_BONUS_RANDOM. Two fresh contexts
// with the same seed give identical RND_dn() draws, so any difference
// between the two totals below is exactly the combo-bonus delta.
void test_calculate_total_attack_reads_combo_bonus_table(TestSuite* suite)
{ printf("\n=== calculate_total_attack() reads gstate->combo_bonus_table, "
           "not a hardcoded table ===\n");

  struct gamestate gs = {0};
  CombatZone_init(&gs.combat_zone[PLAYER_A]);
  CombatZone_add(&gs.combat_zone[PLAYER_A], CHAMP_HUMAN_A);
  CombatZone_add(&gs.combat_zone[PLAYER_A], CHAMP_HUMAN_B);

  config_t cfg = {0};
  cfg.prng_seed = 42;

  GameContext* ctx_random = create_game_context(&cfg);
  gs.combo_bonus_table = COMBO_BONUS_RANDOM;
  int16_t total_random = calculate_total_attack(&gs, PLAYER_A, ctx_random);
  destroy_game_context(ctx_random);

  GameContext* ctx_custom = create_game_context(&cfg); // same seed -> same rolls
  gs.combo_bonus_table = COMBO_BONUS_CUSTOM;
  int16_t total_custom = calculate_total_attack(&gs, PLAYER_A, ctx_custom);
  destroy_game_context(ctx_custom);

  int delta = total_random - total_custom;
  print_test_result("COMBO_BONUS_RANDOM vs COMBO_BONUS_CUSTOM delta (10 - 7)",
                    EXPECTED_COMBO_DELTA, delta);
  suite->passed += (delta == EXPECTED_COMBO_DELTA);
  suite->failed += (delta != EXPECTED_COMBO_DELTA);
} // test_calculate_total_attack_reads_combo_bonus_table

int main(void)
{ TestSuite suite = {"Combat Combo-Bonus Table Tests", 0, 0};

  printf("\n=== ORACLE COMBAT TEST SUITE ===\n");

  test_setup_game_defaults_to_random(&suite);
  test_calculate_total_attack_reads_combo_bonus_table(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
} // main
