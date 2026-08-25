# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Oracle ("Les Champions d'Arcadie") is a fixed-pool strategic dueling card game implemented in portable C, and a testbed for progressively stronger game-playing AI (Random → rule-based → heuristic → Monte Carlo → IS-MCTS), with a Bradley-Terry rating system (`src/rating/`, implemented 2026-08-23) for objective strength measurement. It's a hobby/research project by a solo developer (JonathanFerron), currently in active early development — expect unimplemented stubs, TODOs, and some drift between docs and code.

**Dev environment**: Kubuntu Linux, GCC, GNU Make, editor Kate, Python available. Cross-platform (MSYS2/Windows + Linux) portability remains a goal and is kept working, but Kubuntu is the primary/active target — `doc/oracle_design.md`'s "Geany"/"MSYS2/Arch" is outdated.

## Build & Run

```bash
make                              # build bin/oracle (auto-discovers all src/**/*.c)
make clean                        # remove obj/ and bin/oracle*
make debug                        # rebuild with -O0 -DDEBUG -DDEBUG_ENABLED=1
make format                       # format all .c/.h via astyle (uses .astylerc — see below; excludes ideas/)
make help                         # list targets
```

Run modes (see `src/main/cmdline.c` for the full option table; both single-letter and `--long.form` spellings work):

```bash
./bin/oracle --stda.auto --numsim=1000     # automated AI vs AI simulation
./bin/oracle --stda.cli                    # interactive CLI (human vs AI, etc.)
./bin/oracle -sa -p -n 5                   # short forms: auto mode, fixed default seed, 5 games
./bin/oracle --stda.tui                    # ncurses TUI (human-vs-AI or AI-vs-AI, TAB toggles PLAY/COMMAND mode)
```

Other modes (`stda.sim/.gui`, `server`, `client.*`) are wired into `main.c`'s dispatch switch but currently just print "not yet implemented" — see `src/main/main.c`. `stda.tui` is real as of 2026-07-14 and, as of 2026-07-23 (Milestone 2), supports full human-vs-AI play, not just AI-vs-AI display — see below.

Bash tab-completion: `source tools/oracle-completion.bash` (e.g. from `~/.bashrc`). It calls the binary's hidden `--oracle-complete[=WHAT]` option (intentionally absent from `--help`/`print_usage()`) for every candidate list — option spellings, `-A`/`--ai` agent codes, `-u`/`--ui.lang` codes — so the script has nothing to hardcode.

### Tests

`make test_combo` builds and runs `testsrc/test_combo_bonus.c` (20/20 passing as of 2026-07-14, fixed as part of the folder-8 cleanup pass): include paths and Makefile variables now point at `core/combo_bonus.{c,h}`/`core/game_constants.{c,h}`, the stale `test_order_mapping()` (testing the since-removed `get_order_from_species()`) was deleted, and the remaining `CombatCard` literals were given explicit `.order` fields matching each species (the struct gained that field after the test was originally written, so positional initializers were silently leaving it zero and causing spurious order-match bonuses).

`make test_stda_auto` diffs `./bin/oracle.exe -sa -p` output against `bin/expectedresults.txt` — note it invokes the `.exe` (MSYS2/Windows) binary name even though the Linux target is `bin/oracle`; adjust the binary name to match your platform when running this check manually.

