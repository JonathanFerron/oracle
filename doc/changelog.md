# Oracle Changelog

Completed work, most recent first. `doc/oracle_todo.md` tracks what's still open;
this file is where finished items go so the todo list doesn't keep growing.

---

## 2026-07-14 — TUI layout: shortcuts hint moved, vertical hand, discard corners

Further Milestone 1 polish.

- **Moved the "TAB to toggle play/command modes" hint into the Shortcuts panel**
  (merged with the existing "(context sensitive - M2)" note, wrapped via new
  `tui_print_wrapped()`); `win_command` is now just a bare `> ` prompt.
- **Player A's hand is now a vertical stack** (`tui_draw_hand_vertical()`, one card
  per row, matching the target PDF) instead of the horizontal wrapping row used
  elsewhere. All entries share one x position (centered on the widest entry) so the
  stack reads as a clean column instead of each line being independently (and
  raggedly) centered.
- **Each player's full discard pile now renders as a compact card grid**, growing
  from one corner of the table toward the vertical middle: Player B's grows down
  from the top-left (respecting the blank separator below its status bar), adding
  a new column to the right once a column fills; Player A's mirrors this exactly
  from the bottom-right corner, growing up, adding columns to the left. New shared
  `tui_draw_discard_column()` handles both directions via signed row/column steps,
  with a safety clamp so columns stop before crossing into the centered hand/deck
  /combat-zone content in the middle. Verified up to a 17-card / 2-column pile (B)
  and 14-card / 2-column pile (A) via `tmux`, both totals matching exactly.
- **Corrected an oversized assumption**: hand-related buffers/loops assumed up to
  10-12 cards; the game rule (`discard_to_7_cards()`, called every `end_of_turn()`)
  actually caps hand at 7, and M1 only ever renders after a full turn completes
  (never mid-turn) -- so 7 is a real, not defensive, bound. Tightened
  `tui_draw_hand()`/`tui_draw_hand_vertical()`'s arrays and loop caps from
  12 to 7 accordingly.
