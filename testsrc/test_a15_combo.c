// test_a15_combo.c
// Unit tests for A15 Risk Threshold's shared combo-participation scorer
// (ai_strat_a15_cards.c's a15_combo_participation(), the function R9b's
// play choice, R1/R5's discard-to-7 victim, and R2's mulligan victim all
// depend on) and a couple of R4's exact death-probability sanity checks
// (ai_strat_a15_prob.c). See ai_strat_a15.h's header comment for the rule
// statements these serve.

#include <stdio.h>

#include "../src/core/game_constants.h"
#include "../src/ai_strat/ai_strat_a15_cards.h"
#include "../src/ai_strat/ai_strat_a15_prob.h"

#define TEST_PASS "\033[32m\xE2\x9C\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xE2\x9C\x97 FAIL\033[0m"

typedef struct
{ const char* name;
  int passed;
  int failed;
} TestSuite;

static void check(TestSuite* suite, const char* name, bool ok)
{ printf("  %s: %s\n", ok ? TEST_PASS : TEST_FAIL, name);
  suite->passed += ok;
  suite->failed += !ok;
} // check

// First fullDeck[] champion of the given species not already in `exclude`
// (0-terminated list, at most 8 entries) -- lets each test pick distinct
// concrete cards without hardcoding fragile fullDeck[] indices.
static uint8_t find_champion_species(ChampionSpecies species, const uint8_t* exclude,
                                     uint8_t exclude_count)
{ for(int i = 0; i < FULL_DECK_SIZE; i++)
  { if(fullDeck[i].card_type != CHAMPION_CARD || fullDeck[i].species != species) continue;
    bool skip = false;
    for(uint8_t j = 0; j < exclude_count; j++)
      if(exclude[j] == (uint8_t)i) skip = true;
    if(!skip) return (uint8_t)i;
  }
  return UINT8_MAX;
} // find_champion_species

// Jonathan's worked example (2026-09-10): 2 Dragons + 2 Elves + 2 Humans +
// 1 Dwarf + 1 Aven -- the Aven (no species/order/color partner at all)
// should score lowest, the Dwarf (Order A, shared with the Elves and
// Humans -- see doc/game_rules_doc.md's Orders table) next.
static void test_worked_example(TestSuite* suite)
{ printf("\n=== a15_combo_participation(): Jonathan's worked example ===\n");

  uint8_t used[8] = {0};
  uint8_t dragon1 = find_champion_species(SPECIES_DRAGON, used, 0);
  used[0] = dragon1;
  uint8_t dragon2 = find_champion_species(SPECIES_DRAGON, used, 1);
  used[1] = dragon2;
  uint8_t elf1 = find_champion_species(SPECIES_ELF, used, 2);
  used[2] = elf1;
  uint8_t elf2 = find_champion_species(SPECIES_ELF, used, 3);
  used[3] = elf2;
  uint8_t human1 = find_champion_species(SPECIES_HUMAN, used, 4);
  used[4] = human1;
  uint8_t human2 = find_champion_species(SPECIES_HUMAN, used, 5);
  used[5] = human2;
  uint8_t dwarf = find_champion_species(SPECIES_DWARF, used, 6);
  used[6] = dwarf;
  uint8_t aven = find_champion_species(SPECIES_AVEN, used, 7);

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  uint8_t hand_cards[8] = { dragon1, dragon2, elf1, elf2, human1, human2, dwarf, aven };
  for(uint8_t i = 0; i < 8; i++) Hand_add(&gs.hand[PLAYER_A], hand_cards[i]);

  int p_aven = a15_combo_participation(&gs, PLAYER_A, aven);
  int p_dwarf = a15_combo_participation(&gs, PLAYER_A, dwarf);
  int p_dragon = a15_combo_participation(&gs, PLAYER_A, dragon1);
  int p_elf = a15_combo_participation(&gs, PLAYER_A, elf1);
  int p_human = a15_combo_participation(&gs, PLAYER_A, human1);

  // Exact values (marginal contribution -- see a15_combo_participation()'s
  // header comment on why this must be marginal, not raw subset bonus):
  // Aven=5 (a bare 2-color match, e.g. with a Dragon or Human -- Dragon/
  // Human/Aven are all COLOR_ORANGE per game_constants.c, Aven has no
  // species or order partner in this hand at all), Dwarf=7 (a bare 2-order
  // match, e.g. with an Elf or Human -- Human/Elf/Dwarf all share ORDER_A),
  // Dragon=Elf=Human=10 (each has a real same-species partner). This is a
  // stricter, more informative check than the ordering alone -- it pins the
  // exact reasoning, not just "lower than."
  check(suite, "Aven scores exactly 5 (bare color match, no species/order partner)",
        p_aven == 5);
  check(suite, "Dwarf scores exactly 7 (bare order match, no species partner)",
        p_dwarf == 7);
  check(suite, "A species-pair holder (Dragon) scores exactly 10", p_dragon == 10);
  check(suite, "Aven scores lower than Dwarf", p_aven < p_dwarf);
  check(suite, "Dwarf scores lower than a Dragon (which has a real species partner)",
        p_dwarf < p_dragon);
  check(suite, "Elf and Human (both species-pair holders) match Dragon's score",
        p_elf == p_dragon && p_human == p_dragon);
  check(suite, "The Aven -- the true lowest scorer -- is the next victim",
        a15_pick_victim(&gs, PLAYER_A) == aven);
} // test_worked_example

