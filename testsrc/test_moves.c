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
#include "../src/ai_strat/ai_strat_hbt.h"
#include "../src/ai_strat/ai_strat_hbt_enum.h"
#include "../src/ai_strat/ai_strat_hbt2ply_reply.h"
#include "../src/core/game_state.h"
#include "../src/core/game_constants.h"
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
// Cost-3 champion, expected_attack 15.5 / expected_defense 10.5 -- a real
// threat, unlike CHAMP_A-D's cost-0 pokes (max expected_attack 3.5), used
// by A9 HBT 2-Ply's tests below where a rational defender must actually
// want to block for the ply to have anything to bite on.
#define CHAMP_STRONG 33

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
  StrategySet rollout_strats = {0};
  set_player_strategy_by_type(&rollout_strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout_strats, PLAYER_B, AI_STRATEGY_RANDOM);
  float result = mc_playout(&gs, PLAYER_A, &first, &rollout_strats, &sim_ctx, MAX_NUMBER_OF_TURNS);

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

  StrategySet rollout_strats = {0};
  set_player_strategy_by_type(&rollout_strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout_strats, PLAYER_B, AI_STRATEGY_RANDOM);

  int wins = 0, losses = 0, draws = 0;
  for(int i = 0; i < 200; i++)
  { struct gamestate gs = {0};
    setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
    begin_of_turn(&gs, ctx);

    GameContext sim_ctx = mc_fork_context(ctx, (uint32_t)(1000 + i));
    GameMove first = { .type = MOVE_PASS };
    float result = mc_playout(&gs, PLAYER_A, &first, &rollout_strats, &sim_ctx, MAX_NUMBER_OF_TURNS);

    if(result == 1.0f) wins++;
    else if(result == 0.0f) losses++;
    else draws++;
  }

  check(suite, "no crash across 200 playouts", 200, wins + losses + draws);
  check(suite, "both outcomes occur (not degenerate)", 1, wins > 0 && losses > 0);

  destroy_game_context(ctx);
} // test_mc_playout_stress

void test_mc_playout_from_isolated(TestSuite* suite)
{ printf("\n=== mc_playout_from: root untouched, parent RNG untouched, valid result ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 555;
  GameContext* ctx = create_game_context(&cfg);

  struct gamestate gs = {0};
  setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
  begin_of_turn(&gs, ctx); // turn 1, PLAYER_A -- no draw, so no RNG use here

  struct gamestate gs_before = gs;
  MTRand ctx_rng_before = ctx->rng;

  GameContext sim_ctx = mc_fork_context(ctx, 4242);
  StrategySet rollout_strats = {0};
  set_player_strategy_by_type(&rollout_strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout_strats, PLAYER_B, AI_STRATEGY_RANDOM);
  float result = mc_playout_from(&gs, PLAYER_A, &rollout_strats, &sim_ctx, MAX_NUMBER_OF_TURNS);

  check(suite, "result is 0.0, 0.5, or 1.0",
        1, result == 0.0f || result == 0.5f || result == 1.0f);
  check(suite, "root gamestate byte-identical after playout",
        1, memcmp(&gs, &gs_before, sizeof(struct gamestate)) == 0);
  check(suite, "parent ctx->rng byte-identical after playout",
        1, memcmp(&ctx_rng_before, &ctx->rng, sizeof(MTRand)) == 0);

  destroy_game_context(ctx);
} // test_mc_playout_from_isolated

void test_mc_playout_from_stress(TestSuite* suite)
{ printf("\n=== mc_playout_from: stress (200 playouts, no crash, mixed outcomes) ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 2;
  GameContext* ctx = create_game_context(&cfg);

  StrategySet rollout_strats = {0};
  set_player_strategy_by_type(&rollout_strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout_strats, PLAYER_B, AI_STRATEGY_RANDOM);

  int wins = 0, losses = 0, draws = 0;
  for(int i = 0; i < 200; i++)
  { struct gamestate gs = {0};
    setup_game(INITIAL_CASH_DEFAULT, &gs, ctx);
    begin_of_turn(&gs, ctx);

    GameContext sim_ctx = mc_fork_context(ctx, (uint32_t)(2000 + i));
    float result = mc_playout_from(&gs, PLAYER_A, &rollout_strats, &sim_ctx, MAX_NUMBER_OF_TURNS);

    if(result == 1.0f) wins++;
    else if(result == 0.0f) losses++;
    else draws++;
  }

  check(suite, "no crash across 200 playouts", 200, wins + losses + draws);
  check(suite, "both outcomes occur (not degenerate)", 1, wins > 0 && losses > 0);

  destroy_game_context(ctx);
} // test_mc_playout_from_stress

// Fixed moves for the scripted attack/defense strategies below, read by
// name rather than passed as a closure -- C function pointers can't capture
// state, and this is test-only code driving a StrategySet the same way
// every real attack_strategy/defense_strategy does (gstate->current_player
// / 1 - gstate->current_player, see ai_strat_random.c).
static GameMove g_scripted_attack_move;
static GameMove g_scripted_defense_move;

static void scripted_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ apply_move(gstate, gstate->current_player, &g_scripted_attack_move, ctx);
} // scripted_attack_strategy

static void scripted_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ apply_move(gstate, 1 - gstate->current_player, &g_scripted_defense_move, ctx);
} // scripted_defense_strategy

