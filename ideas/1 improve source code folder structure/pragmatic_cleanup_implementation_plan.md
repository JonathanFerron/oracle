# Folder-8 Source Structure Cleanup — Pragmatic Pass Implementation Plan

**Status**: DONE (2026-07-14). Steps 1-3 and 5 executed and verified; Step 4 (explicit
non-scope) still applies going forward. This folder is now `ideas/1 improve source code
folder structure/`, still informally called "folder 8" below as its original id.

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

### Step 2 — Split `cli_display.c` (done 2026-07-14)

Kept a **single shared header** `cli_display.h` (avoids touching every caller's include
list) and split the implementation:

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

### Step 3 — Makefile updates (done 2026-07-14)

- Added `$(SRCDIR)/ui/cli/cli_action_display.c` to `TEST_RECALL_OBJS`/`TEST_RECALL_SRCS`
  (`test_recall.c`'s `cli_input.c` calls `display_recallable_champions`, now in the new
  file; it still also needs plain `cli_display.c` since `process_attack_command` calls
  `display_game_status`/`display_cli_help`).
- Fixed `TEST_COMBO_SRCS`/`TEST_COMBO_OBJS` to use `$(SRCDIR)/core/combo_bonus.c` and
  `$(SRCDIR)/core/game_constants.c`; also added an auto-run step (`./$(TEST_COMBO_TARGET)`)
  to the `test_combo` target so it behaves like `test_recall`/`test_cash_exchange`.
- Fixed `testsrc/test_combo_bonus.c`'s includes to `"../src/core/combo_bonus.h"` /
  `"../src/core/game_constants.h"`, and removed `test_order_mapping()` (and its call in
  `main()`) since `get_order_from_species()` no longer exists.
- **Extra fix discovered during execution, beyond original scope**: fixing the includes
  alone left `make test_combo` building but failing 6/20 tests. Root cause: `CombatCard`
  (`combo_bonus.h`) gained a third field, `.order`, since this test was written; the
  test's positional initializers (`{ SPECIES_HUMAN, COLOR_RED }`) only filled the first
  two members, silently leaving `.order` zero for every card and causing spurious
  same-order-bonus matches. Fixed by adding the correct `.order` value to every
  `CombatCard` literal (species→order mapping verified against `game_constants.c`'s
  `fullDeck[]`: Human/Elf/Dwarf→`ORDER_A`, Hobbit→`ORDER_B`, Orc/Goblin→`ORDER_C`), plus
  one test case ("Three same color") that needed different species chosen (it originally
  used Human+Dwarf, both `ORDER_A`, contaminating the color-only test) — now 20/20 pass.
- Not fixed (deferred, unrelated to this pass): `test_stda_auto`'s `./bin/oracle.exe` →
  platform-appropriate binary name.

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

### Step 5 — Update docs (done 2026-07-14)

- `CLAUDE.md`: updated the `ui/cli/` module-layout bullet to mention the
  `cli_action_display.c` split; replaced the stale "`stda_cli.c` currently exceeds the
  soft file limit" line with the resolved `cli_display.c` split (past tense); updated the
  "Interactive-only features" bullets' file references (`display_combat_details_cli`,
  `display_player_discard*` now live in `cli_action_display.c`); updated the Tests
  section to reflect `test_combo` now passing 20/20, with the `.order`-field root cause
  noted.
- `doc/oracle_todo.md` / `doc/oracle_roadmap.md`: to be marked done in a follow-up edit.
- `doc/oracle_design.md`'s stale "File Size Violations" section (still references the old
  `stda_cli.c` monolith) remains a known, separate, larger doc-sync task — still out of
  scope for this pass.

## Verification (executed 2026-07-14 — all passed)

1. `make clean && make` — clean build, no new `-Wall` warnings. ✅
2. `./bin/oracle -a -p | diff -w -B - bin/expectedresults.txt` — identical. ✅
3. `make test_recall` (10/10) and `make test_cash_exchange` (6/6). ✅
4. `make test_combo` — 20/20 (see Step 3's extra `.order`-field fix). ✅
5. Re-ran `testsrc/cli_scripts/*.txt` — same markers present ("Combat Resolution",
   "Recalled 1 champion", discard counts, "DWARF" in the cash-exchange transcript). ✅
6. `valgrind --leak-check=full --show-leak-kinds=all` on `-a -p -n 3` and on
   `hva_combat_display.txt` — 0 errors, 0 leaks, all heap blocks freed, on both. ✅
7. `wc -l` — `cli_display.c` 233 lines, `cli_action_display.c` 357 lines, both under the
   500-line soft limit. ✅
8. `astyle --project --suffix=none` on the touched files — no changes needed (already
   matched project style). ✅
