// test_moves.c
// Test suite for src/actions/move_gen.c's get_available_moves() and
// move_apply.c's apply_move() -- the engine-level move enumeration/
// application A8 Simple Monte Carlo (and later A9-A11) searches over and
// plays. Hand-crafted game states rather than setup_game()'s shuffled deal,
// so exact fullDeck[] indices/costs are known and moves are deterministic.
//
// fullDeck[] indices used below (see game_constants.c's "Full deck
// definition" comment and its 0-based layout: 102 champions, 9 draw-2s,
// 6 draw-3s, 3 cash cards):
//   CHAMP_A/B/C = 0/1/2  -- champions, cost 0, power 10/14/12
//   CHAMP_D     = 3      -- champion, cost 1
//   DRAW2       = 102    -- draw card, cost 1, draw_num 2, choose_num 1
//   DRAW3       = 111    -- draw card, cost 2, draw_num 3, choose_num 2
//   CASH        = 117    -- cash card, cost 0, exchange_cash 5

#include <stdio.h>
#include <string.h>

#include "../src/actions/move_gen.h"
#include "../src/actions/move_apply.h"
#include "../src/ai_strat/ai_strat_playout.h"
#include "../src/core/game_state.h"
#include "../src/core/turn_logic.h"

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

static uint8_t count_type(const GameMove* moves, uint8_t n, MoveType type)
{ uint8_t c = 0;
  for(uint8_t i = 0; i < n; i++)
    if(moves[i].type == type) c++;
  return c;
} // count_type

static struct gamestate blank_state(PlayerID player, uint16_t budget, TurnPhase phase)
{ struct gamestate gs = {0};
  Hand_init(&gs.hand[player]);
  Discard_init(&gs.discard[player]);
  gs.current_cash_balance[player] = budget;
  gs.turn_phase = phase;
  return gs;
} // blank_state

void test_empty_hand(TestSuite* suite)
{ printf("\n=== EMPTY HAND: only MOVE_PASS ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
  GameMove moves[MOVE_GEN_MAX_MOVES];

  uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
  check(suite, "exactly 1 move", 1, n);
  check(suite, "it is MOVE_PASS", MOVE_PASS, moves[0].type);
} // test_empty_hand

void test_champion_subsets(TestSuite* suite)
{ printf("\n=== CHAMPION SUBSETS: three cost-0 champions ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_C);
  MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
  GameMove moves[MOVE_GEN_MAX_MOVES];

  uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
  // pass(1) + singles(3) + pairs(3) + triple(1) = 8
  check(suite, "8 total moves", 8, n);
  check(suite, "7 MOVE_CHAMPIONS", 7, count_type(moves, n, MOVE_CHAMPIONS));
} // test_champion_subsets

void test_champion_affordability(TestSuite* suite)
{ printf("\n=== CHAMPION AFFORDABILITY: individual vs cumulative cost ===\n");

  { struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_A); // cost 0
    Hand_add(&gs.hand[PLAYER_A], CHAMP_D); // cost 1, unaffordable at budget 0
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    GameMove moves[MOVE_GEN_MAX_MOVES];

    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "budget 0: pass + {A} only", 2, n);
  }

  { struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_A); // cost 0
    Hand_add(&gs.hand[PLAYER_A], CHAMP_D); // cost 1
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    GameMove moves[MOVE_GEN_MAX_MOVES];

    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    // pass + {A} + {D} + {A,D} (combined cost 1 <= budget 1)
    check(suite, "budget 1: pass + A + D + {A,D}", 4, n);
  }
} // test_champion_affordability

void test_draw_moves(TestSuite* suite)
{ printf("\n=== DRAW CARD ===\n");

  MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
  GameMove moves[MOVE_GEN_MAX_MOVES];

  { struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], DRAW2); // cost 1, unaffordable at budget 0
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "unaffordable draw card: pass only", 1, n);
  }

  { struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], DRAW2);
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "affordable draw card: pass + draw", 2, n);
    check(suite, "1 MOVE_DRAW", 1, count_type(moves, n, MOVE_DRAW));
  }
} // test_draw_moves

