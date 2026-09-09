// test_puct.c
// Test suite for A14 AlphaOracle Prime Plus I's PUCT selection/tree
// storage (ai_strat_ismcts_tree.c's ismcts_puct_score(), ISMCTSArena's
// policy/policy_dim fields) and search integration (ai_strat_puct_search.c,
// ai_strat_puct.c) -- see doc/ai_agents.md's A14 section. Mirrors
// test_ismcts.c's own two-layer shape: white-box unit tests against
// hand-built trees first, then integration tests against real gamestates.
//
// Parallel to test_ismcts.c, not edits to it -- that file's
// test_uct_unvisited_child_is_infinite/test_uct_flips_for_opponent_perspective
// encode plain-UCT behaviour this agent intentionally departs from, and
// must keep passing for A10/A11 unchanged.
//
// No trained weights exist yet for this agent (Stage 3 -- corpus
// generation/training -- hasn't run), so every test here runs against an
// UNLOADED net: puctnet_forward()'s own documented fallback (value 0.5,
// all-zero policy logits, which reduce to a uniform distribution
// regardless of the legal move list). That's deliberate, not a gap: it
// exercises exactly the "uniform-prior fallback" contract real play relies
// on before/without a weights file, and it's what makes this agent's
// safety guarantee (decide_and_apply() degrading fully to plain A10)
// testable without needing real weights at all.

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../src/ai_strat/ai_strat_ismcts_tree.h"
#include "../src/ai_strat/ai_strat_ismcts_search.h"
#include "../src/ai_strat/ai_strat_ismcts1.h"
#include "../src/ai_strat/ai_strat_puct.h"
#include "../src/ai_strat/ai_strat_puct_search.h"
#include "../src/ai_strat/ai_strat_puct_net.h"
#include "../src/ai_strat/ai_strat_playout.h"
#include "../src/core/game_state.h"
#include "../src/core/turn_logic.h"

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

/* ========================================================================
   ismcts_puct_score(): white-box unit tests, hand-built trees.
   ======================================================================== */

static void test_puct_score_unvisited_uses_fpu_not_infinity(TestSuite* suite)
{ printf("\n=== PUCT: an untried move (child == ISMCTS_NO_NODE) scores finite, "
           "not +infinity ===\n");

  ISMCTSNode storage[2];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 2, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A);
  arena.nodes[root].visits = 4;
  arena.nodes[root].total_score = 3.0f; // parent_q (root_player's own view) == 0.75

  // c_puct == 0.0f isolates the Q term (the exploration term vanishes).
  float score = ismcts_puct_score(&arena, root, ISMCTS_NO_NODE, 4, 1.0f, 0.0f, 0.2f);
  check(suite, "score is finite (FPU, not +infinity)", isfinite(score));
  check(suite, "score == parent_q - fpu_reduction == 0.55", close(score, 0.55f, 1e-6f));
} // test_puct_score_unvisited_uses_fpu_not_infinity

static void test_puct_score_root_no_visits_uses_neutral_baseline(TestSuite* suite)
{ printf("\n=== PUCT: a parent with zero visits falls back to a neutral 0.5 baseline ===\n");

  ISMCTSNode storage[1];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 1, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A); // visits == 0 here

  float score = ismcts_puct_score(&arena, root, ISMCTS_NO_NODE, 0, 1.0f, 0.0f, 0.2f);
  check(suite, "score == 0.5 - fpu_reduction == 0.3 (c_puct=0 isolates Q)",
        close(score, 0.3f, 1e-6f));
} // test_puct_score_root_no_visits_uses_neutral_baseline

static void test_puct_score_matches_formula_for_existing_child(TestSuite* suite)
{ printf("\n=== PUCT: an existing child matches c_puct * P(a) * sqrt(N) / (1+n) exactly ===\n");

  ISMCTSNode storage[2];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 2, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A);
  GameMove m = { .type = MOVE_PASS };
  uint32_t child = ismcts_create_child(&arena, root, &m, PLAYER_B);
  arena.nodes[child].visits = 3;
  arena.nodes[child].total_score = 1.5f; // mean == 0.5, root_player's own seat

  float score = ismcts_puct_score(&arena, root, child, 9, 0.4f, 2.0f, 0.2f);
  // q = 0.5 (root itself decides here, since child's PARENT -- root -- is
  // PLAYER_A == root_player, no perspective flip); u = 2.0*0.4*sqrt(9)/(1+3)
  float expected = 0.5f + 2.0f * 0.4f * sqrtf(9.0f) / 4.0f;
  check(suite, "score matches the closed-form PUCT formula", close(score, expected, 1e-5f));
} // test_puct_score_matches_formula_for_existing_child

