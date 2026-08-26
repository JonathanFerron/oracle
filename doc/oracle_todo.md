# Oracle Development TODO

**Scope of this document**: actionable near-term checkboxes. For phase-level ordering
and long-horizon vision see `doc/oracle_roadmap.md`. For architecture/design rationale
see `doc/oracle_design.md`. For a dated history of finished work see `doc/changelog.md`.

**Quick Status**: Core game loop, all interactive-mode features (recall, cash exchange,
mulligan, discard-to-7, combat/discard display), the source folder structure cleanup, and
TUI Milestones 1–2 + polish pass are complete — see `doc/changelog.md`. `A1` Value Based
("The Apprentice", 2026-08-21), `A2` Combo Threshold ("The Showboat", 2026-08-22,
calibrated), `A3` Borealis (the Bradley-Terry benchmark, 2026-08-23, calibrated), `A4`
Balanced Rules ("Bean Counter", 2026-08-24, calibrated, measured rating 36 — below the
Borealis anchor, see `doc/changelog.md`), `A5` Heuristic ("Eps-Gam-Del", 2026-08-25,
calibrated, measured rating 60 — above the Borealis anchor, the first agent to clear
it), `A6` Tactical ("Pressure Cooker", 2026-08-25, calibrated, measured rating 52 —
also above the Borealis anchor, see `doc/changelog.md`), `A7` Hybrid HBT
("The Grandmaster", 2026-08-25, calibrated, measured rating 62 — the highest measured
rating so far, above all three agents it combines (`A4`/`A5`/`A6`) in the same
roster-wide fit despite a pairwise loss to `A5` specifically, see `doc/changelog.md`),
and `A8` Simple Monte Carlo ("The Soothsayer", 2026-08-25, calibrated, measured rating
35 — below the Borealis anchor and not raised by extra rollout budget, a diagnosed and
honestly-reported ceiling rather than a defect, see `doc/changelog.md`)
are implemented, and the
Bradley-Terry rating system itself (2026-08-23, `src/rating/`) is now built on top of
them — see `doc/oracle_roadmap.md`'s "Next Up" for what's next (`A9`). The strategy-set
build sites
also gained a shared `AIStrategyType -> function pointer` registry (`ai_strategy.c`) as
part of `A1` -- see "Checklist: Adding a New AI Strategy" below, which now reflects that
mechanism rather than the old per-file hardcoding. As of `A3`, that registry also carries
optional per-agent `mulligan_strategy[]`/`discard_strategy[]` hooks (`ai_strategy.h`),
defaulting to the shared power-based heuristic (`ai_strat_lib_heuristics.c`).

---

## Next Up

See `doc/oracle_roadmap.md` — this file intentionally doesn't duplicate that ordering.
The mechanical steps for implementing whichever agent is next are in "Checklist: Adding a
New AI Strategy" below.

**Future `src/` directories, created only when their first real file lands** (tracked
here so it isn't lost between sessions — see `ideas/2 engine and action system design/
target_folder_structure_v4.md`'s ownership table for the full picture):

- `deck_formats/` — draft/deck-format feature (`ideas/10 Draft Format and Game Depth
  Addition Ideas/`)
- `game_rules/` — game-engine refactor needs a home for rules data separate from `core/`
- `network/` — client/server (`ideas/8 client server/`)
- `persistence/` — save/load game state (`ideas/6 save and load gamestate/`)
- `config/` — configuration file system (`ideas/7 config file/`)
- `platform/` — if/when platform-specific code (beyond the current `#ifdef _WIN32`
  blocks) grows enough to warrant its own directory

