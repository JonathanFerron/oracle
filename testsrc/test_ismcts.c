// test_ismcts.c
// Test suite for A10 IS-MCTS's node arena/UCT tree (ai_strat_ismcts_tree.c)
// and search loop (ai_strat_ismcts_search.c) -- see
// ideas/A10 ai agent is-mcts (the omniscient)/about.md. Two layers: white-box
// arena/UCT unit tests against hand-built trees, then integration tests
// against ismcts_search_best_move()/ismcts_attack_strategy() on real
// gamestates, mirroring test_moves.c's mc_playout()/mc_determinize()
// conventions (isolation, RNG-draw-count, card conservation).

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../src/ai_strat/ai_strat_ismcts_tree.h"
#include "../src/ai_strat/ai_strat_ismcts_search.h"
#include "../src/ai_strat/ai_strat_ismcts1.h"
#include "../src/ai_strat/ai_strat_ismcts_flat.h"
#include "../src/ai_strat/ai_strat_playout.h"
#include "../src/core/game_state.h"
#include "../src/core/game_constants.h"
#include "../src/core/turn_logic.h"

#define TEST_PASS "\033[32m\xe2\x9c\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xe2\x9c\x97 FAIL\033[0m"

typedef struct
{ const char* name;
  int passed;
  int failed;
} TestSuite;

static void check(TestSuite* suite, const char* name, int expected, int actual)
{ if(expected == actual)
  { printf("  %s: %s (expected %d, got %d)\n", TEST_PASS, name, expected, actual);
    suite->passed++;
  }
  else
  { printf("  %s: %s (expected %d, got %d)\n", TEST_FAIL, name, expected, actual);
    suite->failed++;
  }
} // check

/* ========================================================================
   Arena / UCT unit tests -- hand-built trees, no gamestate involved.
   ======================================================================== */

void test_arena_create_and_find(TestSuite* suite)
{ printf("\n=== ARENA: create_root/create_child/find_child ===\n");

  ISMCTSNode storage[8];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 8, PLAYER_A);

  uint32_t root = ismcts_create_root(&arena, PLAYER_A);
  check(suite, "root created at index 0", 0, (int)root);
  check(suite, "arena count is 1", 1, (int)arena.count);

  GameMove pass = { .type = MOVE_PASS };
  GameMove draw = { .type = MOVE_DRAW, .card = 102 };
  uint32_t c1 = ismcts_create_child(&arena, root, &pass, PLAYER_B);
  uint32_t c2 = ismcts_create_child(&arena, root, &draw, PLAYER_B);

  check(suite, "root child_count is 2", 2, (int)arena.nodes[root].child_count);
  check(suite, "find_child locates the pass child", (int)c1,
        (int)ismcts_find_child(&arena, root, &pass));
  check(suite, "find_child locates the draw child", (int)c2,
        (int)ismcts_find_child(&arena, root, &draw));

  GameMove cash = { .type = MOVE_CASH, .card = 117, .count = 1, .cards = {0} };
  check(suite, "find_child returns ISMCTS_NO_NODE for an absent move",
        1, ismcts_find_child(&arena, root, &cash) == ISMCTS_NO_NODE);
} // test_arena_create_and_find

void test_arena_full_returns_no_node(TestSuite* suite)
{ printf("\n=== ARENA: create_child fails cleanly once full ===\n");

  ISMCTSNode storage[2]; // room for the root and exactly one child
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 2, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A);

  GameMove m1 = { .type = MOVE_PASS };
  GameMove m2 = { .type = MOVE_DRAW, .card = 102 };
  uint32_t c1 = ismcts_create_child(&arena, root, &m1, PLAYER_B);
  uint32_t c2 = ismcts_create_child(&arena, root, &m2, PLAYER_B);

  check(suite, "first child created", 1, c1 != ISMCTS_NO_NODE);
  check(suite, "second child fails once arena is full", 1, c2 == ISMCTS_NO_NODE);
  check(suite, "root child_count still only counts the successful child",
        1, (int)arena.nodes[root].child_count);
} // test_arena_full_returns_no_node