// The bug this regression test catches (found via the worked example above,
// 2026-09-10): a card with NO species/order/color match to anything in hand
// must score 0 participation even when it happens to be groupable with an
// unrelated pair that scores on its own -- calc_random_bonus() (combo_bonus.c)
// falls through to the bare pair tier when a 3-card group's third member
// matches nothing, so a naive "best subset bonus" (rather than MARGINAL
// contribution) would misread the unrelated card as participating in a
// combo it did nothing to earn.
static void test_unrelated_card_scores_zero(TestSuite* suite)
{ printf("\n=== a15_combo_participation(): unrelated card scores 0, not 10 ===\n");

  uint8_t used[2] = {0};
  uint8_t human1 = find_champion_species(SPECIES_HUMAN, used, 0);
  used[0] = human1;
  uint8_t human2 = find_champion_species(SPECIES_HUMAN, used, 1);
  used[1] = human2;
  uint8_t lycan = find_champion_species(SPECIES_LYCAN, used, 2); // Indigo, Order E --
  // no species/order/color match to Human (Orange, Order A) at all.

  struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Hand_add(&gs.hand[PLAYER_A], human1);
  Hand_add(&gs.hand[PLAYER_A], human2);
  Hand_add(&gs.hand[PLAYER_A], lycan);

  check(suite, "Lycan's own MARGINAL contribution is 0, not the group's 10",
        a15_combo_participation(&gs, PLAYER_A, lycan) == 0);
  check(suite, "A real species-pair member (Human) still scores its full 10",
        a15_combo_participation(&gs, PLAYER_A, human1) == 10);
} // test_unrelated_card_scores_zero

// A single champion's undefended death probability is exact: with
// attack_base + combo_bonus already >= own_energy, the dice roll can only
// add, so P(death) must be 1.0 regardless of the die.
static void test_p_death_certain(TestSuite* suite)
{ printf("\n=== a15_p_death_undefended(): certain-death sanity ===\n");

  uint8_t attacker_idx = UINT8_MAX;
  for(int i = 0; i < FULL_DECK_SIZE; i++)
    if(fullDeck[i].card_type == CHAMPION_CARD && fullDeck[i].attack_base >= 5)
    { attacker_idx = (uint8_t)i;
      break;
    }

  struct gamestate gs = {0};
  CombatZone_init(&gs.combat_zone[PLAYER_A]);
  CombatZone_add(&gs.combat_zone[PLAYER_A], attacker_idx);
  gs.current_energy[PLAYER_B] = 1; // attack_base alone already kills

  float p = a15_p_death_undefended(&gs, PLAYER_B);
  check(suite, "P(death) == 1.0 when attack_base alone exceeds energy",
        p > 0.999f && p <= 1.0f);
} // test_p_death_certain