**Primary regression check (do this after any change that shouldn't alter game outcomes)**: run `./bin/oracle -a -p` and diff against `bin/expectedresults.txt` — this is the main way changes are validated as behavior-preserving (same as what `test_stda_auto` automates, modulo the `.exe` naming issue above). Also play one interactive game via `stda.cli` with the standard seed (`-p`) as a manual sanity check. Worth turning both into a proper automated test at some point.

`make test_recall` and `make test_cash_exchange` build and run small standalone unit-test binaries (`testsrc/test_recall.c`, `testsrc/test_cash_exchange.c`) covering the recall mechanic and the interactive/AI cash-exchange paths, including the champion-card-index-0 edge case. Both link a minimal subset of `src/` objects directly (see the `TEST_*` variables in the Makefile) rather than depending on the `test_combo` pattern.

`make test_rating` builds and runs `testsrc/test_rating.c` (41/41 passing as of 2026-08-23), covering the Bradley-Terry rating system (`src/rating/`): scale round-trip, the Borealis anchor property, probability symmetry, adaptive-A monotonicity, both batch solvers (MM and gradient ascent, including MM recovering known synthetic strengths), CSV round-trip, and several regressions for defects fixed on porting the v2 spec (win-count overflow, leaderboard underflow, all-draws handling). Since `src/rating/` depends only on `game_types.h` + libc, this test needs no other engine objects.

`testsrc/cli_scripts/` holds canned stdin scripts for repeatable manual verification of interactive-only features (recall, cash exchange, combat display, discard display) — run via `./bin/oracle -sl -p < testsrc/cli_scripts/<name>.txt`; see that directory's README for what each one exercises and what to look for. These aren't auto-asserted (ANSI-colored free-form output isn't worth pinning byte-for-byte), but they make manual re-verification consistent instead of ad hoc.

There is no other automated test runner yet; most other validation is manual play-testing via `stda.cli` or statistical inspection of `stda.auto` output.

## Architecture

### Module layout (`src/`)

- `core/` — game engine: `game_types.h` (all enums/structs — start here), `game_constants.c/h` (the 120-card deck, `fullDeck[]`), `game_state.c` (setup/init), `card_actions.c` (play/draw/discard), `combat.c` (combat resolution), `combo_bonus.c` (combo bonus math), `turn_logic.c` (turn/phase orchestration), `game_context.c/h` (see GameContext pattern below).
- `ai_strat/` — AI strategies as function pointers (`ai_strategy.h`); `ai_strat_random`, `ai_strat_valuebased`, `ai_strat_combo_threshold`, `ai_strat_borealis`, `ai_strat_balanced_rules`, and `ai_strat_heuristic` are functional, the rest (`simplemc1`, `ismcts1`) are design stubs.
- `roles/stda/` — "standalone" mode entry points: `stda_auto.c` (batch simulation + stats/histogram), `stda_cli.c` (interactive game loop glue), `stda_tui.c` (TUI mode entry point: pre-ncurses player configuration, setup, and the per-turn game loop; AI-vs-AI still uses the original M1 `play_turn()` fast path) + `stda_tui_interactive.c/h` (TUI Milestone 2: human-turn handlers — attack/defense PLAY-mode digit-staging and COMMAND-mode line editing, mulligan, discard-to-7, and the phase-by-phase per-turn orchestrator), `stda_rating.c` (`MODE_STDA_RATING`/`--stda.rating`: round-robin Bradley-Terry benchmark over every implemented agent) + `stda_rating_track.c` (`--rating.track`: opt-in human rating tracking shared by `stda_cli.c`/`stda_tui.c`, off by default).
- `ui/cli/` — CLI presentation split into `cli_display.c` (core status/turn rendering), `cli_action_display.c` (action-flow/card-selection rendering: mulligan, discard, recall, cash exchange, combat breakdown), `cli_input.c` (thin wrapper: intercepts CLI-only diagnostic commands `gmst`/`shod`/`help`, delegates the rest to `ui/interactive/game_commands.c`), `cli_io.c` (the CLI's `UiIO` backend — see `ui/shared/ui_io.h`), `cli_game.c` (loop wiring).
- `ui/interactive/` — `game_commands.c` (attack/defense command dispatch, champion-play validation) + `game_commands_cards.c` (recall, cash exchange): the UI-agnostic interactive command grammar/rules shared between CLI and TUI, added 2026-07-23 (TUI Milestone 2, Pass 1) so the two UIs don't duplicate the rules. Each function takes a `UiIO*` (`ui/shared/ui_io.h`) instead of touching stdio/ncurses directly; board/state rendering is deliberately *not* part of this — each UI still renders its own way.
- `ui/shared/` — `player_config.c`/`player_selection.c` (player type/name/strategy setup), `localization.h` (EN/FR/ES macro-based i18n), `ui_constants.h` (command return codes/input buffer sizes, shared across `ui/cli`, `ui/shared`, `ui/interactive`), `ui_io.h` (the `UiIO` seam struct).
- `ui/tui/` — ncurses window layout and drawing for `stda.tui` mode (responsive to terminal size), split three ways after Milestone 2: `tui_render.c` (screen lifecycle, layout, status bars, info-column shell), `tui_render_playarea.c` (hand/deck-discard/combat-zone board drawing), `tui_render_io.c` (message log, input predicates, command-line drawing, combat-details rendering). `tui_render.h` deliberately keeps `<ncurses.h>` out (see "ncurses/ChampionColor collision" below) and exposes safe wrappers (`tui_get_input()`, `tui_input_is_quit/resize/tab/enter/escape/backspace/printable()`, `tui_draw_command_line()`, `tui_format_card()`, `tui_add_message_colored()`, `tui_show_combat_details()`) so no other file needs `<ncurses.h>` directly. `tui_input.c/h` is the TUI's `UiIO` backend (see `ui/interactive/` above), built entirely on those wrappers.
- `ui/gui/`, `ui/simulation/` — not implemented yet; contain only planning `.txt` notes.
- `structures/` — `deckstack.c` (fixed-size LIFO array for draw piles), `card_collection.c` (fixed-size collection used for hand/discard/combat zone).
- `util/` — `mtwister.c` (Mersenne Twister PRNG), `rnd.c` (dice/roll wrappers), `prng_seed.c` (secure seed generation/parsing), `debug.h` (compile-time debug macros).
- `main/` — `main.c` (mode dispatch), `cmdline.c` (getopt_long_only-based arg parsing → `config_t`).
- `rating/` — Bradley-Terry rating system (implemented 2026-08-23): `rating.h` (the single public header), `rating_core.c` (registration/lookup/strength↔rating math), `rating_update.c` (incremental `A^delta` updates for live play), `rating_batch.c` (order-independent MLE fit — MM default, gradient ascent kept for cross-checking), `rating_csv.c` (persistence, the first file I/O anywhere in `src/`). Deliberately depends only on `game_types.h` + libc, no `src/ui/`/`src/ai_strat/` — see `rating.h`'s own comment and `doc/changelog.md`.
- `actions/`, `visibility/` — currently just planning notes (`.txt` files), no implementation yet. Don't assume code exists here.

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
- Client/server and GUI modes are still placeholders only (see `main.c` dispatch — they just print a message); `stda.tui` is real and, since Milestone 2 (2026-07-23), supports human-vs-AI play — see `doc/changelog.md`. Not yet done: visual highlighting of staged cards in the hand display (shown as a `[n,m]` list in the command-line row instead), a help overlay, and TUI↔SIM mode switching.
- **ncurses' `COLOR_RED`/`COLOR_GREEN`/etc. `#define`s collide with this codebase's own `ChampionColor` enum** (`COLOR_RED`/`COLOR_INDIGO`/`COLOR_ORANGE`, `game_types.h`) — only `COLOR_RED` actually overlaps, but it's enough to silently corrupt the enum if `<ncurses.h>` is included before `game_types.h` in the same translation unit. `tui_render.h` avoids this by never including `<ncurses.h>` (it forward-declares `WINDOW` as an opaque `struct _win_st*`); `tui_render.c` includes `game_types.h` first, then `<ncurses.h>`, then immediately `#undef COLOR_RED`, and uses its own `NC_RED`/`NC_GREEN`/etc. constants (plain POSIX curses color numbers) for `init_pair()` instead of ncurses' macros. Keep this pattern if any other file ever needs both ncurses and `ChampionColor` together.
- `setup_game()` (`game_state.c`) does not initialize `gstate->turn_phase`/`player_to_move` — only `begin_of_turn()` (the first thing `play_turn()`/`attack_phase()` call) does. The CLI never notices because it always runs `begin_of_turn()` before displaying anything; `stda_tui.c`'s `tui_setup()` draws once before the first `play_turn()` call, so it explicitly sets both fields itself (found via a valgrind uninitialized-read report during TUI M1 verification).

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
- `ideas/` — numbered folders of design explorations/proposals and **prototypes**, not yet implemented and not canonical. Useful for intent/rationale on planned features (recall, drafting formats, client/server, GUI, rating system), but don't copy its conventions into real code — port the *intent*, re-fit to current structure/includes/naming/GameContext. The TUI design-exploration folder's original flat-file prototype (pre-reorg includes, linked-list hands, a single `tui.c`) was fully superseded by the real `src/ui/tui/tui_render.c/h` + `src/roles/stda/stda_tui.c` (Milestone 1, 2026-07-14) and deleted 2026-08-20; that folder (formerly `ideas/3 tui/`) was then renamed to `ideas/3 misc ui ideas/` since nothing TUI-specific remained in it — see its `README.md`.
- `aicalibsrc/` — AI-agent parameter calibration tooling, one self-contained subfolder per
  agent (C harness + Python driver); see the "AI strategies" bullet below.
- `testsrc/` — unit tests (`test_combo_bonus.c`, `test_recall.c`, `test_cash_exchange.c`, `test_rating.c`, all current, see Tests section above) plus `cli_scripts/`, canned interactive-CLI input scripts for manual regression checks.

## How work gets done here

- **Incremental**: implement one function at a time; keep each focused and single-purpose.
- **Refactors use the 4-part method** from `doc/REFACTORING.md`: (1) new files first, (2) integrate into existing files, (3) pattern-by-pattern changes given as diff-style red/green edits with file + function + line references, (4) cleanup/removal.
- **Keep docs in sync**: when architecture or status changes, update `doc/oracle_todo.md` checkboxes and `doc/oracle_design.md` — both are dated "Last Updated: December 2025" and drift otherwise.
- **Definition of done**: ≤35-line functions, no new compiler warnings, public functions commented, valgrind-clean, formatted with `make format`, todo/design docs updated, and the primary regression check (see Tests section) passes.

## Out of scope / don't-do-yet

- **No premature optimization.** Current perf is fine (10k sims < 5 min). No memory pools, caching, or PGO unless profiling (gprof) shows a real bottleneck.
- **Network/client-server** is designed but not built — don't scaffold `sh_`/`sr_`/`cl_`/`pr_`-style modules unless explicitly asked.
- **SDL3 GUI** is long-horizon — ignore unless the task is specifically about it.
- **AI strategies beyond Random, Value Based, Combo Threshold, Borealis, Balanced Rules, and Heuristic are stubs.** `A1` Value Based ("The Apprentice", `src/ai_strat/ai_strat_valuebased.c`) was implemented 2026-08-21; `A2` Combo Threshold ("The Showboat", `src/ai_strat/ai_strat_combo_threshold.c`) was implemented and calibrated 2026-08-22; `A3` Borealis (the Bradley-Terry benchmark, `src/ai_strat/ai_strat_borealis.c`/`ai_strat_borealis_enum.c`) was implemented and calibrated 2026-08-23; `A4` Balanced Rules ("Bean Counter", `src/ai_strat/ai_strat_balanced_rules.c`) was implemented and calibrated 2026-08-24, measured rating 36 (below the Borealis anchor — a legitimate result, see `doc/changelog.md`, not the `~62` design-intent estimate); `A5` Heuristic ("Eps-Gam-Del", `src/ai_strat/ai_strat_heuristic.c`) was implemented and calibrated 2026-08-25, measured rating 60 — the first agent to measure *above* the Borealis anchor, against a `~70` design-intent estimate (see `doc/changelog.md`) — see `doc/changelog.md` for all five agents' scoring models and measured strength vs Random and vs each other. `A3` is also the first agent with a `mulligan_strategy[]`/`discard_strategy[]` override in `StrategySet` (`src/ai_strat/ai_strategy.h`) — see "Interactive-only features" above and `ai_strat_lib_heuristics.c` for the shared default every other agent still falls back to. CLI/config menus still list Tactical/Hybrid/Monte Carlo/IS-MCTS as "not yet implemented" and fall back to Random; that "not yet implemented" label is driven by `ai_strategy_is_implemented()` (`src/ai_strat/ai_strategy.c`)'s registry rather than hardcoded per-strategy, so a new agent needs only one line added to that registry to go live everywhere (CLI, TUI, `stda.auto`). The Bradley-Terry rating system (`src/rating/`, `--stda.rating`/`--rating.track`) was implemented 2026-08-23 on top of this roster — see `doc/changelog.md`; current active work is `A6` Tactical, see `doc/oracle_todo.md`. TUI Milestones 1 and 2 are both done (`doc/changelog.md`). Calibration tooling for a new agent follows `aicalibsrc/<agent>/` (one self-contained subfolder per agent, C harness + Python driver) — see `aicalibsrc/value/README.md`, `aicalibsrc/combo/README.md`, `aicalibsrc/borealis/README.md`, `aicalibsrc/balanced/README.md`, `aicalibsrc/heuristic/README.md`; past two dials, a full parameter grid becomes infeasible, so `aicalibsrc/combo/calibrate_combo_threshold.py`, `aicalibsrc/borealis/calibrate_borealis.py`, `aicalibsrc/balanced/calibrate_balanced.py`, and `aicalibsrc/heuristic/calibrate_heuristic.py` all added a `differential_evolution`-based `optimize` subcommand instead — `A4`'s and `A5`'s also gained an `--identity-safe` mode that bounds the search to keep the agent's designed character intact after a free search found a much stronger but far-off-spec optimum (see `doc/changelog.md`; unlike `A4`'s case, `A5`'s free-search optimum turned out to be a legitimate finding rather than erosion once playtraced, but the character-preserving candidate measured statistically indistinguishable anyway and shipped). One `ideas/A#` folder per agent (`A1`-`A11`), numbered to match that agent's `AIStrategyType` enum ordinal (now declared in `src/core/game_types.h`, not `ui/shared/player_config.h`); general info and calibration tooling live in `ideas/G1`/`ideas/G2` instead of occupying an A-line slot — see `ideas/G1 AI agent general info/oracle_ai_agent_names.md` for the canonical roster and CLI shorthands (one per agent — no aliases).