static void test_puct_score_flips_for_opponent_perspective(TestSuite* suite)
{ printf("\n=== PUCT: a non-root-player parent's mean is flipped to its own win rate "
           "(mirrors test_ismcts.c's UCT counterpart) ===\n");

  ISMCTSNode storage[2];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 2, PLAYER_A); // root_player = PLAYER_A
  uint32_t root = ismcts_create_root(&arena, PLAYER_B); // PLAYER_B decides at the root
  GameMove m = { .type = MOVE_PASS };
  uint32_t child = ismcts_create_child(&arena, root, &m, PLAYER_A);

  // total_score is always stored from root_player's (PLAYER_A's) seat: 0.9
  // over 10 visits means PLAYER_A wins 90% of the time, i.e. PLAYER_B (who
  // chooses at the root) should see this child as only a 10% mean.
  arena.nodes[child].visits = 10;
  arena.nodes[child].total_score = 9.0f;

  float score = ismcts_puct_score(&arena, root, child, 10, 1.0f, 0.0f, 0.2f); // c_puct=0
  check(suite, "PLAYER_B's own view of a PLAYER_A-favouring child is low", score < 0.2f);
} // test_puct_score_flips_for_opponent_perspective

static void test_arena_policy_fields_default_off(TestSuite* suite)
{ printf("\n=== ISMCTSArena: policy/policy_dim default to NULL/0 (A10/A11's own arenas "
           "never touch these) ===\n");

  ISMCTSNode storage[1];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 1, PLAYER_A);

  check(suite, "policy is NULL by default", arena.policy == NULL);
  check(suite, "policy_dim is 0 by default", arena.policy_dim == 0);
} // test_arena_policy_fields_default_off

/* ========================================================================
   puct_compose_priors()/puct_uniform_priors() via the net's own unloaded
   fallback -- ai_strat_puct_net.c's contract, exercised end-to-end.
   ======================================================================== */

static void test_puctnet_unloaded_yields_uniform_policy(TestSuite* suite)
{ printf("\n=== puctnet_value_and_policy: unloaded net -> value 0.5, uniform-reducing "
           "all-zero policy logits ===\n");

  check(suite, "no weights loaded in this test binary", !puctnet_is_loaded());

  config_t cfg = {0};
  cfg.prng_seed = 1234;
  GameContext* ctx = create_game_context(&cfg);
  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  float value;
  float policy[PUCT_POLICY_DIM];
  puctnet_value_and_policy(&gs, PLAYER_A, &value, policy);

  check(suite, "value == 0.5f when unloaded", value == 0.5f);
  bool all_zero = true;
  for(int i = 0; i < PUCT_POLICY_DIM; i++)
    if(policy[i] != 0.0f) all_zero = false;
  check(suite, "policy logits are all-zero when unloaded (softmaxes to uniform)", all_zero);

  destroy_game_context(ctx);
} // test_puctnet_unloaded_yields_uniform_policy

/* ========================================================================
   Integration tests -- real gamestates, ismcts_search_best_move()/
   puct_attack_strategy().
   ======================================================================== */

static ISMCTSParams small_puct_ismcts_params(uint32_t iterations, uint32_t max_nodes)
{ ISMCTSParams p = ISMCTS_DEFAULTS;
  p.limit_iterations = iterations;
  p.limit_max_nodes = max_nodes;
  return p;
} // small_puct_ismcts_params

static void test_puct_search_runs_and_returns_legal_move(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move (PUCTParams != NULL): runs without crashing, "
           "returns a legal move ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 8080;
  GameContext* ctx = create_game_context(&cfg);
  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSParams params = small_puct_ismcts_params(300, 5000);
  PUCTParams puct = PUCT_DEFAULTS;
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);
  GameContext sim_ctx = mc_fork_context(ctx, 909);

  GameMove move = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx, &params, &rollout, &puct);
  check(suite, "a move type in the valid enum range was returned",
        move.type >= MOVE_PASS && move.type <= MOVE_CASH);

  destroy_game_context(ctx);
} // test_puct_search_runs_and_returns_legal_move