**Back burner (explicitly deferred)**: save/load game state
(`ideas/6 save and load gamestate/`), configuration file system
(`ideas/7 config file/`). Also noted as a separate future project (2026-08-22, not
started): investigate first-player-vs-second-player advantage (visible across multiple
agent matchups during `A2` measurement -- e.g. `combo` beat `rand` 84.85% seated as
Player A vs 91.11% seated as Player B, `n=10000` each; `A1` showed the same pattern at
90.1%/94.5%) and whether tuning game-rule parameters such as the mulligan card count
(`AVERAGE_POWER_FOR_MULLIGAN`'s `max_cards = 2` in `apply_mulligan()`, `stda_auto.c`)
changes it -- these are genuine game-design levers, not just engine bugs, since the
game rules are original and adjustable.

---

## Core Game Logic (`src/core/`)

- [ ] Better error handling — consistent error enum instead of ad hoc `bool`/`printf`:
  ```c
  typedef enum
  { GAME_OK = 0,
    GAME_ERR_EMPTY_DECK,
    GAME_ERR_INSUFFICIENT_CASH,
    GAME_ERR_INVALID_CARD,
    GAME_ERR_ILLEGAL_MOVE
  } GameError;

  GameError DeckStk_pop_safe(struct deck_stack* deck, uint8_t* out);
  ```
- [ ] Add `DeckStk_size()` helper, `DeckStk_peek_at(index)` for debugging
  (`structures/deckstack.c`)

## Config Structure Scattered

Configuration handling is split across `cmdline.c` (parsing), `main.c` (cleanup),
`stda_auto.c`/`stda_cli.c` (usage). Centralize in a new `config.h`/`config.c` once the
config-file system (`ideas/7 config file/`) is picked up — not before, to avoid building
the centralization twice.

## Magic Numbers

```c
// stda_auto.c - BAD
if(genRand(&MTwister_rand_struct) > 0.47) return;

// GOOD
#define DEFENSE_PROBABILITY 0.47  // Tunable strategy parameter
if(genRand(&MTwister_rand_struct) > DEFENSE_PROBABILITY) return;
```

---

## AI Strategies (`src/ai_strat/`)

See `doc/oracle_roadmap.md`'s "Phase: AI Development" for the full agent ladder and
`ideas/A1`–`ideas/A11` for per-agent design notes (`A#` matches each agent's
`AIStrategyType` enum ordinal; see `ideas/G1 AI agent general info/oracle_ai_agent_names.md`
for the canonical roster). Below are the two agents with enough design-note detail
already sketched out to break into sub-tasks.

- [x] **Agent-specific discard-to-7 / mulligan hooks** — done 2026-08-23, landed alongside
  `A3` as planned. `StrategySet` (`src/ai_strat/ai_strategy.h`) gained optional
  `mulligan_strategy[2]`/`discard_strategy[2]` function-pointer slots; `ai_strategy.c`'s
  `STRATEGY_REGISTRY` fills any unset slot with the shared power-based default (moved
  verbatim into new `src/ai_strat/ai_strat_lib_heuristics.c/.h`), so Random/`A1`/`A2` stay
  byte-identical (`bin/expectedresults.txt` reverified unchanged). `A3` Borealis is the
  first agent to override them, protecting held combo pieces per its handout §7. See
  `doc/changelog.md`, 2026-08-23.
  - [ ] **`exchange_select` was *not* added** — deliberately deferred, not forgotten.
    `select_champion_for_cash_exchange()` is called from inside each agent's own
    `*_attack_strategy()` (`ai_strat_random.c`, `ai_strat_combo_threshold.c`), which only
    receive `(gstate, ctx)` — no `StrategySet*` in scope. Adding this hook would mean
    widening `AttackStrategyFunc`'s signature for every existing agent, for a feature
    nothing has needed yet. Revisit if a future agent's cash-exchange choice actually
    needs to differ from the shared lowest-`power` heuristic.

- [ ] **Calibration driver bug found during `A3`'s calibration, not yet fixed in
  `aicalibsrc/value/`/`aicalibsrc/combo/`.** `run_many()`'s `ProcessPoolExecutor` +
  `as_completed()` returns results in completion order, not submission order; both
  drivers' `sweep`/`selfplay` commands (plus A1's `selfplay` specifically) reattach
  external per-job metadata by list position afterward, which silently mismatches under
  real parallelism. `calibrate_borealis.py` was fixed (tag results at the source via
  `run_match(**tags)` instead); `calibrate_valuebased.py`/`calibrate_combo_threshold.py`
  were left as-is (out of scope for `A3`'s session). This casts real doubt on A1's shipped
  `VB_COST_FLOOR`/`VB_DEFEND_THRESHOLD` specifically, since those were chosen via
  `selfplay` (the vulnerable path) — a re-run with the fix, and likely a re-calibration,
  is worth doing before those values are trusted further. A2's shipped `CT_DEFAULTS` were
  chosen via `optimize` (not vulnerable — see `doc/changelog.md`, 2026-08-23) and are fine
  as shipped. `calibrate_balanced.py` (`A4`, 2026-08-24) was written with the fix from the
  start, so it's not affected — this item is now specifically about porting the fix into
  the two older drivers.

- [ ] **`aicalibsrc/*/calibrate_*.py`'s `DEFAULTS` dicts drift from their shipped C
  constants.** Noticed while writing `aicalibsrc/borealis/`: both `calibrate_valuebased.py`
  and `calibrate_combo_threshold.py` hardcode the *pre-calibration* parameter values in
  their own `DEFAULTS` dict (used as the baseline every `sweep`/`optimize`/`validate` run
  compares against and as the fallback for partial candidate JSON), not the values actually
  shipped in `VB_COST_FLOOR`/`VB_DEFEND_THRESHOLD`/`CT_DEFAULTS` today. A re-run of either
  driver right now would silently compare against a stale baseline. `calibrate_borealis.py`
  still has this problem too. `calibrate_balanced.py` (`A4`, 2026-08-24) does not: its
  `calib_balanced.c` harness gained a `--print-defaults` mode that dumps the compiled
  `BALANCED_DEFAULTS` as JSON, and the Python driver reads `DEFAULTS` from it at import
  time — structurally can't drift. Port the same `--print-defaults` fix into the three
  older harnesses (or script a check that fails loudly if a hand-copied dict drifts from
  the C source) rather than hand-syncing them going forward.

### `A5` Heuristic Strategy (`ai_strat_heuristic.c`) — done, 2026-08-25

- [x] Advantage function (energy advantage, cards advantage, cash advantage — see
  `ai_strat_heuristic.h`'s header comment for the formulas). The parameter is ε/γ/δ
  (three weights, not just ε/γ as this checklist item originally said — that was
  stale against `about.md`'s own three-weight framing; δ (cash weight) is pinned
  during calibration since the argmax is scale-invariant to the three weights
  together, so one is redundant, but it stays a real `HeuristicParams` field)
- [x] 1-move lookahead, action evaluation — closed-form, not clone-and-apply (cloning
  `gamestate` and replaying a move through `card_actions.c` would pull from the
  shared RNG stream and perturb every downstream game); see `doc/changelog.md`
- [x] Parameters: ε (epsilon) for energy weight, γ (gamma) for cards weight, δ
  (delta, pinned) for cash weight, plus a taper exponent (`weight_taper_exponent`)
  and an opponent-hand-size discount (`opp_card_discount`) — the taper collapses
  the design docs' separately-proposed `weight_cards_decay_rate`/
  `weight_cash_decay_rate`/`weight_energy_critical_mult` (`ideas/G2 .../
  ai_params_guide.md`) into one dial rather than three, per `about.md`'s "fixed
  weights per game" framing (0.0 recovers strictly fixed weights)
- [x] Calibration against `borealis` (not `A4` Balanced Rules as this item
  originally said — `A4` itself measured rating 36, below the anchor, so that
  target was superseded before running any search; see `aicalibsrc/heuristic/`
  and `doc/changelog.md`). Measured rating 60, above the Borealis anchor — the
  first agent in the roster to clear it.
- [ ] **Deferred, not forgotten**: the stub's hand-power / probability-weighted
  combo-potential term (`ai_strat_heuristic.h`'s header comment quotes the
  original proposal) — `about.md` calls it an open question and out of scope as
  primary logic for `A5`. Revisit only if measurement shows this agent losing to
  hand-quality blindness; not merely because a richer model is possible.
- [ ] In `stda.cli` mode, when AI-vs-AI play is selected, use "AI strategy name + (A or
  B)" as the player name instead of asking for player 1's name and not player 2's (moved
  here from the old `A4` section — unrelated to any specific agent, just never done)

