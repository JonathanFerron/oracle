# A6 — Tactical · "Pressure Cooker" / "Cocotte-Minute" / "Olla a Presión"

|                      |                                                                                                                 |
| -------------------- | --------------------------------------------------------------------------------------------------------------- |
| Enum                 | `AI_STRATEGY_TACTICAL` (new — missing from the enum until this folder-sort pass; see repo `doc/oracle_todo.md`) |
| Shorthand            | `tactical`                                                                                                      |
| Est. Borealis rating | 74 — design intent, unmeasured                                                                                  |
| Source file          | `src/ai_strat/ai_strat_tactical.c` (not yet written)                                                            |
| Status               | design notes only                                                                                               |

## The one thing this agent does

Classifies the game into a phase (early / mid / late / critical, by energy thresholds) and
derives a single 0.0–1.0 aggression factor from energy difference, hand power, and cash surplus, then reads the position and turns up the heat as the position sharpens.

## Deliberately out of scope

- A fixed, unchanging advantage function — that's `A5` Heuristic. Tactical's whole point is that its weighting *moves* with the position; a static version of this agent is a
  worse Heuristic, not a Tactical agent.
- Resource-target formulas as primary logic (`A4` Balanced Rules) — phase/aggression modelling can consume those targets but doesn't replace them.
- Any lookahead, simulation, or tree search.

## Design sources

- `tactical_design_notes.md` (this folder) — `GamePhase` enum, `StrategicState`
  evaluation, aggression-factor derivation, full attack/defense algorithm sketch.
- `../G1 AI agent general info/balanced_tactical_hbt_comparison.md` — head-to-head reasoning against `A4` Balanced Rules that motivated keeping this as its own agent.
- `../A7 ai agent hybrid hbt (the grandmaster)/hbt_design_notes.md` — this agent supplies the "adaptive aggression / situational awareness" layer of the A7 synthesis.
