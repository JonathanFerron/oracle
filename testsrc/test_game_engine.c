// test_game_engine.c
// Test suite for src/core/game_engine.c -- GUI step 5
// (ideas/9 gui/gui_architecture_synthesis.md section 7). Two kinds of
// coverage: (1) a full AI-vs-AI game driven entirely through the engine
// must match play_stda_auto_game()'s own primitives (setup_game() +
// mulligan + play_turn() loop + the MAX_NUMBER_OF_TURNS cap) bit-for-bit,
// the same guarantee the -a -p regression check gives the real binary;
// (2) hand-built GameEngine states exercising engine_submit()'s legality
// gate, the DISCARD_WAIT skip/trigger, the no-combat-when-attacker-passes
// skip, and event emission/redaction, same hand-crafted style as
// test_moves.c.

#include <stdio.h>
#include <string.h>

#include "../src/core/game_engine.h"
#include "../src/core/game_state.h"
#include "../src/core/turn_logic.h"
#include "../src/core/game_constants.h"
#include "../src/ai_strat/ai_strat_playout.h" // mc_fork_context

#define TEST_PASS "\033[32m\xe2\x9c\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xe2\x9c\x97 FAIL\033[0m"

#define CHAMP_A 0
#define CHAMP_B 1
#define DRAW2   102

typedef struct
{ int passed;
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

/* ========================================================================
   Full AI-vs-AI game: engine must match play_stda_auto_game()'s own
   primitives bit-for-bit, same seed.
   ======================================================================== */

// Zero-initialized before setup_game() -- struct gamestate has padding
// (e.g. before the uint16_t turn field), and since both this and
// run_engine_game() below write the exact same fields in the exact same
// order (that's what's being verified), starting both from an identical
// zeroed state keeps any padding bytes identical too; comparing two
// uninitialized locals via memcmp() would otherwise compare stack garbage.
static struct gamestate run_reference_game(GameContext* ctx, StrategySet* strats)
{ struct gamestate ref = {0};
  setup_game(INITIAL_CASH_DEFAULT, &ref, ctx);
  ref.turn = 0;
  ref.turn_phase = ATTACK;
  ref.player_to_move = ref.current_player;

  strats->mulligan_strategy[PLAYER_B](&ref, PLAYER_B, ctx);

  do
  { play_turn(NULL, &ref, strats, ctx);
  }
  while(ref.turn < MAX_NUMBER_OF_TURNS && !ref.someone_has_zero_energy);

  if(!ref.someone_has_zero_energy)
    ref.game_state = DRAW;
  return ref;
} // run_reference_game

static struct gamestate run_engine_game(GameContext* ctx, StrategySet* strats)
{ GameEngine e = {0};
  EventBuf events = {0};
  engine_init(&e, INITIAL_CASH_DEFAULT, ctx, &events);

  for(;;)
  { events.count = 0;
    PendingDecision pending = engine_advance(&e, ctx, &events);
    if(pending.kind == DECISION_KIND_NONE) break;
    engine_run_ai(&e, strats, ctx, &events);
  }
  return e.state;
} // run_engine_game

static void test_engine_matches_reference_full_game(TestSuite* suite)
{ printf("\n=== full AI-vs-AI game: engine matches play_stda_auto_game()'s "
           "primitives bit-for-bit ===\n");

  StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_RANDOM);

  config_t cfg = {0};
  cfg.prng_seed = 4242;
  GameContext* ctx = create_game_context(&cfg);

  GameContext ref_ctx = mc_fork_context(ctx, 777);
  struct gamestate ref = run_reference_game(&ref_ctx, &strats);

  GameContext eng_ctx = mc_fork_context(ctx, 777);
  struct gamestate eng = run_engine_game(&eng_ctx, &strats);

  check(suite, "final gamestate matches bit-for-bit",
        memcmp(&ref, &eng, sizeof(struct gamestate)) == 0);
  check(suite, "sanity: game actually ended (not both 0-turn no-ops)", ref.turn > 0);

  destroy_game_context(ctx);
} // test_engine_matches_reference_full_game

