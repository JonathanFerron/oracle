# A8 — Simple Monte Carlo · "The Soothsayer" / "Le Devin" / "El Adivino"

| | |
|---|---|
| Enum | `AI_STRATEGY_SIMPLE_MC` |
| Shorthand | `simplemc` |
| Est. Borealis rating | 82 — design intent, unmeasured |
| Source file | `src/ai_strat/ai_strat_simplemc1.c` (design-comment stub exists; logic not yet implemented) |
| Status | design notes only |

## The one thing this agent does

Flat rollouts with progressive pruning, no tree: enumerate all legal moves (max ~93),
simulate each against random play with the candidate pool shrinking geometrically
(`Nm → Nm^¾ → Nm^½ → Nm^¼`, capped 30/10/4) as simulation counts rise
(100 → 200 → 400 → 800, ~1500 total), then return the best-surviving move. "Rolls the
dice a thousand times before choosing" (names file).

## Deliberately out of scope

- Building a tree of any kind — a node per candidate move with no children is what makes
  this "simple." Tree search is `A10` IS-MCTS's job.
- Reasoning about hidden information via determinization — that's also `A10`'s addition.
- Exact/closed-form expected-value computation of dice rolls as a *replacement* for
  simulation — this agent's whole point is sampling; see `A10`'s
  `mcts_depth_strategy.md` for where closed-form dice statistics *do* apply (tree agents,
  not this one).

## Design sources

- `src/ai_strat/ai_strat_simplemc1.c` — the full progressive-pruning schedule, per-stage
  simulation counts, and the note that this agent doubles as an "Interactive Mode AI
  assistant" (a one-parent, many-children special case of the `A10` tree).
- `../A10 ai agent is-mcts (the omniscient)/mcts_depth_strategy.md` — dice-statistics
  background relevant to the family, and how this agent's flat structure differs from a
  real tree.
