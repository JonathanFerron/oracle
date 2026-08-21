# A11 — IS-MCTS + NN · "AlphaOracle Prime" (identical in EN / FR / ES)

| | |
|---|---|
| Enum | `AI_STRATEGY_ISMCTS_NN` |
| Shorthand | `ismctsnn` |
| Est. Borealis rating | 97 — design intent, unmeasured |
| Source file | `src/ai_strat/ai_strat_ismctsnn1.c` (not yet written) |
| Status | design notes only — requires `A10` IS-MCTS as a working prerequisite |

## The one thing this agent does

Replaces `A10`'s random rollouts and UCT-only selection with a trained policy/value
network: the policy head focuses tree search on promising moves, the value head replaces
rollout-to-terminal simulation with a direct position estimate, trained via AlphaZero-style
self-play. "Search plus learned intuition" (names file).

## Deliberately out of scope

- Hand-written heuristics anywhere in the hot search path — if a signal matters, it should
  be learned by the network from self-play data, not encoded by hand (that's the entire
  distinction from `A4`–`A9`).
- Skipping `A10`. This agent is explicitly staged as "IS-MCTS Foundation" first,
  "Neural Network Integration" second — see design sources.
- Any part of the tree/determinization machinery diverging from `A10`'s — this agent
  changes *what guides* the tree, not the tree/determinization structure itself.

## Design sources

- `nn_mcts_overview.md` (this folder) — network architecture (policy/value heads),
  self-play training loop, four-stage implementation path, and the IS-MCTS vs NN+MCTS
  comparison table.
- `../A10 ai agent is-mcts (the omniscient)/ismcts_overview.md` — the prerequisite
  foundation this agent builds on.