static void test_engine_matches_reference_several_seeds(TestSuite* suite)
{ printf("\n=== same check across several seeds ===\n");

  StrategySet strats = {0};
  set_player_strategy_by_type(&strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&strats, PLAYER_B, AI_STRATEGY_RANDOM);

  config_t cfg = {0};
  cfg.prng_seed = 99;
  GameContext* ctx = create_game_context(&cfg);

  int all_matched = 1;
  for(uint32_t seed = 1; seed <= 20; seed++)
  { GameContext ref_ctx = mc_fork_context(ctx, seed);
    struct gamestate ref = run_reference_game(&ref_ctx, &strats);

    GameContext eng_ctx = mc_fork_context(ctx, seed);
    struct gamestate eng = run_engine_game(&eng_ctx, &strats);

    if(memcmp(&ref, &eng, sizeof(struct gamestate)) != 0) all_matched = 0;
  }
  check(suite, "20/20 seeds match bit-for-bit", all_matched);

  destroy_game_context(ctx);
} // test_engine_matches_reference_several_seeds

/* ========================================================================
   Hand-built states: engine_submit() legality gate, DISCARD_WAIT,
   no-combat-on-pass, events.
   ======================================================================== */

static void init_blank(struct gamestate* gs)
{ *gs = (struct gamestate)
  { 0
  };
  Hand_init(&gs->hand[PLAYER_A]);
  Hand_init(&gs->hand[PLAYER_B]);
  Discard_init(&gs->discard[PLAYER_A]);
  Discard_init(&gs->discard[PLAYER_B]);
  CombatZone_init(&gs->combat_zone[PLAYER_A]);
  CombatZone_init(&gs->combat_zone[PLAYER_B]);
  gs->deck[PLAYER_A].top = -1;
  gs->deck[PLAYER_B].top = -1;
  gs->current_energy[PLAYER_A] = 99;
  gs->current_energy[PLAYER_B] = 99;
} // init_blank

static void test_submit_rejects_wrong_player(TestSuite* suite)
{ printf("\n=== engine_submit: wrong player rejected, state untouched ===\n");

  GameEngine e = {0};
  init_blank(&e.state);
  Hand_add(&e.state.hand[PLAYER_A], CHAMP_A);
  e.state.current_player = PLAYER_A;
  e.state.player_to_move = PLAYER_A;
  e.state.turn_phase = ATTACK;
  e.phase = ENG_ATTACK_WAIT;
  e.pending = (PendingDecision)
  { .kind = DECISION_KIND_ATTACK, .player = PLAYER_A
  };

  struct gamestate before = e.state;
  PlayerDecision d = { .kind = DECISION_KIND_ATTACK,
                       .move = { .type = MOVE_CHAMPIONS, .count = 1, .cards = {CHAMP_A} }
                     };

  config_t cfg = {0};
  GameContext* ctx = create_game_context(&cfg);
  EventBuf events = {0};

  bool ok = engine_submit(&e, PLAYER_B, &d, ctx, &events);
  check(suite, "rejected (not the pending player)", !ok);
  check(suite, "state untouched", memcmp(&before, &e.state, sizeof(struct gamestate)) == 0);
  check(suite, "no events pushed", events.count == 0);

  destroy_game_context(ctx);
} // test_submit_rejects_wrong_player