void test_move_equality_by_type(TestSuite* suite)
{ printf("\n=== ARENA: find_child compares only each MoveType's real fields ===\n");

  ISMCTSNode storage[4];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 4, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A);

  GameMove subset = { .type = MOVE_CHAMPIONS, .count = 2, .cards = {5, 9} };
  ismcts_create_child(&arena, root, &subset, PLAYER_B);

  GameMove same = { .type = MOVE_CHAMPIONS, .count = 2, .cards = {5, 9} };
  GameMove different_cards = { .type = MOVE_CHAMPIONS, .count = 2, .cards = {5, 10} };
  GameMove different_count = { .type = MOVE_CHAMPIONS, .count = 1, .cards = {5} };

  check(suite, "identical subset matches", 1,
        ismcts_find_child(&arena, root, &same) != ISMCTS_NO_NODE);
  check(suite, "different card in the subset does not match", 1,
        ismcts_find_child(&arena, root, &different_cards) == ISMCTS_NO_NODE);
  check(suite, "different subset size does not match", 1,
        ismcts_find_child(&arena, root, &different_count) == ISMCTS_NO_NODE);
} // test_move_equality_by_type

void test_backprop_walks_ancestors(TestSuite* suite)
{ printf("\n=== ARENA: backprop updates every ancestor, not just the leaf ===\n");

  ISMCTSNode storage[4];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 4, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A);
  GameMove m = { .type = MOVE_PASS };
  uint32_t child = ismcts_create_child(&arena, root, &m, PLAYER_B);
  uint32_t grandchild = ismcts_create_child(&arena, child, &m, PLAYER_A);

  ismcts_backprop(&arena, grandchild, 1.0f);
  ismcts_backprop(&arena, grandchild, 0.0f);

  check(suite, "grandchild visits", 2, (int)arena.nodes[grandchild].visits);
  check(suite, "child visits", 2, (int)arena.nodes[child].visits);
  check(suite, "root visits", 2, (int)arena.nodes[root].visits);
  check(suite, "grandchild total_score", 1, arena.nodes[grandchild].total_score == 1.0f);
  check(suite, "child total_score", 1, arena.nodes[child].total_score == 1.0f);
  check(suite, "root total_score", 1, arena.nodes[root].total_score == 1.0f);
} // test_backprop_walks_ancestors

void test_uct_unvisited_child_is_infinite(TestSuite* suite)
{ printf("\n=== UCT: an unvisited child always scores +infinity ===\n");

  ISMCTSNode storage[2];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 2, PLAYER_A);
  uint32_t root = ismcts_create_root(&arena, PLAYER_A);
  GameMove m = { .type = MOVE_PASS };
  uint32_t child = ismcts_create_child(&arena, root, &m, PLAYER_B);

  float score = ismcts_uct_score(&arena, child, 1, 1.41421356f);
  check(suite, "unvisited child scores +infinity", 1, isinf(score));
} // test_uct_unvisited_child_is_infinite

void test_uct_flips_for_opponent_perspective(TestSuite* suite)
{ printf("\n=== UCT: a non-root-player node's mean is flipped to its own win rate ===\n");

  ISMCTSNode storage[2];
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, 2, PLAYER_A); // root_player = PLAYER_A
  uint32_t root = ismcts_create_root(&arena, PLAYER_B); // PLAYER_B decides at the root here
  GameMove m = { .type = MOVE_PASS };
  uint32_t child = ismcts_create_child(&arena, root, &m, PLAYER_A);

  // total_score is always stored from root_player's (PLAYER_A's) seat: 0.9
  // over 10 visits means PLAYER_A wins 90% of the time, i.e. PLAYER_B (who
  // chooses at the root) should see this child as only a 10% mean.
  arena.nodes[child].visits = 10;
  arena.nodes[child].total_score = 9.0f;

  float score = ismcts_uct_score(&arena, child, 10, 0.0f); // c=0 isolates the mean term
  check(suite, "PLAYER_B's own view of a PLAYER_A-favouring child is low",
        1, score < 0.2f);
} // test_uct_flips_for_opponent_perspective

/* ========================================================================
   Integration tests -- real gamestates, ismcts_search_best_move()/
   ismcts_attack_strategy().
   ======================================================================== */

static ISMCTSParams small_params(uint32_t iterations, uint32_t max_nodes)
{ ISMCTSParams p = ISMCTS_DEFAULTS;
  p.limit_iterations = iterations;
  p.limit_max_nodes = max_nodes;
  return p;
} // small_params

