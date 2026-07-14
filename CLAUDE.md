# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Oracle ("Les Champions d'Arcadie") is a fixed-pool, two-player strategic dueling card game implemented in portable C, and a testbed for progressively stronger game-playing AI (Random → rule-based → heuristic → Monte Carlo → IS-MCTS), with a Bradley-Terry rating system planned for objective strength measurement. It's a hobby/research project by a solo developer (JonathanFerron), currently in active early development — expect unimplemented stubs, TODOs, and some drift between docs and code.

**Dev environment**: Kubuntu Linux, GCC, GNU Make, editor Kate, Python available. Cross-platform (MSYS2/Windows + Linux) portability remains a goal and is kept working, but Kubuntu is the primary/active target — `doc/oracle_design.md`'s "Geany"/"MSYS2/Arch" is outdated.

## Build & Run

```bash
make                              # build bin/oracle (auto-discovers all src/**/*.c)
make clean                        # remove obj/ and bin/oracle*
make debug                        # rebuild with -O0 -DDEBUG -DDEBUG_ENABLED=1
make format                       # format all .c/.h via astyle (uses .astylerc — see below)
make oldcode                      # build oldsrc/ (pre-refactor code, kept for regression comparison)
make help                         # list targets
```

Run modes (see `src/main/cmdline.c` for the full option table; both single-letter and `--long.form` spellings work):

```bash
./bin/oracle --stda.auto --numsim=1000     # automated AI vs AI simulation
./bin/oracle --stda.cli                    # interactive CLI (human vs AI, etc.)
./bin/oracle -sa -p -n 5                   # short forms: auto mode, fixed default seed, 5 games
```

Other modes (`stda.sim/.tui/.gui`, `server`, `client.*`) are wired into `main.c`'s dispatch switch but currently just print "not yet implemented" — see `src/main/main.c`.

### Tests

`make test_combo` builds and runs `testsrc/test_combo_bonus.c` (20/20 passing as of 2026-07-14, fixed as part of the folder-8 cleanup pass): include paths and Makefile variables now point at `core/combo_bonus.{c,h}`/`core/game_constants.{c,h}`, the stale `test_order_mapping()` (testing the since-removed `get_order_from_species()`) was deleted, and the remaining `CombatCard` literals were given explicit `.order` fields matching each species (the struct gained that field after the test was originally written, so positional initializers were silently leaving it zero and causing spurious order-match bonuses).

`make test_stda_auto` diffs `./bin/oracle.exe -sa -p` output against `bin/expectedresults.txt` — note it invokes the `.exe` (MSYS2/Windows) binary name even though the Linux target is `bin/oracle`; adjust the binary name to match your platform when running this check manually.

