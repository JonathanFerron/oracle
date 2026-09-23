# GUI Architecture Synthesis (SDL3, client/server-ready)

**Written**: 2026-09-22, from a design discussion with Claude Code (Opus 5.5).
**Status**: design only, nothing implemented yet. **This file supersedes** the engine/GUI
integration parts of `ideas/9 gui/game_loop_engine_integration_notes.md`,
`ideas/2 engine and action system design/` (engine state machine, `Action`, strategy
signature change), and the `Action`/`VisibleGameState` sections of `ideas/8 client
server/`. Those files stay as background; §12 below lists what is kept vs. superseded
from each.

**How to use this file**: §1-§2 are the decisions and verified facts; §3 is the target
picture; §4-§9 specify each layer bottom-up; §10 is the step-by-step implementation
roadmap (start there in a new session); §11 lists open questions to settle along the way.

---

## 1. Decisions made (2026-09-22)

| Topic | Decision | Rationale |
|---|---|---|
| Library | **SDL3** + SDL3_image + SDL3_ttf, same library for Linux and Android | Engine/AI are portable C; SDL3 links directly (no bindings), Android is an official SDL3 target. Using one library for both platforms avoids writing the UI code twice — the UI code, not the assets, is the expensive part. |
| Godot 4 | Rejected | Its advantages (tweens, theming, scene-authored UI) aren't needed; costs a GDExtension binding + GDScript + per-ABI builds. |
| raylib | Rejected (close second) | Equally good for 2D in C; Android path less official/polished than SDL3's. |
| Visual scope | Strictly 2D, **no animation** | A played card simply disappears from the hand and appears in the combat zone/discard on the next frame (render-from-state gives this for free). Dice = a die-face image per die type with the rolled number drawn on top. |
| Runtime-swappable | Table background tile; font face and font size | Everything else themed via source + recompile. SDL3 has `SDL_RenderTextureTiled()` (tile) and SDL3_ttf 3.x `TTF_SetFontSize()` (size); face swap = close/reopen font. |
| Assets | One high-resolution set (sized for Galaxy Tab ~2560×1600), GPU-downscaled on desktop | No need to maintain two PNG sets. Fonts from Google Fonts (OFL, fine to bundle). |
| Main loop | GUI owns the loop; use SDL3 **main callbacks** (`SDL_AppInit/AppEvent/AppIterate/AppQuit`) | Recommended SDL3 structure; handles Android lifecycle (pause/resume, surface loss). |
| GUI's view of the game | **Only ever `VisibleGameState`** — never the full `struct gamestate`, even in standalone | One render path for standalone and networked play; hidden info can't leak by accident. |
| Responsiveness | GUI thread **never waits** on the engine, on AI, or (later) on the network | Mouse/window must stay live while an opponent thinks for seconds (A14 ≈ 1.7 s/decision locally; a human opponent over the network, arbitrarily long). |
| Threading model | A **session thread** owns the authoritative engine state and runs AI decisions; GUI talks to it only through message queues | Mirrors the future server exactly: swapping a local session for a network session leaves the GUI untouched. |
| Action type | Reuse/extend the existing `GameMove` (`src/actions/game_move.h`), wrapped in a `PlayerDecision` for mulligan/discard | Don't introduce a parallel `Action` type; `GameMove` is already proven by A8-A14. |
| Strategy signature change (`Action f(const VisibleGameState*)`) | **Deferred** — not a GUI prerequisite | The session owns the real state, so today's mutating strategies keep working. Only needed for AI-as-network-client (far future). |

---

## 2. Verified facts about the current code (2026-09-22)

These drive the design; re-verify if much time has passed.

- **`struct gamestate` is pointer-free POD** (`src/core/game_types.h:120`): `deck_stack`
  (`uint8_t card_indices[40]` + `int8_t top`), `Hand` (`cards[12]` + size), `Discard`
  (`cards[40]` + size), `CombatZone` (`cards[3]` + size), plus scalars
  (`current_player`, `current_cash_balance[2]`, `current_energy[2]`,
  `someone_has_zero_energy`, `turn`, `game_state`, `turn_phase`, `player_to_move`,
  `combo_bonus_table`). A few hundred bytes; `memcpy`/snapshot is trivial.
- **`GameContext`** (`src/core/game_context.h`) = `MTRand rng` + `config_t* config`. The
  RNG is not thread-safe → exactly one thread (the session thread) may ever touch a given
  `GameContext`.
- **`UiIO`** (`src/ui/shared/ui_io.h`) is a *text-line* seam (`message`, blocking
  `read_line`, `show_card_list`) feeding `game_commands.c`'s command grammar. Right for
  CLI/TUI, wrong for a GUI or a network client. CLI/TUI keep it; the GUI does not use it.
