// test_puct_policy.c
// Test suite for A14 AlphaOracle Prime Plus I's action-encoding module
// (ai_strat_puct_policy.c) -- see doc/ai_agents.md's A14 section. Two
// layers: exact-formula unit tests against hand-built logit vectors/moves
// (one per MoveType, matching move_gen.c's actual field population), then
// softmax-level properties (sums to 1, uniform fallback, temperature
// clamp) -- mirroring test_ismcts.c's "white-box unit tests, no gamestate
// involved" shape for its own arena/UCT primitives.

#include <math.h>
#include <stdio.h>

#include "../src/ai_strat/ai_strat_puct_policy.h"
#include "../src/core/game_constants.h"

#define TEST_PASS "\033[32m\xe2\x9c\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xe2\x9c\x97 FAIL\033[0m"

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

static bool close(float a, float b, float tol)
{ return fabsf(a - b) < tol;
} // close

// Linear scans for a fullDeck[] card of a given type -- same pattern
// test_recall.c/test_cash_exchange.c use, rather than hardcoding fragile
// fullDeck indices. DRAW_CARD has two independent counting fields that a
// single "find by number" helper would conflate (this bit the first draft
// of this file): draw_num (2 or 3 -- what ismctsnn_catalog_index() keys on
// to pick catalog slot 102 vs 103) and choose_num (1 or 2 -- how many
// champions a MOVE_RECALL using this card may name, what move_gen.c's
// gen_recall_variants_1/_2 key on) -- kept as separate finders below.
static uint8_t find_card(CardType type)
{ for(uint8_t i = 0; i < FULL_DECK_SIZE; i++)
    if(fullDeck[i].card_type == type) return i;
  return 0; // unreachable for a well-formed deck -- every type exists
} // find_card

static uint8_t find_draw_card_by_draw_num(uint8_t draw_num)
{ for(uint8_t i = 0; i < FULL_DECK_SIZE; i++)
    if(fullDeck[i].card_type == DRAW_CARD && fullDeck[i].draw_num == draw_num) return i;
  return 0; // unreachable -- both Draw-2 and Draw-3 exist (game_constants.c)
} // find_draw_card_by_draw_num

static uint8_t find_draw_card_by_choose_num(uint8_t choose_num)
{ for(uint8_t i = 0; i < FULL_DECK_SIZE; i++)
    if(fullDeck[i].card_type == DRAW_CARD && fullDeck[i].choose_num == choose_num) return i;
  return 0; // unreachable -- both choose_num=1 and choose_num=2 cards exist
} // find_draw_card_by_choose_num

/* ========================================================================
   puct_move_score(): exact formula per MoveType.
   ======================================================================== */

static void test_score_pass_uses_only_type_logit(TestSuite* suite)
{ printf("\n=== puct_move_score: MOVE_PASS depends only on L_type[MOVE_PASS] ===\n");

  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_PASS] = 3.5f;
  logits[PUCT_POLICY_PLAY_OFFSET] = 99.0f; // must be ignored -- PASS has no cards

  GameMove pass = { .type = MOVE_PASS };
  check(suite, "score == L_type[MOVE_PASS], ignoring unrelated logits",
        close(puct_move_score(logits, &pass), 3.5f, 1e-6f));
} // test_score_pass_uses_only_type_logit

static void test_score_champions_uses_mean_and_count(TestSuite* suite)
{ printf("\n=== puct_move_score: MOVE_CHAMPIONS == type + count + mean(play) ===\n");

  uint8_t c1 = find_card(CHAMPION_CARD);
  uint8_t c2 = c1 + 1; // adjacent champion row, per game_constants.c's ordering
  uint8_t t1 = ismctsnn_catalog_index(&fullDeck[c1]);
  uint8_t t2 = ismctsnn_catalog_index(&fullDeck[c2]);

  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_CHAMPIONS] = 1.0f;
  logits[PUCT_POLICY_COUNT_OFFSET + 1] = 2.0f; // count == 2 -> slot [count-1] == [1]
  logits[PUCT_POLICY_PLAY_OFFSET + t1] = 4.0f;
  logits[PUCT_POLICY_PLAY_OFFSET + t2] = 8.0f;

  GameMove move = { .type = MOVE_CHAMPIONS, .count = 2, .cards = {c1, c2} };
  float expected = 1.0f + 2.0f + (4.0f + 8.0f) / 2.0f;
  check(suite, "score == type + count[1] + mean(play[t1], play[t2])",
        close(puct_move_score(logits, &move), expected, 1e-6f));
} // test_score_champions_uses_mean_and_count

