# A10 — IS-MCTS · "The Omniscient" / "L'Omniscient" / "El Omnisciente"

| | |
|---|---|
| Enum | `AI_STRATEGY_ISMCTS` |
| Shorthand | `ismcts` |
| Est. Borealis rating | 92 — design intent, unmeasured |
| Source file | `src/ai_strat/ai_strat_ismcts1.c` (design-comment stub exists; logic not yet implemented) |
| Status | design notes only |

## The one thing this agent does

Information Set Monte Carlo Tree Search: repeatedly determinizes the opponent's hidden
hand/deck consistent with what's publicly known, builds/grows a UCT tree over that sample
(select → expand → simulate → backpropagate), and aggregates across determinizations.
"Deep tree search over hidden information" (names file).

## Deliberately out of scope

- A learned policy/value network guiding the tree — that's `A11`'s addition on top of this
  agent. This agent's rollouts and selection are hand-computed (UCT + closed-form dice
  stats), not learned.
- Hand-written positional heuristics as the primary evaluator — this agent's strength
  comes from search depth and determinization, not from a scoring formula like `A4`–`A7`.
- Sampling dice rolls via Monte Carlo — `mcts_depth_strategy.md` (this folder) shows dice
  have closed-form mean/variance; use that instead of rollout sampling for the dice
  component specifically.

## Design sources

- `ismcts_overview.md` (this folder) — determinization, tree structure, four-phase
  iteration, Oracle-specific hidden-information challenges.
- `mcts_depth_strategy.md` (this folder) — why dice rolls don't need sampling, and how
  that affects achievable search depth given Oracle's branching factor.
- `../A11 ai agent is-mcts + nn (alphaoracle prime)/nn_mcts_overview.md` — the next rung,
  replaces this agent's rollouts/heuristics with a trained network.