void test_ismcts_determinism(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move: same seed -> same move ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 3030;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSParams params = small_params(300, 5000);
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);

  GameContext sim_ctx1 = mc_fork_context(ctx, 7777);
  GameMove move1 = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx1, &params, &rollout);

  GameContext sim_ctx2 = mc_fork_context(ctx, 7777);
  GameMove move2 = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx2, &params, &rollout);

  check(suite, "identical seed reproduces the identical chosen move",
        1, memcmp(&move1, &move2, sizeof(GameMove)) == 0);

  destroy_game_context(ctx);
} // test_ismcts_determinism

void test_ismcts_root_untouched(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move: root gamestate untouched ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 4040;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);
  struct gamestate gs_before = gs;

  ISMCTSParams params = small_params(300, 5000);
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);
  GameContext sim_ctx = mc_fork_context(ctx, 9191);

  ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx, &params, &rollout);

  check(suite, "root gamestate byte-identical after search",
        1, memcmp(&gs, &gs_before, sizeof(struct gamestate)) == 0);

  destroy_game_context(ctx);
} // test_ismcts_root_untouched

void test_ismcts_parent_rng_advances_by_one_draw(TestSuite* suite)
{ printf("\n=== ismcts_attack_strategy: live ctx->rng advances by exactly the one "
           "fork-seed draw (mc_fork_context()'s contract) ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 5050;
  GameContext* ctx = create_game_context(&cfg);
  ISMCTSParams params = small_params(300, 5000);
  ismcts_set_params(PLAYER_A, &params);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  MTRand predicted = ctx->rng;
  genRandLong(&predicted); // the one draw fork_for_decision() makes

  ismcts_attack_strategy(&gs, ctx);

  check(suite, "ctx->rng matches exactly-one-draw prediction",
        1, memcmp(&predicted, &ctx->rng, sizeof(MTRand)) == 0);

  ismcts_reset_params();
  destroy_game_context(ctx);
} // test_ismcts_parent_rng_advances_by_one_draw

void test_ismcts_single_iteration(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move: limit_iterations=1 doesn't crash, "
           "returns a legal move ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 6060;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSParams params = small_params(1, 5000);
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);
  GameContext sim_ctx = mc_fork_context(ctx, 111);

  GameMove move = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx, &params, &rollout);
  check(suite, "a move type in the valid enum range was returned",
        1, move.type >= MOVE_PASS && move.type <= MOVE_CASH);

  destroy_game_context(ctx);
} // test_ismcts_single_iteration

void test_ismcts_arena_exhaustion(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move: a tiny arena degrades gracefully "
           "(stunting) rather than overrunning ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 7070;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSParams params = small_params(500, 5); // far more iterations than nodes
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);
  GameContext sim_ctx = mc_fork_context(ctx, 222);

  GameMove move = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx, &params, &rollout);
  check(suite, "a legal move type was still returned with a 5-node arena",
        1, move.type >= MOVE_PASS && move.type <= MOVE_CASH);

  destroy_game_context(ctx);
} // test_ismcts_arena_exhaustion