static void test_score_draw_uses_play_only(TestSuite* suite)
{ printf("\n=== puct_move_score: MOVE_DRAW == type + play[card], no count term ===\n");

  uint8_t draw2 = find_draw_card_by_draw_num(2);
  uint8_t t = ismctsnn_catalog_index(&fullDeck[draw2]);

  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_DRAW] = 0.5f;
  logits[PUCT_POLICY_PLAY_OFFSET + t] = 2.5f;
  logits[PUCT_POLICY_COUNT_OFFSET] = 99.0f; // must be ignored -- DRAW has no count

  GameMove move = { .type = MOVE_DRAW, .card = draw2 };
  check(suite, "score == type + play[t], count ignored",
        close(puct_move_score(logits, &move), 3.0f, 1e-6f));
} // test_score_draw_uses_play_only

static void test_score_recall_uses_play_and_target(TestSuite* suite)
{ printf("\n=== puct_move_score: MOVE_RECALL == type + count + play[card] + mean(target) ===\n");

  uint8_t draw2 = find_draw_card_by_choose_num(2); // this move's count == 2
  uint8_t champ_a = find_card(CHAMPION_CARD);
  uint8_t champ_b = champ_a + 1;
  uint8_t t_card = ismctsnn_catalog_index(&fullDeck[draw2]);
  uint8_t t_a = ismctsnn_catalog_index(&fullDeck[champ_a]);
  uint8_t t_b = ismctsnn_catalog_index(&fullDeck[champ_b]);

  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_RECALL] = 1.0f;
  logits[PUCT_POLICY_COUNT_OFFSET + 1] = 3.0f; // choose_num == 2 -> slot [1]
  logits[PUCT_POLICY_PLAY_OFFSET + t_card] = 5.0f;
  logits[PUCT_POLICY_TARGET_OFFSET + t_a] = 2.0f;
  logits[PUCT_POLICY_TARGET_OFFSET + t_b] = 6.0f;

  GameMove move = { .type = MOVE_RECALL, .card = draw2, .count = 2, .recall = {champ_a, champ_b} };
  float expected = 1.0f + 3.0f + 5.0f + (2.0f + 6.0f) / 2.0f;
  check(suite, "score == type + count[1] + play[card] + mean(target[a], target[b])",
        close(puct_move_score(logits, &move), expected, 1e-6f));
} // test_score_recall_uses_play_and_target

static void test_score_cash_uses_play_and_target_no_count(TestSuite* suite)
{ printf("\n=== puct_move_score: MOVE_CASH == type + play[card] + target[champ], no count ===\n");

  uint8_t cash = find_card(CASH_CARD);
  uint8_t champ = find_card(CHAMPION_CARD);
  uint8_t t_card = ismctsnn_catalog_index(&fullDeck[cash]);
  uint8_t t_champ = ismctsnn_catalog_index(&fullDeck[champ]);

  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_CASH] = 1.0f;
  logits[PUCT_POLICY_PLAY_OFFSET + t_card] = 3.0f;
  logits[PUCT_POLICY_TARGET_OFFSET + t_champ] = 4.0f;
  logits[PUCT_POLICY_COUNT_OFFSET] = 99.0f; // must be ignored -- CASH has no count term

  GameMove move = { .type = MOVE_CASH, .card = cash, .count = 1, .cards = {champ} };
  check(suite, "score == type + play[card] + target[champ], count ignored",
        close(puct_move_score(logits, &move), 8.0f, 1e-6f));
} // test_score_cash_uses_play_and_target_no_count

