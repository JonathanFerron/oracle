# Folder-8 Source Structure Cleanup — Pragmatic Pass Implementation Plan

**Status**: Planned, not yet implemented (2026-07-13; updated 2026-07-14 to keep the
placeholder `.txt` scaffolding instead of deleting it, and again later 2026-07-14 after
`ideas/` was renumbered a second time -- this folder is now `ideas/1 improve source code
folder structure/`, still informally called "folder 8" below as its original id). This
is the concrete, scoped-down implementation plan -- pick this up in a future session.

## Context

`revised_folder_structure.md` in this same folder (really v4 content — see the
"Verdict" note below) is an ambitious target architecture bundling a full game engine
rewrite (pollable state machine, action system, event callbacks) plus network, rating,
config, persistence, and platform layers. Most of that is explicitly **out of scope
here** — it belongs to `ideas/2 game engine refactoring for GUI and network support/`
(game engine refactoring), which comes *before* `ideas/3 tui/` (TUI — renumbered from
folder 13, then folder 1, across subsequent `ideas/` reorgs) per the project's stated
sequencing, and to `ideas/5 rating system/`, `ideas/8 client server/`, `ideas/6 save and
load gamestate/`, `ideas/7 config file/` etc. for their respective concerns.

A prior reorg (see git log: "reorg files in folders", "small reorg") already moved the
real code into most of the v4 target's directory names (`core/`, `ai_strat/`,
`roles/stda/`, `structures/`, `util/`, `main/`, `ui/{cli,shared,gui,tui,simulation}/`) and
already split the old CLI monolith into orchestration (`roles/stda/stda_cli.c`, 203
lines) + display/input/game (`ui/cli/*`). So the actual remaining "folder structure" gap
is much smaller than the full v4 vision. **This pass is a pragmatic cleanup only** — fix
what's genuinely messy now, don't scaffold empty directories for features that don't
exist yet (`deck_formats/`, `game_rules/`, `interactive/`, `network/`, `persistence/`,
`config/`, `platform/` — those get created in folders 2/3/5/8/etc. (in current numbering)
when real code lands).

**Verdict on `revised_folder_structure v5.md`**: despite the filename, its content is
actually the "v4" design (confirmed against `old/Major Changes from v3 to v4.md`). It
supersedes `old/v1`, `old/v2`, `old/v3` for the target architecture, though those older
drafts contain some richer per-directory rationale (v1/v2) and a more mechanical
filesystem-level migration sequence (v1/v2/v3: create dirs → move files → split
oversized files → fix includes → update Makefile → test) that this pragmatic pass
follows in spirit, at a much smaller scale.

## Audit findings (verified 2026-07-13, updated 2026-07-14)

1. **10 placeholder `.txt` files sit inside real `src/` subdirectories with no
   accompanying code**, all originally **0 bytes** (pure filename-as-reminder, no
   content):

   - `src/actions/action structures, get list of possible actions, validate actions.txt`
   - `src/core/game engine, turn logic, action processor, combat resolution.txt`
   - `src/rating/BT rating system, batch, export, calibration.txt`
   - `src/ui/cli/cli display input and callbacks.txt`
   - `src/ui/gui/future sdl3.txt`
   - `src/ui/shared/ui shared callback functions and context.txt`
   - `src/ui/simulation/simauto/display and callbacks.txt`
   - `src/ui/simulation/simexport/sim export, sim stats.txt`
   - `src/ui/tui/tui display input and callbacks.txt`
   - `src/visibility/visiblegamestate, state filter.txt`

   **Decision (2026-07-14, reversed from the original plan below): keep this
   scaffolding.** The user wants these placeholder directories to stay in the tree as a
   map of planned modules — deleting them was rejected. Instead, each file now holds one
   explanatory sentence pointing at the relevant `ideas/` folder (done as part of this
   update, see Step 1). The one genuinely stale one — `src/ui/cli/cli display input and
   callbacks.txt` — is now marked as superseded by real code rather than deleted, so a
   human can still decide to prune it later once nothing references it.

2. **`src/ui/cli/cli_display.c` has grown to 576 lines**, over the project's soft
   500-line guideline (CLAUDE.md / `doc/oracle_design.md`), driven by the recall/cash
   /combat/discard display work completed just before this planning session.
   `cli_input.c` (487 lines) is under the soft limit — leave alone, just note it as a
   watch item for a future pass if it keeps growing.

3. **`make test_combo` is broken on two independent axes**, not just stale paths:
   
   - `testsrc/test_combo_bonus.c` includes `"../src/combo_bonus.h"` /
     `"../src/game_constants.h"` (pre-reorg flat layout) instead of
     `"../src/core/combo_bonus.h"` / `"../src/core/game_constants.h"`.
   - The Makefile's `TEST_COMBO_SRCS`/`TEST_COMBO_OBJS` reference `$(SRCDIR)/combo_bonus.c`
     instead of `$(SRCDIR)/core/combo_bonus.c` (same issue for `game_constants.c`).
   - **`test_order_mapping()`** in `test_combo_bonus.c` calls `get_order_from_species()`,
     which **no longer exists** in `combo_bonus.c` (only a stale `// OLD:` comment
     references it — order is now static per-card data in `fullDeck[]`, not derived).
     This test function must be removed (not just re-pathed), since the function it
     exercises was refactored away.

4. `stda_cli.c` (203 lines) is **not** actually oversized — `CLAUDE.md`'s current line
   "`stda_cli.c` currently exceeds the soft file limit... (already partially done under
   `ui/cli/`)" is stale and needs updating to point at `cli_display.c` instead.

