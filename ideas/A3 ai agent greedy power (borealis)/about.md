# A3 — Greedy Power · "Borealis" (identical in EN / FR / ES)

| | |
|---|---|
| Enum | `AI_STRATEGY_BOREALIS` (retired name: `AI_STRATEGY_GREEDY_POWER`) |
| Shorthand | `borealis` (sole canonical shorthand — no alias; see the names file's "Required renames") |
| Est. Borealis rating | **50 — the scale anchor, by definition** |
| Source file | `src/ai_strat/ai_strat_borealis.c` + `ai_strat_borealis_enum.c` |
| Status | **implemented and calibrated** (2026-08-23) — see `doc/changelog.md` |

## The one thing this agent does

Exhaustively enumerates every legal champion subset of size 0–3, scores each by
`Σ contribution + combo_bonus − λ·Σ cost`, and plays the best — breaking near-ties at
random (epsilon window) and holding back very large combos for a finishing blow. λ is the
single, monotone strength dial that makes this agent — and only this agent — fit to anchor
the Bradley-Terry rating scale at 50.

## Measured strength (2026-08-23, `doc/changelog.md`)

Calibrated via `aicalibsrc/borealis/calibrate_borealis.py`'s `optimize` (differential
evolution vs `A2` Combo Threshold — vs `rand` is ceiling-effected the way it was for
`A1`/`A2`, and vs `A1` Value Based was already near parity, so `A2` had the most
headroom). At the handout's untuned defaults (`luna_value=0.5`), Borealis actually *lost*
to `A2`. Both seats, validated:

| Opponent | Uncalibrated defaults | Calibrated (shipped) |
| --- | --- | --- |
| `combo` (`A2`) | 43.63% | **69.13%** [68.67%, 69.58%] (40,000 games) |
| `value` (`A1`) | 49.63% | **74.19%** [73.51%, 74.87%] (16,000 games) |
| `rand` | 93.13% | **99.33%** [99.19%, 99.45%] (16,000 games) |

A manual sweep found the true `luna_value` optimum (~4.0–4.5) sits far above the
handout's own default guess (0.5) or its §13 sweep grid's max (2.0); the shipped value
(4.5846, from `optimize`) was re-checked for §8's unimodality property (quadratic fit of
a follow-up sweep, other five parameters fixed) and confirmed concave-down. See
`doc/changelog.md`'s 2026-08-23 entry for the full parameter values, the reasoning behind
`lethal_combo_bonus`/`min_hand_size_target` landing at their search bounds, and a
parallel-execution bug found and fixed in the calibration driver along the way.

## Deliberately out of scope

- Pruning candidates on combo bonus before scoring — that threshold-gated shortcut is
  precisely what `A2` Combo Threshold does, and precisely why it isn't the benchmark
  (see `A2`'s handout §3).
- Resource targets tied to opponent energy (`A4`), phase/aggression modelling (`A6`),
  advantage functions (`A5`), any lookahead or simulation.
- Weakening via randomly skipping correct plays — weaken only by moving λ off-optimum,
  which stays diffuse and legible rather than exploitable.

## Design sources

- `greedy_power_borealis_handout.md` (this folder) — full spec: scoring model,
  enumeration bounds, epsilon tie-break, lethal-combo holding, calibration protocol.
- `../G1 AI agent general info/oracle_ai_agent_names.md` — canonical roster; this agent is
  the "50 by definition" anchor row.