**Primary regression check (do this after any change that shouldn't alter game outcomes)**: run `./bin/oracle -a -p` and diff against `bin/expectedresults.txt` — this is the main way changes are validated as behavior-preserving (same as what `test_stda_auto` automates, modulo the `.exe` naming issue above). Also play one interactive game via `stda.cli` with the standard seed (`-p`) as a manual sanity check. Worth turning both into a proper automated test at some point.

`make test_recall` and `make test_cash_exchange` build and run small standalone unit-test binaries (`testsrc/test_recall.c`, `testsrc/test_cash_exchange.c`) covering the recall mechanic and the interactive/AI cash-exchange paths, including the champion-card-index-0 edge case. Both link a minimal subset of `src/` objects directly (see the `TEST_*` variables in the Makefile) rather than depending on the `test_combo` pattern.

`testsrc/cli_scripts/` holds canned stdin scripts for repeatable manual verification of interactive-only features (recall, cash exchange, combat display, discard display) — run via `./bin/oracle -sl -p < testsrc/cli_scripts/<name>.txt`; see that directory's README for what each one exercises and what to look for. These aren't auto-asserted (ANSI-colored free-form output isn't worth pinning byte-for-byte), but they make manual re-verification consistent instead of ad hoc.

There is no other automated test runner yet; most other validation is manual play-testing via `stda.cli` or statistical inspection of `stda.auto` output.

## Architecture

### Module layout (`src/`)

- `core/` — game engine: `game_types.h` (all enums/structs — start here), `game_constants.c/h` (the 120-card deck, `fullDeck[]`), `game_state.c` (setup/init), `card_actions.c` (play/draw/discard), `combat.c` (combat resolution), `combo_bonus.c` (combo bonus math), `turn_logic.c` (turn/phase orchestration), `game_context.c/h` (see GameContext pattern below).
- `ai_strat/` — AI strategies as function pointers (`ai_strategy.h`); only `ai_strat_random` is functional today, the rest (`balancedrules1`, `heuristic1`, `simplemc1`, `ismcts1`) are design stubs.
- `roles/stda/` — "standalone" mode entry points: `stda_auto.c` (batch simulation + stats/histogram), `stda_cli.c` (interactive game loop glue).
- `ui/cli/` — CLI presentation split into `cli_display.c` (core status/turn rendering), `cli_action_display.c` (action-flow/card-selection rendering: mulligan, discard, recall, cash exchange, combat breakdown), `cli_input.c` (parsing/validation), `cli_game.c` (loop wiring).
- `ui/shared/` — `player_config.c`/`player_selection.c` (player type/name/strategy setup), `localization.h` (EN/FR/ES macro-based i18n).
- `ui/gui/`, `ui/tui/`, `ui/simulation/` — not implemented yet; contain only planning `.txt` notes.
- `structures/` — `deckstack.c` (fixed-size LIFO array for draw piles), `card_collection.c` (fixed-size collection used for hand/discard/combat zone).
- `util/` — `mtwister.c` (Mersenne Twister PRNG), `rnd.c` (dice/roll wrappers), `prng_seed.c` (secure seed generation/parsing), `debug.h` (compile-time debug macros).
- `main/` — `main.c` (mode dispatch), `cmdline.c` (getopt_long_only-based arg parsing → `config_t`).
- `actions/`, `rating/`, `visibility/` — currently just planning notes (`.txt` files), no implementation yet. Don't assume code exists here.

Some file/module names in `doc/oracle_design.md` (e.g. `strat_random.c`, flat `src/*.c`) reflect an older pre-reorg layout; trust the actual `src/` tree (with `core/`, `ai_strat/`, `roles/stda/`, etc. subdirectories) over that doc when they disagree.

### GameContext pattern

All RNG/config state flows through a `GameContext*` passed explicitly to game functions — there is deliberately no global RNG or global config. When adding new game logic that needs randomness or config, thread `GameContext* ctx` through the call chain rather than reaching for a global; this is what keeps simulation deterministic/testable (seedable RNG) and multi-instance-safe.

### Strategy framework

AI strategies are attack/defense function pointer pairs (`AttackStrategyFunc`/`DefenseStrategyFunc` in `ai_strategy.h`) grouped in a `StrategySet` (one pair per `PlayerID`). Adding a new AI means: implement `<name>_attack_strategy()`/`<name>_defense_strategy()` in `src/ai_strat/`, register in the strategy set setup, and add it to the player-config strategy menu (`ui/shared/player_config.c`).

### Turn/phase flow

`turn_logic.c` drives: `begin_of_turn()` (draw, except first player turn 1) → `attack_phase()` (attacker plays champions/draw/cash/pass) → `defense_phase()` (defender plays 0–3 champions or declines) → `resolve_combat()` (`combat.c`: `attack = Σ(base + roll(dice)) + combo_bonus`, `defense = Σ(roll(dice)) + combo_bonus`, `damage = max(attack-defense,0)`) → `end_of_turn()` (collect luna, discard-to-7, switch player). Energy starts at 99; first to 0 loses.

### Interactive-only features (CLI, not yet in the AI strategy layer)

- **Recall** (`card_actions.c`'s `choose_num`, `cli_input.c`'s `handle_recall_choice`/`validate_and_recall_champions`): playing a draw/recall card lets the interactive player choose Draw N or Recall **exactly** M champions from discard (never "up to" — recall is only offered when discard holds ≥ M champions, and the sub-prompt re-asks until exactly M valid indices are given). The Random AI never recalls, always draws.
- **Cash exchange** (`card_actions.c`'s `play_cash_card_interactive` vs `play_cash_card_ai`): the interactive player picks which champion to exchange (`cli_input.c`'s `prompt_champion_exchange`); the AI path still auto-picks lowest-power via `select_champion_for_cash_exchange()`.
- **Combat results display** (`combat.c`'s `resolve_combat_with_details`, `cli_action_display.c`'s `display_combat_details_cli`): shown whenever either combatant is human; `stda_auto` always uses the plain `resolve_combat()` so its RNG-dependent results stay untouched.
- **Discard pile display** (`cli_action_display.c`'s `display_player_discard`/`_detailed`, `gmst`/`shod` commands).

### Known architectural gaps (don't be surprised)

- `select_champion_for_cash_exchange()` (AI-only heuristic) used to return card index `0` as a "not found" sentinel, ambiguous with champion index 0 being a real selection. Fixed to use `UINT8_MAX` as the sentinel instead — this changed `stda_auto`'s RNG-dependent play sequence (different hand state after the fix fires), so `bin/expectedresults.txt` was regenerated at the same time. If you ever see `-a -p` diverge from that file again, first check whether it's a deliberate behavior change (regenerate the baseline, documented in the commit) versus an actual regression (fix your change instead).
- Config is scattered across `cmdline.c` (parsing), `main.c` (cleanup), `stda_auto.c`/`stda_cli.c` (usage) rather than centralized.
- Client/server and GUI/TUI modes are placeholders only (see `main.c` dispatch — they just print a message).

## Code style

- **C23**, compiled with `gcc -Wall -std=c23`.
- **Formatting is astyle-driven, not the usual K&R/Allman style** — run `make format` rather than hand-formatting. Key `.astylerc` settings: run-in braces (opening brace stays on the same "logical" line but statements after it start on the next line — see any function in `src/` for the pattern, e.g. `main.c`), 2-space indent, pointer alignment to type (`int* ptr`), tabs converted to spaces. Match this style when hand-editing between formatting runs.
- **Function length**: target ≤35 lines, firm limit 100 (`README.md`/`oracle_roadmap.md` say "<30" — treat 35/100 as authoritative).
- **File length**: target ≤400, soft limit ≤500, firm limit 1000 lines.
- **Line-count exclusions**: comments/docs, blank lines, switch case-label lists, and simple if-else chains don't count toward either limit. `cli_display.c` exceeded the soft file limit after the recall/cash/combat/discard display work (576 lines); it was split (2026-07-14) into `cli_display.c` (core status/turn display, ~230 lines) and `cli_action_display.c` (action-flow/card-selection display, ~360 lines), both now under the limit.
- Snake_case is the target naming convention; some legacy camelCase exists (known debt) — don't propagate it in new code.
- **Module prefixes** on public functions, matching the module: `RND_`, `DeckStk_`, `Hand_`, `Discard_`, `tui_`, etc.
- Manual/duplicated code is preferred over macro-magic abstractions for readability.
- Cross-platform target: MSYS2/Windows and Linux both need to keep working (see `#ifdef _WIN32` blocks for UTF-8 console setup and `prng_seed.c`'s platform-specific secure RNG).
- **Trilingual UI**: every user-facing string must go through `LOCALIZED_STRING(en, fr, es)` / `LOCALIZED_STRING_L(lang, en, fr, es)` (`ui/shared/localization.h`) with English, French, and Spanish variants. The in-game/world language is French; the UI itself defaults to English.
- **Error handling**: return `bool` for success/failure; anything with a `create_*`/allocator has a matching `destroy_*`/`free_*`.

## Other directories

- `doc/` — design docs (`oracle_design.md`, may lag actual code structure — verify against `src/` when in doubt), `oracle_roadmap.md`, `oracle_todo.md`, `game_rules_doc.md` (full rules).
- `ideas/` — numbered folders of design explorations/proposals and **prototypes**, not yet implemented and not canonical. Useful for intent/rationale on planned features (recall, drafting formats, client/server, GUI, rating system), but don't copy its conventions into real code — port the *intent*, re-fit to current structure/includes/naming/GameContext. Known mismatches: `ideas/3 tui/` (renumbered from `ideas/13 tui/`, then `ideas/1 tui/`, across subsequent reorgs) uses flat includes (`#include "game_types.h"`) and calls the file `tui.c`, whereas the real module will be `stda_tui.c` under `src/roles/stda/` with pathed includes (`#include "../core/game_types.h"`); prototype code there predates both the CLI split (`cli_display.c`/`cli_input.c`/`cli_game.c`) and the linked-list → fixed-array migration for hands/decks.
- `oldsrc/` — pre-refactor implementation, kept only for `make oldcode` regression comparisons. Don't extend it.
- `aicalibsrc/` — planning notes for AI-agent parameter calibration tooling, not yet implemented.
- `testsrc/` — unit tests (`test_combo_bonus.c`, `test_recall.c`, `test_cash_exchange.c`, all current, see Tests section above) plus `cli_scripts/`, canned interactive-CLI input scripts for manual regression checks.

## How work gets done here

- **Incremental**: implement one function at a time; keep each focused and single-purpose.
- **Refactors use the 4-part method** from `doc/REFACTORING.md`: (1) new files first, (2) integrate into existing files, (3) pattern-by-pattern changes given as diff-style red/green edits with file + function + line references, (4) cleanup/removal.
- **Keep docs in sync**: when architecture or status changes, update `doc/oracle_todo.md` checkboxes and `doc/oracle_design.md` — both are dated "Last Updated: December 2025" and drift otherwise.
- **Definition of done**: ≤35-line functions, no new compiler warnings, public functions commented, valgrind-clean, formatted with `make format`, todo/design docs updated, and the primary regression check (see Tests section) passes.

## Out of scope / don't-do-yet

- **No premature optimization.** Current perf is fine (10k sims < 5 min). No memory pools, caching, or PGO unless profiling (gprof) shows a real bottleneck.
- **Network/client-server** is designed but not built — don't scaffold `sh_`/`sr_`/`cl_`/`pr_`-style modules unless explicitly asked.
- **SDL3 GUI** is long-horizon — ignore unless the task is specifically about it.
- **AI strategies beyond Random are stubs.** CLI/config menus list Balanced/Heuristic/Hybrid/Monte Carlo/IS-MCTS as "not yet implemented" and fall back to Random. Current active work is finishing turn logic in interactive (CLI) mode.