// R8's endgame quantity, sanity-checked at both extremes and for
// monotonicity in the attack horizon (more attacks can only help, never
// hurt, so P(finish within 4) must never be lower than P(finish within 1)
// from the same position).
static void test_p_finish_within_sanity(TestSuite* suite)
{ printf("\n=== a15_p_finish_within(): boundary and monotonicity sanity ===\n");

  // Near-empty hand, opponent at full energy -- finishing within any
  // horizon this turn should be essentially impossible.
  struct gamestate weak = {0};
  Hand_init(&weak.hand[PLAYER_A]);
  Discard_init(&weak.discard[PLAYER_A]);
  Discard_init(&weak.discard[PLAYER_B]);
  CombatZone_init(&weak.combat_zone[PLAYER_A]);
  CombatZone_init(&weak.combat_zone[PLAYER_B]);
  weak.current_energy[PLAYER_B] = 99;
  Hand_init(&weak.hand[PLAYER_B]);
  for(uint8_t i = 0; i < 7; i++) Hand_add(&weak.hand[PLAYER_B], i); // opponent hand size only matters as a count

  float p_weak = a15_p_finish_within(&weak, PLAYER_A, 4);
  check(suite, "Empty hand vs. full-energy opponent: P(finish within 4) is near 0",
        p_weak < 0.05f);

  // A hand stacked with the deck's highest-attack_base champions, opponent
  // at 1 energy -- finishing within any horizon should be essentially certain.
  struct gamestate strong = {0};
  Hand_init(&strong.hand[PLAYER_A]);
  Discard_init(&strong.discard[PLAYER_A]);
  Discard_init(&strong.discard[PLAYER_B]);
  CombatZone_init(&strong.combat_zone[PLAYER_A]);
  CombatZone_init(&strong.combat_zone[PLAYER_B]);
  Hand_init(&strong.hand[PLAYER_B]);
  strong.current_energy[PLAYER_B] = 1;

  uint8_t added = 0;
  for(int i = 0; i < FULL_DECK_SIZE && added < 3; i++)
    if(fullDeck[i].card_type == CHAMPION_CARD && fullDeck[i].attack_base >= 5)
    { Hand_add(&strong.hand[PLAYER_A], (uint8_t)i);
      added++;
    }

  float p_strong = a15_p_finish_within(&strong, PLAYER_A, 4);
  check(suite, "3 strong champions vs. 1-energy opponent: P(finish within 4) is near 1",
        p_strong > 0.95f);

  // Monotonicity, using the same "weak" position at every horizon.
  float p1 = a15_p_finish_within(&weak, PLAYER_A, 1);
  float p2 = a15_p_finish_within(&weak, PLAYER_A, 2);
  float p3 = a15_p_finish_within(&weak, PLAYER_A, 3);
  float p4 = a15_p_finish_within(&weak, PLAYER_A, 4);
  check(suite, "P(finish within N) is non-decreasing in N",
        p1 <= p2 + 1e-6f && p2 <= p3 + 1e-6f && p3 <= p4 + 1e-6f);
} // test_p_finish_within_sanity

int main(void)
{ TestSuite suite = {"A15 Risk Threshold Combo/Probability Tests", 0, 0};

  printf("\n=== A15 COMBO PARTICIPATION / DEATH PROBABILITY TEST SUITE ===\n");

  test_worked_example(&suite);
  test_unrelated_card_scores_zero(&suite);
  test_p_death_certain(&suite);
  test_p_finish_within_sanity(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
} // main