### `A6` Tactical Strategy (`ai_strat_tactical.c`) — done, 2026-08-25

No pre-written checklist existed for this agent (unlike `A5`, which had a
comment-only stub source file) — `about.md` and `tactical_design_notes.md` were the
only design material.

- [x] Phase classification (`GamePhase`: early/mid/late/critical by energy
  thresholds) and a single 0.0-1.0 aggression factor derived from energy
  difference, hand power, and cash surplus — see `ai_strat_tactical.h`'s header
  comment for the formulas. The sketch's `GamePhase` thresholds and its separate
  aggression "smell blood" cutoffs were unified onto one tunable 3-threshold set
  shared by both, rather than kept as two independent step functions.
- [x] `decide_num_attackers()` — called but never implemented in the sketch;
  filled in as aggression-scaled champion count (see the bug fix below for the
  final mechanism, fixed aggression bands rather than the first attempt's
  proportional rounding).
- [x] **Bug found by playtracing, not just poor calibration**: the first
  `decide_num_attackers()` fill-in (`round(aggression * min(3, affordable))`) put
  the single-affordable-champion case's decision boundary exactly on
  `aggression_factor`'s neutral baseline (0.5), causing the agent to pass on its
  only affordable champion far more often than intended — measured **losing to
  Random**, the only implemented agent ever to do so. Fixed with four fixed
  aggression bands instead of proportional rounding; see `doc/changelog.md`.