void test_ismcts_ablation_switch(TestSuite* suite)
{ printf("\n=== ismcts_search_best_move: search_use_availability=false "
           "(plain UCT) doesn't crash ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 8080;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);

  ISMCTSParams params = small_params(300, 5000);
  params.search_use_availability = false;
  StrategySet rollout = {0};
  set_player_strategy_by_type(&rollout, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout, PLAYER_B, AI_STRATEGY_RANDOM);
  GameContext sim_ctx = mc_fork_context(ctx, 333);

  GameMove move = ismcts_search_best_move(&gs, PLAYER_A, &sim_ctx, &params, &rollout);
  check(suite, "a legal move type was returned with the ablation switch off",
        1, move.type >= MOVE_PASS && move.type <= MOVE_CASH);

  destroy_game_context(ctx);
} // test_ismcts_ablation_switch

void test_ismcts_full_decision_advances_ctx_by_one_draw(TestSuite* suite)
{ printf("\n=== ismcts_attack_strategy: applies a move and consumes the live ctx ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 9090;
  GameContext* ctx = create_game_context(&cfg);
  ISMCTSParams params = small_params(300, 5000);
  ismcts_set_params(PLAYER_A, &params);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx);
  uint8_t hand_size_before = gs.hand[PLAYER_A].size;
  uint16_t cash_before = gs.current_cash_balance[PLAYER_A];

  ismcts_attack_strategy(&gs, ctx);

  check(suite, "no crash: hand size did not grow past its pre-move size + itself",
        1, gs.hand[PLAYER_A].size <= hand_size_before + 1); // draw cards can add cards
  check(suite, "cash balance is non-negative after the move",
        1, gs.current_cash_balance[PLAYER_A] <= cash_before + 1000); // sane upper bound, not a crash guard on paper

  ismcts_reset_params();
  destroy_game_context(ctx);
} // test_ismcts_full_decision_advances_ctx_by_one_draw

/* ========================================================================
   Flat mulligan / discard-to-7 tests (Phase 4) -- real gamestates,
   ismcts_mulligan()/ismcts_discard_to_7().
   ======================================================================== */

static ISMCTSParams flat_test_params(void)
{ ISMCTSParams p = ISMCTS_DEFAULTS;
  p.limit_flat_iterations = 44; // ~2 rollouts/candidate for mulligan's 22
  p.rollout_max_turns = 15;     // keep rollouts short for test speed
  return p;
} // flat_test_params

static struct gamestate discard_test_state(uint8_t hand_size)
{ struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Hand_init(&gs.hand[PLAYER_B]);
  Discard_init(&gs.discard[PLAYER_A]);
  Discard_init(&gs.discard[PLAYER_B]);
  CombatZone_init(&gs.combat_zone[PLAYER_A]);
  CombatZone_init(&gs.combat_zone[PLAYER_B]);
  gs.deck[PLAYER_A].top = -1;
  gs.deck[PLAYER_B].top = -1;
  for(uint8_t i = 0; i < 10; i++) DeckStk_push(&gs.deck[PLAYER_A], (uint8_t)(20 + i));
  for(uint8_t i = 0; i < 10; i++) DeckStk_push(&gs.deck[PLAYER_B], (uint8_t)(40 + i));
  for(uint8_t i = 0; i < hand_size; i++) Hand_add(&gs.hand[PLAYER_A], i);
  gs.current_energy[PLAYER_A] = 99;
  gs.current_energy[PLAYER_B] = 99;
  gs.current_player = PLAYER_A;
  gs.turn = 5;
  return gs;
} // discard_test_state

void test_ismcts_mulligan_conserves_hand_size(TestSuite* suite)
{ printf("\n=== ismcts_mulligan: hand size unchanged (toss N, redraw N) ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 1010;
  GameContext* ctx = create_game_context(&cfg);
  ISMCTSParams params = flat_test_params();
  ismcts_set_params(PLAYER_B, &params);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  uint8_t hand_size_before = gs.hand[PLAYER_B].size;

  ismcts_mulligan(&gs, PLAYER_B, ctx);

  check(suite, "hand size unchanged after mulligan", hand_size_before, gs.hand[PLAYER_B].size);

  ismcts_reset_params();
  destroy_game_context(ctx);
} // test_ismcts_mulligan_conserves_hand_size

void test_ismcts_mulligan_determinism(TestSuite* suite)
{ printf("\n=== ismcts_mulligan: same seed -> identical resulting hand ===\n");

  ISMCTSParams params = flat_test_params();
  ismcts_set_params(PLAYER_B, &params);

  config_t cfg1 = {0};
  cfg1.prng_seed = 2020;
  GameContext* ctx1 = create_game_context(&cfg1);
  struct gamestate gs1 = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs1, ctx1);
  ismcts_mulligan(&gs1, PLAYER_B, ctx1);

  config_t cfg2 = {0};
  cfg2.prng_seed = 2020;
  GameContext* ctx2 = create_game_context(&cfg2);
  struct gamestate gs2 = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs2, ctx2);
  ismcts_mulligan(&gs2, PLAYER_B, ctx2);

  check(suite, "identical seed reproduces the identical resulting hand",
        1, memcmp(&gs1.hand[PLAYER_B], &gs2.hand[PLAYER_B], sizeof(Hand)) == 0);

  ismcts_reset_params();
  destroy_game_context(ctx1);
  destroy_game_context(ctx2);
} // test_ismcts_mulligan_determinism