- Verified: `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20; `tmux`-driven valgrind pass (0
  definitely-lost, same ncurses/terminfo pattern as before).
- **Known gap, discussed but not yet addressed**: `stda_tui.c` calls `play_turn()`
  in full per keypress, and `resolve_combat()` clears both combat zones before that
  call returns -- so `gstate->combat_zone` is always empty at draw time, meaning
  `tui_draw_combat_zone()`'s card-rendering path (as opposed to its "(0):" empty
  case) is not exercised by normal AI-vs-AI play under M1. Real coverage needs
  either a one-off synthetic/manual check or Milestone 2's finer-grained
  per-phase advancement (which would naturally pause after `attack_phase()`
  /`defense_phase()` while combat zones are populated).

## 2026-07-14 — TUI layout: mirrored status bars, combat-zone clustering, console wrap

Further Milestone 1 polish, still before starting Milestone 2.

- **Status bars now mirror across the screen's horizontal center line.** Player
  name is centered within the play-area ("table") width (previously the whole
  status line was left-jammed against column 0 of the full-width window, ignoring
  the info column alongside it); lunas/energy sit on the left edge of the table for
  both bars, status/role on the right edge for both bars (Player B's top bar
  previously had status/role on the left and lunas/energy on the right -- the
  opposite of the bottom bar). New shared helpers `tui_print_centered()` /
  `tui_print_3segment()` (moved into a new "Layout helpers" section, used by both
  the status bars and the play-area code) compute position from `pane_width`
  (`getmaxx(win_play)`), not the status window's own full-terminal width.
- **Both players' combat zones now cluster near the vertical middle**, next to the
  `-- combat zone --` divider, with hand/deck/discard pushed to the outer edges
  (near each player's own status bar) -- previously Player B's combat zone sat
  right under its hand/deck near the top, leaving a large blank gap before the
  divider, while Player A's deck/discard/hand sat right under its combat zone near
  the middle, leaving a large blank gap before the bottom status bar (backwards
  from what was intended).
- **One blank separator row** now sits between each status bar and the block next
  to it (top: below Player B's bar; bottom: above Player A's bar). Caught and fixed
  a bug during verification: the first pass put Player A's blank row in the middle
  of the reserved bottom block instead of as the very last row adjacent to the
  status bar -- a scripted `tmux` comparison against Player B's (correctly
  positioned) separator caught the asymmetry.
- **Console messages now wrap instead of truncating.** New
  `tui_build_console_segments()` wraps the most recent messages (bounded lookback)
  into fixed-width segments in chronological order; the display then takes just the
  tail segments that fit the console's height, same "recent window" logic as
  before but at wrapped-line granularity instead of raw-message granularity.
- Verified via `tmux` at 140x45 and the user's actual 281x65 Konsole size, plus the
  full regression/test/valgrind pass (identical `-a -p`, 10/10, 6/6, 20/20, 0
  definitely-lost).
- **Watch item**: `tui_render.c` is now 602 lines, over the 500-line soft limit (not
  the 1000-line firm one). Deferred splitting it while the layout is still being
  actively iterated on (per `cli_display.c`'s precedent, split once feature work in
  this file settles rather than mid-iteration).

## 2026-07-14 — `-h` usage text: added an Examples section

`print_usage()` (`src/main/cmdline.c`) now ends with 3 real-world usage examples (the
most common invocations so far): `-a -p` (automated AI-vs-AI, fixed seed), `-l -u=fr`
(interactive CLI, French UI), `-t -u=fr` (TUI, French UI). Along the way, confirmed
`-u=fr` (short option with `=`) actually works: `getopt_long_only` matches single-letter
names against the long-options table too (`"u"` is registered there), so it splits on
`=` the same way `--ui.lang=fr` does — not just a short-option-attached quirk.

## 2026-07-14 — `-A`/`--ai` now lists AI-agent shorthands instead of erroring

`./oracle -A` previously required an argument via getopt and just threw an unhelpful
getopt error if omitted. Now:

- `-A`/`--ai` takes an **optional** argument. Bare `-A` (or `--ai`) prints the list of
  11 agent shorthands (same roster as the CLI's `display_ai_strategy_menu()`) and exits
  cleanly (exit 0); an unrecognized value (`-Afoo`) prints an error plus the same list
  and exits with failure (exit 1); a valid shorthand proceeds to `MODE_CLIENT_AI` as
  before (still an unimplemented stub).
- New shorthand table (lowercase, letters/digits, <=10 chars each), case-insensitive
  matching: `rand`, `value`, `greedy`, `combo`/`borealis` (two aliases for the same
  agent, A4), `balanced`, `heuristic`, `hbt`, `hbt2ply`, `simplemc`, `ismcts`,
  `ismctsnn`. Implemented as `parse_ai_strategy_shorthand()` /
  `print_ai_agent_shorthand_list()` in `src/ui/shared/player_config.c/h` (reusing the
  existing `AIStrategyType` enum and `get_strategy_display_name()` rather than
  duplicating a second list), called from `src/main/cmdline.c`'s `case 'A':`.
- `print_usage()`'s `-A` entry now matches the `=[VALUE]` convention already used by
  `-u`/`-p` (optional args) and `-i`/`-o` (required args), plus an explicit note that the
  argument must be attached (`-Afoo`/`--ai=foo`), not space-separated — same
  getopt-driven limitation `-u`/`-p` already have, just undocumented there.
- Verified: all four combinations (bare, valid, invalid, both short/long forms) behave
  as designed; `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20.

## 2026-07-14 — TUI layout: centered play-area content ("table" feel)

Follow-up polish on Milestone 1 before starting Milestone 2. The play area previously
left-justified every label and card row at column 1 of `win_play`, so on any terminal
wider than the bare minimum the whole right side of the play area was empty space —
didn't read as a card table the way the target PDF/xlsx layout does.

- `src/ui/tui/tui_render.c`: added `tui_print_centered()` (single-line labels) and
  `tui_draw_card_row()` (a shared, wrapping, per-row-centered layout for both
  `tui_draw_hand()`'s and `tui_draw_combat_zone()`'s card lists, via a small `TuiCardCell`
  struct so both call sites build pre-formatted cells and hand them to one layout
  routine instead of duplicating the wrap/measure logic). Hand/combat-zone headers,
  deck/discard counts, and the `-- combat zone --` divider are now all horizontally
  centered in the play window; card rows are centered as a block per row too.
- Verified visually via `tmux` (now installed) at several sizes, including the practical
  minimum (100x30) and the user's actual full-screen Konsole size (281x65) — confirmed
  the "please enlarge" fallback and the 100x30 minimum both work correctly (an earlier
  live-resize report of needing 143x43 turned out to be Konsole not having reached
  100x30 yet mid-drag, not a bug).