static void test_score_deterministic(TestSuite* suite)
{ printf("\n=== puct_move_score: pure function -- same inputs, same output ===\n");

  uint8_t c1 = find_card(CHAMPION_CARD);
  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_PLAY_OFFSET + ismctsnn_catalog_index(&fullDeck[c1])] = 7.0f;

  GameMove move = { .type = MOVE_CHAMPIONS, .count = 1, .cards = {c1} };
  float a = puct_move_score(logits, &move);
  float b = puct_move_score(logits, &move);
  check(suite, "two calls with identical inputs return identical scores", a == b);
} // test_score_deterministic

/* ========================================================================
   puct_compose_priors() / puct_uniform_priors(): softmax-level properties.
   ======================================================================== */

static void test_compose_priors_sum_to_one(TestSuite* suite)
{ printf("\n=== puct_compose_priors: sums to 1.0 over the legal move list ===\n");

  uint8_t c1 = find_card(CHAMPION_CARD);
  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_PASS] = 0.3f;
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_CHAMPIONS] = -1.2f;
  logits[PUCT_POLICY_PLAY_OFFSET + ismctsnn_catalog_index(&fullDeck[c1])] = 2.1f;

  GameMove moves[3] =
  { { .type = MOVE_PASS },
    { .type = MOVE_CHAMPIONS, .count = 1, .cards = {c1} },
    { .type = MOVE_CHAMPIONS, .count = 1, .cards = {(uint8_t)(c1 + 1)} },
  };
  float probs[3];
  puct_compose_priors(logits, moves, 3, 1.0f, probs);

  float sum = probs[0] + probs[1] + probs[2];
  check(suite, "probabilities sum to 1.0", close(sum, 1.0f, 1e-5f));
  check(suite, "every probability is non-negative",
        probs[0] >= 0.0f && probs[1] >= 0.0f && probs[2] >= 0.0f);
} // test_compose_priors_sum_to_one

static void test_compose_priors_equal_logits_are_uniform(TestSuite* suite)
{ printf("\n=== puct_compose_priors: all-zero logits -> uniform distribution ===\n");

  float logits[PUCT_POLICY_DIM] = {0};
  GameMove moves[4] =
  { { .type = MOVE_PASS },
    { .type = MOVE_PASS },
    { .type = MOVE_PASS },
    { .type = MOVE_PASS },
  };
  float probs[4];
  puct_compose_priors(logits, moves, 4, 1.0f, probs);

  bool all_uniform = true;
  for(int i = 0; i < 4; i++)
    if(!close(probs[i], 0.25f, 1e-5f)) all_uniform = false;
  check(suite, "every probability is exactly 1/n", all_uniform);
} // test_compose_priors_equal_logits_are_uniform

static void test_compose_priors_nonpositive_temperature_defaults_to_one(TestSuite* suite)
{ printf("\n=== puct_compose_priors: temperature <= 0.0f falls back to 1.0f ===\n");

  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_PASS] = 1.7f;
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_DRAW] = -0.4f;

  uint8_t draw2 = find_draw_card_by_draw_num(2);
  GameMove moves[2] = { { .type = MOVE_PASS }, { .type = MOVE_DRAW, .card = draw2 } };

  float probs_explicit[2];
  float probs_zero[2];
  float probs_negative[2];
  puct_compose_priors(logits, moves, 2, 1.0f, probs_explicit);
  puct_compose_priors(logits, moves, 2, 0.0f, probs_zero);
  puct_compose_priors(logits, moves, 2, -5.0f, probs_negative);

  check(suite, "temperature=0.0f matches explicit 1.0f",
        close(probs_zero[0], probs_explicit[0], 1e-6f));
  check(suite, "temperature=-5.0f matches explicit 1.0f",
        close(probs_negative[0], probs_explicit[0], 1e-6f));
} // test_compose_priors_nonpositive_temperature_defaults_to_one

static void test_uniform_priors(TestSuite* suite)
{ printf("\n=== puct_uniform_priors: n moves -> each 1/n, summing to 1.0 ===\n");

  float probs[5];
  puct_uniform_priors(5, probs);

  bool all_equal = true;
  float sum = 0.0f;
  for(int i = 0; i < 5; i++)
  { if(!close(probs[i], 0.2f, 1e-6f)) all_equal = false;
    sum += probs[i];
  }
  check(suite, "every probability is exactly 1/n", all_equal);
  check(suite, "probabilities sum to 1.0", close(sum, 1.0f, 1e-5f));
} // test_uniform_priors

