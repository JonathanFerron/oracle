// test_session.c
// Headless threaded test harness for src/roles/stda/stda_session.c -- GUI
// step 6. A "human" seat (PLAYER_A) submits uniformly random legal
// decisions built from each poll's legal[]/view.my_hand, driving the full
// session thread / mutex / condvar path end to end against a real AI
// opponent (PLAYER_B). Run under helgrind
// (`valgrind --tool=helgrind ./bin/test_session`) and ThreadSanitizer
// (`make test_session_tsan`) per the plan file's own Step 6 note -- neither
// is wired as a default `make test_session` step since both need a
// separately-built binary (helgrind: any build; tsan: `-fsanitize=thread`).
//
// Every wait loop below is bounded by wall-clock time (WAIT_TIMEOUT_SECONDS),
// not an iteration count: under helgrind's heavy per-memory-access
// instrumentation (or tsan's own overhead), a fixed iteration budget can run
// out purely from slowdown, with no real bug -- a real hang/deadlock must
// still fail loudly, just on real elapsed time instead.

#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#include "../src/roles/stda/stda_session.h"
#include "../src/core/game_constants.h"
#include "../src/ai_strat/ai_strategy.h"
#include "../src/util/rnd.h"

#define TEST_PASS "\033[32m\xe2\x9c\x93 PASS\033[0m"
#define TEST_FAIL "\033[31m\xe2\x9c\x97 FAIL\033[0m"

// Generous enough for a heavily-instrumented (helgrind/tsan) run, but still
// bounded -- a hung/deadlocked session must fail the test, never hang the
// suite forever.
#define WAIT_TIMEOUT_SECONDS 60

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

static time_t deadline(void)
{ return time(NULL) + WAIT_TIMEOUT_SECONDS;
} // deadline

// ATTACK/DEFENSE: a uniformly random legal[] entry. MULLIGAN/DISCARD_TO_7:
// a random count in [min_cards, max_cards] (min==max for DISCARD_TO_7, no
// real choice there) of distinct cards from the viewer's own hand.
static PlayerDecision random_decision(const SessionUpdate* u, GameContext* rng)
{ PlayerDecision d = { .kind = u->pending.kind };

  if(u->pending.kind == DECISION_KIND_ATTACK || u->pending.kind == DECISION_KIND_DEFENSE)
  { d.move = u->legal[RND_randn(u->legal_count, rng)];
    return d;
  }

  uint8_t span = (uint8_t)(u->max_cards - u->min_cards + 1);
  uint8_t count = (uint8_t)(u->min_cards + RND_randn(span, rng));

  uint8_t pool[12];
  uint8_t pool_n = u->view.my_hand.size;
  memcpy(pool, u->view.my_hand.cards, pool_n);
  if(count > pool_n) count = pool_n;

  for(uint8_t i = 0; i < count; i++)
  { uint8_t j = (uint8_t)(i + RND_randn((uint8_t)(pool_n - i), rng));
    uint8_t tmp = pool[i];
    pool[i] = pool[j];
    pool[j] = tmp;
    d.cards[i] = pool[i];
  }
  d.count = count;
  return d;
} // random_decision

static SessionClient* start_random_vs_random(GameContext* session_ctx)
{ PlayerType types[NUM_PLAYERS] = { INTERACTIVE_PLAYER, AI_PLAYER };
  StrategySet strategies = {0};
  set_player_strategy_by_type(&strategies, PLAYER_A, AI_STRATEGY_RANDOM); // unused: A is "human"
  set_player_strategy_by_type(&strategies, PLAYER_B, AI_STRATEGY_RANDOM);
  return session_local_start(types, &strategies, INITIAL_CASH_DEFAULT, session_ctx);
} // start_random_vs_random

// Polls until `pending.kind` is DECISION_KIND_NONE (game over) or the
// deadline passes. Whenever it's PLAYER_A's turn, submits a random legal
// decision on its behalf. Returns false (timed out) or true with `*out`
// holding the final (game-over) update.
static bool play_to_completion(SessionClient* c, GameContext* rng, SessionUpdate* out)
{ time_t dl = deadline();
  while(time(NULL) < dl)
  { if(session_client_poll(c, out))
    { if(out->pending.kind == DECISION_KIND_NONE) return true;
      if(out->pending.player == PLAYER_A)
      { SessionCommand cmd = { .type = CMD_SUBMIT, .decision = random_decision(out, rng) };
        session_client_send(c, &cmd);
      }
    }
    thrd_yield();
  }
  return false;
} // play_to_completion

static GameStateEnum run_one_game(uint32_t seed, bool* timed_out)
{ config_t cfg = {0};
  cfg.prng_seed = seed;
  GameContext* session_ctx = create_game_context(&cfg);

  config_t rng_cfg = {0};
  rng_cfg.prng_seed = seed ^ 0xC0FFEEu;
  GameContext* harness_rng = create_game_context(&rng_cfg);

  SessionClient* c = start_random_vs_random(session_ctx);

  SessionUpdate u = {0};
  bool finished = play_to_completion(c, harness_rng, &u);
  *timed_out = !finished;

  session_client_close(c);
  destroy_game_context(session_ctx);
  destroy_game_context(harness_rng);
  return finished ? u.view.game_state : ACTIVE;
} // run_one_game

