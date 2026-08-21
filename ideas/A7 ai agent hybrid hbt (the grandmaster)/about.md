# A7 — Hybrid HBT · "The Grandmaster" / "Le Grand Maître" / "El Gran Maestro"

| | |
|---|---|
| Enum | `AI_STRATEGY_HYBRID_HBT` |
| Shorthand | `hbt` |
| Est. Borealis rating | 78 — design intent, unmeasured |
| Source file | `src/ai_strat/ai_strat_hbt.c` (not yet written) |
| Status | design notes only |

## The one thing this agent does

A fixed three-layer synthesis, in this order and no other: `A4` Balanced Rules **filters**
viable moves by resource constraints, `A6` Tactical **weights** the advantage function
dynamically by game phase/aggression, and `A5` Heuristic **ranks** the filtered moves by
the resulting weighted advantage function. Named "The Grandmaster" as synthesis of the
three approaches below it on the ladder.

**Tension flagged, not a blocker:** guideline (b) for this sort asks each agent for one
distinct personality rather than "a little of everything." A7 (and `A9` HBT 2-Ply) are
*by roster design* syntheses — that's what "hybrid" and "Grandmaster" mean. The resolution
taken here: the personality **is** the fixed three-layer synthesis itself, and nothing
below is license to add a fourth mechanism.

## Deliberately out of scope

- Any mechanism not already present in `A4`, `A5`, or `A6`. If an idea doesn't trace to one
  of those three, it belongs in a different agent, not bolted onto this one.
- Lookahead beyond the 1-move evaluation `A5` already does — 2-ply lookahead is `A9`'s job.
- Any sampling/simulation (`A8` Simple Monte Carlo and above).

## Design sources

- `hbt_design_notes.md`, `strat_hbt_sketch.h` (this folder) — the three-layer decision
  model, dynamic advantage-weight adjustment, `Move`/`StrategicState` struct sketches.
- `../G1 AI agent general info/balanced_tactical_hbt_comparison.md` — the original
  head-to-head reasoning across `A4`/`A5`/`A6` that produced this synthesis.
- `../A4 ai agent balanced rules (bean counter)/about.md`,
  `../A5 ai agent heuristic (eps-gam-del)/about.md`,
  `../A6 ai agent tactical (pressure cooker)/about.md` — the three source agents.
- `../A9 ai agent hbt 2 ply (grandmaster ii)/about.md` — the next rung, adds one
  opponent-response ply on top of this agent.
