// stda_session.c
// SessionClient implementation -- see stda_session.h for the design
// rationale (poll not push, batched publish cadence, C11 <threads.h>).
// `update`/`published_version`/`cmd`/`has_cmd` are the only fields ever
// touched by more than one thread (guarded by `lock`); everything else
// (`engine`, `ctx`, `strategies`, `player_types`, `viewer`, `initial_cash`)
// belongs to the session thread alone from session_local_start() onward.

#include <stdlib.h>
#include <threads.h>

#include "stda_session.h"
#include "../../actions/move_gen.h"
#include "../../ai_strat/ai_strat_lib_heuristics.h" // mulligan_get_max_cards

struct SessionClient
{ GameEngine engine;
  GameContext* ctx;
  StrategySet strategies;
  PlayerType player_types[NUM_PLAYERS];
  PlayerID viewer;
  uint16_t initial_cash;

  mtx_t lock;
  cnd_t cond;
  SessionUpdate update;
  uint64_t published_version;
  uint64_t polled_version;

  SessionCommand cmd;
  bool has_cmd;

  thrd_t thread;
};

// ATTACK/DEFENSE: every legal GameMove, exhaustively -- UINT8_MAX as
// "no cap" on the recall/cash sub-choice variant count (correction #1's own
// "to verify when implementing" note in the plan file, settled here: 0
// means "cap to the single default variant", not "unlimited", so the cap
// itself must be a real large value). MULLIGAN/DISCARD_TO_7 have no legal[]
// (any subset of the right size is legal) -- min/max communicate the size
// constraint instead, per synthesis section 4.3.
static void fill_legal(SessionUpdate* u, const struct gamestate* gs, PendingDecision pending)
{ u->legal_count = 0;
  u->min_cards = 0;
  u->max_cards = 0;

  if(pending.kind == DECISION_KIND_ATTACK || pending.kind == DECISION_KIND_DEFENSE)
  { MoveGenLimits limits = { .max_recall_variants = UINT8_MAX, .max_cash_variants = UINT8_MAX };
    u->legal_count = get_available_moves(gs, pending.player, &limits, u->legal, MOVE_GEN_MAX_MOVES);
  }
  else if(pending.kind == DECISION_KIND_MULLIGAN)
    u->max_cards = mulligan_get_max_cards();
  else if(pending.kind == DECISION_KIND_DISCARD_TO_7)
    u->min_cards = u->max_cards = (uint8_t)(gs->hand[pending.player].size - 7);
} // fill_legal

static void publish(SessionClient* c, PendingDecision pending, EventBuf events, bool rejected)
{ for(uint8_t i = 0; i < events.count; i++)
    event_filter_for_viewer(&events.ev[i], c->viewer);

  SessionUpdate u = {0};
  visibility_filter(&c->engine.state, c->viewer, &u.view);
  u.pending = pending;
  fill_legal(&u, &c->engine.state, pending);
  u.events = events;
  u.rejected = rejected;

  mtx_lock(&c->lock);
  c->update = u;
  c->published_version++;
  mtx_unlock(&c->lock);
} // publish

// Silently runs automatic steps and AI decisions, accumulating every event
// into `events` (which may already hold events carried over from a just-
// applied human decision -- see session_thread_main()), until a decision is
// pending on a non-AI seat or the game ends.
static PendingDecision advance_until_human_or_over(SessionClient* c, EventBuf* events)
{ PendingDecision pending;
  for(;;)
  { EventBuf step = {0};
    pending = engine_advance(&c->engine, c->ctx, &step);
    event_buf_append(events, &step);

    if(pending.kind == DECISION_KIND_NONE) return pending;
    if(c->player_types[pending.player] != AI_PLAYER) return pending;

    EventBuf ai_step = {0};
    engine_run_ai(&c->engine, &c->strategies, c->ctx, &ai_step);
    event_buf_append(events, &ai_step);
  }
} // advance_until_human_or_over

// Blocks until a command is available, consumes it. CMD_QUIT is just
// another command -- there is no separate should-stop flag; a client always
// eventually sends CMD_QUIT (session_client_close() does this itself), even
// once the game is already over, to let the thread exit.
static void wait_for_command(SessionClient* c, SessionCommand* out)
{ mtx_lock(&c->lock);
  while(!c->has_cmd)
    cnd_wait(&c->cond, &c->lock);
  *out = c->cmd;
  c->has_cmd = false;
  mtx_unlock(&c->lock);
} // wait_for_command