void test_recall_moves(TestSuite* suite)
{ printf("\n=== RECALL ===\n");

  GameMove moves[MOVE_GEN_MAX_MOVES];

  { // Empty discard: no recall even though the draw card is affordable.
    struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], DRAW2);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "empty discard: 0 MOVE_RECALL", 0, count_type(moves, n, MOVE_RECALL));
  }

  { // choose_num == 1, 3 recallable champions, capped to 2 variants.
    struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], DRAW2);
    Discard_add(&gs.discard[PLAYER_A], CHAMP_A);
    Discard_add(&gs.discard[PLAYER_A], CHAMP_B);
    Discard_add(&gs.discard[PLAYER_A], CHAMP_C);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "3 recallable, cap 2: exactly 2 MOVE_RECALL",
          2, count_type(moves, n, MOVE_RECALL));
  }

  { // choose_num == 2 (DRAW3), discard holds exactly 2 -> exactly 1 pair.
    struct gamestate gs = blank_state(PLAYER_A, 2, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], DRAW3);
    Discard_add(&gs.discard[PLAYER_A], CHAMP_A);
    Discard_add(&gs.discard[PLAYER_A], CHAMP_B);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "choose_num 2, discard holds 2: exactly 1 MOVE_RECALL",
          1, count_type(moves, n, MOVE_RECALL));
  }

  { // choose_num == 2, discard holds only 1 -> not enough to recall.
    struct gamestate gs = blank_state(PLAYER_A, 2, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], DRAW3);
    Discard_add(&gs.discard[PLAYER_A], CHAMP_A);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "choose_num 2, discard holds 1: 0 MOVE_RECALL",
          0, count_type(moves, n, MOVE_RECALL));
  }

  { // max_recall_variants == 0 disables recall outright.
    struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], DRAW2);
    Discard_add(&gs.discard[PLAYER_A], CHAMP_A);
    MoveGenLimits limits = { .max_recall_variants = 0, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "max_recall_variants=0: 0 MOVE_RECALL", 0, count_type(moves, n, MOVE_RECALL));
  }
} // test_recall_moves

void test_cash_moves(TestSuite* suite)
{ printf("\n=== CASH ===\n");

  GameMove moves[MOVE_GEN_MAX_MOVES];

  { // No champion in hand at all -- cash card can never be played.
    struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], CASH);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "no champion in hand: 0 MOVE_CASH", 0, count_type(moves, n, MOVE_CASH));
  }

  { // One champion: exactly one exchange target regardless of the cap.
    struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], CASH);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "1 champion: exactly 1 MOVE_CASH", 1, count_type(moves, n, MOVE_CASH));
  }

  { // Three champions (power 10/14/12), cap 2: lowest-power two (A, C) chosen.
    struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], CASH);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_C);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "3 champions, cap 2: exactly 2 MOVE_CASH", 2, count_type(moves, n, MOVE_CASH));

    int saw_a = 0, saw_c = 0, saw_b = 0;
    for(uint8_t i = 0; i < n; i++)
    { if(moves[i].type != MOVE_CASH) continue;
      if(moves[i].cards[0] == CHAMP_A) saw_a = 1;
      if(moves[i].cards[0] == CHAMP_C) saw_c = 1;
      if(moves[i].cards[0] == CHAMP_B) saw_b = 1;
    }
    check(suite, "targets are the 2 lowest-power champions (A and C)", 1, saw_a && saw_c && !saw_b);
  }

  { // max_cash_variants == 0 still yields exactly 1 (the default lowest-power target).
    struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
    Hand_add(&gs.hand[PLAYER_A], CASH);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
    Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
    MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 0 };
    uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
    check(suite, "max_cash_variants=0: exactly 1 MOVE_CASH", 1, count_type(moves, n, MOVE_CASH));
  }
} // test_cash_moves

