# `stda_auto.c` Split Plan

**Status**: Not yet started. Do this alongside CSV export work (this folder, `ideas/11`),
not as a standalone refactor.

**Context**: This plan was worked out in a Claude.ai chat reviewing the codebase +
`doc/oracle_roadmap.md`, `doc/oracle_todo.md`, and the code review HTML. It supersedes
any earlier/looser "split stda_auto.c" notes elsewhere in `ideas/`.

---

## Why now, why not earlier

`stda_auto.c` (`src/roles/stda/stda_auto.c`) is flagged in `doc/oracle_todo.md` as
needing a refactor ("Extract simulation.c module"), but it's not on the roadmap's
"Next Up" list (TUI Milestone 2, then AI strategies A1–A4 take priority). The natural
trigger for doing this work is **CSV export**, because export needs to hook into the
same per-game loop and stats structures this split would touch anyway. Doing both at
once avoids two separate PRs touching the same working code.

## What's already done (don't redo this)

- Histogram magic numbers already extracted to `src/roles/stda/stats_constants.h`
  (`HISTOGRAM_NUM_BINS`, `HISTOGRAM_BIN_WIDTH`, `HISTOGRAM_MIN_VALUE`, etc.)
- HDCLL → fixed-array migration is complete. `apply_mulligan()` and friends use
  `Hand_remove()` / `Discard_add()` on fixed arrays now, not linked lists. No impact on
  this plan's structure, but don't be surprised the code looks different from older
  snapshots.
- Destination folders already scaffolded (placeholder `.txt` files only, no code):
  - `src/ui/simulation/simauto/` — display/callbacks for `stda.sim` mode
    (interactive-ish batch simulation UI). **Note**: this was scoped for `stda.sim`,
    which is distinct from `stda.auto`'s headless loop — don't assume `stda_auto.c`
    output code belongs here without double-checking scope.
  - `src/ui/simulation/simexport/` — CSV/export destination, tied to this `ideas/11`
    folder.

## Current file contents (what's being split)

`stda_auto.c` currently contains, in order:

1. `run_mode_stda_auto()` — mode entry point (~25 lines)
2. `run_simulation()` — orchestration loop, N games (~10 lines)
3. `play_stda_auto_game()` — single game execution (~40 lines)
4. `apply_mulligan()` — AI mulligan logic, power-heuristic (~40 lines)
5. `record_final_stats()` — stats accumulation (~20 lines)
6. `createHistogram()` + `present_results()` — histogram + console output (~90 lines)

## The split

### Phase 1 — Stats extraction (lower risk, do first even within this ticket)

Move out of `stda_auto.c`:

- `record_final_stats()` — mode-agnostic stats accumulation. Best home: a shared
  `sim_stats.c/h`, callable by both `stda_auto.c` and the new export code, since CSV
  export needs the same per-game data this function already collects.
- `createHistogram()` + `present_results()` — console-only presentation. Can stay
  adjacent to `stda_auto.c` (e.g. new `src/roles/stda/sim_stats.c` alongside it) rather
  than moving into `src/ui/simulation/simauto/`, since that folder is scoped for
  `stda.sim`'s interactive display, not `stda.auto`'s headless output. **Decide this
  placement explicitly before moving code** — don't default into `simauto/` just
  because it exists.

No changes to the game loop itself in this phase. Zero risk to working sim behavior.

### Phase 2 — Engine extraction (do as part of CSV export wiring)

Move out of `stda_auto.c`:

- `run_simulation()`
- `play_stda_auto_game()`

Rationale: CSV export needs a hook into the per-game loop to emit a row per game (and
possibly per-turn detail). Give that loop a stable, reusable home once, at the same
time export is wired in, rather than refactoring it now and touching it again shortly
after for export.

Suggested destination: keep in `src/roles/stda/` (e.g. `sim_engine.c/h`) unless the
export work reveals a cleaner shared boundary — this is deliberately left flexible
until you're in the code with the actual export requirements in hand.

### Explicitly out of scope for this split

- **`apply_mulligan()`** — still carries a `// TODO: look at moving ... to the
  strategy code instead` comment in the source. Leave it in place. It's AI-only
  (power-heuristic) logic; interactive mulligan (TUI Milestone 2 territory) is what
  will eventually force a real decision on where strategy-owned logic like this lives.
  Extracting it now, ahead of that decision, risks creating an awkward home that gets
  moved again shortly after.

## Suggested sequencing when this ticket is picked up

1. Confirm CSV export requirements first (per-game fields needed, summary fields
   needed — see `ideas/11` spec) since that determines what `sim_stats.c` needs to
   expose.
2. Do Phase 1 (stats extraction) — decide `sim_stats.c` placement explicitly (see
   above), move `record_final_stats()`, `createHistogram()`, `present_results()`.
3. Wire CSV export against the new `sim_stats.c` functions.
4. Do Phase 2 (engine extraction) as needed to add per-game export hooks into
   `run_simulation()` / `play_stda_auto_game()`.
5. Leave `apply_mulligan()` untouched.
6. Update `doc/oracle_design.md` module list and `doc/oracle_todo.md` checkboxes once
   done.

## Non-goals / explicit reminders for whoever (Claude Code included) picks this up

- Don't move code into `src/ui/simulation/simauto/` without confirming that's actually
  the right scope (it's for `stda.sim`, not `stda.auto`).
- Don't touch `apply_mulligan()`.
- Don't do this as a standalone refactor PR — it should ride along with CSV export
  work in this same `ideas/11` effort.
- Keep functions within the project's size guidelines (≤35 lines target, ≤100 firm
  limit; ≤400 lines/file target, ≤1000 firm limit) as new files are created.