/* ========================================================================
   Coverage: every MoveType, and the shared card catalog stays in sync.
   ======================================================================== */

static void test_every_movetype_scores_finite(TestSuite* suite)
{ printf("\n=== puct_move_score: every MoveType produces a finite score ===\n");

  uint8_t champ = find_card(CHAMPION_CARD);
  uint8_t draw1 = find_draw_card_by_choose_num(1); // this move's count == 1
  uint8_t draw2 = find_draw_card_by_draw_num(2);
  uint8_t cash = find_card(CASH_CARD);
  float logits[PUCT_POLICY_DIM] = {0};

  GameMove moves[5] =
  { { .type = MOVE_PASS },
    { .type = MOVE_CHAMPIONS, .count = 1, .cards = {champ} },
    { .type = MOVE_DRAW, .card = draw2 },
    { .type = MOVE_RECALL, .card = draw1, .count = 1, .recall = {champ} },
    { .type = MOVE_CASH, .card = cash, .count = 1, .cards = {champ} },
  };
  bool all_finite = true;
  for(int i = 0; i < 5; i++)
    if(!isfinite(puct_move_score(logits, &moves[i]))) all_finite = false;
  check(suite, "all 5 MoveTypes score to a finite value", all_finite);
} // test_every_movetype_scores_finite

static void test_catalog_index_matches_documented_layout(TestSuite* suite)
{ printf("\n=== ismctsnn_catalog_index: matches ai_strat_ismctsnn_state.h's documented "
           "champion_id-1 / 102 / 103 / 104 layout (this module's only shared dependency) ===\n");

  uint8_t champ = find_card(CHAMPION_CARD);
  uint8_t draw2 = find_draw_card_by_draw_num(2);
  uint8_t draw3 = find_draw_card_by_draw_num(3);
  uint8_t cash = find_card(CASH_CARD);

  check(suite, "champion catalog index == champion_id - 1",
        ismctsnn_catalog_index(&fullDeck[champ]) == fullDeck[champ].champion_id - 1);
  check(suite, "draw-2 catalog index == 102", ismctsnn_catalog_index(&fullDeck[draw2]) == 102);
  check(suite, "draw-3 catalog index == 103", ismctsnn_catalog_index(&fullDeck[draw3]) == 103);
  check(suite, "cash catalog index == 104", ismctsnn_catalog_index(&fullDeck[cash]) == 104);
} // test_catalog_index_matches_documented_layout

/* ========================================================================
   puct_move_features(): the corpus-logging decomposition (Stage 2).
   ======================================================================== */

static void test_move_features_champions_fills_play_leaves_target_empty(TestSuite* suite)
{ printf("\n=== puct_move_features: MOVE_CHAMPIONS fills play[], leaves target[] at -1 ===\n");

  uint8_t c1 = find_card(CHAMPION_CARD);
  uint8_t c2 = (uint8_t)(c1 + 1);
  GameMove move = { .type = MOVE_CHAMPIONS, .count = 2, .cards = {c1, c2} };

  PUCTMoveFeatures f;
  puct_move_features(&move, &f);

  check(suite, "type preserved", f.type == MOVE_CHAMPIONS);
  check(suite, "count preserved", f.count == 2);
  check(suite, "play[0] is c1's catalog index", f.play[0] == (int8_t)ismctsnn_catalog_index(&fullDeck[c1]));
  check(suite, "play[1] is c2's catalog index", f.play[1] == (int8_t)ismctsnn_catalog_index(&fullDeck[c2]));
  check(suite, "play[2] unused (-1)", f.play[2] == -1);
  check(suite, "target[] entirely unused (-1)",
        f.target[0] == -1 && f.target[1] == -1 && f.target[2] == -1);
} // test_move_features_champions_fills_play_leaves_target_empty

