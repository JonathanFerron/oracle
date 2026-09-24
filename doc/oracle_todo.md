# Oracle Development TODO

**Scope of this document**: actionable near-term checkboxes. For phase-level ordering
and long-horizon vision see `doc/oracle_roadmap.md`.



---

## Next Up

See `doc/oracle_roadmap.md` — this file intentionally doesn't duplicate that ordering.
The mechanical steps for implementing whichever agent is next are in "Checklist: Adding a New AI Strategy" below.

**Future `src/` directories, created only when their first real file lands** (also see `ideas/2 engine and action system design/
target_folder_structure_v4.md`'s ownership table for the full picture):

- `deck_formats/` — draft/deck-format feature (`ideas/10 Draft Format and Game Depth
  Addition Ideas/`)
- `game_rules/` — game-engine refactor needs a home for rules data separate from `core/`
- `network/` — client/server (`ideas/8 client server/`)
- `persistence/` — save/load game state (`ideas/6 save and load gamestate/`)
- `config/` — configuration file system (`ideas/7 config file/`)
- `platform/` — if/when platform-specific code (beyond the current `#ifdef _WIN32`
  blocks) grows enough to warrant its own directory



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
config-file system (`ideas/7 config file/`) is picked up — not before, to avoid building the centralization twice.

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
`doc/ai_agents.md` for per-agent design notes and the canonical roster/ratings table.

- [ ] In `stda.cli` mode, when AI-vs-AI play is selected, use "AI strategy name + (A or
  B)" as the player name instead of asking for player 1's name and not player 2's 

### `A11` IS-MCTS + NN (`ai_strat_ismctsnn.c`, "AlphaOracle Prime") — done and registered, 2026-09-03, rating 74 (new roster ceiling)

See `doc/ai_agents.md`'s A11 section for the full detail behind every item below — this checklist is the short/actionable form of that.

- [x] **Item 4**: Stage 4 policy head + PUCT — done, registered 2026-09-08/09 as
  `A14` "AlphaOracle Prime Plus I" (registered unconditionally on the number, not
  gated by it — see `doc/ai_agents.md`'s A14 section and `doc/changelog.md`'s
  2026-09-09 entry for the full record, including a genuine null result on the
  strength question after a full dial-calibration effort). Rating 62 as originally
  shipped (`use_puct=true`, real PUCT selection); **2026-09-22: `use_puct=false`
  (plain UCT selection over the same net) measured a decisive win and shipped as
  the new default, rating ~75** — see `doc/ai_agents.md`'s A14 section, 2026-09-22
  addendum, and `doc/changelog.md`'s same-date entries. **2026-09-23: round 1 of
  self-play bootstrapping (`A16` Session 3) confirmed a real gain — a net retrained
  on `A14`'s own self-play data pooled with the original corpus beat the round-0
  baseline by +2.25pp [+0.74,+3.76]pp vs `A11` (n=8220), promoted in place as
  "AlphaOracle Prime Plus II" (doc name only, same `A14` slot,
  `assets/puct/plus2_weights.bin`)** — see `doc/ai_agents.md`'s A14 section,
  2026-09-23 addendum.
- [x] **Naming decided**: flavor name stays "AlphaOracle Prime" for this whole
  UCT+value-net lineage; a hypothetical future Stage 4 (PUCT+policy) agent would be
  "AlphaOracle Prime Plus I" (revised 2026-09-04 from an earlier "Prime II"
  placeholder — see `doc/changelog.md`'s 2026-09-04 entry — its own "Plus" lineage
  since Stage 4 changes the search mechanism itself, not one added lookahead ply the
  way `A7`→`A9` "Grandmaster"→"Grandmaster II" did), not a corpus-size or
  algorithm-technical suffix on the display name.
- [x] **"Bigger training corpus" follow-up attempted and falsified, 2026-09-04** — see
  `doc/changelog.md`'s entry that date. No retrain shipped; shipped `A11` unchanged.

### `A15` Risk Threshold (`ai_strat_a15*.c`, "The Daredevil") — done and registered, 2026-09-10, rating 48