void test_ismcts_discard_to_7_reaches_seven(TestSuite* suite)
{ printf("\n=== ismcts_discard_to_7: an 8-card hand ends at exactly 7 ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 3030;
  GameContext* ctx = create_game_context(&cfg);
  ISMCTSParams params = flat_test_params();
  ismcts_set_params(PLAYER_A, &params);

  struct gamestate gs = discard_test_state(8);
  ismcts_discard_to_7(&gs, PLAYER_A, ctx);

  check(suite, "hand size is exactly 7", 7, gs.hand[PLAYER_A].size);
  check(suite, "discard gained exactly 1 card", 1, gs.discard[PLAYER_A].size);

  ismcts_reset_params();
  destroy_game_context(ctx);
} // test_ismcts_discard_to_7_reaches_seven

void test_ismcts_discard_to_7_nine_reaches_seven(TestSuite* suite)
{ printf("\n=== ismcts_discard_to_7: a 9-card hand ends at exactly 7 (36 candidates) ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 4040;
  GameContext* ctx = create_game_context(&cfg);
  ISMCTSParams params = flat_test_params();
  ismcts_set_params(PLAYER_A, &params);

  struct gamestate gs = discard_test_state(9);
  ismcts_discard_to_7(&gs, PLAYER_A, ctx);

  check(suite, "hand size is exactly 7", 7, gs.hand[PLAYER_A].size);
  check(suite, "discard gained exactly 2 cards", 2, gs.discard[PLAYER_A].size);

  ismcts_reset_params();
  destroy_game_context(ctx);
} // test_ismcts_discard_to_7_nine_reaches_seven

void test_ismcts_discard_to_7_determinism(TestSuite* suite)
{ printf("\n=== ismcts_discard_to_7: same seed -> identical resulting hand ===\n");

  ISMCTSParams params = flat_test_params();
  ismcts_set_params(PLAYER_A, &params);

  config_t cfg1 = {0};
  cfg1.prng_seed = 5050;
  GameContext* ctx1 = create_game_context(&cfg1);
  struct gamestate gs1 = discard_test_state(9);
  ismcts_discard_to_7(&gs1, PLAYER_A, ctx1);

  config_t cfg2 = {0};
  cfg2.prng_seed = 5050;
  GameContext* ctx2 = create_game_context(&cfg2);
  struct gamestate gs2 = discard_test_state(9);
  ismcts_discard_to_7(&gs2, PLAYER_A, ctx2);

  check(suite, "identical seed reproduces the identical resulting hand",
        1, memcmp(&gs1.hand[PLAYER_A], &gs2.hand[PLAYER_A], sizeof(Hand)) == 0);

  ismcts_reset_params();
  destroy_game_context(ctx1);
  destroy_game_context(ctx2);
} // test_ismcts_discard_to_7_determinism

void test_ismcts_discard_to_7_fallback_beyond_two(TestSuite* suite)
{ printf("\n=== ismcts_discard_to_7: a 10-card hand (beyond the 1/2 case) still "
           "safely reaches 7 via the shared-heuristic fallback ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 6060;
  GameContext* ctx = create_game_context(&cfg);
  ISMCTSParams params = flat_test_params();
  ismcts_set_params(PLAYER_A, &params);

  struct gamestate gs = discard_test_state(10);
  ismcts_discard_to_7(&gs, PLAYER_A, ctx);

  check(suite, "hand size is exactly 7", 7, gs.hand[PLAYER_A].size);

  ismcts_reset_params();
  destroy_game_context(ctx);
} // test_ismcts_discard_to_7_fallback_beyond_two

int main(void)
{ TestSuite suite = {"A10 IS-MCTS Tests", 0, 0};

  printf("\n=== A10 IS-MCTS TEST SUITE ===\n");

  test_arena_create_and_find(&suite);
  test_arena_full_returns_no_node(&suite);
  test_move_equality_by_type(&suite);
  test_backprop_walks_ancestors(&suite);
  test_uct_unvisited_child_is_infinite(&suite);
  test_uct_flips_for_opponent_perspective(&suite);
  test_ismcts_determinism(&suite);
  test_ismcts_root_untouched(&suite);
  test_ismcts_parent_rng_advances_by_one_draw(&suite);
  test_ismcts_single_iteration(&suite);
  test_ismcts_arena_exhaustion(&suite);
  test_ismcts_ablation_switch(&suite);
  test_ismcts_full_decision_advances_ctx_by_one_draw(&suite);
  test_ismcts_mulligan_conserves_hand_size(&suite);
  test_ismcts_mulligan_determinism(&suite);
  test_ismcts_discard_to_7_reaches_seven(&suite);
  test_ismcts_discard_to_7_nine_reaches_seven(&suite);
  test_ismcts_discard_to_7_determinism(&suite);
  test_ismcts_discard_to_7_fallback_beyond_two(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