void test_mc_advance_to_decision_matches_play_turn(TestSuite* suite)
{ printf("\n=== mc_advance_to_decision: matches a real play_turn()+begin_of_turn() "
           "sequence bit-for-bit ===\n");

  struct gamestate base = {0};
  Hand_init(&base.hand[PLAYER_A]);
  Hand_init(&base.hand[PLAYER_B]);
  Discard_init(&base.discard[PLAYER_A]);
  Discard_init(&base.discard[PLAYER_B]);
  CombatZone_init(&base.combat_zone[PLAYER_A]);
  CombatZone_init(&base.combat_zone[PLAYER_B]);
  base.deck[PLAYER_A].top = -1;
  base.deck[PLAYER_B].top = -1;
  DeckStk_push(&base.deck[PLAYER_A], 10);
  DeckStk_push(&base.deck[PLAYER_A], 11);
  DeckStk_push(&base.deck[PLAYER_A], 12);
  DeckStk_push(&base.deck[PLAYER_B], 13);
  DeckStk_push(&base.deck[PLAYER_B], 14);
  DeckStk_push(&base.deck[PLAYER_B], 15);
  Hand_add(&base.hand[PLAYER_A], CHAMP_A);
  base.current_cash_balance[PLAYER_A] = 10;
  base.current_cash_balance[PLAYER_B] = 10;
  base.current_energy[PLAYER_A] = 99;
  base.current_energy[PLAYER_B] = 99;
  base.current_player = PLAYER_A;
  base.turn = 0;

  g_scripted_attack_move = (GameMove)
  { .type = MOVE_CHAMPIONS, .count = 1, .cards = {CHAMP_A}
  };
  g_scripted_defense_move = (GameMove)
  { .type = MOVE_PASS
  };

  StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_RANDOM);
  strats.attack_strategy[PLAYER_A] = scripted_attack_strategy;
  strats.defense_strategy[PLAYER_B] = scripted_defense_strategy;

  config_t cfg = {0};
  cfg.prng_seed = 8080;
  GameContext* ctx = create_game_context(&cfg);

  // Reference: a real play_turn() for turn 1, then begin_of_turn() to reach
  // turn 2's pending attack decision without yet making it.
  struct gamestate sim_ref = base;
  GameContext ref_ctx = mc_fork_context(ctx, 8080);
  play_turn(NULL, &sim_ref, &strats, &ref_ctx);
  begin_of_turn(&sim_ref, &ref_ctx);

  // Tree path: the same two decisions applied via mc_advance_to_decision(),
  // same seed so dice rolls/draws line up.
  struct gamestate sim_tree = base;
  GameContext tree_ctx = mc_fork_context(ctx, 8080);
  begin_of_turn(&sim_tree, &tree_ctx);
  bool cont1 = mc_advance_to_decision(&sim_tree, PLAYER_A, &g_scripted_attack_move,
                                      &strats, &tree_ctx);
  check(suite, "after attack: game still in progress", 1, cont1);
  check(suite, "after attack: turn_phase is DEFENSE", DEFENSE, sim_tree.turn_phase);
  bool cont2 = mc_advance_to_decision(&sim_tree, PLAYER_B, &g_scripted_defense_move,
                                      &strats, &tree_ctx);
  check(suite, "after defense: game still in progress", 1, cont2);
  check(suite, "after defense: turn_phase is ATTACK (next turn began)",
        ATTACK, sim_tree.turn_phase);

  check(suite, "final gamestate matches play_turn()+begin_of_turn() bit-for-bit",
        1, memcmp(&sim_ref, &sim_tree, sizeof(struct gamestate)) == 0);

  destroy_game_context(ctx);
} // test_mc_advance_to_decision_matches_play_turn