void test_defense_phase(TestSuite* suite)
{ printf("\n=== DEFENSE PHASE: champions only, no draw/recall/cash ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 2, DEFENSE);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
  Hand_add(&gs.hand[PLAYER_A], DRAW2);
  Hand_add(&gs.hand[PLAYER_A], CASH);
  Discard_add(&gs.discard[PLAYER_A], CHAMP_C);
  MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
  GameMove moves[MOVE_GEN_MAX_MOVES];

  uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, MOVE_GEN_MAX_MOVES);
  // pass + {A} + {B} + {A,B} = 4
  check(suite, "4 total moves", 4, n);
  check(suite, "0 MOVE_DRAW", 0, count_type(moves, n, MOVE_DRAW));
  check(suite, "0 MOVE_RECALL", 0, count_type(moves, n, MOVE_RECALL));
  check(suite, "0 MOVE_CASH", 0, count_type(moves, n, MOVE_CASH));
} // test_defense_phase

void test_max_out_truncation(TestSuite* suite)
{ printf("\n=== max_out TRUNCATION ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_C);
  MoveGenLimits limits = { .max_recall_variants = 2, .max_cash_variants = 2 };
  GameMove moves[3];

  uint8_t n = get_available_moves(&gs, PLAYER_A, &limits, moves, 3);
  check(suite, "capped to max_out", 3, n);
  check(suite, "first entry is still MOVE_PASS", MOVE_PASS, moves[0].type);
} // test_max_out_truncation

void test_apply_move_pass(TestSuite* suite)
{ printf("\n=== APPLY: MOVE_PASS is a no-op ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 3, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  GameContext ctx = {0};
  GameMove m = { .type = MOVE_PASS };

  apply_move(&gs, PLAYER_A, &m, &ctx);
  check(suite, "hand untouched", 1, gs.hand[PLAYER_A].size);
  check(suite, "cash untouched", 3, gs.current_cash_balance[PLAYER_A]);
} // test_apply_move_pass

void test_apply_move_champions(TestSuite* suite)
{ printf("\n=== APPLY: MOVE_CHAMPIONS ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
  GameContext ctx = {0};
  GameMove m = { .type = MOVE_CHAMPIONS, .count = 2, .cards = {CHAMP_A, CHAMP_B} };

  apply_move(&gs, PLAYER_A, &m, &ctx);
  check(suite, "hand emptied", 0, gs.hand[PLAYER_A].size);
  check(suite, "combat zone holds both", 2, gs.combat_zone[PLAYER_A].size);
  check(suite, "cash unchanged (both cost 0)", 0, gs.current_cash_balance[PLAYER_A]);
} // test_apply_move_champions

void test_apply_move_draw(TestSuite* suite)
{ printf("\n=== APPLY: MOVE_DRAW ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], DRAW2);
  gs.deck[PLAYER_A].top = -1;
  DeckStk_push(&gs.deck[PLAYER_A], CHAMP_D);
  DeckStk_push(&gs.deck[PLAYER_A], CHAMP_A);
  GameContext ctx = {0}; // deck has enough cards -- no reshuffle, so no RNG use
  GameMove m = { .type = MOVE_DRAW, .card = DRAW2 };

  apply_move(&gs, PLAYER_A, &m, &ctx);
  check(suite, "drew draw_num (2) cards", 2, gs.hand[PLAYER_A].size);
  check(suite, "deck emptied", 1, DeckStk_isEmpty(&gs.deck[PLAYER_A]));
  check(suite, "draw card discarded", 1, gs.discard[PLAYER_A].size);
  check(suite, "cost paid", 0, gs.current_cash_balance[PLAYER_A]);
} // test_apply_move_draw

void test_apply_move_recall(TestSuite* suite)
{ printf("\n=== APPLY: MOVE_RECALL ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 1, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], DRAW2);
  Discard_add(&gs.discard[PLAYER_A], CHAMP_A);
  GameContext ctx = {0};
  GameMove m = { .type = MOVE_RECALL, .card = DRAW2, .count = 1, .recall = {CHAMP_A} };

  apply_move(&gs, PLAYER_A, &m, &ctx);
  check(suite, "champion recalled to hand", 1, Hand_contains(&gs.hand[PLAYER_A], CHAMP_A));
  check(suite, "draw card left hand", 0, Hand_contains(&gs.hand[PLAYER_A], DRAW2));
  check(suite, "discard holds only the draw card now", 1, gs.discard[PLAYER_A].size);
  check(suite, "cost paid", 0, gs.current_cash_balance[PLAYER_A]);
} // test_apply_move_recall

void test_apply_move_cash(TestSuite* suite)
{ printf("\n=== APPLY: MOVE_CASH ===\n");

  struct gamestate gs = blank_state(PLAYER_A, 0, ATTACK);
  Hand_add(&gs.hand[PLAYER_A], CASH);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  GameContext ctx = {0};
  GameMove m = { .type = MOVE_CASH, .card = CASH, .count = 1, .cards = {CHAMP_A} };

  apply_move(&gs, PLAYER_A, &m, &ctx);
  check(suite, "hand emptied", 0, gs.hand[PLAYER_A].size);
  check(suite, "cash and champion both in discard", 2, gs.discard[PLAYER_A].size);
  check(suite, "5 lunas received", 5, gs.current_cash_balance[PLAYER_A]);
} // test_apply_move_cash

// Sums every card index currently owned by either player (hand + discard +
// combat_zone + deck) into `counts` (indexed by fullDeck[] index) and
// returns the total. A valid state always totals 80 (40 per player, per
// setup_game()'s allocation) with every count 0 or 1 -- a duplicate or a
// dropped card shows up as a count >1 somewhere or a total != 80.
static uint16_t total_owned_cards(const struct gamestate* gs, int* counts)
{ uint16_t total = 0;

  for(int p = 0; p < 2; p++)
  { for(uint8_t i = 0; i < gs->hand[p].size; i++)
    { counts[gs->hand[p].cards[i]]++;
      total++;
    }
    for(uint8_t i = 0; i < gs->discard[p].size; i++)
    { counts[gs->discard[p].cards[i]]++;
      total++;
    }
    for(uint8_t i = 0; i < gs->combat_zone[p].size; i++)
    { counts[gs->combat_zone[p].cards[i]]++;
      total++;
    }
    for(int8_t i = 0; i <= gs->deck[p].top; i++)
    { counts[gs->deck[p].card_indices[i]]++;
      total++;
    }
  }
  return total;
} // total_owned_cards

void test_mc_fork_context_independent(TestSuite* suite)
{ printf("\n=== mc_fork_context: parent RNG untouched ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 42;
  GameContext* ctx = create_game_context(&cfg);
  MTRand before = ctx->rng;

  GameContext sim_ctx = mc_fork_context(ctx, 999);
  for(int i = 0; i < 10; i++) genRandLong(&sim_ctx.rng);

  check(suite, "parent ctx->rng byte-identical after fork use",
        1, memcmp(&before, &ctx->rng, sizeof(MTRand)) == 0);

  destroy_game_context(ctx);
} // test_mc_fork_context_independent

void test_mc_determinize_invariants(TestSuite* suite)
{ printf("\n=== mc_determinize: conservation invariants ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 777;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate sim = {0};
  setup_game(INITIAL_CASH_DEFAULT, &sim, ctx);

  Hand hand_a_before = sim.hand[PLAYER_A];
  Discard discard_a_before = sim.discard[PLAYER_A];
  Discard discard_b_before = sim.discard[PLAYER_B];
  CombatZone cz_a_before = sim.combat_zone[PLAYER_A];
  CombatZone cz_b_before = sim.combat_zone[PLAYER_B];
  uint8_t hand_b_size_before = sim.hand[PLAYER_B].size;
  uint8_t deck_a_size_before = (uint8_t)(sim.deck[PLAYER_A].top + 1);
  uint8_t deck_b_size_before = (uint8_t)(sim.deck[PLAYER_B].top + 1);

  int counts_before[FULL_DECK_SIZE] = {0};
  uint16_t total_before = total_owned_cards(&sim, counts_before);

  GameContext sim_ctx = mc_fork_context(ctx, 12345);
  mc_determinize(&sim, PLAYER_A, &sim_ctx);

  check(suite, "observer's own hand unchanged",
        1, memcmp(&sim.hand[PLAYER_A], &hand_a_before, sizeof(Hand)) == 0);
  check(suite, "discard A unchanged",
        1, memcmp(&sim.discard[PLAYER_A], &discard_a_before, sizeof(Discard)) == 0);
  check(suite, "discard B unchanged",
        1, memcmp(&sim.discard[PLAYER_B], &discard_b_before, sizeof(Discard)) == 0);
  check(suite, "combat zone A unchanged",
        1, memcmp(&sim.combat_zone[PLAYER_A], &cz_a_before, sizeof(CombatZone)) == 0);
  check(suite, "combat zone B unchanged",
        1, memcmp(&sim.combat_zone[PLAYER_B], &cz_b_before, sizeof(CombatZone)) == 0);

  check(suite, "opponent hand size preserved", hand_b_size_before, sim.hand[PLAYER_B].size);
  check(suite, "observer deck size preserved",
        deck_a_size_before, (uint8_t)(sim.deck[PLAYER_A].top + 1));
  check(suite, "opponent deck size preserved",
        deck_b_size_before, (uint8_t)(sim.deck[PLAYER_B].top + 1));

  int counts_after[FULL_DECK_SIZE] = {0};
  uint16_t total_after = total_owned_cards(&sim, counts_after);
  check(suite, "total owned cards conserved (80)", total_before, total_after);

  int max_count = 0;
  for(int i = 0; i < FULL_DECK_SIZE; i++)
    if(counts_after[i] > max_count) max_count = counts_after[i];
  check(suite, "no card duplicated across zones", 1, max_count <= 1);

  destroy_game_context(ctx);
} // test_mc_determinize_invariants

void test_mc_playout_isolated(TestSuite* suite)
{ printf("\n=== mc_playout: root untouched, parent RNG untouched, valid result ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 555;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx); // turn 1, PLAYER_A -- no draw, so no RNG use here

  struct gamestate gs_before = gs;
  MTRand ctx_rng_before = ctx->rng;

  GameContext sim_ctx = mc_fork_context(ctx, 4242);
  GameMove first = { .type = MOVE_PASS };
  float result = mc_playout(&gs, PLAYER_A, &first, &sim_ctx, MAX_NUMBER_OF_TURNS);

  check(suite, "result is 0.0, 0.5, or 1.0",
        1, result == 0.0f || result == 0.5f || result == 1.0f);
  check(suite, "root gamestate byte-identical after playout",
        1, memcmp(&gs, &gs_before, sizeof(struct gamestate)) == 0);
  check(suite, "parent ctx->rng byte-identical after playout",
        1, memcmp(&ctx_rng_before, &ctx->rng, sizeof(MTRand)) == 0);

  destroy_game_context(ctx);
} // test_mc_playout_isolated

void test_mc_playout_stress(TestSuite* suite)
{ printf("\n=== mc_playout: stress (200 playouts, no crash, mixed outcomes) ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 1;
  GameContext* ctx = create_game_context(&cfg);

  int wins = 0, losses = 0, draws = 0;
  for(int i = 0; i < 200; i++)
  { struct gamestate gs = {0};
    setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
    begin_of_turn(&gs, ctx);

    GameContext sim_ctx = mc_fork_context(ctx, (uint32_t)(1000 + i));
    GameMove first = { .type = MOVE_PASS };
    float result = mc_playout(&gs, PLAYER_A, &first, &sim_ctx, MAX_NUMBER_OF_TURNS);

    if(result == 1.0f) wins++;
    else if(result == 0.0f) losses++;
    else draws++;
  }

  check(suite, "no crash across 200 playouts", 200, wins + losses + draws);
  check(suite, "both outcomes occur (not degenerate)", 1, wins > 0 && losses > 0);

  destroy_game_context(ctx);
} // test_mc_playout_stress

int main(void)
{ TestSuite suite = {"Move Generation Tests", 0, 0};

  printf("\n=== ORACLE MOVE GENERATION TEST SUITE ===\n");

  test_empty_hand(&suite);
  test_champion_subsets(&suite);
  test_champion_affordability(&suite);
  test_draw_moves(&suite);
  test_recall_moves(&suite);
  test_cash_moves(&suite);
  test_defense_phase(&suite);
  test_max_out_truncation(&suite);
  test_apply_move_pass(&suite);
  test_apply_move_champions(&suite);
  test_apply_move_draw(&suite);
  test_apply_move_recall(&suite);
  test_apply_move_cash(&suite);
  test_mc_fork_context_independent(&suite);
  test_mc_determinize_invariants(&suite);
  test_mc_playout_isolated(&suite);
  test_mc_playout_stress(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