static void test_submit_rejects_illegal_move(TestSuite* suite)
{ printf("\n=== engine_submit: illegal move rejected via decision_is_legal() ===\n");

  GameEngine e = {0};
  init_blank(&e.state);
  Hand_add(&e.state.hand[PLAYER_A], CHAMP_A); // cost 0
  e.state.current_cash_balance[PLAYER_A] = 0;
  e.state.current_player = PLAYER_A;
  e.state.player_to_move = PLAYER_A;
  e.state.turn_phase = ATTACK;
  e.phase = ENG_ATTACK_WAIT;
  e.pending = (PendingDecision)
  { .kind = DECISION_KIND_ATTACK, .player = PLAYER_A
  };

  // Card 5 is never in hand -- decision_is_legal() must reject it.
  PlayerDecision d = { .kind = DECISION_KIND_ATTACK,
                       .move = { .type = MOVE_CHAMPIONS, .count = 1, .cards = {5} }
                     };

  config_t cfg = {0};
  GameContext* ctx = create_game_context(&cfg);
  EventBuf events = {0};

  bool ok = engine_submit(&e, PLAYER_A, &d, ctx, &events);
  check(suite, "rejected", !ok);
  check(suite, "still waiting on the same ATTACK decision", e.phase == ENG_ATTACK_WAIT);

  destroy_game_context(ctx);
} // test_submit_rejects_illegal_move

static void test_attack_pass_skips_defense_and_combat(TestSuite* suite)
{ printf("\n=== ATTACK: MOVE_PASS skips DEFENSE_WAIT/COMBAT entirely ===\n");

  GameEngine e = {0};
  init_blank(&e.state);
  DeckStk_push(&e.state.deck[PLAYER_B], CHAMP_B); // so the next BEGIN_TURN (B's) can draw
  e.state.current_player = PLAYER_A;
  e.state.player_to_move = PLAYER_A;
  e.state.turn_phase = ATTACK;
  e.phase = ENG_ATTACK_WAIT;
  e.pending = (PendingDecision)
  { .kind = DECISION_KIND_ATTACK, .player = PLAYER_A
  };

  config_t cfg = {0};
  GameContext* ctx = create_game_context(&cfg);
  EventBuf events = {0};
  PlayerDecision d = { .kind = DECISION_KIND_ATTACK, .move = { .type = MOVE_PASS } };

  bool ok = engine_submit(&e, PLAYER_A, &d, ctx, &events);
  check(suite, "submit accepted", ok);
  check(suite, "phase goes straight to ENG_END_TURN (no defense wait)",
        e.phase == ENG_END_TURN);

  PendingDecision pending = engine_advance(&e, ctx, &events);
  check(suite, "engine_advance() lands on SWITCH_PLAYER -> next attacker "
               "(no COMBAT event emitted)", pending.kind == DECISION_KIND_ATTACK);

  bool saw_combat = false;
  for(uint8_t i = 0; i < events.count; i++)
    if(events.ev[i].type == EVT_COMBAT_RESOLVED) saw_combat = true;
  check(suite, "no EVT_COMBAT_RESOLVED", !saw_combat);

  destroy_game_context(ctx);
} // test_attack_pass_skips_defense_and_combat

static void test_discard_wait_triggers_and_skips(TestSuite* suite)
{ printf("\n=== ENG_DISCARD_WAIT: triggered only when hand > 7 ===\n");

  config_t cfg = {0};
  GameContext* ctx = create_game_context(&cfg);

  { // hand of 8 after luna collection -- must wait for a discard.
    GameEngine e = {0};
    init_blank(&e.state);
    for(uint8_t i = 0; i < 8; i++) Hand_add(&e.state.hand[PLAYER_A], i);
    e.state.current_player = PLAYER_A;
    e.phase = ENG_END_TURN;

    EventBuf events = {0};
    PendingDecision pending = engine_advance(&e, ctx, &events);
    check(suite, "8-card hand: waits on DISCARD_TO_7",
          pending.kind == DECISION_KIND_DISCARD_TO_7 && pending.player == PLAYER_A);
  }

  { // hand of 7 after luna collection -- no wait, straight through.
    GameEngine e = {0};
    init_blank(&e.state);
    for(uint8_t i = 0; i < 7; i++) Hand_add(&e.state.hand[PLAYER_A], i);
    e.state.current_player = PLAYER_A;
    e.state.hand[PLAYER_B] = e.state.hand[PLAYER_A]; // so BEGIN_TURN's next attacker has cards too
    DeckStk_push(&e.state.deck[PLAYER_B], CHAMP_B); // so that BEGIN_TURN's draw doesn't underflow
    e.phase = ENG_END_TURN;

    EventBuf events = {0};
    PendingDecision pending = engine_advance(&e, ctx, &events);
    check(suite, "7-card hand: no discard wait, reaches next ATTACK decision",
          pending.kind == DECISION_KIND_ATTACK);
  }

  destroy_game_context(ctx);
} // test_discard_wait_triggers_and_skips