- **Strategies mutate state directly**: `AttackStrategyFunc`/`DefenseStrategyFunc` are
  `void f(struct gamestate*, GameContext*)`; `MulliganStrategyFunc`/`DiscardStrategyFunc`
  are `void f(struct gamestate*, PlayerID, GameContext*)` (`src/ai_strat/ai_strategy.h`).
  No decision value is returned.
- **`GameMove`** (`src/actions/game_move.h`): `MOVE_PASS`, `MOVE_CHAMPIONS` (1-3,
  `cards[3]`), `MOVE_DRAW`, `MOVE_RECALL` (`recall[3]`), `MOVE_CASH` (`cards[0]` = champion
  exchanged). Covers attack and defense fully, including the recall and cash-exchange
  sub-choices. **Does not cover mulligan or discard-to-7.**
- **`get_available_moves(gstate, player, limits, out, max_out)`** (`move_gen.h`) enumerates
  legal moves for the *current* `turn_phase`; `MOVE_GEN_MAX_MOVES` = 128; `MoveGenLimits`
  caps recall/cash *variants* (search-agent pruning). **`apply_move()`** (`move_apply.h`)
  applies without re-checking legality.
- **Turn flow exists in (at least) two hand-maintained copies**: `play_turn()`
  (`src/core/turn_logic.c`) and `tui_play_turn_with_humans()`
  (`src/roles/stda/stda_tui_interactive.c:235`), each managing `turn_phase` /
  `player_to_move` itself. (The CLI's loop likely has similar logic — check `cli_game.c`.)
- **Mulligan orchestration lives in roles, not core**: `apply_mulligan()` is in
  `src/roles/stda/stda_auto.h/.c`; second player (B) only, up to 2 cards
  (`doc/game_rules_doc.md` §"Mulligan").
- **Discard piles are public** (`doc/game_rules_doc.md` glossary: "Face-up pile of used
  cards visible to both players"). Deck order is hidden from everyone, including the owner.
- **`resolve_combat_with_details()`** (`combat.h`) fills a `CombatDetails` (per-champion
  species/color/dice/rolls/base/totals, combos, damage, defender energy before/after) and
  its comment says it "mirrors the exact math/RNG order of `resolve_combat()`".
- **`draw_1_card()`** returns `void` (the drawn card isn't reported).
- `src/visibility/` holds only a placeholder `.txt`; no visible-state type exists yet.
- **Makefile auto-discovers all `src/**/*.c`** — so adding SDL-dependent files under
  `src/ui/gui/` would make *every* build (incl. `stda.auto` calibration builds) require
  SDL3 headers/libs. Must be handled (see §9.8).

---

## 3. Target architecture

```
┌──────────────────────── GUI thread (SDL main callbacks) ─────────────────────────┐
│ AppEvent: input → staging (selected cards, sub-choice) → PlayerDecision → submit  │
│ AppIterate: drain session updates → keep latest VisibleGameState + events log     │
│             → layout → render (never blocks)                                      │
└───────────────▲─────────────────────────────────────────────┬────────────────────┘
                │ SessionUpdate (view, pending, legal moves,   │ SessionCommand
                │ events) via queue + SDL_PushEvent wake-up    │ (submit/resign/quit) via queue
┌───────────────┴─────────────────────────────────────────────▼────────────────────┐
│ Session (roles/stda/stda_session.c) — own thread, owns GameEngine + GameContext   │
│  loop: engine_advance() → if pending decision is AI: call existing strategy       │
│        if human: publish update, wait on command queue → engine_submit()          │
└───────────────┬───────────────────────────────────────────────────────────────────┘
                │ plain function calls, single-threaded
┌───────────────▼───────────────────────────────────────────────────────────────────┐
│ Engine step driver (core/game_engine.c) — the ONE turn flow                        │
│ visibility/ (VisibleGameState filter)  actions/ (GameMove, PlayerDecision, legality)│
│ existing core: card_actions, combat, game_state, combo_bonus                        │
└────────────────────────────────────────────────────────────────────────────────────┘

Later (network): replace the local Session with a NetSession (client side) +
a server process running the same Session loop, with sockets instead of queues.
The GUI code does not change.
```

| Layer | Location | Status |
|---|---|---|
| `PlayerDecision` (wraps `GameMove` + mulligan/discard lists), legality check | `src/actions/` | extend existing |
| `VisibleGameState` + filter | `src/visibility/` | new |
| `GameEvent` + per-viewer event filtering | `src/visibility/` or `src/core/` | new |
| Step driver `GameEngine` | `src/core/game_engine.c/h` | new; becomes the single turn flow |
| Session (thread + queues) | `src/roles/stda/stda_session.c/h` | new |
| SDL3 GUI | `src/ui/gui/` + `src/roles/stda/stda_gui.c` | new |

CLI/TUI are **not** migrated as part of this; they keep `UiIO`. They *may* later be
ported onto the step driver to eliminate their private copies of the turn flow.

---

## 4. Layer 1 — `PlayerDecision` (the "Action")

### 4.1 Why a wrapper rather than growing `GameMove`

`GameMove` is used in large arrays by the search agents (`MOVE_GEN_MAX_MOVES` = 128 per
node enumeration, MCTS arenas in A10/A11/A14). Widening it (e.g. to carry a 5-card
discard list) would grow every tree node and risk perf/memory regressions in agents that
have nothing to do with the GUI. So keep `GameMove` untouched and wrap it:

```c
// src/actions/player_decision.h (proposed)
typedef enum
{ DECISION_KIND_NONE = 0,
  DECISION_KIND_MULLIGAN,   // second player, before turn 1: discard 0-2, draw as many
  DECISION_KIND_ATTACK,     // GameMove, turn_phase == ATTACK
  DECISION_KIND_DEFENSE,    // GameMove (MOVE_PASS or MOVE_CHAMPIONS), turn_phase == DEFENSE
  DECISION_KIND_DISCARD_TO_7
} DecisionKind;

#define DECISION_MAX_CARDS 12   // == Hand capacity; discard-to-7 can exceed 3

typedef struct
{ DecisionKind kind;
  GameMove move;                       // ATTACK / DEFENSE
  uint8_t count;                       // MULLIGAN / DISCARD_TO_7
  uint8_t cards[DECISION_MAX_CARDS];   // fullDeck[] indices, not hand positions
} PlayerDecision;
```

(Alternative considered: a union. Plain struct preferred — it's small, pointer-free,
trivially copyable across the thread queue, and easy to serialize field by field later.)

**Card identity**: always `fullDeck[]` indices (as `GameMove` already does), never hand
positions — hand positions shift as cards are removed and differ between the engine's
state and whatever order the GUI displays.

### 4.2 Legality check (the server-side "never trust the client")

```c
// Returns true iff `d` is legal for `player` in the engine's current state.
bool decision_is_legal(const struct gamestate* gstate, PlayerID player,
                       DecisionKind expected, const PlayerDecision* d);
```

- ATTACK/DEFENSE: enumerate with `get_available_moves()` using **exhaustive** limits (the
  recall/cash variant caps exist for search pruning; a human may legitimately choose any
  variant) and check membership. **To verify when implementing**: what `MoveGenLimits`
  values mean "no cap" (the header says `max_cash_variants == 0` still emits one variant,
  so 0 is *not* "unlimited" — probably need `UINT8_MAX` or a new flag).
- Membership needs a **canonical comparison**: `MOVE_CHAMPIONS` subsets and `MOVE_RECALL`
  lists must compare order-insensitively (sort both, or compare as sets).
- MULLIGAN: player is the second player, before turn 1, `count` ≤ 2, all cards in hand,
  no duplicates.
- DISCARD_TO_7: `count == hand.size - 7` exactly, all cards in hand, no duplicates.

### 4.3 Legal-move list for the GUI

The session sends the human's legal `GameMove` list with each update (same as a real
server would). The GUI uses it to decide which cards are clickable and when the
Confirm button is enabled — **no rules code on the client**. The session still runs
`decision_is_legal()` on every submission.

For mulligan/discard the "list" is implicit (any subset of the right size), so send the
constraint (`min`, `max` count) rather than enumerating subsets.

---

## 5. Layer 2 — `VisibleGameState`

### 5.1 Proposed struct

Built from today's real types (not the stale `HDCLList`/`PlayerState` sketch in
`ideas/8`):

```c
// src/visibility/visible_state.h (proposed)
typedef struct
{ PlayerID viewer;                    // whose eyes; see §5.3 for spectators

  // Public, per player (index by PlayerID)
  uint8_t  energy[2];
  uint16_t cash[2];                   // "lunas"
  uint8_t  hand_count[2];
  uint8_t  deck_count[2];             // deck_stack.top + 1
  Discard  discard[2];                // full contents — discard piles are face-up
  CombatZone combat_zone[2];

  // Private to viewer
  Hand     my_hand;                   // opponent's hand: count only (above)

  // Flow
  uint16_t turn;
  TurnPhase turn_phase;
  PlayerID current_player;            // attacker this turn
  PlayerID player_to_move;
  ComboBonusTable combo_bonus_table;  // needed to show combo bonuses correctly
  bool     game_over;
  GameStateEnum game_state;           // winner when game_over
} VisibleGameState;

void visibility_filter(const struct gamestate* gs, PlayerID viewer,
                       VisibleGameState* out);
```

- Deliberately **no deck order, no opponent hand contents, no RNG state**.
- Reuses `Hand`/`Discard`/`CombatZone` so existing display helpers' logic can be
  mirrored easily.
- Pointer-free and fixed-size → trivially copied across the thread queue. For the network
  later, **serialize field by field** (fixed endianness), don't `memcpy` the struct raw
  (padding/endianness/ABI differences between Linux x86_64 and Android arm64).

### 5.1a N-player readiness (roadmap constraint)

`doc/oracle_roadmap.md` (item 3, "SDL3 GUI") requires the GUI to be built so that the
later 3-4 player engine rework is an *extension*, not a rewrite: **the renderer loops over
players rather than hardcoding a "my side / their side" layout.** The `[2]` arrays above
mirror today's 2-player `struct gamestate` and are fine for now, but:
- keep per-player data in arrays indexed by `PlayerID` (as above), never as separate
  `my_*`/`opp_*` fields (the `ideas/8` sketch's `opp_energy` etc. style is exactly what to
  avoid); `my_hand` is the one deliberate exception (only the viewer's hand is ever
  visible);
- render and lay out by iterating players, with the viewer's seat placed at the bottom
  and the other seats distributed around the table by a layout function that takes the
  player count;
- use a named constant (e.g. `NUM_PLAYERS`, or whatever the engine rework introduces)
  rather than a literal `2` in new GUI/visibility code.

### 5.2 What the view does *not* carry

Transient information (dice rolls, which card was just drawn, what the opponent just
played) is not state — it goes in **events** (§6). The view is "what's on the table now".

### 5.3 Spectators (optional, cheap)

A `VIEWER_SPECTATOR` value (both hands shown as counts only) costs almost nothing and is
handy for AI-vs-AI viewing in the GUI and for a future "watch" feature. Decide when
implementing; not needed for M1.

### 5.4 Not needed

The `CardVisibility` per-card enum in `ideas/8 client server/game_loop_and_client_api_notes.md`
stays unnecessary — the rules have no partial/temporary reveals.

---

## 6. Layer 3 — Events

The old `UICallbacks` idea (`ideas/2` refactoring doc §5) turned into **data** instead of
callbacks: callbacks would fire on the session thread, where the GUI can't safely act;
events are values put on a queue. Over the network they become the `MSG_EVENT_*`
messages of `ideas/8` unchanged.

### 6.1 Proposed event set

```c
typedef enum
{ EVT_GAME_STARTED,
  EVT_MULLIGAN_DONE,     // player, count (+ cards: they go to discard, which is public)
  EVT_TURN_BEGAN,        // turn number, attacker
  EVT_CARD_DRAWN,        // player, card — card REDACTED for the non-owner
  EVT_DECK_RESHUFFLED,   // player (discard → deck)
  EVT_MOVE_PLAYED,       // player, GameMove (champions, draw/recall/cash card are public;
                         //   recalled champion identities are public — they came from discard)
  EVT_COMBAT_RESOLVED,   // CombatDetails (dice rolls, combos, damage, energy before/after)
  EVT_LUNA_COLLECTED,    // player
  EVT_DISCARDED_TO_7,    // player, cards (public — discard pile is face-up)
  EVT_GAME_OVER          // winner
} GameEventType;

typedef struct
{ GameEventType type;
  PlayerID player;
  union
  { GameMove move;
    CombatDetails combat;
    struct { uint8_t count; uint8_t cards[DECISION_MAX_CARDS]; } cards;
    uint8_t card;          // EVT_CARD_DRAWN (0xFF = redacted)
    uint16_t turn;
  } u;
} GameEvent;

void event_filter_for_viewer(GameEvent* e, PlayerID viewer); // redact private fields
```

(A union is fine here — events are produced/consumed by our own code; for the network,
serialize per type.)

### 6.2 How events get produced without touching `card_actions.c`

The step driver (§7) calls the existing primitives and **derives events by comparing
state before/after** each step, rather than threading callbacks through every primitive:

- Drawn cards = `hand_after \ hand_before` (set difference on `fullDeck[]` indices).
- Reshuffle = deck count went up.
- `EVT_MOVE_PLAYED` for a human: the submitted `GameMove`. For an AI (which mutates state
  directly): reconstruct what's needed for display from the diff — cards that left the
  hand into the combat zone (champions), the draw/recall/cash card that went to discard,
  recalled champions (discard → hand). This only has to be good enough for a message log
  line, not a perfect `GameMove`.
- Combat: always call `resolve_combat_with_details()` in the driver, which provides
  `CombatDetails` directly. **Must confirm via the `-a -p` baseline** that it really
  consumes the RNG identically to `resolve_combat()` (its comment claims so).

The optional ideas/2 §6 change (`draw_1_card()` returning the card) is then unnecessary.

### 6.3 Event pacing in the GUI

No animation, but AI turns happen instantly — the player needs to see *what* happened.
Minimum: a message-log panel (like the TUI's Game Messages box) fed by events, plus a
combat-result panel from `EVT_COMBAT_RESOLVED` that stays visible until the next turn's
combat or a click. Optional later: a configurable short pause after each AI move.

---

## 7. Layer 4 — The step driver (`core/game_engine.c`)

### 7.1 Purpose

One pollable implementation of the turn flow that runs until the next point where a
player must decide, then stops and reports who and what. It replaces, over time, every
hand-maintained copy of the flow (`play_turn()`, `tui_play_turn_with_humans()`, the CLI
loop, `apply_mulligan()`'s placement in `stda_auto.c`), and becomes the single owner of
`turn_phase` / `player_to_move` bookkeeping.

### 7.2 API (proposed)

```c
typedef struct
{ DecisionKind kind;      // DECISION_KIND_NONE only when game over
  PlayerID player;
} PendingDecision;

typedef enum   // internal; not exposed to UIs
{ ENG_SETUP, ENG_MULLIGAN_WAIT, ENG_BEGIN_TURN, ENG_ATTACK_WAIT,
  ENG_DEFENSE_WAIT, ENG_COMBAT, ENG_END_TURN, ENG_DISCARD_WAIT,
  ENG_SWITCH_PLAYER, ENG_GAME_OVER
} EnginePhase;

typedef struct
{ struct gamestate state;   // authoritative
  EnginePhase phase;
  PendingDecision pending;
} GameEngine;

typedef struct { GameEvent ev[32]; uint8_t count; } EventBuf;

void engine_init(GameEngine* e, uint16_t initial_cash, GameContext* ctx, EventBuf* out);
// Runs automatic steps until a decision is needed or the game ends.
PendingDecision engine_advance(GameEngine* e, GameContext* ctx, EventBuf* out);
// Validates + applies a decision for the pending player, then returns; call
// engine_advance() again afterwards. Returns false (state untouched) if illegal.
bool engine_submit(GameEngine* e, PlayerID p, const PlayerDecision* d,
                   GameContext* ctx, EventBuf* out);
// Lets an existing mutating AI strategy make the pending decision in place.
void engine_run_ai(GameEngine* e, const StrategySet* s, GameContext* ctx, EventBuf* out);
```

Only four wait states exist (mulligan, attack, defense, discard-to-7). No
`*_RESOLVE` or `COMBAT_DISPLAY` phases (those in `ideas/2 …/unified_state_machine.txt`
mix UI concerns into the engine). Recall and cash-exchange sub-choices are **inside** a
single `GameMove`, so they are not engine states — the GUI stages them locally (§9.4).

### 7.3 Flow (must match today's `play_turn()` exactly)

```
SETUP (setup_game) ─► MULLIGAN_WAIT (player B; skip if configured off / AI does it inline)
  ─► BEGIN_TURN: turn++, turn_phase=ATTACK, player_to_move=attacker,
                 draw 1 (except player A on turn 1)
  ─► ATTACK_WAIT (attacker)
  ─► after attack: turn_phase=DEFENSE, player_to_move=defender
       if combat_zone[attacker].size > 0: DEFENSE_WAIT (defender) ─► COMBAT
       else skip to END_TURN
  ─► COMBAT: resolve_combat_with_details; if someone_has_zero_energy → GAME_OVER
  ─► END_TURN: player_to_move=attacker; collect_1_luna
       if hand > 7: DISCARD_WAIT (attacker)
  ─► SWITCH_PLAYER: change_current_player ─► BEGIN_TURN
```

**RNG-order constraint (the key correctness requirement)**: for AI-vs-AI games driven
through the engine, every `RND_*` call must happen in exactly the order `play_turn()` +
`apply_mulligan()` make them today. Then `./bin/oracle -a -p` stays byte-identical to
`bin/expectedresults.txt` — a strong, free regression test for the whole refactor.
Watch in particular: where mulligan sits relative to `gstate.turn` initialization (the
`stda_auto.c` uninitialized-read bug fixed during A10 is exactly this kind of ordering
subtlety), and `resolve_combat_with_details` vs `resolve_combat`.

### 7.4 AI decisions inside the driver

`engine_run_ai()` just calls the existing function pointer on `e->state`
(`attack_strategy[p]`, `defense_strategy[p]`, `mulligan_strategy[p]`, and
`discard_to_7_cards()` for discard), then diffs for events, then updates `phase`. No
strategy is modified. Note the existing `attack_phase()` sets `turn_phase=DEFENSE` /
`player_to_move` *after* the strategy returns; the driver does the same.

### 7.5 Migration path (keeps everything working at every step)

1. Implement the driver; add a hidden/dev switch making `stda.auto` run games through
   `engine_advance()` + `engine_run_ai()` instead of `play_turn()`.
2. Diff `-a -p` output against the baseline; iterate until identical. Also compare a few
   `-A`-agent matchups with fixed seeds, including a tree-search agent (A10/A11) — they
   clone state and are the most sensitive to phase-field values.
3. Switch `stda.auto` over; make `play_turn()` a thin wrapper on the driver (or delete it).
4. (Optional, later) Port CLI/TUI orchestrators onto the driver so their private copies
   of the flow disappear.

---

## 8. Layer 5 — The session (`roles/stda/stda_session.c`)

### 8.1 Messages

```c
typedef struct
{ VisibleGameState view;          // filtered for this client's player
  PendingDecision pending;        // kind + player; client checks player == self
  uint8_t legal_count;            // valid when pending.player == self and kind is
  GameMove legal[MOVE_GEN_MAX_MOVES]; //   ATTACK/DEFENSE
  uint8_t min_cards, max_cards;   // MULLIGAN / DISCARD_TO_7 constraints
  uint8_t event_count;
  GameEvent events[32];           // already filtered for this viewer
  bool rejected;                  // last submission was illegal (with reason code later)
} SessionUpdate;

typedef enum { CMD_SUBMIT, CMD_RESIGN, CMD_QUIT } SessionCommandType;
typedef struct { SessionCommandType type; PlayerDecision decision; } SessionCommand;
```

(`SessionUpdate` with 128 legal moves is a few KB — fine for a queue. Could drop the
legal list for non-decision updates.)

### 8.2 Session thread loop

```
engine_init
loop:
  pending = engine_advance(...)                 // automatic steps; collect events
  publish SessionUpdate to each human client    // view + events + pending + legal moves
  if game over: publish, wait for CMD_QUIT, exit
  if pending.player is AI:  engine_run_ai(...)  // may take seconds — GUI unaffected
  else: block on command queue (SDL_WaitCondition)
        CMD_SUBMIT → engine_submit; if false → publish {rejected=true}, wait again
        CMD_RESIGN/QUIT → end game / exit thread
```

### 8.3 Threading primitives (SDL3, portable to Android)

- `SDL_CreateThread` for the session; `SDL_Mutex` + `SDL_Condition` for the command
  queue (session blocks on it); a mutex-protected single-slot or small ring buffer for
  updates.
- Wake the GUI with a custom event: `SDL_RegisterEvents(1)` once, then
  `SDL_PushEvent()` from the session thread after publishing (thread-safe in SDL). The GUI
  handles that event in `SDL_AppEvent` by draining the update queue.
- `GameContext`/RNG and `GameEngine` are **touched only by the session thread**. The GUI
  only sees copies (`SessionUpdate`).

### 8.4 Client-side interface (what the GUI links against)

So the GUI never knows whether the game is local or remote:

```c
typedef struct SessionClient SessionClient;   // opaque
bool session_client_poll(SessionClient* c, SessionUpdate* out);  // non-blocking
void session_client_send(SessionClient* c, const SessionCommand* cmd);
void session_client_close(SessionClient* c);

SessionClient* session_local_start(const config_t* cfg, const PlayerConfig* pc);
// later: SessionClient* session_net_connect(const char* host, int port);
```

(This is the opaque-handle style from `ideas/8 …/game_loop_and_client_api_notes.md`,
chosen over exposed structs.)

### 8.5 Known wrinkles

- **Quitting during an AI think**: strategies can't be interrupted mid-search, so the
  GUI's quit waits for the current AI decision to finish (up to ~2 s with A14) before
  joining the thread. Acceptable for M1; a cooperative cancel flag checked by the
  tree-search agents' iteration loops is a later option.
- **Two humans on one device** (hot-seat): works — session publishes to "the local
  client" and the GUI shows whichever player is pending; hidden-hand etiquette (a
  "pass the device" screen) is a UX question for later.
- **AI-vs-AI watching in the GUI**: works with a spectator view (§5.3); might want a
  per-move delay there.

---

## 9. Layer 6 — The SDL3 GUI

### 9.1 Files (proposed)

- `src/roles/stda/stda_gui.c` — mode entry (`MODE_STDA_GUI`, already in `game_mode_t`
  and wired to a "not yet implemented" stub in `main.c`); pre-GUI player config can reuse
  `ui/shared/player_config.c`.
- `src/ui/gui/gui_app.c` — SDL main callbacks, window/renderer lifecycle.
- `src/ui/gui/gui_layout.c` — computes rectangles for every region from window size.
- `src/ui/gui/gui_render.c` — draws table, hands, zones, discard, info panels.
- `src/ui/gui/gui_input.c` — hit-testing + staging state machine → `PlayerDecision`.
- `src/ui/gui/gui_assets.c` — textures, fonts, runtime swap of background tile and font.
- `src/ui/gui/gui_log.c` — message log fed by events.

### 9.2 Main callbacks mapping

- `SDL_AppInit`: parse config, create window/renderer (`SDL_CreateWindowAndRenderer`,
  resizable; use logical presentation or compute layout from actual size), load assets,
  start the session.
- `SDL_AppEvent`: input (mouse/touch/keyboard) → staging; session wake-up event →
  drain updates; window resize → relayout; quit → `CMD_QUIT`.
- `SDL_AppIterate`: layout + render from the latest `SessionUpdate`. Nothing here waits.
  (Can skip redraws when nothing changed to save battery on Android.)
- `SDL_AppQuit`: close session client (joins thread), free assets.

### 9.3 Rendering

- Render **everything from the latest `VisibleGameState` each frame** (immediate-mode
  style); no retained scene graph. Cards moving between zones "for free".
- Background: `SDL_RenderTextureTiled()` with the current tile texture.
- Cards: champion PNG + text overlays (cost, dice, base attack) — the card layout spec in
  `oracle_sdl3_gui_plan.md` §4 is still useful input.
- Dice in the combat panel: one die-face image per die type used by `fullDeck[]`
  (check which dice exist — `defense_dice` values), rolled number drawn on top with
  SDL3_ttf.
- Text: SDL3_ttf `TTF_TextEngine` (`TTF_CreateRendererTextEngine`) + `TTF_Text` objects
  for cached glyph rendering. All strings via `LOCALIZED_STRING` (EN/FR/ES).
- Scaling: one high-res asset set, linear filtering (`SDL_SetTextureScaleMode`).

### 9.4 Input staging (per decision kind)

The GUI builds one complete `PlayerDecision` locally, then submits once.

| Pending | Interaction | Confirm enabled when |
|---|---|---|
| MULLIGAN | toggle 0-2 hand cards | always (0 = keep hand) |
| ATTACK | toggle 1-3 champions **or** select one draw/recall card (then choose Draw vs Recall + pick exactly N champions from a discard overlay) **or** select one cash card (then pick the champion to exchange); Pass button | staged selection equals a move in `legal[]` (canonical compare, §4.2) |
| DEFENSE | toggle 0-3 champions; Decline button | staged set ∈ `legal[]` |
| DISCARD_TO_7 | toggle exactly `hand-7` cards | count == required |

Clicking cards that can't be part of any legal move does nothing (derive clickability
from `legal[]`). Staged cards visibly raised/highlighted (the TUI's missing feature,
cheap here). Keyboard shortcuts from `oracle_sdl3_gui_plan.md` §3.2 can map onto the same
staging actions.

### 9.5 Layout

Compute region rectangles from the window size (like the TUI's responsive layout): viewer's
seat at the bottom (own hand), the other seat(s) placed around the table by a function of
the player count (today: one opponent at the top — hand back-sides with count, deck count,
discard, combat zone), middle = combat panel/log, side = info (energy, lunas, turn, whose
move). See §5.1a: loop over players, don't hardcode two sides.
**Clay** (single-header C flexbox-like layout) is an option if hand-computed rects get
painful; not needed up front. Touch-sized hit targets (≥ ~48 dp) with Android in mind.

### 9.6 Runtime settings

Background tile path, font path, font size — settable at runtime (a small settings
overlay or keyboard shortcuts) and optionally persisted (`ideas/7 config file`).
Font size change → `TTF_SetFontSize()` then rebuild cached `TTF_Text` objects; face
change → `TTF_CloseFont`/`TTF_OpenFont` + rebuild.

### 9.7 "Opponent is thinking"

When `pending.player != self`, show an indicator (text/spinner-free static label is fine)
and ignore hand clicks. Window stays fully responsive: this is the whole point of §8.

### 9.8 Build integration (must solve before the first GUI file lands)

Makefile auto-discovers `src/**/*.c`. Options:
1. Exclude `src/ui/gui/` + `stda_gui.c` from the default `SRCS` glob; add `make gui`
   building `bin/oracle-gui` (or `bin/oracle` with GUI) with
   `pkg-config --cflags --libs sdl3 sdl3-ttf sdl3-image`.
2. Or a `HAVE_SDL3` switch that `main.c` checks to wire `MODE_STDA_GUI`.
Recommendation: (1) + (2) together — default builds stay SDL-free (calibration boxes,
`release_tools`), GUI build opts in. **To verify**: SDL3/SDL3_ttf/SDL3_image package
availability on the current Kubuntu release (may need building from source or a PPA).

### 9.9 Android (later milestone)

SDL3's Android project template (Gradle + NDK) compiles the same C sources for arm64;
`SDL_IOFromFile` reads bundled assets from the APK transparently. Keep all asset loading
going through SDL's I/O (no raw `fopen` in GUI code) and paths relative to
`SDL_GetBasePath()` on desktop. The engine's `prng_seed.c` needs an Android-safe secure
seed path check (`getrandom()` is available on Android's bionic — verify).

---

## 10. Implementation roadmap

Each step ends in a working, regression-checked build. Follow the repo's usual
definition of done (≤35-line functions, `make format`, valgrind-clean, docs updated).

1. **`VisibleGameState` + `visibility_filter()`** (`src/visibility/`), plus a tiny unit
   test (`testsrc/test_visibility.c`): opponent hand never copied, counts correct,
   discards/combat zones copied. No behavior change anywhere.
2. **`PlayerDecision` + `decision_is_legal()`** (`src/actions/`), resolving the
   "exhaustive `MoveGenLimits`" and canonical-comparison questions (§4.2). Unit test with
   hand-built states.
3. **`GameEvent` + viewer filtering**; the before/after diff helpers (drawn cards,
   reshuffle, AI move reconstruction).
4. **Step driver** (`src/core/game_engine.c`), including moving mulligan orchestration
   into it. Drive `stda.auto` through it behind a switch; **`-a -p` must match
   `bin/expectedresults.txt` byte-for-byte**; also fixed-seed checks with A10/A11.
   Then switch `stda.auto` over permanently.
5. **Session** (`stda_session.c`) + `SessionClient` local implementation. Test headless
   first: a tiny test harness (or a `--stda.session-test` dev mode) that plays a human
   seat by submitting random legal decisions from the update's `legal[]`, on a second
   thread, to shake out queue/threading bugs (run under valgrind/helgrind or
   `-fsanitize=thread`).
6. **Build plumbing** (§9.8), `MODE_STDA_GUI` wiring, an SDL3 window that shows the
   table background and the text "Oracle" — prove the toolchain.
7. **GUI M1 (desktop)**: layout, render the view, message log, staging for attack/
   defense/mulligan/discard, combat panel with dice, opponent-thinking indicator, runtime
   font/tile swap. Human vs any AI (including A14 to prove responsiveness).
8. **GUI M2**: polish (keyboard shortcuts, tooltips, settings persistence, spectator
   AI-vs-AI view, rating tracking hook `--rating.track` like CLI/TUI).
9. **Android port**.
10. **(Future) network**: `NetSession` + server process running the session loop;
    serialization of `VisibleGameState`/`PlayerDecision`/`GameEvent`; then the security
    material from `ideas/8` (auth, sequence numbers, server-side RNG — already true by
    construction since only the session/server owns `GameContext`).
11. **(Future, optional)** strategy signature → `GameMove f(const VisibleGameState*, ...)`
    for AI-as-network-client; as a side benefit this proves agents can't read hidden
    info (agents have **not** been audited for that; irrelevant while AI runs inside the
    session on the full state).

---

## 11. Open questions (settle during implementation)

- `MoveGenLimits` "no cap" values for exhaustive legality enumeration (§4.2).
- Does `resolve_combat_with_details()` really preserve RNG order (baseline diff, §7.3)?
- Where exactly mulligan's RNG calls sit relative to setup/turn init today
  (`stda_auto.c`'s `apply_mulligan()`, and the CLI/TUI interactive mulligan paths).
- Spectator view in M1 or later?
- Hot-seat (two humans, one device): support in M1 or not?
- Minimum per-AI-move display delay: needed, and configurable?
- SDL3 packages on the current Kubuntu release vs. building from source.
- Whether CLI/TUI should eventually be ported onto the step driver (removes duplicated
  flow; not required for the GUI).

---

## 12. Status of the older notes

| File | Keep | Superseded by this file |
|---|---|---|
| `ideas/9 gui/oracle_sdl3_gui_plan.md` | Card layout/rendering (§4), font manager ideas (§5), asset pipeline/texture cache (§6), keyboard/touch input ideas (§3), desktop enhancements (§8) | Platform list (Windows/MSYS2, Arch, iOS — current targets are Kubuntu then Android; iOS/Windows explicitly not goals), Geany, "≤30 lines/≤500 lines" (now ≤35/100 per function, 1000 per file) |
| `ideas/9 gui/game_loop_engine_integration_notes.md` | "UI owns the loop" principle; one GUI for standalone + client | Standalone wrapper runs AI **synchronously in the frame update** (freezes the UI for search agents) → §8 session thread; standalone renders full `GameState*` → §5 view-only rendering; `Action*` → §4 |
| `ideas/2 …/unified_state_machine.txt`, `game_engine_impl.txt` | Pollable engine idea, `engine_advance`/`submit` shape | Fine-grained `*_RESOLVE`/`COMBAT_DISPLAY` phases → §7's four wait states; `Action*` → `PlayerDecision` |
| `ideas/2 …/game engine refactoring approach…md` | Separation of decision vs. application; "who uses apply" table | `UICallbacks` → §6 events-as-data; strategy signature change as **prerequisite** → deferred (§10 step 11); `draw_1_card()` return change → unnecessary (§6.2) |
| `ideas/8 client server/…Ideas 2 (consolidated).md` | Authoritative server model, 3-tier visibility table, message-type taxonomy, combat protocol sequence, security & validation (for the network milestone) | `Action`/`ActionType` → §4 (DRAW_CARD isn't a decision; DEFEND/DECLINE redundant; QUIT/REQUEST_GAMESTATE/sequence_num are protocol envelope); `VisibleGameState` (stale types, omits discard contents despite its own "public" tier) → §5 |
| `ideas/8 client server/game_loop_and_client_api_notes.md` | Opaque-handle API style (adopted in §8.4); poll-based client loop idea | Visibility enum (not needed); "strategy change is a prerequisite" → deferred |