void test_mc_advance_to_decision_conserves_cards(TestSuite* suite)
{ printf("\n=== mc_advance_to_decision: card conservation across a full turn ===\n");

  struct gamestate sim = {0};
  Hand_init(&sim.hand[PLAYER_A]);
  Hand_init(&sim.hand[PLAYER_B]);
  Discard_init(&sim.discard[PLAYER_A]);
  Discard_init(&sim.discard[PLAYER_B]);
  CombatZone_init(&sim.combat_zone[PLAYER_A]);
  CombatZone_init(&sim.combat_zone[PLAYER_B]);
  sim.deck[PLAYER_A].top = -1;
  sim.deck[PLAYER_B].top = -1;
  DeckStk_push(&sim.deck[PLAYER_A], 10);
  DeckStk_push(&sim.deck[PLAYER_B], 13);
  Hand_add(&sim.hand[PLAYER_A], CHAMP_A);
  sim.current_cash_balance[PLAYER_A] = 10;
  sim.current_energy[PLAYER_A] = 99;
  sim.current_energy[PLAYER_B] = 99;
  sim.current_player = PLAYER_A;
  sim.turn = 0;

  int counts_before[FULL_DECK_SIZE] = {0};
  uint16_t total_before = total_owned_cards(&sim, counts_before);

  g_scripted_attack_move = (GameMove)
  { .type = MOVE_CHAMPIONS, .count = 1, .cards = {CHAMP_A}
  };
  g_scripted_defense_move = (GameMove)
  { .type = MOVE_PASS
  };

  StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_RANDOM);

  config_t cfg = {0};
  cfg.prng_seed = 999;
  GameContext* ctx = create_game_context(&cfg);
  GameContext sim_ctx = mc_fork_context(ctx, 4321);

  begin_of_turn(&sim, &sim_ctx);
  mc_advance_to_decision(&sim, PLAYER_A, &g_scripted_attack_move, &strats, &sim_ctx);
  mc_advance_to_decision(&sim, PLAYER_B, &g_scripted_defense_move, &strats, &sim_ctx);

  int counts_after[FULL_DECK_SIZE] = {0};
  uint16_t total_after = total_owned_cards(&sim, counts_after);
  check(suite, "total owned cards conserved", total_before, total_after);

  int max_count = 0;
  for(int i = 0; i < FULL_DECK_SIZE; i++)
    if(counts_after[i] > max_count) max_count = counts_after[i];
  check(suite, "no card duplicated across zones", 1, max_count <= 1);

  destroy_game_context(ctx);
} // test_mc_advance_to_decision_conserves_cards

void test_mc_advance_to_decision_parent_rng_untouched(TestSuite* suite)
{ printf("\n=== mc_advance_to_decision: parent ctx->rng untouched ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 321;
  GameContext* ctx = create_game_context(&cfg);
  MTRand before = ctx->rng;

  struct gamestate sim = {0};
  Hand_init(&sim.hand[PLAYER_A]);
  Hand_init(&sim.hand[PLAYER_B]);
  Discard_init(&sim.discard[PLAYER_A]);
  Discard_init(&sim.discard[PLAYER_B]);
  CombatZone_init(&sim.combat_zone[PLAYER_A]);
  CombatZone_init(&sim.combat_zone[PLAYER_B]);
  sim.deck[PLAYER_A].top = -1;
  sim.deck[PLAYER_B].top = -1;
  DeckStk_push(&sim.deck[PLAYER_A], 10);
  DeckStk_push(&sim.deck[PLAYER_B], 13);
  Hand_add(&sim.hand[PLAYER_A], CHAMP_A);
  sim.current_energy[PLAYER_A] = 99;
  sim.current_energy[PLAYER_B] = 99;
  sim.current_player = PLAYER_A;

  g_scripted_attack_move = (GameMove)
  { .type = MOVE_CHAMPIONS, .count = 1, .cards = {CHAMP_A}
  };
  g_scripted_defense_move = (GameMove)
  { .type = MOVE_PASS
  };

  StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_RANDOM);

  GameContext sim_ctx = mc_fork_context(ctx, 1111);
  begin_of_turn(&sim, &sim_ctx);
  mc_advance_to_decision(&sim, PLAYER_A, &g_scripted_attack_move, &strats, &sim_ctx);
  mc_advance_to_decision(&sim, PLAYER_B, &g_scripted_defense_move, &strats, &sim_ctx);

  check(suite, "parent ctx->rng byte-identical after fork use",
        1, memcmp(&before, &ctx->rng, sizeof(MTRand)) == 0);

  destroy_game_context(ctx);
} // test_mc_advance_to_decision_parent_rng_untouched

