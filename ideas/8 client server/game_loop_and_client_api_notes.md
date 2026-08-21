# Supplementary Game-Loop & Client API Notes

**Status**: secondary to `Client Server Architecture Ideas 1.md` and `Client Server
Architecture Ideas 2 (consolidated).md` in this folder — those two are the
authoritative, detailed design (protocol, server main loop, client network loop, action
validation, AI integration). This file only holds a couple of things **not** already
covered there: an alternative (opaque-handle) API shape, a simpler single-threaded
client loop, and a poll-based server-loop variant. Consolidated (2026-08-20) from three
now-deleted files in `ideas/2 engine and action system design/`
(`More notes on client-server preparation.md`, `client_game_gui.txt`, and the Server
example from `mode_usage_examples.txt`).

---

## Important: strategy function signature — use the Action-returning form

The consolidated doc's "Strategy Interface" section shows
`typedef void (*AttackStrategyFunc)(gamestate* gs, GameContext* ctx)` — the **current,
today's-codebase** signature, where a strategy mutates `gstate` directly. That signature
does not work for a networked AI client: a client (AI or human) never has the server's
authoritative `gamestate`, only a `VisibleGameState`, and can't call `apply_action()`
itself (see that doc's own `run_mode_client_ai()` example a few lines later, which
correctly has `ai_choose_action()` *return* an `Action` instead).

The corrected form — needed for exactly the scenario this folder exists for (AI agent
as a network client, server in the middle, human on another device) — is designed in
`ideas/2 …/game engine refactoring approach to prepare for client server separation of
duties using clean state machine approach.md`, §2 "Refactor Strategy Functions to
Return Actions":

```c
// Old (today's codebase, and what the consolidated doc's "Strategy Interface" shows):
typedef void (*AttackStrategyFunc)(struct gamestate* gstate, GameContext* ctx);

// Needed for network clients (including AI clients):
typedef Action (*AttackStrategyFunc)(const VisibleGameState* vgs, GameContext* ctx);
```

Whoever implements the client/server split should treat this signature change, and the
`get_list_of_possible_actions()` generator it depends on, as a prerequisite — not
optional polish. See that doc's "who uses `apply_action()`" table (Server / Human
Client / Simple AI Client / MCTS AI Client / Standalone) for how each component type
is expected to use the resulting `Action` differently.

---

## Alternative API shape: opaque handles

The consolidated doc exposes `GameServer`/`ServerGameState`/`ClientState` as concrete,
inspectable structs. An alternative, more encapsulated shape — opaque pointers with a
narrow accessor API — is worth keeping as an option:

```c
// game_server.h
typedef struct GameServer GameServer;

GameServer* server_create(uint16_t initial_cash);
void server_apply_action(GameServer* srv, PlayerID player, Action* action);
VisibleGameState server_get_view(GameServer* srv, PlayerID observer);
void server_destroy(GameServer* srv);

// game_client.h
typedef struct GameClient GameClient;

GameClient* client_create(PlayerID player_id);
void client_receive_state(GameClient* cli, VisibleGameState* state);
Action client_get_action(GameClient* cli);  // AI or human input
void client_destroy(GameClient* cli);
```

Trade-off versus the consolidated doc's exposed-struct style: less flexible for direct
field access (e.g. server admin tooling poking at session internals), but a cleaner
boundary and easier to keep the server's internals from leaking into client code by
accident.

### Visibility enum (not present in the consolidated doc)

A finer-grained alternative to that doc's `VisibleGameState` (which just omits hidden
fields) — an explicit per-card visibility tag, useful if partial/temporary visibility
ever matters (e.g. a card briefly revealed then hidden again):

```c
typedef enum
{ CARD_VISIBLE,
  CARD_KNOWN_IN_OPPONENT_HAND,
  CARD_HIDDEN,
  CARD_KNOWN_IN_OWN_DECK,
  CARD_KNOWN_IN_OPPONENT_DECK
} CardVisibility;

typedef struct
{ struct gamestate base;
  CardVisibility visibility[2][FULL_DECK_SIZE];
} GameStateWithVisibility;
```

Not needed for the base hidden-hand/hidden-deck model the consolidated doc already
handles — keep as a fallback if a more granular visibility rule is ever added to the
game rules.

---

## Alternative: single-threaded, poll-based client loop

The consolidated doc's "Client Network Loop" uses two threads (a network-recv thread
plus the UI thread, coordinated with a mutex). A simpler single-threaded alternative,
non-blocking and polled once per frame — the natural fit if the client is also driving
a GUI main loop (see `ideas/9 gui/game_loop_engine_integration_notes.md`, which this
was designed alongside):

```c
// roles/client/client_game.h
typedef struct
{ VisibleGameState* visible_state;
  NetworkConnection* connection;
  bool running;
  bool waiting_for_input;
  int my_player_id;
  ActionQueue* pending_actions;
} ClientGameContext;

ClientGameContext* client_init_game_gui(const char* server_addr, int port);
void client_update_game_gui(ClientGameContext* ctx, float dt);      // non-blocking: polls network
bool client_submit_action_gui(ClientGameContext* ctx, Action* action);
bool client_is_waiting_for_input(ClientGameContext* ctx);
VisibleGameState* client_get_visible_state(ClientGameContext* ctx); // for rendering
void client_cleanup_game_gui(ClientGameContext* ctx);
```

Trade-off versus the pthread design: simpler (no mutex, no cross-thread state sharing),
but network reads must be non-blocking/polled (e.g. `recv()` with `MSG_DONTWAIT` or a
short-timeout `select()`) rather than the consolidated doc's blocking-recv-in-a-thread
approach — fine for a GUI already running a 60 FPS poll loop, less natural for a
blocking CLI/TUI client (which should just reuse the consolidated doc's threaded design
instead, or block directly on `recv()` since it has no animation loop to keep alive).

---

## Alternative: poll-based server loop (assumes the folder-2 unified engine)

The consolidated doc's "Server Main Loop" uses `select()` across all client sockets and
manages sessions directly. A shorter variant, assuming `core/game_engine.c`'s pollable
state machine (`ideas/2 …/unified_state_machine.txt`) already exists, showing how a
server would sit on top of it rather than reimplementing turn/phase logic itself:

```c
void server_game_loop(ServerSession* session)
{ GameEngine* engine = engine_create(session->config);

  while(engine_get_phase(engine) != PHASE_GAME_OVER)
  { engine_run_until_input(engine, session->ctx);

    if(engine_needs_input(engine))
    { PlayerID player = engine_get_active_player(engine);
      broadcast_visible_state(session, engine);

      Action* action = server_receive_action(session, player, NETWORK_TIMEOUT);
      if(action && engine_submit_action(engine, action))
      { advance_to_resolve_phase(engine);
        broadcast_visible_state(session, engine);
      }
    }
  }

  broadcast_game_over(session, engine);
  engine_destroy(engine);
}
```

This is simpler than the consolidated doc's select()-based multi-session loop because
it handles exactly one game session and assumes something else (a listener/dispatcher)
already routed this session's sockets here — it isn't a replacement for that doc's
connection-acceptance and multi-session management, just a sketch of the single-session
turn loop once a session exists, reusing the shared engine instead of duplicating phase
logic per-role.