- Re-verified: `-a -p` regression identical, `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20, and a `tmux`-driven valgrind pass (0
  definitely-lost, same ncurses/terminfo "possibly lost" pattern as Milestone 1).

## 2026-07-14 — TUI mode Milestone 1 (ncurses display skeleton)

`stda.tui` (`-t`/`--stda.tui`) is real: an ncurses text UI matching the target layout in
`Template TUI Game Interface.pdf`/`Gabarit Interface de Jeu Version Texte.xlsx` (2/3 play
area + 1/3 info column, mirrored top/bottom status bars, scrolling console). Milestone 1
is **display-only** — AI-vs-AI, one turn advances per keypress, no human interaction yet
(that's Milestone 2, see `doc/oracle_todo.md`).

- New `src/ui/tui/tui_render.c/h`: all ncurses window layout + drawing, fully responsive
  (`tui_layout()` recomputes every window from the live terminal size, handles
  `KEY_RESIZE`, shows a "please enlarge terminal" fallback below `TUI_MIN_COLS`x
  `TUI_MIN_ROWS` (100x30) and recovers cleanly once resized back up).
- New `src/roles/stda/stda_tui.c/h`: the real `run_mode_stda_tui()`, reusing
  `initialize_cli_game()`/`cleanup_cli_game()`/`apply_mulligan()` and driving `play_turn()`
  once per keypress; `q`/`Q` quits.
- `makefile`: added `-lncursesw` to `LIBS`.
- `game_constants.c/h`: added `CHAMPION_SPECIES_ABBR[]` (3-letter card labels), matching
  `CHAMPION_SPECIES_NAMES`'s existing English-only convention (species names aren't
  localized elsewhere in the codebase either).
- **Two real bugs found via testing, both fixed**: (1) the top status bar duplicated the
  PDF mockup's literal "Actif / En attente" header text instead of resolving it to a
  single computed label per player (copy-paste artifact caught by a scripted PTY
  walkthrough); (2) `setup_game()` never initializes `gstate->turn_phase`/
  `player_to_move` (only `begin_of_turn()` does) — the CLI never notices because it always
  runs `begin_of_turn()` before displaying anything, but the TUI draws once before the
  first `play_turn()` call, so `stda_tui.c`'s setup now sets both fields explicitly
  (caught by valgrind as an uninitialized-value read).
- **ncurses/`ChampionColor` naming collision** (`COLOR_RED` is both an ncurses macro and
  this codebase's own enum constant): `tui_render.h` never includes `<ncurses.h>` (forward
  -declares `WINDOW` as opaque); `tui_render.c` includes `game_types.h` first, then
  `<ncurses.h>`, then `#undef COLOR_RED`, using its own `NC_RED`/etc. constants for
  `init_pair()`. See `CLAUDE.md`'s "Known architectural gaps" for the durable note.
- Also fixed a related loop bug: pressing a key while the terminal was below the minimum
  size used to silently advance a game turn with nothing visible to show for it; now
  ignored until resized back up.