static void test_several_full_games(TestSuite* suite)
{ printf("\n=== full games via the session thread, human seat random-legal ===\n");

  int all_ok = 1;
  for(uint32_t seed = 1; seed <= 10; seed++)
  { bool timed_out = false;
    GameStateEnum result = run_one_game(seed, &timed_out);
    if(timed_out || result == ACTIVE) all_ok = 0;
  }
  check(suite, "10/10 games reached a real terminal state, no hang", all_ok);
} // test_several_full_games

static bool poll_until_pending(SessionClient* c, SessionUpdate* out)
{ time_t dl = deadline();
  while(time(NULL) < dl)
  { if(session_client_poll(c, out) && out->pending.kind != DECISION_KIND_NONE) return true;
    thrd_yield();
  }
  return false;
} // poll_until_pending

static void test_resign_path(TestSuite* suite)
{ printf("\n=== CMD_RESIGN: session ends the game immediately ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 55;
  GameContext* ctx = create_game_context(&cfg);
  SessionClient* c = start_random_vs_random(ctx);

  SessionUpdate u = {0};
  check(suite, "reached a real pending decision before resigning",
        poll_until_pending(c, &u));

  SessionCommand resign = { .type = CMD_RESIGN };
  session_client_send(c, &resign);

  bool over = false;
  GameStateEnum result = ACTIVE;
  time_t dl = deadline();
  while(time(NULL) < dl && !over)
  { if(session_client_poll(c, &u) && u.pending.kind == DECISION_KIND_NONE)
    { over = true;
      result = u.view.game_state;
    }
    thrd_yield();
  }
  check(suite, "game ended after resign, no hang", over);
  check(suite, "PLAYER_B wins (PLAYER_A resigned)", result == PLAYER_B_WINS);

  session_client_close(c);
  destroy_game_context(ctx);
} // test_resign_path

static void test_rejected_submission_then_retry(TestSuite* suite)
{ printf("\n=== illegal submission: rejected, session keeps waiting; a real "
           "decision then still succeeds ===\n");

  config_t cfg = {0};
  cfg.prng_seed = 99;
  GameContext* ctx = create_game_context(&cfg);
  SessionClient* c = start_random_vs_random(ctx);

  SessionUpdate u = {0};
  bool got = poll_until_pending(c, &u);
  check(suite, "reached a real pending decision", got);
  if(!got)
  { session_client_close(c);
    destroy_game_context(ctx);
    return;
  }

  PendingDecision original_pending = u.pending;

  // An obviously-illegal decision: right kind, but a move that can't be in
  // legal[] (fullDeck indices 250-252 don't exist -- FULL_DECK_SIZE is
  // nowhere near that -- so this can never appear in any hand's legal moves)
  // or, for MULLIGAN/DISCARD_TO_7, a count past max_cards.
  PlayerDecision bogus = { .kind = original_pending.kind };
  if(original_pending.kind == DECISION_KIND_ATTACK || original_pending.kind == DECISION_KIND_DEFENSE)
    bogus.move = (GameMove)
  { .type = MOVE_CHAMPIONS, .count = 3, .cards = {250, 251, 252}
  };
  else
    bogus.count = (uint8_t)(u.max_cards + 5);

  SessionCommand cmd = { .type = CMD_SUBMIT, .decision = bogus };
  session_client_send(c, &cmd);

  bool saw_rejected = false;
  time_t dl = deadline();
  while(time(NULL) < dl && !saw_rejected)
  { if(session_client_poll(c, &u) && u.rejected) saw_rejected = true;
    thrd_yield();
  }
  check(suite, "rejected flag observed", saw_rejected);
  check(suite, "still the same pending decision",
        u.pending.kind == original_pending.kind && u.pending.player == original_pending.player);

  config_t rng_cfg = {0};
  rng_cfg.prng_seed = 12345;
  GameContext* harness_rng = create_game_context(&rng_cfg);
  SessionCommand real_cmd = { .type = CMD_SUBMIT, .decision = random_decision(&u, harness_rng) };
  session_client_send(c, &real_cmd);

  bool progressed = false;
  dl = deadline();
  while(time(NULL) < dl && !progressed)
  { if(session_client_poll(c, &u) && !u.rejected) progressed = true;
    thrd_yield();
  }
  check(suite, "a real decision after the rejection still goes through", progressed);

  destroy_game_context(harness_rng);
  session_client_close(c);
  destroy_game_context(ctx);
} // test_rejected_submission_then_retry

int main(void)
{ TestSuite suite = {0, 0};

  printf("Running stda_session tests...\n");

  test_several_full_games(&suite);
  test_resign_path(&suite);
  test_rejected_submission_then_retry(&suite);

  printf("\n%d passed, %d failed\n", suite.passed, suite.failed);
  return suite.failed == 0 ? 0 : 1;
} // main
