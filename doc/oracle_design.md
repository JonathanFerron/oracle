# Oracle: Technical Design Document

**Les Champions d'Arcadie / The Arcadian Champions of Light**

Technical design for Oracle, a fixed-pool strategic dueling card game implemented in
portable C.

**Scope of this document**: architecture and design rationale — *why* the code is shaped
the way it is. For day-to-day working conventions (build commands, code style, function/
file size limits) see `CLAUDE.md`. For complete game rules see `doc/game_rules_doc.md`.
For open work items see `doc/oracle_todo.md`; for phase-level sequencing see
`doc/oracle_roadmap.md`; for a dated history of finished work see `doc/changelog.md`.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Current Implementation Status](#2-current-implementation-status)
3. [Core Data Structures](#3-core-data-structures)
4. [GameContext Pattern](#4-gamecontext-pattern)
5. [Module Map](#5-module-map)
6. [Turn/Phase Flow](#6-turnphase-flow)
7. [Strategy Framework](#7-strategy-framework)
8. [Combat System](#8-combat-system)
9. [UI Architecture](#9-ui-architecture)
10. [Modes and Command Line](#10-modes-and-command-line)
11. [Planned Architecture](#11-planned-architecture)
12. [Key Design Decisions](#12-key-design-decisions)
13. [Known Architectural Gaps](#13-known-architectural-gaps)

---

## 1. Architecture Overview

### Design Principles

- **Function length**: target ≤35 lines, firm limit 100 (comments/whitespace excluded).
- **File length**: target ≤400 lines, soft limit ≤500, firm limit 1000.
- **Separation of concerns**: game logic, UI, and AI are independent; UI talks to game
  logic through explicit seams (`UiIO`, the interactive command grammar), never the
  reverse.
- **Testability**: the `GameContext` pattern enables dependency injection (seedable RNG).
- **Extensibility**: the strategy framework enables pluggable AI via function pointers.
- **Manual implementation preferred**: duplication is acceptable for readability over
  macro magic.

### Current Architecture

```
main.c (mode dispatch, config lifecycle)
  └─ cmdline.c (getopt_long_only-based arg parsing → config_t)
  └─ roles/stda/{stda_auto,stda_cli,stda_tui}.c (mode entry points)
       └─ core/game_state.c (setup_game)
       └─ core/turn_logic.c (begin_of_turn → attack_phase → defense_phase →
                              resolve_combat → end_of_turn)
            └─ core/card_actions.c, core/combat.c, core/combo_bonus.c
       └─ ai_strat/* (AttackStrategyFunc / DefenseStrategyFunc per player)
       └─ ui/{cli,tui}/* behind ui/shared/ui_io.h (UiIO) and
            ui/interactive/game_commands*.c (shared command grammar)
```

`stda.sim`, `server`, and all `client.*` modes are wired into `main.c`'s dispatch switch
but each just prints "not yet implemented" — see §10 and §11.

---

## 2. Current Implementation Status

### What's Working

**Core Game Engine**: 120-card deck with full attributes; game state management (energy,
cash, hands, decks); combat resolution with dice rolling; combo bonus calculations (all 3
deck types); turn-based flow; card playing (champions, draw/recall, cash); recall
mechanic; interactive cash-exchange champion choice; detailed combat-results display;
discard-pile inspection.

**AI Framework**: strategy function-pointer system; `ai_strat_random` is the only
functional strategy (baseline, ~50% win rate in mirror matches, as expected). Eleven
further agents (`ideas/A1`–`ideas/A11`, one folder per agent, numbered to match the
agent's `AIStrategyType` enum ordinal) are registered as named stub menu entries — see §7.

**User Interface**: command-line argument parsing (`getopt_long_only`, short/`--long`/
`--long.form` spellings, hidden `--oracle-complete` completion helper); CLI mode (human
vs AI, human vs human, AI vs AI) with ANSI color and UTF-8 symbols; TUI mode (ncurses),
playable human-vs-AI as of Milestone 2 (2026-07-23) plus a UI/playability polish pass
(2026-07-24) — see `doc/changelog.md`; player configuration and assignment; localization
(English, French, Spanish).

**Infrastructure**: `GameContext` pattern for RNG and config; compile-time debug macro
system; Mersenne Twister RNG; fixed-size collections for deck/hand/discard/combat zone
(no linked lists — see §12); PRNG seed management (random or specified, secure on both
platforms).

### What's Missing

**AI Strategies**: everything past Random — see §7 and `doc/oracle_todo.md`.

**Features**: save/load game state, configuration file system, CSV export, rating
system, network multiplayer, GUI mode, `stda.sim` mode. All designed, none built — see
§11.

---

## 3. Core Data Structures

### Card Structure

```c
// In game_types.h
struct card
{ CardType card_type;
  uint8_t cost;

  // Champion fields
  uint8_t champion_id;
  uint8_t defense_dice;        // d4, d6, d8, d12, d20
  uint8_t attack_base;         // 0-5
  ChampionColor color;         // RED, INDIGO, ORANGE
  ChampionSpecies species;     // 15 species total
  ChampionOrder order;         // ORDER_A through ORDER_E

  // Draw card fields
  uint8_t draw_num;            // 2 or 3
  uint8_t choose_num;          // recall count (exact, mandatory)

  // Calculated efficiency values
  float expected_attack;
  float expected_defense;
  float attack_efficiency;
  float defense_efficiency;
  float power;                 // overall card value

  // Cash card fields
  uint8_t exchange_cash;       // 5 lunas
};
```

**Full Deck**: 120 cards — 102 champions (34 per color, 5 orders, 15 species), 9 draw-2
cards (cost 1), 6 draw-3 cards (cost 2), 3 cash-exchange cards (cost 0, give 5 lunas).
Lives in `fullDeck[120]`, `src/core/game_constants.c`.

### Game State Structure

```c
// In game_types.h
struct gamestate
{ PlayerID current_player;              // PLAYER_A or PLAYER_B
  uint16_t current_cash_balance[2];     // Luna for each player
  uint8_t current_energy[2];            // Health for each player
  bool someone_has_zero_energy;         // Game end flag

  struct deck_stack deck[2];            // Draw piles (40 cards each)
  Hand hand[2];
  Discard discard[2];
  CombatZone combat_zone[2];

  uint16_t turn;                        // Turn counter (1-based, both players)
  GameStateEnum game_state;             // PLAYER_A_WINS, PLAYER_B_WINS, DRAW, ACTIVE
  TurnPhase turn_phase;                 // ATTACK or DEFENSE
  PlayerID player_to_move;              // Who makes next decision
};
```

**Key design decisions**: separate deck/hand/discard/combat per player; every collection
is a **fixed-size array** (deck: LIFO stack, max 40; hand: max 15; discard: max 40;
combat zone: max 3) — the original circular-linked-list implementation for
hand/discard/combat zone has been fully migrated away; energy starts at 99, first to 0
loses.

### Data Structure Implementations

- **Deck stack** (`structures/deckstack.c`): fixed-size array, LIFO push/pop, draw pile
  only.
- **Hand / Discard / CombatZone** (`structures/card_collection.c`): fixed-size arrays
  with add/remove-by-index operations (`Hand_add`, `Hand_remove`, `Discard_add`, etc.).

---

## 4. GameContext Pattern

### Purpose

Eliminate global state and enable testability by passing all "context" (RNG, config,
future network state) through a single pointer.

```c
// In game_context.h
typedef struct
{ MTRand rng;              // Mersenne Twister state
  config_t* config;        // Runtime configuration
} GameContext;
```

```c
// Usage
GameContext* ctx = create_game_context(cfg);
setup_game(initial_cash, &gstate, ctx);
play_turn(&gstats, &gstate, strategies, ctx);
draw_1_card(&gstate, player, ctx);
destroy_game_context(ctx);
```

**Benefits**: testability (inject a mock/seeded RNG), thread safety (each thread its own
context), extensibility (add context fields without changing signatures), no hidden
globals.

**Migration status**: done — every game function that needs randomness or config takes
`GameContext*`; no global RNG remains anywhere in `src/`.

---

## 5. Module Map

```
src/
├── core/         game_types.h (start here), game_constants.c/h, game_state.c,
│                 card_actions.c, combat.c, combo_bonus.c, turn_logic.c,
│                 game_context.c/h
├── ai_strat/     ai_strategy.h/.c (StrategySet framework), ai_strat_random.c
│                 (functional), ai_strat_{balancedrules1,heuristic1,simplemc1,
│                 ismcts1}.c (design-note stubs, one per future agent)
├── roles/stda/   stda_auto.c (batch sim), stda_cli.c (interactive loop glue),
│                 stda_tui.c + stda_tui_interactive.c (TUI setup/loop + human-turn
│                 handlers)
├── ui/cli/       cli_display.c (status/turn), cli_action_display.c (recall/cash/
│                 combat/discard display), cli_input.c (CLI-only command intercepts,
│                 delegates the rest), cli_io.c (CLI's UiIO backend), cli_game.c
│                 (loop wiring)
├── ui/interactive/ game_commands.c + game_commands_cards.c: UI-agnostic interactive
│                 command grammar shared between CLI and TUI, each function taking a
│                 UiIO* instead of touching stdio/ncurses directly
├── ui/shared/    player_config.c/.h, player_selection.c/.h, localization.h,
│                 ui_constants.h, ui_io.h (the UiIO seam)
├── ui/tui/       tui_render.c (screen lifecycle/layout/status bars),
│                 tui_render_playarea.c (hand/deck/combat-zone drawing),
│                 tui_render_io.c (message log, input predicates, command line),
│                 tui_input.c/h (TUI's UiIO backend)
├── ui/gui/       not implemented — planning notes only
├── ui/simulation/ not implemented — planning notes only
├── structures/   deckstack.c (fixed-size LIFO), card_collection.c (fixed-size
│                 hand/discard/combat-zone)
├── util/         mtwister.c, rnd.c, prng_seed.c, debug.h
├── main/         main.c (mode dispatch), cmdline.c (arg parsing → config_t)
└── actions/, rating/, visibility/  planning notes only, no implementation
```

**Directories that don't exist yet**: `deck_formats/`, `game_rules/`, `network/`,
`persistence/`, `config/`, `platform/` — created only when their first real file lands.
See `ideas/2 engine and action system design/
target_folder_structure_v4.md` for the full target layout and which `ideas/` folder owns
each.

### Layering rules

- `core/` depends only on `structures/` and `util/`; it never includes anything from
  `ui/`, `ai_strat/`, or `roles/`.
- `ai_strat/` depends on `core/` (reads/mutates `gamestate` directly) and nothing in
  `ui/`.
- `roles/stda/` orchestrates `core/` + `ai_strat/` + `ui/`; it's the only layer allowed
  to know about all three.
- `ui/cli/` and `ui/tui/` each implement the `UiIO` seam (`ui/shared/ui_io.h`) and call
  into `ui/interactive/` for command grammar shared between them; board/state rendering
  is deliberately *not* shared — each UI renders its own way.
- Some file/module names in older notes (e.g. `strat_random.c`, a flat `src/*.c` layout)
  reflect a pre-reorg layout; trust the tree above over anything that disagrees.

---

## 6. Turn/Phase Flow

```
Turn N (Active Player = Current Player)
├── 1. begin_of_turn()
│   ├── Increment turn counter
│   ├── Set phase = ATTACK
│   ├── Draw 1 card (except first player, turn 1)
│   └── Set player_to_move = current_player
│
├── 2. attack_phase()
│   ├── Attacker plays 0-3 champions, OR 1 draw/recall/cash card, OR passes
│   └── Set phase = DEFENSE, player_to_move = opponent
│
├── 3. defense_phase() [if combat]
│   ├── Defender plays 0-3 champions OR declines
│   └── resolve_combat()
│       ├── attack = Σ(base + roll(dice)) + combo_bonus
│       ├── defense = Σ(roll(dice)) + combo_bonus   (no attack_base on defense)
│       ├── damage = max(attack - defense, 0)
│       └── Clear combat zones
│
├── 4. end_of_turn()
│   ├── Collect 1 luna (current_player)
│   ├── Discard to 7 cards (if hand > 7)
│   └── Switch current_player
│
└── Check win condition: energy[defender] == 0 → set game_state, end game
```

**Edge cases**: first player on turn 1 doesn't draw; empty deck reshuffles discard to
form a new deck; `setup_game()` does **not** initialize `turn_phase`/`player_to_move` —
see §13.

---

## 7. Strategy Framework

```c
// In ai_strategy.h
typedef void (*AttackStrategyFunc)(struct gamestate* gstate, GameContext* ctx);
typedef void (*DefenseStrategyFunc)(struct gamestate* gstate, GameContext* ctx);

typedef struct
{ AttackStrategyFunc attack_strategy[2];   // one per player
  DefenseStrategyFunc defense_strategy[2];
} StrategySet;
```

Strategies modify `gstate` directly (play cards, update combat zone) — there is no
intermediate action/move representation yet (see §11).

### Adding a new strategy

As of `A1` (2026-08-21), dispatch is a single table-driven registry
(`src/ai_strat/ai_strategy.c`'s `STRATEGY_REGISTRY[]`), not per-file hardcoding:

1. Implement `<name>_attack_strategy()` / `<name>_defense_strategy()` in
   `src/ai_strat/ai_strat_<name>.c`.
2. Add one line to `ai_strategy.c`'s `STRATEGY_REGISTRY[]` table (plus the include).
   `ai_strategy_is_implemented()` and `set_player_strategy_by_type()` are the single
   dispatch point every mode (`stda.auto`, CLI, TUI) already goes through, so that one
   line flips the interactive menu's "not yet implemented" label to "available" and
   makes `-Aa`/`-Ab` accept the new shorthand everywhere, with no further edits.
3. Add its shorthand to `AI_STRATEGY_SHORTHANDS[]` (`src/ui/shared/player_config.c`) if
   not already present — the enum slot, menu label, and display name are typically
   already there from the initial folder-sort/enum pass.

See `doc/oracle_todo.md`'s "Checklist: Adding a New AI Strategy" for the full mechanical
steps.

### Planned agent order

The `AIStrategyType` enum and CLI menu (`src/ui/shared/player_config.h`) list all eleven
planned agents, each commented with its `ideas/A#` folder:

| Enum | Status | `ideas/` folder |
| --- | --- | --- |
| `AI_STRATEGY_RANDOM` | **implemented** | — |
| `AI_STRATEGY_VALUE_BASED` | **implemented** (2026-08-21) | `A1` (The Apprentice) |
| `AI_STRATEGY_COMBO_THRESHOLD` | **implemented** (2026-08-22) | `A2` (The Showboat) |
| `AI_STRATEGY_BOREALIS` | stub — active work | `A3` — the rating-scale benchmark agent |
| `AI_STRATEGY_BALANCED` | stub | `A4` (Bean Counter) |
| `AI_STRATEGY_HEURISTIC` | stub | `A5` (ε-γ-δ) |
| `AI_STRATEGY_TACTICAL` | stub | `A6` (Pressure Cooker) |
| `AI_STRATEGY_HYBRID_HBT` | stub | `A7` (The Grandmaster — synthesis of `A4`/`A5`/`A6`) |
| `AI_STRATEGY_SIMPLE_MC` | stub | `A8` (The Soothsayer) |
| `AI_STRATEGY_HBT_2PLY` | stub | `A9` (Grandmaster II) |
| `AI_STRATEGY_ISMCTS` | stub | `A10` (The Omniscient) |
| `AI_STRATEGY_ISMCTS_NN` | stub | `A11` (AlphaOracle Prime) |

Implementation order is `A1 → A2 → A3`, not just easiest-first: the rating system
(`ideas/5`) needs the Borealis benchmark agent (`A3`), which itself needs `A1`–`A2` to
exist for comparison — both now do. General info and calibration tooling live in
`ideas/G1 AI agent general info/` and `ideas/G2 ai agent parameters storing and
optimization/` — support material, not agents, so they carry no enum entry and no longer
occupy a slot on the A-line.

### Random Strategy (reference implementation)

`ai_strat_random.c`: attack plays any affordable card (special case: skips cash cards if
no champions are in hand); defense has a 47% chance to play a random affordable champion.
Deliberately not meant to be strong — it's the baseline other agents are measured
against.

---

## 8. Combat System

### Combo Bonus Calculation

```c
// In combo_bonus.h
typedef struct
{ ChampionSpecies species;
  ChampionColor color;
  ChampionOrder order;
} CombatCard;

int calculate_combo_bonus(CombatCard* cards, int num_cards, DeckType deck_type);
```

**Priority order** (Random deck): species matches (2+ same species) > order matches (2+
same order, no species match) > color matches (2+ same color, no species/order match).
Monochrome/custom decks route through a simplified `calc_prebuilt_bonus()`.

**Example**: 3 Humans (same species): +16. 2 Humans + 1 Elf (same order A): +14. 2
Humans + 1 Hobbit (same color): +13. 2 Orange cards (no species/order match): +5.

### Combat Resolution

```c
// In combat.c
void resolve_combat(struct gamestate* gstate, GameContext* ctx)
{ int16_t total_attack = calculate_total_attack(gstate, attacker, ctx);
  int16_t total_defense = calculate_total_defense(gstate, defender, ctx);
  int16_t damage = max(total_attack - total_defense, 0);
  gstate->current_energy[defender] -= damage;

  if(gstate->current_energy[defender] == 0)
  { gstate->someone_has_zero_energy = true;
    gstate->game_state = (attacker == PLAYER_A) ? PLAYER_A_WINS : PLAYER_B_WINS;
  }
  clear_combat_zones(gstate, ctx);
}
```

`resolve_combat_with_details()` is the same math with per-champion roll/combo breakdown
captured for display — used whenever either combatant is human; `stda_auto` always uses
plain `resolve_combat()` so its RNG-dependent output stays untouched.

---

## 9. UI Architecture

### The `UiIO` seam

`ui/shared/ui_io.h` defines a struct of function pointers (read a line, print a message,
etc.) that lets `ui/interactive/game_commands.c`/`game_commands_cards.c` implement the
attack/defense command grammar, champion-play validation, recall, and cash-exchange
**once**, shared between CLI and TUI. `ui/cli/cli_io.c` and `ui/tui/tui_input.c` are the
two concrete backends. Board/state rendering is *not* part of this seam — each UI
renders its own way (`ui/cli/cli_display.c`+`cli_action_display.c` vs
`ui/tui/tui_render*.c`).

### CLI mode

Command grammar: `cham 1 2 3` (play champions at hand indices 1,2,3 — 1-based for the
user, converted to 0-based internally), `draw 2` (play draw/recall card at index 2),
`cash 1` (play cash-exchange card at index 1), `pass`, plus CLI-only diagnostics `gmst`
(status), `shod` (discard detail), `help`. Player configuration flow: type selection
(human/AI/mixed), names, AI strategy menu, assignment mode (direct/inverted/random).

### TUI mode

ncurses, responsive to terminal size (≥100×30, `KEY_RESIZE`-aware). Milestone 1
(2026-07-14): display skeleton, AI-vs-AI, one turn per keypress. Milestone 2
(2026-07-23): real human-vs-AI play — pre-ncurses player configuration (reuses the CLI's
menu flow), `TAB`-toggled PLAY mode (digit-staging champion selection, `P` to pass) and
COMMAND mode (same grammar as CLI, via the `UiIO` seam), recall, cash exchange,
mulligan, discard-to-7, live combat-result display. AI-vs-AI still uses the original
Milestone-1 `play_turn()` fast path, not the interactive per-phase orchestrator. A
UI/playability polish pass (2026-07-24) tuned layout, message routing, and card
formatting — see `doc/changelog.md` for the full breakdown.

**ncurses/`ChampionColor` collision**: ncurses' `COLOR_RED` etc. collide with this
codebase's own `ChampionColor` enum (`COLOR_RED`/`COLOR_INDIGO`/`COLOR_ORANGE` in
`game_types.h`) — only `COLOR_RED` actually overlaps, but enough to silently corrupt the
enum if `<ncurses.h>` is included before `game_types.h` in the same translation unit.
`tui_render.h` avoids the whole problem by never including `<ncurses.h>` (it forward-
declares `WINDOW` as `struct _win_st*`); `tui_render.c` includes `game_types.h` first,
then `<ncurses.h>`, then immediately `#undef COLOR_RED`, using its own `NC_RED`/
`NC_GREEN`/etc. constants for `init_pair()`. Keep this pattern anywhere else that needs
both ncurses and `ChampionColor`.

### Localization

Macro-based, language enum-indexed:

```c
#define LOCALIZED_STRING(en, fr, es) \
    ((const char*[]){en, fr, es}[cfg->language])
```

Every user-facing string in the CLI and TUI goes through this (or the explicit-language
`LOCALIZED_STRING_L` variant). English (default), French, Spanish.

---

## 10. Modes and Command Line

**Entry point**: `main.c` → `parse_options()` (`cmdline.c`, `getopt_long_only`-based) →
dispatch switch on `config_t.mode`.

| Mode | Flag(s) | Status |
| --- | --- | --- |
| `stda.auto` | `-a` / `--stda.auto` | implemented (batch AI-vs-AI simulation) |
| `stda.cli` | `-l` / `--stda.cli` | implemented (interactive CLI) |
| `stda.tui` | `-t` / `--stda.tui` | implemented (ncurses TUI, human-vs-AI) |
| `stda.sim` | `-s` / `--stda.sim` | stub — prints "not yet implemented" |
| `stda.gui` | `-g` / `--stda.gui` | stub |
| `server` | `-S` / `--server` | stub |
| `client.{sim,cli,tui,gui}` | `-C/-L/-T/-G` | stub |

Other options: `-h/--help`, `-v/--verbose`, `-V/--version`, `-n/--numsim=N`,
`-u/--ui.lang=[en|fr|es]`, `-p/--prng.seed=[SEED]` (random if omitted), `-i/-o` for
input/output file redirection, `-A/--ai=<code>` for AI agent shorthand. Every option has
both a single-letter and a `--long.form` spelling (both under `getopt_long_only`).

**Hidden completion helper**: `--oracle-complete[=WHAT]` dumps completion candidates
(option spellings, `=agents` for `-A` shorthand codes, `=langs` for language codes) and
exits — intentionally absent from `print_usage()`/`--help`, used by
`tools/oracle-completion.bash`.

---

## 11. Planned Architecture

Long-horizon designs live in `ideas/`; this section is pointers only, not a duplicate
spec.

- **Engine state machine, action structs, UI callbacks** — `ideas/2 game engine
  refactoring for GUI and network support/`. Would replace the current
  "strategy mutates `gstate` directly" model with a pollable state machine
  (`engine_step()`/`engine_run_until_input()`) and `Action*` objects, enabling the same
  engine to serve blocking (CLI/TUI/server) and event-driven (GUI) loop styles, and
  giving client/server a serializable action format. Not started.
- **Client/server** — `ideas/8 client server/`. Server owns full `GameState` and
  validates actions; clients hold a `VisibleGameState` with hidden information filtered
  out. Not started; don't scaffold `sh_`/`sr_`/`cl_`/`pr_`-style modules ahead of this.
- **SDL3 GUI** — `ideas/9 gui/oracle_sdl3_gui_plan.md`. Long-horizon; ignore unless a
  task specifically targets it.
- **Bradley-Terry rating system** — `ideas/5 rating system/` (v2 spec). Each AI has a
  rating (1–99, displayed) and internal strength; the Borealis (`A4`) agent is the
  rating-50 benchmark. Needs `A1`–`A4` implemented first.
- **CSV export** — `ideas/4 match results export/`. Per-game and summary CSVs for
  external (R/Python) analysis of simulation runs. Planned to ride along with the
  `stda_auto.c` split (see `ideas/2 …/stda_auto_split_plan.md`) since both touch the
  same per-game loop.
- **Save/load, config file** — `ideas/6 save and load gamestate/`,
  `ideas/7 config file/`. Explicitly back-burnered.

---

## 12. Key Design Decisions

**Why `GameContext`?** A global `MTwister_rand_struct` made testing difficult; passing a
context pointer everywhere makes RNG injectable and the code thread-safe/extensible.

**Why function pointers for AI?** Pluggable strategies without switch statements;
`StrategySet` holds one attack/defense pair per player; adding an AI is additive, no
existing code changes.

**Why separate deck types?** Random/monochrome/custom decks need different combo rules;
passing `DeckType` into `calculate_combo_bonus()` keeps one call site handling all three.

**Why fixed arrays for every collection?** The original design used a stack for the deck
and a circular linked list for hand/discard/combat zone. That migration is now
**complete** — every collection (deck, hand, discard, combat zone) is a fixed-size array
(`structures/deckstack.c`, `structures/card_collection.c`), chosen for simplicity and
cache-friendliness over the marginal flexibility linked lists offered; max sizes are
generous enough (40/15/40/3) that overflow isn't a practical concern.

**Why `PRNG` seed in `config_t`?** It's a configuration setting controlling program
behavior — one source of truth, passed once into `create_game_context()`.

**Why `UINT8_MAX` as the cash-exchange "not found" sentinel?**
`select_champion_for_cash_exchange()` (AI-only heuristic in `card_actions.c`) used to
return `0` to mean "no champion found," which is ambiguous with champion index `0` being
a real, valid selection. Fixed to return `UINT8_MAX` instead. This changed `stda_auto`'s
RNG-dependent play sequence (different hand state after the fix fires), so
`bin/expectedresults.txt` was regenerated at the same time it landed — if `-a -p` output
ever diverges from that file again, check first whether it's a deliberate change
(regenerate the baseline, document it) versus a real regression.

---

## 13. Known Architectural Gaps

(See `doc/oracle_todo.md` for the full actionable backlog — this is a short "don't be
surprised" list, not a duplicate of it.)

- `setup_game()` (`game_state.c`) does not initialize `gstate->turn_phase` /
  `player_to_move` — only `begin_of_turn()` (the first thing `play_turn()`/
  `attack_phase()` call) does. The CLI never notices because it always runs
  `begin_of_turn()` before displaying anything; `stda_tui.c`'s `tui_setup()` draws once
  before the first `play_turn()` call, so it explicitly sets both fields itself (found
  via a valgrind uninitialized-read report during TUI Milestone 1 verification).
- `select_champion_for_cash_exchange()` (AI-only heuristic) is strategy-shaped logic
  living in `card_actions.c` rather than `ai_strat/` — architectural boundary violation,
  low impact, to be moved when smarter AIs need it.
- Config is scattered across `cmdline.c` (parsing), `main.c` (cleanup), `stda_auto.c`/
  `stda_cli.c` (usage) rather than centralized.
- Client/server and GUI modes are placeholders only; `stda.sim` likewise. `stda.tui` is
  real (§9).
- TUI still lacks: visual highlighting of staged cards directly in the hand display
  (currently a `[n,m]` list in the command-line row instead), a help overlay, and
  TUI↔SIM mode switching.

---

## Development Environment

**Primary platform**: Kubuntu Linux, GCC, GNU Make, editor Kate. Cross-platform
portability (MSYS2/Windows) remains a goal and is kept working (`#ifdef _WIN32` blocks
for UTF-8 console setup and `prng_seed.c`'s platform-specific secure RNG), but Kubuntu is
the active target.

**Build system**: GNU Make with automatic source discovery
(`SOURCES := $(shell find $(SRCDIR) -type f -name *.c)`). `make` (default build),
`make clean`, `make debug` (`-O0 -DDEBUG -DDEBUG_ENABLED=1`), `make test_combo`/
`test_recall`/`test_cash_exchange`/`test_stda_auto`, `make format` (astyle,
`ideas/` excluded), `make help`.

**Compiler**: `gcc -Wall -std=c23`. Default flags `-g -Og -Wall -std=c23`.

**Code formatting**: astyle-driven (`.astylerc`), not hand K&R/Allman — run-in braces,
2-space indent, pointer-aligned-to-type (`int* ptr`).

---

*Last Updated: August 2026*
