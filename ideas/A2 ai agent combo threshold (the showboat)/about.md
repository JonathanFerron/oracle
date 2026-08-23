# A2 — Combo Threshold · "The Showboat" / "Le Frimeur" / "El Fanfarrón"

|                      |                                                                         |
| -------------------- | ----------------------------------------------------------------------- |
| Enum                 | `AI_STRATEGY_COMBO_THRESHOLD` (retired name: `AI_STRATEGY_COMBO_AWARE`) |
| Shorthand            | `combo` (sole canonical shorthand — `showboat` was never shipped as one; see "What Changed and Why" in the names file) |
| Est. Borealis rating | 37 — design intent, unmeasured (see below: measured strength vs `A1`/Random, no Borealis benchmark yet) |
| Source file          | `src/ai_strat/ai_strat_combo_threshold.c` |
| Status               | **implemented and calibrated** (2026-08-22) — see `doc/changelog.md` |

## The one thing this agent does

Chases champion combinations whose combo bonus clears a tunable threshold, hoards big combos for a finishing blow, and defends probabilistically — sometimes declining a defence it should take. This was the original candidate for the rating-scale benchmark; it lost that role to `A3` Borealis precisely because these traits are exploitable rather than diffuse (see this folder's handout §3), which is exactly what makes it a legible, teachable rung below the benchmark instead.

## Measured strength (2026-08-22, `doc/changelog.md`)

Calibrated via `aicalibsrc/combo/calibrate_combo_threshold.py`'s `optimize` (differential
evolution vs `A1` Value Based, since vs-Random saturates near a ceiling the same way it did
for `A1`). Both seats, 80,000 games:

| Opponent | Uncalibrated defaults | Calibrated (shipped) |
| --- | --- | --- |
| `value` (`A1`) | 49.94% | **58.77%** [58.29%, 59.26%] |
| `rand` | 88.11% | **92.78%** [92.52%, 93.03%] |

The optimizer's raw winner reached 77.3% vs `value` but was rejected and hand-patched:
`aggression_level` divides both combo thresholds (handout §5.4), and the optimizer had
pushed it to 2.21, which collapsed the effective 2-card threshold low enough to admit
every combo bonus (including a plain color pair) — erasing this agent's stated
"chases only the spectacular" selectivity. Shipped `aggression_level=1.3` keeps that
selectivity identical to the untuned defaults while keeping the optimizer's gains on the
other eight parameters. See `doc/changelog.md`'s 2026-08-22 entry for the full parameter
values and reasoning.

## Deliberately out of scope

- Exhaustive subset enumeration or a single monotone strength dial — that's `A3` Borealis.
- Reliable defence — the probabilistic decline is the point, not a bug to fix.
- Resource targets tied to opponent energy (`A4` Balanced Rules), phase/aggression
  modelling (`A6` Tactical), advantage functions over energy/cards/cash (`A5` Heuristic), any lookahead or simulation.

## Design sources

- `combo_threshold_handout.md` (this folder) — full spec; §3 explains the benchmark
  reassignment, §8 explains why the weaknesses are kept as personality.
- `../A3 ai agent greedy power (borealis)/greedy_power_borealis_handout.md` §8 — the λ-dial argument this design lost to.
- `../G1 AI agent general info/oracle_ai_agent_names.md` — canonical roster/ratings, "What Changed and Why" section.