static void test_move_features_recall_fills_both_play_and_target(TestSuite* suite)
{ printf("\n=== puct_move_features: MOVE_RECALL fills play[0] (the card played) AND "
           "target[] (the champions pulled from discard) ===\n");

  uint8_t draw2 = find_draw_card_by_choose_num(2);
  uint8_t champ_a = find_card(CHAMPION_CARD);
  uint8_t champ_b = (uint8_t)(champ_a + 1);
  GameMove move = { .type = MOVE_RECALL, .card = draw2, .count = 2, .recall = {champ_a, champ_b} };

  PUCTMoveFeatures f;
  puct_move_features(&move, &f);

  check(suite, "count preserved", f.count == 2);
  check(suite, "play[0] is the recall card's catalog index",
        f.play[0] == (int8_t)ismctsnn_catalog_index(&fullDeck[draw2]));
  check(suite, "play[1]/[2] unused (-1)", f.play[1] == -1 && f.play[2] == -1);
  check(suite, "target[0]/[1] are the two recalled champions' catalog indices",
        f.target[0] == (int8_t)ismctsnn_catalog_index(&fullDeck[champ_a]) &&
        f.target[1] == (int8_t)ismctsnn_catalog_index(&fullDeck[champ_b]));
  check(suite, "target[2] unused (-1)", f.target[2] == -1);
} // test_move_features_recall_fills_both_play_and_target

static void test_move_features_pass_is_all_empty(TestSuite* suite)
{ printf("\n=== puct_move_features: MOVE_PASS has no play/target cards at all ===\n");

  GameMove move = { .type = MOVE_PASS };
  PUCTMoveFeatures f;
  puct_move_features(&move, &f);

  check(suite, "count is 0", f.count == 0);
  check(suite, "play[] entirely unused (-1)",
        f.play[0] == -1 && f.play[1] == -1 && f.play[2] == -1);
  check(suite, "target[] entirely unused (-1)",
        f.target[0] == -1 && f.target[1] == -1 && f.target[2] == -1);
} // test_move_features_pass_is_all_empty

static void test_move_features_matches_move_score(TestSuite* suite)
{ printf("\n=== puct_move_features: composing it by hand matches puct_move_score() "
           "exactly (single source of truth, not two divergent decompositions) ===\n");

  uint8_t cash = find_card(CASH_CARD);
  uint8_t champ = find_card(CHAMPION_CARD);
  GameMove move = { .type = MOVE_CASH, .card = cash, .count = 1, .cards = {champ} };

  float logits[PUCT_POLICY_DIM] = {0};
  logits[PUCT_POLICY_TYPE_OFFSET + MOVE_CASH] = 1.5f;
  logits[PUCT_POLICY_PLAY_OFFSET + ismctsnn_catalog_index(&fullDeck[cash])] = 2.5f;
  logits[PUCT_POLICY_TARGET_OFFSET + ismctsnn_catalog_index(&fullDeck[champ])] = -3.5f;

  PUCTMoveFeatures f;
  puct_move_features(&move, &f);
  float hand_computed = logits[PUCT_POLICY_TYPE_OFFSET + f.type]
                        + logits[PUCT_POLICY_PLAY_OFFSET + f.play[0]]
                        + logits[PUCT_POLICY_TARGET_OFFSET + f.target[0]];

  check(suite, "puct_move_score(move) == score computed from puct_move_features(move)",
        close(puct_move_score(logits, &move), hand_computed, 1e-6f));
} // test_move_features_matches_move_score

int main(void)
{ TestSuite suite = {"A14 PUCT Policy Tests", 0, 0};

  printf("\n=== A14 PUCT POLICY (ACTION ENCODING) TEST SUITE ===\n");

  test_score_pass_uses_only_type_logit(&suite);
  test_score_champions_uses_mean_and_count(&suite);
  test_score_draw_uses_play_only(&suite);
  test_score_recall_uses_play_and_target(&suite);
  test_score_cash_uses_play_and_target_no_count(&suite);
  test_score_deterministic(&suite);
  test_compose_priors_sum_to_one(&suite);
  test_compose_priors_equal_logits_are_uniform(&suite);
  test_compose_priors_nonpositive_temperature_defaults_to_one(&suite);
  test_uniform_priors(&suite);
  test_every_movetype_scores_finite(&suite);
  test_catalog_index_matches_documented_layout(&suite);
  test_move_features_champions_fills_play_leaves_target_empty(&suite);
  test_move_features_recall_fills_both_play_and_target(&suite);
  test_move_features_pass_is_all_empty(&suite);
  test_move_features_matches_move_score(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
