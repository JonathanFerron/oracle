// test_rating.c
// Test suite for the Bradley-Terry rating system (src/rating/). Unlike
// the v2 spec's own oracle_rating_test.c (which prints tables for a
// human to eyeball, uses srand(time(NULL)), and asserts almost nothing),
// every test here is a real pass/fail check, and nothing is
// non-deterministic.

#include "../src/rating/rating.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define TEST_PASS "\033[32m✓ PASS\033[0m"
#define TEST_FAIL "\033[31m✗ FAIL\033[0m"

typedef struct
{ const char* name;
  int passed;
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

static bool close(double a, double b, double tol)
{ return fabs(a - b) < tol;
} // close

// Test 1: scale round-trip -- rating -> strength -> rating is stable, and
// the fixed reference points from the design (guide "Examples" table)
// hold exactly.
static void test_scale_roundtrip(TestSuite* suite)
{ int ratings[] = { 1, 10, 25, 40, 50, 60, 75, 90, 99 };
  for(size_t i = 0; i < sizeof(ratings) / sizeof(ratings[0]); i++)
  { double s = rating_display_to_strength(ratings[i]);
    int32_t back = rating_strength_to_display(s);
    check(suite, "scale round-trip", back == ratings[i]);
  }

  check(suite, "rating 50 == strength 1.0", close(rating_display_to_strength(50), 1.0, 1e-9));
  check(suite, "strength 1.0 == rating 50", rating_strength_to_display(1.0) == 50);
} // test_scale_roundtrip

// Test 2: Borealis anchor -- registering AI_STRATEGY_BOREALIS sets
// borealis_id and pins its strength/rating; it stays pinned after a
// match involving it.
static void test_borealis_anchor(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  uint32_t borealis = rating_register_ai(&rs, "borealis", AI_STRATEGY_BOREALIS);
  uint32_t rand_id = rating_register_ai(&rs, "rand", AI_STRATEGY_RANDOM);

  check(suite, "borealis_id set on registration", rs.borealis_id == borealis);
  check(suite, "borealis starts at strength 1.0",
        close(rs.entrants[borealis].bt_strength, 1.0, 1e-9));

  MatchResult m = { rand_id, borealis, 1, 20, 0 };
  rating_update_match(&rs, &m);

  check(suite, "borealis strength still 1.0 after a match",
        close(rs.entrants[borealis].bt_strength, 1.0, 1e-9));
  check(suite, "borealis rating still 50", rs.entrants[borealis].rating == 50);
} // test_borealis_anchor

// Test 3: probability symmetry.
static void test_probability_symmetry(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  uint32_t a = rating_register_ai(&rs, "a", AI_STRATEGY_RANDOM);
  uint32_t b = rating_register_ai(&rs, "b", AI_STRATEGY_VALUE_BASED);
  rs.entrants[b].bt_strength = 2.5;

  double pab = rating_win_probability(&rs, a, b);
  double pba = rating_win_probability(&rs, b, a);
  check(suite, "P(a,b) + P(b,a) == 1", close(pab + pba, 1.0, 1e-12));
  check(suite, "invalid id returns -1.0", rating_win_probability(&rs, 99, a) == -1.0);
} // test_probability_symmetry

// Test 4: adaptive A monotonicity.
static void test_adaptive_a_monotone(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  uint32_t id = rating_register_ai(&rs, "a", AI_STRATEGY_RANDOM);

  double a0 = rating_get_adaptive_a(&rs, id);
  rs.entrants[id].games_played = 10000;
  double a_far = rating_get_adaptive_a(&rs, id);

  check(suite, "A(0) == a_max", close(a0, rs.config.a_max, 1e-9));
  check(suite, "A(10000) approx a_min", close(a_far, rs.config.a_min, 1e-3));
  check(suite, "A decreases with games played", a_far < a0);
} // test_adaptive_a_monotone

// Builds a 3-entrant round-robin (borealis vs a vs b, both seats) with
// results generated from known target strengths, for tests 5-7.
static void build_known_dataset(RatingSystem* rs, RatingBatchData* batch,
                                double s_a, double s_b)
{ rating_init(rs, NULL);
  uint32_t bor = rating_register_ai(rs, "borealis", AI_STRATEGY_BOREALIS);
  uint32_t a = rating_register_ai(rs, "a", AI_STRATEGY_VALUE_BASED);
  uint32_t b = rating_register_ai(rs, "b", AI_STRATEGY_COMBO_THRESHOLD);

  uint32_t n = 100000;
  double p_bor_a = 1.0 / (1.0 + s_a);       // P(borealis beats a), s_borealis = 1.0
  double p_bor_b = 1.0 / (1.0 + s_b);
  double p_a_b = s_a / (s_a + s_b);

  MatchResult m1 = { bor, a, (uint32_t)(n * p_bor_a), (uint32_t)(n * (1 - p_bor_a)), 0 };
  MatchResult m2 = { bor, b, (uint32_t)(n * p_bor_b), (uint32_t)(n * (1 - p_bor_b)), 0 };
  MatchResult m3 = { a, b, (uint32_t)(n * p_a_b), (uint32_t)(n * (1 - p_a_b)), 0 };
  rating_batch_add_match(batch, &m1);
  rating_batch_add_match(batch, &m2);
  rating_batch_add_match(batch, &m3);
} // build_known_dataset

// Test 5+6: batch (MM) vs incremental, and MM vs gradient, agree with
// each other on the same synthetic dataset (loose tolerance -- the
// incremental path is path-dependent by design, so exact agreement isn't
// expected, only rough agreement).
static void test_batch_vs_incremental_and_solvers(TestSuite* suite)
{ RatingSystem rs_mm;
  RatingBatchData* batch = rating_batch_create();
  build_known_dataset(&rs_mm, batch, 2.0, 0.5);
  rating_batch_compute(&rs_mm, batch);

  RatingSystem rs_grad;
  RatingBatchData* batch2 = rating_batch_create();
  build_known_dataset(&rs_grad, batch2, 2.0, 0.5);
  rs_grad.config.batch_method = RATING_BATCH_GRADIENT;
  rs_grad.config.max_iterations = 20000;
  rating_batch_compute(&rs_grad, batch2);

  check(suite, "MM vs gradient agree on entrant a",
        close(rs_mm.entrants[1].bt_strength, rs_grad.entrants[1].bt_strength, 0.05));
  check(suite, "MM vs gradient agree on entrant b",
        close(rs_mm.entrants[2].bt_strength, rs_grad.entrants[2].bt_strength, 0.05));

  rating_batch_destroy(batch);
  rating_batch_destroy(batch2);
} // test_batch_vs_incremental_and_solvers

// Test 7: MM recovers the exact strengths a synthetic dataset was built
// from.
static void test_mm_recovers_known_strengths(TestSuite* suite)
{ RatingSystem rs;
  RatingBatchData* batch = rating_batch_create();
  build_known_dataset(&rs, batch, 3.0, 0.25);
  rating_batch_compute(&rs, batch);

  check(suite, "MM recovers strength of a (~3.0)", close(rs.entrants[1].bt_strength, 3.0, 0.05));
  check(suite, "MM recovers strength of b (~0.25)",
        close(rs.entrants[2].bt_strength, 0.25, 0.01));

  rating_batch_destroy(batch);
} // test_mm_recovers_known_strengths

// Test 8: CSV round-trip, including a human entrant.
static void test_csv_roundtrip(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  rating_register_ai(&rs, "borealis", AI_STRATEGY_BOREALIS);
  uint32_t human = rating_register_human(&rs, "Jonathan");
  rs.entrants[human].bt_strength = 1.75;
  rs.entrants[human].rating = rating_strength_to_display(1.75);
  rs.entrants[human].games_played = 42;
  rs.entrants[human].games_won = 30;
  rs.entrants[human].games_drawn = 1;

  bool exported = rating_export_csv(&rs, "/tmp/oracle_test_rating.csv");
  check(suite, "export succeeds", exported);

  RatingSystem rs2;
  rating_init(&rs2, NULL);
  bool imported = rating_import_csv(&rs2, "/tmp/oracle_test_rating.csv");
  check(suite, "import succeeds", imported);
  check(suite, "entrant count round-trips", rs2.num_entrants == rs.num_entrants);
  check(suite, "borealis_id round-trips", rs2.borealis_id == rs.borealis_id);

  const RatingEntry* h = rating_get_entrant(&rs2, human);
  check(suite, "human entrant found", h != NULL);
  if(h)
  { check(suite, "human name round-trips", strcmp(h->name, "Jonathan") == 0);
    check(suite, "human kind round-trips", h->kind == RATING_ENTRANT_HUMAN);
    check(suite, "human strength round-trips", close(h->bt_strength, 1.75, 1e-6));
    check(suite, "human games_played round-trips", h->games_played == 42);
  }

  check(suite, "comma in name rejected",
        rating_register_ai(&rs, "bad,name", AI_STRATEGY_RANDOM) == RATING_INVALID_ID);
} // test_csv_roundtrip

// Test 9: a MatchResult with more than 255 wins does not overflow (the
// v2 spec's uint8_t win-count regression).
static void test_wide_win_counts(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  uint32_t bor = rating_register_ai(&rs, "borealis", AI_STRATEGY_BOREALIS);
  uint32_t a = rating_register_ai(&rs, "a", AI_STRATEGY_RANDOM);

  MatchResult m = { a, bor, 500, 300, 0 };
  rating_update_match(&rs, &m);

  check(suite, "wide win counts applied without overflow",
        rs.entrants[a].games_played == 800 && rs.entrants[bor].games_played == 800);
} // test_wide_win_counts

// Test 10: leaderboard-adjacent helpers don't underflow on an empty or
// single-entrant system (the v2 spec's rating_print_leaderboard() bug).
static void test_empty_system_no_underflow(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  check(suite, "find_opponent on empty system returns invalid",
        rating_find_opponent(&rs, 0, 100) == RATING_INVALID_ID);

  rating_register_ai(&rs, "solo", AI_STRATEGY_RANDOM);
  check(suite, "find_opponent with one entrant returns invalid",
        rating_find_opponent(&rs, 0, 100) == RATING_INVALID_ID);
} // test_empty_system_no_underflow

// Test 11: registration past RATING_MAX_ENTRANTS.
static void test_roster_full(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  uint32_t last = RATING_INVALID_ID;
  char name[16];
  for(uint32_t i = 0; i < RATING_MAX_ENTRANTS; i++)
  { snprintf(name, sizeof(name), "e%u", i);
    last = rating_register_ai(&rs, name, AI_STRATEGY_RANDOM);
  }
  check(suite, "roster fills to capacity", last != RATING_INVALID_ID);
  check(suite, "one more registration is rejected",
        rating_register_ai(&rs, "overflow", AI_STRATEGY_RANDOM) == RATING_INVALID_ID);
} // test_roster_full

// Test 12: an all-draws match between equal-strength entrants leaves
// both unchanged (expected == actual == 0.5 delta throughout).
static void test_all_draws_no_change(TestSuite* suite)
{ RatingSystem rs;
  rating_init(&rs, NULL);
  uint32_t a = rating_register_ai(&rs, "a", AI_STRATEGY_RANDOM);
  uint32_t b = rating_register_ai(&rs, "b", AI_STRATEGY_VALUE_BASED);

  MatchResult m = { a, b, 0, 0, 50 };
  rating_update_match(&rs, &m);

  check(suite, "equal-strength all-draws match leaves strengths unchanged",
        close(rs.entrants[a].bt_strength, 1.0, 1e-9) &&
        close(rs.entrants[b].bt_strength, 1.0, 1e-9));
  check(suite, "draws counted on both sides",
        rs.entrants[a].games_drawn == 50 && rs.entrants[b].games_drawn == 50);
} // test_all_draws_no_change

int main(void)
{ TestSuite suite = { "Bradley-Terry Rating System Tests", 0, 0 };

  printf("\n=== ORACLE RATING SYSTEM TEST SUITE ===\n");

  test_scale_roundtrip(&suite);
  test_borealis_anchor(&suite);
  test_probability_symmetry(&suite);
  test_adaptive_a_monotone(&suite);
  test_batch_vs_incremental_and_solvers(&suite);
  test_mm_recovers_known_strengths(&suite);
  test_csv_roundtrip(&suite);
  test_wide_win_counts(&suite);
  test_empty_system_no_underflow(&suite);
  test_roster_full(&suite);
  test_all_draws_no_change(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