void test_mc_advance_to_decision_game_over(TestSuite* suite)
{ printf("\n=== mc_advance_to_decision: returns false when the game ends ===\n");

  struct gamestate sim = {0};
  Hand_init(&sim.hand[PLAYER_A]);
  Hand_init(&sim.hand[PLAYER_B]);
  Discard_init(&sim.discard[PLAYER_A]);
  Discard_init(&sim.discard[PLAYER_B]);
  CombatZone_init(&sim.combat_zone[PLAYER_A]);
  CombatZone_init(&sim.combat_zone[PLAYER_B]);
  sim.deck[PLAYER_A].top = -1;
  sim.deck[PLAYER_B].top = -1;
  Hand_add(&sim.hand[PLAYER_A], CHAMP_STRONG); // exp_atk 15.5 -- plenty to finish 1 energy off
  sim.current_energy[PLAYER_A] = 99;
  sim.current_energy[PLAYER_B] = 1; // any positive attack roll ends the game
  sim.current_player = PLAYER_A;
  sim.turn_phase = ATTACK;

  StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_RANDOM);

  config_t cfg = {0};
  cfg.prng_seed = 55;
  GameContext* ctx = create_game_context(&cfg);
  GameContext sim_ctx = mc_fork_context(ctx, 66);

  GameMove attack = { .type = MOVE_CHAMPIONS, .count = 1, .cards = {CHAMP_STRONG} };
  bool cont1 = mc_advance_to_decision(&sim, PLAYER_A, &attack, &strats, &sim_ctx);
  check(suite, "after attack: defender must still decide", 1, cont1);

  GameMove decline = { .type = MOVE_PASS };
  bool cont2 = mc_advance_to_decision(&sim, PLAYER_B, &decline, &strats, &sim_ctx);
  check(suite, "after undefended lethal attack: mc_advance_to_decision returns false",
        0, cont2);
  check(suite, "someone_has_zero_energy is set", 1, sim.someone_has_zero_energy);

  destroy_game_context(ctx);
} // test_mc_advance_to_decision_game_over

/* ========================================================================
   A9 HBT 2-Ply: hbt2ply_score_attack_subset() -- the central regression
   guard is that reply_trust == 0 recovers A7's own decision exactly (see
   ai_strat_hbt2ply.h). Hand-crafted states, same style as the rest of this
   file, rather than setup_game()'s shuffled deal.
   ======================================================================== */

static struct gamestate hbt_test_state(uint8_t own_energy, uint8_t opp_energy, uint16_t own_cash)
{ struct gamestate gs = {0};
  Hand_init(&gs.hand[PLAYER_A]);
  Hand_init(&gs.hand[PLAYER_B]);
  Discard_init(&gs.discard[PLAYER_A]);
  Discard_init(&gs.discard[PLAYER_B]);
  CombatZone_init(&gs.combat_zone[PLAYER_A]);
  CombatZone_init(&gs.combat_zone[PLAYER_B]);
  gs.current_energy[PLAYER_A] = own_energy;
  gs.current_energy[PLAYER_B] = opp_energy;
  gs.current_cash_balance[PLAYER_A] = own_cash;
  gs.turn_phase = ATTACK;
  return gs;
} // hbt_test_state

// A7's own one-ply score for this exact subset, recomputed independently
// via the same shared functions A7's (static) evaluate_attack_subset()
// calls internally -- this is the ground truth reply_trust == 0 must match
// bit-for-bit.
static float a7_reference_score(const struct gamestate* gs, const uint8_t* cards,
                                uint8_t count, const HBTParams* base, const HBTState* state)
{ float opp_energy = (float)gs->current_energy[PLAYER_B];
  float dmg = predicted_damage(cards, count, opp_energy);
  uint16_t cost = 0;
  for(uint8_t i = 0; i < count; i++) cost += fullDeck[cards[i]].cost;

  return hbt_advantage((float)gs->current_energy[PLAYER_A], opp_energy - dmg,
                       (float)gs->hand[PLAYER_A].size - (float)count,
                       (float)gs->hand[PLAYER_B].size,
                       (float)gs->current_cash_balance[PLAYER_A] - (float)cost,
                       (float)gs->current_cash_balance[PLAYER_B], base, state);
} // a7_reference_score

