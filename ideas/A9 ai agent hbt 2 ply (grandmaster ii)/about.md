# A9 — HBT 2-Ply · "Grandmaster II" / "Grand Maître II" / "Gran Maestro II"

| | |
|---|---|
| Enum | `AI_STRATEGY_HBT_2PLY` |
| Shorthand | `hbt2ply` |
| Est. Borealis rating | 85 — design intent, unmeasured |
| Source file | `src/ai_strat/ai_strat_hbt2ply.c` (not yet written) |
| Status | design notes only |

## The one thing this agent does

`A7` Hybrid HBT plus one opponent-response ply: for each candidate move, estimate the
opponent's best reply (using `A7`'s own ranking, applied from the opponent's side) and
score on the resulting two-ply expectation rather than the immediate position. "The
Grandmaster, now anticipating your reply" (names file).

**Same flagged tension as `A7`:** this agent is a synthesis by design (Grandmaster II).
The distinct personality here is specifically *the added ply*, not a new evaluation
mechanism — see `A7`'s `about.md` for the fuller discussion.

## Deliberately out of scope

- Any evaluation mechanism not already in `A4`/`A5`/`A6`/`A7` — the only new thing this
  agent adds over `A7` is the second ply.
- Sampling/rollouts (`A8`) or a real search tree with backpropagation (`A10`) — this stays
  a fixed 2-ply minimax-on-expectations, not a general search.
- Determinization / hidden-information reasoning (`A10`'s addition).

## Design sources

- `../A7 ai agent hybrid hbt (the grandmaster)/hbt_design_notes.md`,
  `../A7 ai agent hybrid hbt (the grandmaster)/strat_hbt_sketch.h` — the base agent this
  one extends by one ply.
- `../G1 AI agent general info/oracle_ai_agent_names.md` — confirms ordering:
  "Simple Monte Carlo is weaker than HBT 2-Ply."
