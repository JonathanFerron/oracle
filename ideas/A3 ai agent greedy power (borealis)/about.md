# A3 — Greedy Power · "Borealis" (identical in EN / FR / ES)

| | |
|---|---|
| Enum | `AI_STRATEGY_BOREALIS` (retired name: `AI_STRATEGY_GREEDY_POWER`) |
| Shorthand | `borealis`, alias `greedy` |
| Est. Borealis rating | **50 — the scale anchor, by definition** |
| Source file | `src/ai_strat/ai_strat_borealis.c` (not yet written) |
| Status | design notes only |

## The one thing this agent does

Exhaustively enumerates every legal champion subset of size 0–3, scores each by
`Σ contribution + combo_bonus − λ·Σ cost`, and plays the best — breaking near-ties at
random (epsilon window) and holding back very large combos for a finishing blow. λ is the
single, monotone strength dial that makes this agent — and only this agent — fit to anchor
the Bradley-Terry rating scale at 50.

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