## Plan

### Step 1 — Fill in the 10 stray empty `.txt` files (done 2026-07-14)

Rather than deleting these placeholders (the original plan below), each now contains one
sentence describing its purpose and pointing at the relevant `ideas/` folder, so the
scaffolding stays honest instead of being silently empty. No directory removal, no git
tracking changes beyond the file contents.

### Step 2 — Split `cli_display.c`

Keep a **single shared header** `cli_display.h` (avoids touching every caller's include
list) but split the implementation:

- `cli_display.c` (core status/turn display, ~220 lines — the functions that predate the
  recall/cash/combat/discard feature work): `display_player_prompt`,
  `display_player_hand`, `display_attack_state`, `display_game_status`,
  `display_cli_help`, `display_turn_header`, `display_game_summary`.
- **New** `cli_action_display.c` (~360 lines, the action-flow/card-selection display
  helpers — everything that grew from the recall/cash/combat/discard work, plus the
  pre-existing mulligan/discard-to-7 prompts since they're the same kind of helper):
  `display_card_with_power`, `display_mulligan_prompt`, `display_discard_prompt`,
  `display_player_discard`, `display_player_discard_detailed`,
  `display_recallable_champions`, `display_exchangeable_champions`,
  `display_combat_side` (static), and `display_combat_details_cli`.

Both files `#include "cli_display.h"`. No changes needed to callers (`cli_input.c`,
`cli_game.c`) since they already just `#include "cli_display.h"`.

### Step 3 — Makefile updates

- Add `$(SRCDIR)/ui/cli/cli_action_display.c` to `TEST_RECALL_OBJS`/`TEST_RECALL_SRCS`
  (`test_recall.c`'s `cli_input.c` calls `display_recallable_champions`, now in the new
  file; it still also needs plain `cli_display.c` since `process_attack_command` calls
  `display_game_status`/`display_cli_help`).
- Fix `TEST_COMBO_SRCS`/`TEST_COMBO_OBJS` to use `$(SRCDIR)/core/combo_bonus.c` and
  `$(SRCDIR)/core/game_constants.c`.
- Fix `testsrc/test_combo_bonus.c`'s includes to `"../src/core/combo_bonus.h"` /
  `"../src/core/game_constants.h"`, and remove `test_order_mapping()` (and its call in
  `main()`) since `get_order_from_species()` no longer exists.
- Consider also fixing `test_stda_auto`'s `./bin/oracle.exe` → platform-appropriate binary
  name while touching this area (small, optional, already flagged in CLAUDE.md).

### Step 4 — Explicitly NOT doing (documented so it isn't re-litigated later)

- No renaming `ai_strat_*.c` → `aistrat_*.c` (v4's proposed name) — pure churn, no
  functional benefit, current names are fine.
- No creating `deck_formats/`, `game_rules/`, `interactive/`, `network/`, `persistence/`,
  `config/`, `platform/` directories now — create each when its first real file is added
  (`ideas/2 game engine refactoring.../` for engine/actions/callbacks work, `ideas/3
  tui/` for TUI, `ideas/5 rating system/` for rating, `ideas/8 client server/` for
  client/server, `ideas/6 save and load gamestate/`/`ideas/7 config file/` for
  save-load/config when un-backburnered). This list is also tracked in
  `doc/oracle_todo.md` so it isn't lost between sessions.
- No touching `core/game_engine.c` / action-system / callback-system — that's
  `ideas/2 game engine refactoring for GUI and network support/`.

### Step 5 — Update docs

- `CLAUDE.md`: update the `ui/cli/` module-layout bullet to mention the
  `cli_action_display.c` split; fix the stale "`stda_cli.c` currently exceeds the soft
  file limit" line to instead describe `cli_display.c`'s split (past tense, resolved);
  update the "Interactive-only features" bullets' file references
  (`display_combat_details_cli`, `display_player_discard*`, `display_recallable_champions`,
  `display_exchangeable_champions` now live in `cli_action_display.c`).
- `doc/oracle_todo.md` / `doc/oracle_roadmap.md`: mark this folder-8 pass done once
  executed.
- `doc/oracle_design.md`'s stale "File Size Violations" section (still references the old
  `stda_cli.c` monolith) is a known, separate, larger doc-sync task — explicitly out of
  scope for this pass.

## Verification (for whoever executes this)

1. `make clean && make` — confirm clean build, no new `-Wall` warnings.
2. `./bin/oracle -a -p | diff -w -B - bin/expectedresults.txt` — must stay identical
   (pure file-move/split, zero behavior change expected).
3. `make test_recall` and `make test_cash_exchange` — both should still pass (10/10, 6/6)
   after the Makefile source-list update in Step 3.
4. `make test_combo` — should now build and pass after Step 3's fixes.
5. Re-run the `testsrc/cli_scripts/*.txt` interactive scripts — same transcripts as
   before (grep for the same markers used previously: "Exchanged DWARF", "Recalled 1
   champion", "Combat Resolution", discard counts).
6. `valgrind --leak-check=full` on the auto path and at least one interactive script —
   should remain 0 errors/0 leaks (no behavior change expected, but cheap to re-confirm
   given file moves touch the build graph).
7. `git status` — confirm nothing unexpected changed; the 10 placeholder `.txt` files
   from Step 1 stay in the tree (with their explanatory sentences) rather than
   disappearing.
