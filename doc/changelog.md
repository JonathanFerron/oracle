# Oracle Changelog

Completed work, most recent first. `doc/oracle_todo.md` tracks what's still open;
this file is where finished items go so the todo list doesn't keep growing.

---

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