// Handles every command that arrives while `pending` is still the engine's
// live decision, looping on rejected submissions WITHOUT re-publishing via
// advance_until_human_or_over() -- doing so would immediately overwrite the
// single-slot "rejected" update with a fresh, un-rejected republish of the
// exact same unchanged pending decision before the client ever had a chance
// to poll it (found via helgrind: a real bug, not a race the tool flagged
// itself -- the outer publish-then-loop-back shape silently clobbered the
// rejection under a slow enough poller, which is why it usually "worked").
// Returns once the pending decision is actually resolved (a legal submit,
// a resign, or the game already being over) or CMD_QUIT arrives; `*carry`
// collects whatever events that resolution produced. Returns false only for
// CMD_QUIT (caller must stop the thread).
static bool resolve_pending(SessionClient* c, PendingDecision pending, EventBuf* carry)
{ for(;;)
  { SessionCommand cmd;
    wait_for_command(c, &cmd);
    if(cmd.type == CMD_QUIT) return false;
    if(pending.kind == DECISION_KIND_NONE) continue; // game over -- ignore, wait again

    if(cmd.type == CMD_RESIGN)
    { engine_resign(&c->engine, pending.player, carry);
      return true;
    }

    EventBuf submit_events = {0};
    bool ok = engine_submit(&c->engine, pending.player, &cmd.decision, c->ctx, &submit_events);
    if(!ok)
    { publish(c, pending, submit_events, true);
      continue; // same pending, wait for another command -- no re-publish via advance
    }
    *carry = submit_events;
    return true;
  }
} // resolve_pending

static int session_thread_main(void* arg)
{ SessionClient* c = (SessionClient*)arg;
  EventBuf carry = {0};
  engine_init(&c->engine, c->initial_cash, c->ctx, &carry); // seeds carry with EVT_GAME_STARTED

  for(;;)
  { EventBuf events = carry;
    carry = (EventBuf)
    { 0
    };

    PendingDecision pending = advance_until_human_or_over(c, &events);
    publish(c, pending, events, false);

    if(!resolve_pending(c, pending, &carry)) break;
  }

  return 0;
} // session_thread_main

static PlayerID first_interactive_seat(const PlayerType player_types[NUM_PLAYERS])
{ for(uint8_t p = 0; p < NUM_PLAYERS; p++)
    if(player_types[p] == INTERACTIVE_PLAYER) return (PlayerID)p;
  return VIEWER_SPECTATOR;
} // first_interactive_seat

SessionClient* session_local_start(const PlayerType player_types[NUM_PLAYERS],
                                   const StrategySet* strategies,
                                   uint16_t initial_cash, GameContext* ctx)
{ SessionClient* c = calloc(1, sizeof(SessionClient));
  if(!c) return NULL;

  c->ctx = ctx;
  c->initial_cash = initial_cash;
  c->strategies = *strategies;
  for(uint8_t p = 0; p < NUM_PLAYERS; p++) c->player_types[p] = player_types[p];
  c->viewer = first_interactive_seat(player_types);

  mtx_init(&c->lock, mtx_plain);
  cnd_init(&c->cond);

  if(thrd_create(&c->thread, session_thread_main, c) != thrd_success)
  { mtx_destroy(&c->lock);
    cnd_destroy(&c->cond);
    free(c);
    return NULL;
  }

  return c;
} // session_local_start

bool session_client_poll(SessionClient* c, SessionUpdate* out)
{ mtx_lock(&c->lock);
  bool has_new = c->published_version != c->polled_version;
  if(has_new)
  { *out = c->update;
    c->polled_version = c->published_version;
  }
  mtx_unlock(&c->lock);
  return has_new;
} // session_client_poll

void session_client_send(SessionClient* c, const SessionCommand* cmd)
{ mtx_lock(&c->lock);
  c->cmd = *cmd;
  c->has_cmd = true;
  cnd_signal(&c->cond);
  mtx_unlock(&c->lock);
} // session_client_send

void session_client_close(SessionClient* c)
{ if(!c) return;

  SessionCommand quit = { .type = CMD_QUIT };
  session_client_send(c, &quit);
  thrd_join(c->thread, NULL);

  mtx_destroy(&c->lock);
  cnd_destroy(&c->cond);
  free(c);
} // session_client_close