static void test_puct_search_root_untouched(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move (PUCT): root gamestate untouched ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 8181;
  GameContext* ctx = create_game_context(&cfg);
  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);
  struct gamestate gs_before = gs;

  ISMCTSParams params = small_puct_ismcts_params(300, 5000);
  PUCTParams puct = PUCT_DEFAULTS;
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);
  GameContext sim_ctx = mc_fork_context(ctx, 1717);

  ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx, &params, &rollout, &puct);

  check(suite, "root gamestate byte-identical after search",
        memcmp(&gs, &gs_before, sizeof(struct gamestate)) == 0);

  destroy_game_context(ctx);
} // test_puct_search_root_untouched

static void test_puct_search_determinism(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move (PUCT): same seed -> same move ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 8282;
  GameContext* ctx = create_game_context(&cfg);
  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSParams params = small_puct_ismcts_params(300, 5000);
  PUCTParams puct = PUCT_DEFAULTS;
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);

  GameContext sim_ctx1 = mc_fork_context(ctx, 5151);
  GameMove move1 = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx1, &params, &rollout, &puct);

  GameContext sim_ctx2 = mc_fork_context(ctx, 5151);
  GameMove move2 = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx2, &params, &rollout, &puct);

  check(suite, "identical seed reproduces the identical chosen move",
        memcmp(&move1, &move2, sizeof(GameMove)) == 0);

  destroy_game_context(ctx);
} // test_puct_search_determinism

static void test_puct_select_or_expand_prefers_higher_prior_untried_move(TestSuite* suite)
{ printf("\n=== puct_select_or_expand: a strong prior on a LATER-enumerated move type "
           "wins over the first-enumerated one (PASS) -- not first-enumerated-wins ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 2222;
  GameContext* ctx = create_game_context(&cfg);
  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx); // fresh hand, ~85% champions in the deck -- a
  // MOVE_CHAMPIONS candidate is affordable on essentially every seed

  ISMCTSNode storage[4];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 4, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A);
  // A fresh (0-visit) root makes denom==0, zeroing every candidate's
  // exploration term regardless of prior -- correct PUCT behaviour (no
  // information yet to explore with), but it would wash out exactly the
  // property this test wants to demonstrate. One visit gives the prior
  // real weight (denom=1) without needing a full search loop.
  arena.nodes[root].visits = 1;

  float policy[PUCT_POLICY_DIM] = {0};
  policy[PUCT_POLICY_TYPE_OFFSET + MOVE_PASS] = -100.0f; // enumerated first, but crushed
  policy[PUCT_POLICY_TYPE_OFFSET + MOVE_CHAMPIONS] = 100.0f; // enumerated later, favoured
  arena.policy = policy;
  arena.policy_dim = PUCT_POLICY_DIM;

  MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 3 };
  ISMCTSParams ismcts_params = ISMCTS_DEFAULTS;
  PUCTParams puct = PUCT_DEFAULTS;

  SelectOutcome outcome = puct_select_or_expand(&arena, root, &gs, &limits,
                                                &ismcts_params, &puct);
  check(suite, "an expansion was chosen", outcome.expand);
  check(suite, "the chosen move is MOVE_CHAMPIONS, not the first-enumerated MOVE_PASS",
        outcome.expand_move.type == MOVE_CHAMPIONS);

  destroy_game_context(ctx);
} // test_puct_select_or_expand_prefers_higher_prior_untried_move

static void test_puct_select_or_expand_use_widening_caps_fresh_node_to_pass(TestSuite* suite)
{ printf("\n=== puct_select_or_expand: use_widening=true caps a fresh (0-visit) node "
           "to its first-enumerated move regardless of prior ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 2323;
  GameContext* ctx = create_game_context(&cfg);
  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSNode storage[4];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 4, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A); // visits == 0

  float policy[PUCT_POLICY_DIM] = {0};
  policy[PUCT_POLICY_TYPE_OFFSET + MOVE_CHAMPIONS] = 100.0f; // strongly favoured...
  arena.policy = policy;
  arena.policy_dim = PUCT_POLICY_DIM;

  MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 3 };
  ISMCTSParams ismcts_params = ISMCTS_DEFAULTS; // search_expand_threshold=3: 0 visits -> cap=1
  PUCTParams puct = PUCT_DEFAULTS;
  puct.use_widening = true;

  SelectOutcome outcome = puct_select_or_expand(&arena, root, &gs, &limits,
                                                &ismcts_params, &puct);
  check(suite, "...but widening still caps a fresh node to index 0 (MOVE_PASS)",
        outcome.expand && outcome.expand_move.type == MOVE_PASS);

  destroy_game_context(ctx);
} // test_puct_select_or_expand_use_widening_caps_fresh_node_to_pass

