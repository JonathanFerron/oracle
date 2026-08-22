# A5 — Heuristic · "ε-γ-δ" (deliberately breaks the flavour-name pattern — see below)

|                      |                                                                                              |
| -------------------- | -------------------------------------------------------------------------------------------- |
| Enum                 | `AI_STRATEGY_HEURISTIC`                                                                      |
| Shorthand            | `heuristic`                                                                                  |
| Est. Borealis rating | 70 — design intent, unmeasured                                                               |
| Source file          | `src/ai_strat/ai_strat_heuristic1.c` (design-comment stub exists; logic not yet implemented) |
| Status               | design notes only                                                                            |

## The one thing this agent does

Reduces the whole game to one weighted advantage function,
`Advantage = ε·EnergyAdv + γ·CardsAdv + δ·CashAdv`, and picks the legal move (1-move lookahead) that maximises it. Its entire identity is its three weights — the names file notes this is why it's named after them rather than given a flavour name, "a conscious exception, not an oversight."

## Deliberately out of scope

- Dynamic/adaptive weights — a fixed ε, γ, δ per game is the point. Weights that change with game phase belong to `A6` Tactical and the synthesis in `A7` Hybrid HBT.
- Resource-target formulas as a first-class mechanism (`A4` Balanced Rules) — though the design notes flag open questions about whether specific ε/γ settings *reduce to* Balanced Rules' behaviour.
- Subset enumeration or combo-bonus scoring as primary logic.

## Design sources

- `src/ai_strat/ai_strat_heuristic1.c` — the formulas, calibration approach (AI-vs-AI sweeps over ε/γ), and open design questions (equivalence to Balanced Rules; whether to add a hand-power/combo-potential term).
- `../A7 ai agent hybrid hbt (the grandmaster)/hbt_design_notes.md` — this agent supplies the "systematic move evaluation" layer of the A7 synthesis.
- `../G1 AI agent general info/balanced_tactical_hbt_comparison.md` — comparison against `A4` and `A6`.