Verified via `make clean && make` (no new warnings), `./bin/oracle -a -p` regression
(identical), `make test_recall`/`test_cash_exchange`/`test_combo` (10/10, 6/6, 20/20), a
scripted PTY walkthrough (`python3` + the `pty` module) driving `stda.tui` through several
turns, a resize up/down/below-minimum/recovery cycle, and FR localization, plus a
valgrind pass (0 definitely-lost bytes; the only "possibly lost" blocks trace entirely
into `libncursesw`/`libtinfo` terminfo internals, a well-known false-positive pattern, not
Oracle's own code).

## 2026-07-14 — Source folder structure cleanup (pragmatic pass)

Pragmatic pass only (not the full v4 engine rewrite) — see
`ideas/1 improve source code folder structure/pragmatic_cleanup_implementation_plan.md`
for full detail.

- Split `cli_display.c` (576 lines) into `cli_display.c` (233 lines, core status/turn
  display) + new `cli_action_display.c` (357 lines, action-flow/card-selection display)
  — both now under the 500-line soft limit.
- Fixed `make test_combo`: stale include paths, stale Makefile paths, removed a test of
  the since-removed `get_order_from_species()`, and fixed a latent test bug where
  `CombatCard` literals left the (later-added) `.order` field zero, causing spurious
  order-match bonuses — now 20/20 passing.
- Doc sync: `CLAUDE.md` module layout / file-size / test-status notes updated.

Verified via `make clean && make` (no new warnings), `./bin/oracle -a -p` regression
(identical to `bin/expectedresults.txt`), `make test_recall`/`test_cash_exchange`/
`test_combo` (10/10, 6/6, 20/20), `testsrc/cli_scripts/` re-run, and a full valgrind pass
(0 errors/0 leaks, auto + interactive).

## 2026-07-14 — CLI AI-strategy menu synced with planned agent roster

`display_ai_strategy_menu()`/`get_ai_strategy_choice()`/`get_strategy_display_name()` in
`src/ui/shared/player_config.c` and the `AIStrategyType` enum now list all 11 planned
agents (`A1`-`A11`, skipping `A2` since parameter storing/optimization is calibration
tooling, not an agent) as "not yet implemented" stub menu entries, in `ideas/A#` order,
each with a comment cross-referencing its `ideas/A#` folder. `A4`'s menu entry is
explicitly labeled "Combo Aware [Borealis benchmark]". The former "Hybrid" entry is
confirmed to be `A7` (tactical+HBT: Heuristics+Balanced+Tactical) and is now labeled
"Hybrid (HBT)".

## 2026-07-14 — `ideas/` folder renumbering

Folders were renumbered twice in one session: first to flatten decimal numbers (`12.1`,
`14.3`, etc.) into plain integers, then to pull all AI-agent folders into their own
`A1`-`A11` namespace (kept in their existing relative order) so adding new AI ideas
doesn't require renumbering everything else. `ismcts_nn_overview.md` became its own
folder (`A11`) since it also covers the NN+MCTS extension, distinct from plain IS-MCTS
(`A10`). See `git log` if an old number (e.g. `ideas/8/`, `ideas/14.3/`) shows up in an
older doc or commit message.

## 2026-07-13 — Turn Logic Module: recall, cash exchange, combat display, discard display

Complete Turn Logic Module: full game loop working end-to-end in interactive mode with
all the rules.

- **Display Discard Pile in CLI Mode** — `gmst` (summary) and `shod` (detailed,
  power-sorted) commands; see `ideas/done/4 ...`.
- **Recall Card functionality in stda.cli mode** — recall is **exact and mandatory** (a
  "recall 1 / draw 2" card recalls exactly 1 champion, "recall 2 / draw 3" recalls
  exactly 2; recall is only offered when discard holds enough champions). The Random AI
  engine still only ever draws (never recalls), which is fine given it's not meant to be
  strong. See `ideas/done/2 ...`, `doc/game_rules_doc.md` (recall section corrected to
  match), and `testsrc/test_recall.c`. Implementation: `validate_and_recall_champions()`
  + `handle_recall_choice()` in `cli_input.c`, UI via `display_recallable_champions()`.
- **Combat results display in stda.cli mode** — per-champion rolls/base/combo/damage
  breakdown, shown whenever a human is involved; `stda.auto` unaffected. See
  `ideas/done/3 ...`. Implementation: `display_combat_details_cli()` in
  `ui/cli/cli_display.c` (now `cli_action_display.c` after the 2026-07-14 split).
- **Cash card champion selection in interactive mode** — ask user to select the champion
  card to exchange instead of the AI power-heuristic auto-pick; interactive path
  (`play_cash_card_interactive`) lets the human pick freely. Along the way, fixed a real
  bug in the AI heuristic (`select_champion_for_cash_exchange` conflated "not found"
  with card index 0, a valid champion, using it as a sentinel — now uses `UINT8_MAX`).
  See `ideas/done/5 ...` and `testsrc/test_cash_exchange.c`.

**Note**: fixing the index-0 sentinel bug changed `stda_auto`'s RNG-dependent play
sequence (different AI hand state whenever that bug used to fire), so
`bin/expectedresults.txt` was regenerated (2026-07-13) to reflect the corrected behavior
— this was a deliberate re-baseline, not a regression.

All four verified via `make test_recall` / `make test_cash_exchange`, the
`testsrc/cli_scripts/` manual scripts, a full valgrind pass (auto + interactive), and the
`./bin/oracle -a -p` regression check against the regenerated `bin/expectedresults.txt`.
