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
- Treating a player's whole remaining deck as one uniform unknown pool once a reshuffle
  has happened — see the correctness note below.

## `A8`'s shipped determinization is deliberately not reshuffle-aware

`A8` Simple Monte Carlo was implemented 2026-08-25 (`doc/changelog.md`) and, contrary to
this document's own earlier framing, ended up including determinization
(`ai_strat_playout.h`'s `mc_determinize()`) rather than leaving it entirely to this
agent — the design-comment stub `A8` was built from called for it explicitly. `A8`'s
version treats a player's whole remaining deck as one uniform unknown pool regardless of
reshuffles, i.e. exactly the simplification the correctness note below says not to make.
That gap is deliberate, not an oversight in `A8` (see its own `about.md`) -- it's what
`A10` still adds beyond `A8`, alongside the tree itself.

## Correctness note: reshuffle narrows the determinization, don't discard that

`card_actions.c`'s `shuffle_discard_and_form_deck()` reshuffles a player's *own* discard
back into their own deck when it empties (per-player, not shared). After that reshuffle,
that deck's **composition is exactly known** (it's precisely the prior discard pile,
which was visible) — only the **draw order** is still hidden. A determinization that
samples card identities uniformly from "everything not in hand, not yet played" throws
away that composition information post-reshuffle. Determinize by sampling *permutations
of the known composition* for any player-deck that has been reshuffled, not by treating
the whole remaining pool as uniformly unknown. Full reasoning and the downstream
implications for opponent-hand inference and `A11`'s state encoding:
`../A11 ai agent is-mcts + nn (alphaoracle prime)/local_training_plan.md`.

## Design sources

- `ismcts_overview.md` (this folder) — determinization, tree structure, four-phase
  iteration, Oracle-specific hidden-information challenges.
- `mcts_depth_strategy.md` (this folder) — why dice rolls don't need sampling, and how
  that affects achievable search depth given Oracle's branching factor.
- `../A11 ai agent is-mcts + nn (alphaoracle prime)/nn_mcts_overview.md` — the next rung,
  replaces this agent's rollouts/heuristics with a trained network.
- `../A11 ai agent is-mcts + nn (alphaoracle prime)/local_training_plan.md` — includes the
  reshuffle-aware determinization note above, in full, plus a hardware-grounded local
  training plan for `A11`.