static void test_max_turns_cap_ends_in_draw(TestSuite* suite)
{ printf("\n=== MAX_NUMBER_OF_TURNS cap: GAME_OVER as a draw ===\n");

  GameEngine e = {0};
  init_blank(&e.state);
  e.state.turn = MAX_NUMBER_OF_TURNS; // the cap was already reached
  e.state.current_player = PLAYER_A;
  e.phase = ENG_BEGIN_TURN;

  config_t cfg = {0};
  GameContext* ctx = create_game_context(&cfg);
  EventBuf events = {0};

  PendingDecision pending = engine_advance(&e, ctx, &events);
  check(suite, "no further decision pending", pending.kind == DECISION_KIND_NONE);
  check(suite, "phase is ENG_GAME_OVER", e.phase == ENG_GAME_OVER);
  check(suite, "game_state is DRAW", e.state.game_state == DRAW);
  check(suite, "turn was not incremented past the cap", e.state.turn == MAX_NUMBER_OF_TURNS);

  bool saw_game_over = false;
  for(uint8_t i = 0; i < events.count; i++)
    if(events.ev[i].type == EVT_GAME_OVER) saw_game_over = true;
  check(suite, "EVT_GAME_OVER emitted", saw_game_over);

  destroy_game_context(ctx);
} // test_max_turns_cap_ends_in_draw

/* ========================================================================
   Events: EVT_CARD_DRAWN identity + redaction.
   ======================================================================== */

static void test_card_drawn_event_and_redaction(TestSuite* suite)
{ printf("\n=== EVT_CARD_DRAWN: correct card, redacted for the other viewer ===\n");

  GameEngine e = {0};
  init_blank(&e.state);
  DeckStk_push(&e.state.deck[PLAYER_A], CHAMP_B);
  e.state.current_player = PLAYER_A;
  e.state.turn = 1; // not player A's turn 1 -- begin_of_turn() will draw
  e.phase = ENG_BEGIN_TURN;

  config_t cfg = {0};
  GameContext* ctx = create_game_context(&cfg);
  EventBuf events = {0};
  engine_advance(&e, ctx, &events);

  GameEvent* drawn = NULL;
  for(uint8_t i = 0; i < events.count; i++)
    if(events.ev[i].type == EVT_CARD_DRAWN) drawn = &events.ev[i];

  check(suite, "EVT_CARD_DRAWN emitted", drawn != NULL);
  if(drawn)
  { check(suite, "correct card identity for the owner", drawn->u.card == CHAMP_B);

    GameEvent for_opponent = *drawn;
    event_filter_for_viewer(&for_opponent, PLAYER_B);
    check(suite, "redacted for the non-owner", for_opponent.u.card == EVT_CARD_REDACTED);

    GameEvent for_owner = *drawn;
    event_filter_for_viewer(&for_owner, PLAYER_A);
    check(suite, "not redacted for the owner", for_owner.u.card == CHAMP_B);
  }

  destroy_game_context(ctx);
} // test_card_drawn_event_and_redaction

int main(void)
{ TestSuite suite = {0, 0};

  printf("Running game_engine tests...\n");

  test_engine_matches_reference_full_game(&suite);
  test_engine_matches_reference_several_seeds(&suite);
  test_submit_rejects_wrong_player(&suite);
  test_submit_rejects_illegal_move(&suite);
  test_attack_pass_skips_defense_and_combat(&suite);
  test_discard_wait_triggers_and_skips(&suite);
  test_max_turns_cap_ends_in_draw(&suite);
  test_card_drawn_event_and_redaction(&suite);

  printf("\n%d passed, %d failed\n", suite.passed, suite.failed);
  return suite.failed == 0 ? 0 : 1;
} // main