See `doc/ai_agents.md`'s A15 section and `doc/changelog.md`'s 2026-09-10 entry for the
full record — a transcription of Jonathan's own real-table play, not a rating-target
design; the rating is diagnostic, not a pass/fail bar.

- [x] R1-R9 rule chain implemented and registered.
- [x] R8's endgame trigger redesigned mid-calibration — the hand-size-derived horizon
  measured well but was reading "reckless" rather than "calculated" (Jonathan's own
  read, confirmed by a diagnostic showing >90% of its P(finish) evaluations below 0.2
  regardless of horizon); replaced with checking all 4 horizons directly.
- [x] `aicalibsrc/daredevil/` calibration tooling built and run (sweeps + joint
  `optimize`, both mechanisms).
- [x] Measured via `--rating.agents` round-robin (11 non-tree-search agents + `borealis`,
  `simplemc`/`clairvoy` excluded on cost grounds).

---

## Game Modes (`src/roles/stda/`)

### Automated Simulation (`stda_auto.c`)

- [ ] Support multiple deck-construction methods (currently only the random deal
  exists; `struct gamestate.combo_bonus_table` correctly plumbs which combo-bonus table to score with, fixed 2026-08-28 -- see the Bug Tracker -- but nothing yet
  sets it to anything but `COMBO_BONUS_RANDOM`, since no non-random deck-building
  method exists yet; see `G3`/`ideas/10` in `doc/oracle_roadmap.md`'s "Next Up")
  
  

### CLI Mode (`stda_cli.c`)

- [ ] Save/load game state (`ideas/6 save and load gamestate/`, back-burnered)

### TUI Mode (`stda_tui.c`)



- [ ] Visual highlighting of staged cards directly in the hand display (currently just a
  `[n,m]` list in the command-line row)
- [ ] Help overlay (CLI's `gmst`/`shod`/`help` have no TUI equivalent; board is always
  visible so `gmst`/`shod` are moot, but a `help` command/key listing the grammar would
  help)
- [ ] Move the pre-ncurses player-setup questions (mode/name/AI-strategy prompts) into the Console box instead of plain stdio before `initscr()` — touches CLI-shared setup code (`ui/shared/player_config.c`/`player_selection.c`), planned as its own milestone
- [ ] Render deck-card contents once a card-visibility model exists (currently deck
  stays a count-only label; only meaningful after a discard-shuffled-into-deck mechanic is modeled)



### GUI Mode (`stda_gui.c`, `ui/gui/`) — M1 "hello window" in progress, see `doc/oracle_roadmap.md`'s "SDL3 GUI" item

- [x] Step 3 (`ideas/9 gui/gui_architecture_synthesis.md` section 4/§10):
  `PlayerDecision` + `decision_is_legal()` (`src/actions/player_decision.h/.c`),
  `testsrc/test_player_decision.c`/`make test_player_decision` (29/29 passing).
  ATTACK/DEFENSE moves check canonical (order-insensitive) membership against
  `get_available_moves()` built with limits `{1,1}`; the recall/cash
  sub-choice is checked structurally against the real discard/hand instead
  (a human may pick any variant, not just the one `move_gen.c`'s
  `RECALL_POOL_CAP`-capped template samples) -- see the correction #1 note
  in the plan file.
- [x] Step 4 (`ideas/9 gui/gui_architecture_synthesis.md` section 6/§10):
  `GameEvent` + `event_filter_for_viewer()` (`src/visibility/game_event.h/.c`)
  and the generic `cards_added()`/`cards_removed()` before/after diff
  helpers (work on any `cards[]`+`size` array -- Hand/Discard/CombatZone all
  share that shape), `testsrc/test_game_event.c`/`make test_game_event`
  (15/15 passing). Header-only dependency on `GameMove`/`CombatDetails`;
  `game_event.c` links nothing else, same dependency-free tier as
  `visible_state.c`. `EVT_CARD_DRAWN` is the only event needing redaction
  today -- every other event type is already public per the rules doc.
- [ ] French/Spanish localization: `-u=fr`/`-u=es` currently has no effect on
  `bin/oracle-gui` (confirmed 2026-09-23) -- the hello-window step has no
  `LOCALIZED_STRING` calls at all yet (window title, "Oracle" wordmark are
  plain English literals). Needs doing once there's real UI text to
  localize (labels, buttons, the message log) -- CLI/TUI's existing
  `LOCALIZED_STRING`/`LOCALIZED_STRING_L` macros (`ui/shared/localization.h`)
  are the established pattern to reuse, not a new one.

### Simulation UI (`stda.sim`) — back burnered

- [ ] ncurses-based results display, live progress bar, win-rate display, strategy
  comparison table, parameter controls, ASCII-art histograms, export commands, mode switching (SIM ↔ TUI)

---

## Utilities (`src/`)

### Command-Line Parsing

- [ ] Add `--config` option
- [ ] Add `--deck` option (random/mono/custom/the 3 drafting formats/the other formats added since to the wish list)



---

## New Features to Add

### Configuration System

See `ideas/7 config file/` for implementation notes.

- [ ] `config.c/h` implementation, INI-style parser, `read_config_file()`, default
  configuration, user config (`~/.oraclerc`), command-line override, `save_config()`

---

## Testing & Quality

- [ ] Memory leak detection (valgrind) — routine spot-checks already happen per-change;
  formalize into a repeatable target
- [ ] Review all functions >35 lines / files >500 lines for possible splits (soft
  targets — see `doc/oracle_design.md` §1)

---

## Documentation Tasks

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

Strategy dispatch is a single table-driven registry
(`src/ai_strat/ai_strategy.c`) rather than three separate hardcoded call sites --
stda_auto.c, `cli_game.c` (shared by CLI and TUI), and the interactive menu
(`player_config.c`) all consult it, so a new agent only needs to be registered once.

1. [ ] Create `src/ai_strat/ai_strat_<name>.c` + `.h` (`ai_strat_valuebased.{c,h}` is the reference; also check whether `ai_strat_common.{c,h}`'s
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
5. [ ] Add exactly one shorthand to `player_config.c`'s `AI_STRATEGY_SHORTHANDS[]` if not already present (confirm, don't assume -- see `doc/ai_agents.md`'s roster table)
6. [ ] Test against Random AI (10,000 games via `--stda.auto -Aa <name> -Ab rand`, and the reverse seat order -- see `A1`'s changelog entry for why both seats matter)
7. [ ] Measure win rate; compare against other implemented strategies. Don't assume a design doc's speculative win-rate estimate is correct; investigate discrepancies before trusting either the doc or the code
8. [ ] Once a `--stda.rating` fit exists for the new agent, update its entry in
   `player_config.c`'s `AI_STRATEGY_RATINGS[]` (measured rating, `measured = true`) so the interactive AI strategy menu (`display_ai_strategy_menu()`) stops showing the `~`-prefixed design-intent estimate
9. [ ] Document in `STRATEGY_GUIDE.md` (create it once ≥2 agents exist — see
   "Documentation Tasks")
10. [ ] Update `doc/oracle_roadmap.md`'s "Recently Completed" / status

---

## Checklist: Adding a New Game Mode

1. [ ] Add mode to `game_mode_t` enum (`game_types.h`)
2. [ ] Add command-line option (`cmdline.c`)
3. [ ] Implement `run_mode_<name>()` in `main.c`, replacing the "not yet implemented" stub
4. [ ] Create entry-point module(s) under `src/roles/<role>/` (e.g. `src/roles/stda/` for a new standalone UI)
5. [ ] Implement mode-specific UI, reusing `ui/shared/ui_io.h` and
   `ui/interactive/game_commands*.c` where the mode is interactive
6. [ ] Handle mode initialization/cleanup
7. [ ] Test mode thoroughly (primary regression check + a manual play session)
8. [ ] Update `README.md` and `--help` text (`cmdline.c`)
9. [ ] Update `doc/oracle_roadmap.md`

---

*Last Updated: September 2026*