- [x] Calibration against `borealis` (`aicalibsrc/tactical/`) — sixteen free
  parameters, the largest search space of any agent so far. No `--identity-safe`
  run was needed (the first unconstrained search reported no personality flags,
  unlike `A4`'s and `A5`'s free-search runs). Measured rating 52, above the
  Borealis anchor — the second agent, after `A5`, to clear it.
- [x] This agent's own identity-erosion check (`check_personality_flags()` in
  `calibrate_tactical.py`) — not a per-parameter ratio test like `A4`'s/`A5`'s,
  since `about.md` names the exact failure mode ("a static version of this agent
  is a worse Heuristic, not a Tactical agent"): computes `aggression_factor`
  across a synthetic-position battery and flags if its range collapses.

---

## Game Modes (`src/roles/stda/`)

### Automated Simulation (`stda_auto.c`)

Per-player agent selection (`-Aa`/`-ai.a`/`--ai.a` and `-Ab`/`-ai.b`/`--ai.b`,
`cmdline.c`) is done (2026-08-21, added alongside `A1`) — see `doc/changelog.md`.

- [ ] **Refactor**: extract `sim_stats.c`/`sim_engine.c` — see
  `ideas/2 engine and action system design/stda_auto_split_plan.md`
  for the phased split plan. Do this alongside CSV export, not standalone.
- [ ] Support multiple deck types (currently hardcoded random)
- [ ] Better statistics: confidence intervals, effect size calculations, win-rate
  standard error
- [ ] CSV export integration (`ideas/4 match results export/`)
- [ ] Progress display during long runs

### CLI Mode (`stda_cli.c`)

- [ ] Save/load game state (`ideas/6 save and load gamestate/`, back-burnered)

### TUI Mode (`stda_tui.c`)

Milestones 1–2 and the UI/playability polish pass are done — see `doc/changelog.md`.

**Left for a future pass** (not blocking):

- [ ] Visual highlighting of staged cards directly in the hand display (currently just a
  `[n,m]` list in the command-line row)
- [ ] Help overlay (CLI's `gmst`/`shod`/`help` have no TUI equivalent; board is always
  visible so `gmst`/`shod` are moot, but a `help` command/key listing the grammar would
  help)
- [ ] TUI ↔ SIM mode switching (low priority; `stda.sim` doesn't exist yet either)
- [ ] Move the pre-ncurses player-setup questions (mode/name/AI-strategy prompts) into
  the Console box instead of plain stdio before `initscr()` — touches CLI-shared setup
  code (`ui/shared/player_config.c`/`player_selection.c`), planned as its own milestone
- [ ] Render deck-card contents once a card-visibility model exists (currently deck
  stays a count-only label; only meaningful after a discard-shuffled-into-deck mechanic
  is modeled)

**Key files**: `src/roles/stda/stda_tui.c` (setup/loop) + `stda_tui_interactive.c/h`
(human-turn handlers), `src/ui/tui/tui_render.c` + `_playarea.c` + `_io.c` (rendering),
`tui_input.c/h` (the TUI's `UiIO` backend), `src/ui/shared/ui_io.h` (the seam),
`src/ui/interactive/game_commands.c/_cards.c` (the shared rules), `src/ui/cli/cli_io.c`
(the CLI's `UiIO` backend, for comparison).

### Simulation UI (`stda.sim`) — not started

- [ ] ncurses-based results display, live progress bar, win-rate display, strategy
  comparison table, parameter controls, ASCII-art histograms, export commands, mode
  switching (SIM ↔ TUI)

---

## Utilities (`src/`)

### Command-Line Parsing

- [ ] Add `--config` option
- [ ] Add `--deck` option (random/mono/custom/the 3 drafting formats)

### Game Context

- [ ] Document usage patterns in `doc/oracle_design.md` (partially covered — expand with
  worked examples if it grows)

### Debug System

- [ ] Add debug levels (INFO, WARN, ERROR)
- [ ] Add file/line number to debug output

---

## New Features to Add

### Configuration System

See `ideas/7 config file/` for implementation notes.

- [ ] `config.c/h` implementation, INI-style parser, `read_config_file()`, default
  configuration, user config (`~/.oraclerc`), command-line override, `save_config()`

### CSV Export System

See `ideas/4 match results export/` for the full specification.

- [ ] `sim_export.c/h` implementation, `SimExporter` structure,
  `generate_simparam_string()`, `export_game_result()`, `export_summary()`, detail CSV
  (per-game), summary CSV (aggregate), filename conventions, integration with
  `stda_auto` mode

### Rating System

- [x] Bradley-Terry rating system implemented (2026-08-23, `src/rating/`) — ported the
  math/design of `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/`, not the
  prototype file, fixing a dozen-plus defects along the way. `rating_init()`,
  `rating_register_ai()`/`_human()`, `rating_update_match()` (incremental A^delta),
  `rating_win_probability()`, adaptive-A, Borealis rebalancing (the spec's "keeper
  rebalancing" — Borealis is the anchor, not a separate "Keeper" concept), CSV
  persistence, two batch solvers (MM default, gradient ascent kept for cross-checking),
  and the `--stda.rating` round-robin benchmark + `--rating.track` human tracking. See
  `doc/changelog.md`'s 2026-08-23 entry for the full defect list and measured results.

---

## Testing & Quality

- [ ] Memory leak detection (valgrind) — routine spot-checks already happen per-change;
  formalize into a repeatable target
- [ ] Profile hot paths (gprof) — not needed yet, see CLAUDE.md "No premature
  optimization"
- [ ] Run with `-Wextra` (fix all resulting warnings)
- [ ] Check with cppcheck
- [ ] Review all functions >35 lines / files >500 lines for possible splits (soft
  targets, not urgent — see `doc/oracle_design.md` §1)

---

## Documentation Tasks

- [ ] Doxygen comments on all public functions
- [ ] `STRATEGY_GUIDE.md` — AI strategy descriptions (write once ≥2 agents beyond Random
  exist, so there's something to compare)
- [ ] `PROTOCOL.md` (once network code exists)
- [ ] Diagrams for `doc/oracle_design.md` (flow charts, class diagrams) beyond the
  existing `doc/Diagramme déroulement du jeu.svg`

---

## Bug Tracker

No known open bugs. Add entries here as they're found.

---

## Action Items (preparation for client/server and MCTS)

- [x] `get_available_moves()` function (legal-move enumeration, needed by both MC/MCTS
  agents and the future action-validation layer) — done 2026-08-25 as part of `A8`:
  `src/actions/move_gen.{c,h}`, reusable by `A9`-`A11`. See `doc/changelog.md`.
- [x] Game state cloning, for MCTS rollouts — done 2026-08-25 as part of `A8`: trivial
  (`struct gamestate` is pure POD), the real work was `ai_strat_playout.c`'s forked-RNG
  `GameContext` (`mc_fork_context()`) so a clone can actually be simulated forward
  without perturbing the live game's RNG stream. See `doc/changelog.md`.
- [ ] Phase state machine for cleaner turn flow (this is the `ideas/2 …` engine rework)

---

## Technical Debt

### Refactoring Needed

- [ ] `stda_auto.c` mixes simulation logic with presentation (see "Automated Simulation"
  above)
- [ ] `card_actions.c` needs better error handling (see "Core Game Logic" above)
- [ ] `select_champion_for_cash_exchange()` (AI-only heuristic) lives in
  `card_actions.c` instead of `ai_strat/` — move once a smarter AI needs it (see
  `doc/oracle_design.md` §13)

### Architecture Improvements

- [ ] Define clear API boundaries (core vs. modes vs. UI) — mostly already true via the
  `UiIO` seam; formalize the remaining core/ai_strat boundary
- [ ] Create an action-validation layer (before applying actions) — part of the
  `ideas/2 …` engine rework
- [ ] Proper error codes (see "Core Game Logic" above), not just `printf`
- [ ] Logging system (not just `DEBUG_PRINT`)

### Code Cleanup

- [ ] Remove old commented-out code where found
- [ ] Consistent naming — some legacy camelCase remains (known debt, don't propagate to
  new code — see `CLAUDE.md`)
- [ ] Consolidate constants (some in `.h`, some in `.c`)
- [ ] Remove unused functions/variables
- [ ] Update all file headers with a consistent format

---

## Checklist: Adding a New AI Strategy

As of `A1` (2026-08-21), strategy dispatch is a single table-driven registry
(`src/ai_strat/ai_strategy.c`) rather than three separate hardcoded call sites --
`stda_auto.c`, `cli_game.c` (shared by CLI and TUI), and the interactive menu
(`player_config.c`) all consult it, so a new agent only needs to be registered once.

1. [ ] Create `src/ai_strat/ai_strat_<name>.c` + `.h` (`ai_strat_valuebased.{c,h}` is
   the reference; also check whether `ai_strat_common.{c,h}`'s
   `build_affordable_champions()`/`expected_incoming_attack()`/`try_play_draw_card()`
   can be reused instead of re-derived)
2. [ ] Implement `<name>_attack_strategy()`
3. [ ] Implement `<name>_defense_strategy()`
4. [ ] Add one line to `ai_strategy.c`'s `STRATEGY_REGISTRY[]` table, e.g. (as
   landed for `A4`, 2026-08-24):
   ```c
   [AI_STRATEGY_BALANCED] = { balanced_rules_attack_strategy, balanced_rules_defense_strategy },
   ```
   That single line is now sufficient: `ai_strategy_is_implemented()` flips the
   interactive menu's "not yet implemented" label to "available" and drops the
   Random fallback automatically, and `set_player_strategy_by_type()` (used by
   `stda_auto.c` and `cli_game.c`) picks it up with no further edits.
5. [ ] Add exactly one shorthand to `player_config.c`'s `AI_STRATEGY_SHORTHANDS[]` if
   not already present (confirm, don't assume -- see `oracle_ai_agent_names.md`)
6. [ ] Test against Random AI (10,000 games via `--stda.auto -Aa <name> -Ab rand`, and
   the reverse seat order -- see `A1`'s changelog entry for why both seats matter)
7. [ ] Measure win rate; compare against other implemented strategies. Don't assume a
   design doc's speculative win-rate estimate is correct -- `A1`'s handout guessed
   ~60-70% vs Random and the measured result was ~90%, for reasons documented in the
   changelog; investigate discrepancies before trusting either the doc or the code
8. [ ] Once a `--stda.rating` fit exists for the new agent, update its entry in
   `player_config.c`'s `AI_STRATEGY_RATINGS[]` (measured rating, `measured = true`)
   so the interactive AI strategy menu (`display_ai_strategy_menu()`) stops showing
   the `~`-prefixed design-intent estimate
9. [ ] Document in `STRATEGY_GUIDE.md` (create it once ≥2 agents exist — see
   "Documentation Tasks")
10. [ ] Update `doc/oracle_roadmap.md`'s "Recently Completed" / status

---

## Checklist: Adding a New Game Mode

1. [ ] Add mode to `game_mode_t` enum (`game_types.h`)
2. [ ] Add command-line option (`cmdline.c`)
3. [ ] Implement `run_mode_<name>()` in `main.c`, replacing the "not yet implemented"
   stub
4. [ ] Create entry-point module(s) under `src/roles/<role>/` (e.g. `src/roles/stda/` for
   a new standalone UI)
5. [ ] Implement mode-specific UI, reusing `ui/shared/ui_io.h` and
   `ui/interactive/game_commands*.c` where the mode is interactive
6. [ ] Handle mode initialization/cleanup
7. [ ] Test mode thoroughly (primary regression check + a manual play session)
8. [ ] Update `README.md` and `--help` text (`cmdline.c`)
9. [ ] Update `doc/oracle_roadmap.md`

---

*Last Updated: August 2026*
