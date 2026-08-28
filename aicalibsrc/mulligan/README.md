# Mulligan / seat-advantage investigation tooling

Batch tooling for Next Up item 2 (`doc/oracle_roadmap.md`): does the game have a real
first-vs-second-player win-rate advantage, and does the mulligan card-count cap
(`mulligan_get_max_cards()`, `src/ai_strat/ai_strat_lib_heuristics.c`/`.h`) move it?

Unlike every `aicalibsrc/<agent>/` folder, this one isn't tuning an AI agent's playing
strength -- it investigates a single shared game-rule parameter's fairness. See
`calibrate_mulligan.py`'s own module docstring for why the usual
`sweep`/`optimize`/`selfplay`/`validate` shape doesn't fit, and what `seat`/`sweep`
do instead.

## Files

- `calib_mulligan.c` -- C harness. Links the game engine directly (same pattern as
  every `aicalibsrc/<agent>/calib_<agent>.c`), so it runs `run_simulation()` in-process
  with no subprocess-spawn or text-parsing overhead. Build with `make calib_mulligan`
  (from the repo root) -> `bin/calib_mulligan`. CLI: `<numsim> <seed> <agent_a>
  <agent_b> <max_cards>`, or `--print-defaults` to dump the compiled
  `MULLIGAN_DEFAULT_MAX_CARDS` as JSON.
- `calibrate_mulligan.py` -- Python driver on top of that binary. `DEFAULT_MAX_CARDS`
  is read once, at import time, from `bin/calib_mulligan --print-defaults`, so it
  cannot drift from the shipped default.

## Setup

```bash
make calib_mulligan      # from the repo root
```

Same Python dependencies as the other nine harnesses (`numpy`, `pandas`,
`matplotlib` for `sweep --plot`) -- already installed if any of them has been used.

## Usage

```bash
cd aicalibsrc/mulligan

# Seat advantage per agent, self-mirror (agent vs itself, both seats), at the
# current shipped mulligan cap. Any deviation of P(A wins) from 0.5 is purely the
# seat/mulligan effect, since both seats run the identical strategy.
./calibrate_mulligan.py seat --agents rand,value,combo,borealis,balanced,heuristic,tactical,hbt,hbt2ply \
    --numsim 2000 --replicates 4

# Does the mulligan cap actually control the seat effect's size? Sweep it for one
# agent (rand is the cleanest signal -- no strategic skill confound within itself).
./calibrate_mulligan.py sweep --agent rand --max-cards 0 1 2 3 \
    --numsim 2000 --replicates 4 --plot
```

Results are written to `results/*.{csv,png}` (gitignored) plus a stdout summary.

## Why self-mirror, not `--ai.a=X --ai.b=Y` pairs

A self-mirror match (`--ai.a=X --ai.b=X`) is the cleanest experimental design for
isolating the seat effect: both seats run the *identical* strategy, so any deviation
of Player A's win rate from 50% cannot be explained by one agent being stronger than
the other -- it's purely the seat/mulligan asymmetry. Cross-agent pairs (as used for
the original `combo` vs `rand` observation this investigation started from) conflate
the two effects and need mirrored-seat-orientation runs (as `--stda.rating`'s
`play_orientation()` already does) just to *cancel out* the seat effect, which is the
opposite of what this investigation wants to measure.

## `A10` IS-MCTS is excluded from the `sweep`

`ai_strat_ismcts_flat.c`'s `enumerate_mulligan_candidates()` is a fixed 0/1/2-card
subset enumeration (`enumerate_singles()` + `enumerate_pairs()`), not driven by
`mulligan_get_max_cards()` -- extending it to a variable cap needs a real
`enumerate_triples()` and more search-space work, out of scope here. It can still
appear in `seat` (at its own fixed behavior, `--max-cards` has no effect on it), but
not meaningfully in `sweep`.

## Batch-mode Player-A-always-first is intentional, not a gap

`setup_game()` hardcodes Player A to go first in every batch/`-a` game -- this is
deliberate, matching how every calibration/rating tool already measures both seat
orientations explicitly (`stda_rating.c`'s `play_orientation()`, every
`aicalibsrc/*/calib_*.c` harness) rather than relying on a coin flip that would need
a larger sample to average out the same information. Interactive mode has its own
random-first-player option (`player_config.c`'s `get_player_assignment()`,
`ASSIGN_RANDOM`) for ordinary human play.