void test_hbt2ply_reply_trust_zero_matches_a7(TestSuite* suite)
{ printf("\n=== A9 HBT 2-Ply: reply_trust=0 recovers A7 exactly ===\n");

  struct gamestate gs = hbt_test_state(60, 40, 20);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_D);
  Hand_add(&gs.hand[PLAYER_B], CHAMP_C); // opponent's real hand -- unread at
  // reply_trust == 0 (only .size is), stands in for hidden information

  HBTParams base = hbt_get_default_params();
  HBTState state = hbt_evaluate_state(&gs, PLAYER_A, &base);
  HBT2PlyParams params = { .base = base, .reply_trust = 0.0f,
                           .surrogate_pessimism = 1.0f, .ply_energy_ceiling = 99, .ply_beam_width = 0
                         };

  uint8_t subset[2] = { CHAMP_A, CHAMP_B };
  float a9_score = 0.0f;
  bool applicable = hbt2ply_score_attack_subset(&gs, PLAYER_A, subset, 2,
                                                &params, &state, &a9_score);
  check(suite, "subset is applicable (affordable, not a held combo)", 1, applicable);

  float a7_score = a7_reference_score(&gs, subset, 2, &base, &state);
  check(suite, "reply_trust=0 score matches A7's one-ply score bit-for-bit",
        1, a9_score == a7_score);
} // test_hbt2ply_reply_trust_zero_matches_a7

void test_hbt2ply_forced_decline_matches_a7(TestSuite* suite)
{ printf("\n=== A9 HBT 2-Ply: reply_trust=1 with a cashless opponent still "
           "matches A7 (their only legal reply is to decline) ===\n");

  struct gamestate gs = hbt_test_state(60, 40, 20);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_A);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_B);
  Hand_add(&gs.hand[PLAYER_B], CHAMP_C);
  gs.current_cash_balance[PLAYER_B] = 0; // no champion is affordable to block with

  HBTParams base = hbt_get_default_params();
  HBTState state = hbt_evaluate_state(&gs, PLAYER_A, &base);
  HBT2PlyParams params = { .base = base, .reply_trust = 1.0f,
                           .surrogate_pessimism = 1.0f, .ply_energy_ceiling = 99, .ply_beam_width = 0
                         };

  uint8_t subset[1] = { CHAMP_A };
  float a9_score = 0.0f;
  bool applicable = hbt2ply_score_attack_subset(&gs, PLAYER_A, subset, 1,
                                                &params, &state, &a9_score);
  check(suite, "subset is applicable", 1, applicable);

  float a7_score = a7_reference_score(&gs, subset, 1, &base, &state);
  check(suite, "forced decline degrades to A7's undefended score bit-for-bit",
        1, a9_score == a7_score);
} // test_hbt2ply_forced_decline_matches_a7

void test_hbt2ply_ply_changes_score_when_reply_possible(TestSuite* suite)
{ printf("\n=== A9 HBT 2-Ply: the ply is alive (changes the score) when the "
           "opponent can plausibly block ===\n");

  struct gamestate gs = hbt_test_state(60, 40, 20);
  Hand_add(&gs.hand[PLAYER_A], CHAMP_STRONG); // exp_atk 15.5 -- a real threat
  for(uint8_t i = 0; i < 5; i++) Hand_add(&gs.hand[PLAYER_B], CHAMP_C); // size only
  gs.current_cash_balance[PLAYER_B] = 50; // plenty to field a blocker

  HBTParams base = hbt_get_default_params();
  HBTState state = hbt_evaluate_state(&gs, PLAYER_A, &base);
  HBT2PlyParams params = { .base = base, .reply_trust = 1.0f,
                           .surrogate_pessimism = 1.0f, .ply_energy_ceiling = 99, .ply_beam_width = 0
                         };

  uint8_t subset[1] = { CHAMP_STRONG };
  float a9_score = 0.0f;
  bool applicable = hbt2ply_score_attack_subset(&gs, PLAYER_A, subset, 1,
                                                &params, &state, &a9_score);
  check(suite, "subset is applicable", 1, applicable);

  float a7_score = a7_reference_score(&gs, subset, 1, &base, &state);
  check(suite, "two-ply score differs from A7's undefended score "
               "(the ply is not a no-op)", 1, a9_score != a7_score);
} // test_hbt2ply_ply_changes_score_when_reply_possible

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
  test_mc_playout_from_isolated(&suite);
  test_mc_playout_from_stress(&suite);
  test_mc_advance_to_decision_matches_play_turn(&suite);
  test_mc_advance_to_decision_conserves_cards(&suite);
  test_mc_advance_to_decision_parent_rng_untouched(&suite);
  test_mc_advance_to_decision_game_over(&suite);
  test_hbt2ply_reply_trust_zero_matches_a7(&suite);
  test_hbt2ply_forced_decline_matches_a7(&suite);
  test_hbt2ply_ply_changes_score_when_reply_possible(&suite);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);

  return suite.failed > 0 ? 1 : 0;
}