static void test_puct_attack_strategy_matches_a10_when_unloaded(TestSuite* suite)
{ printf("\n=== puct_attack_strategy: with no weights loaded, matches "
           "ismcts_attack_strategy() (A10) bit-for-bit under the same seed -- "
           "the real safety guarantee (see ai_strat_puct.h's puct_load_weights()) ===\n");

  check(suite, "no weights loaded in this test binary", !puctnet_is_loaded());

  config_t cfg_a = {0}, cfg_b = {0};
  cfg_a.prng_seed = cfg_b.prng_seed = 6464;
  GameContext* ctx_a10 = create_game_context(&cfg_a);
  GameContext* ctx_a14 = create_game_context(&cfg_b);

  struct gamestate gs_a10 = {0}, gs_a14 = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs_a10, ctx_a10);
  setup_game(INITIAL_CASH_DEFAULT, &gs_a14, ctx_a14);
  begin_of_turn(&gs_a10, ctx_a10);
  begin_of_turn(&gs_a14, ctx_a14);
  check(suite, "identical seed -> identical initial gamestate (test precondition)",
        memcmp(&gs_a10, &gs_a14, sizeof(struct gamestate)) == 0);

  ISMCTSParams small = small_puct_ismcts_params(300, 5000);
  ismcts_set_params(PLAYER_A, &small);
  puct_set_ismcts_params(PLAYER_A, &small);

  ismcts_attack_strategy(&gs_a10, ctx_a10);
  puct_attack_strategy(&gs_a14, ctx_a14);

  check(suite, "resulting gamestates are byte-identical",
        memcmp(&gs_a10, &gs_a14, sizeof(struct gamestate)) == 0);

  ismcts_reset_params();
  puct_reset_ismcts_params();
  destroy_game_context(ctx_a10);
  destroy_game_context(ctx_a14);
} // test_puct_attack_strategy_matches_a10_when_unloaded

static void test_puct_defense_strategy_applies_a_move(TestSuite* suite)
{ printf("\n=== puct_defense_strategy: applies a move and consumes the live ctx "
           "(unloaded net -- exercises the degrade-to-A10 path end to end) ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 7373;
  GameContext* ctx = create_game_context(&cfg);
  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSParams small = small_puct_ismcts_params(300, 5000);
  puct_set_ismcts_params(PLAYER_A, &small);
  puct_set_ismcts_params(PLAYER_B, &small);

  uint8_t hand_before = gs.hand[PLAYER_B].size;
  puct_defense_strategy(&gs, ctx); // defender is 1 - current_player

  check(suite, "no crash: hand size did not grow past its pre-move size + itself",
        gs.hand[PLAYER_B].size <= hand_before + 1);
  check(suite, "cash balance is non-negative after the move",
        gs.current_cash_balance[PLAYER_B] < 60000); // uint16_t underflow guard

  puct_reset_ismcts_params();
  destroy_game_context(ctx);
} // test_puct_defense_strategy_applies_a_move

int main(void)
{ TestSuite suite = {"A14 PUCT Tests", 0, 0};

  printf("\n=== A14 PUCT SELECTION/SEARCH TEST SUITE ===\n");

  test_puct_score_unvisited_uses_fpu_not_infinity(&suite);
  test_puct_score_root_no_visits_uses_neutral_baseline(&suite);
  test_puct_score_matches_formula_for_existing_child(&suite);
  test_puct_score_flips_for_opponent_perspective(&suite);
  test_arena_policy_fields_default_off(&suite);
  test_puctnet_unloaded_yields_uniform_policy(&suite);
  test_puct_search_runs_and_returns_legal_move(&suite);
  test_puct_search_root_untouched(&suite);
  test_puct_search_determinism(&suite);
  test_puct_select_or_expand_prefers_higher_prior_untried_move(&suite);
  test_puct_select_or_expand_use_widening_caps_fresh_node_to_pass(&suite);
  test_puct_attack_strategy_matches_a10_when_unloaded(&suite);
  test_puct_defense_strategy_applies_a_move(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
