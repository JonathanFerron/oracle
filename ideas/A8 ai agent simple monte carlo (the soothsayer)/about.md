# A8 — Simple Monte Carlo · "The Soothsayer" / "Le Devin" / "El Adivino"

| | |
|---|---|
| Enum | `AI_STRATEGY_SIMPLE_MC` |
| Shorthand | `simplemc` |
| Borealis rating | **35 — measured, 2026-08-25** (design-intent estimate was 82, unmeasured) |
| Source files | `src/ai_strat/ai_strat_simplemc1.{c,h}` + `ai_strat_simplemc_search.{c,h}`; shared infra `src/actions/move_gen.{c,h}`/`move_apply.{c,h}`, `src/ai_strat/ai_strat_playout.{c,h}` |
| Status | **implemented and calibrated** (2026-08-25) — see `doc/changelog.md` |

## The one thing this agent does

Flat rollouts with progressive pruning, no tree: enumerate all legal moves (0-3 champion
subsets, draw, recall, cash; typically ~a few dozen, capped at 128), simulate each via a
fresh determinized clone rolled out with uniformly-random play on both seats to a
terminal win/loss/draw, and prune as evidence accumulates. Two ideas from the original
design notes are reconciled rather than picking one: a small 7-rollout seed round drops
every 0-win candidate outright, then repeated rounds add 25 more rollouts per survivor
and drop anything whose confidence-interval upper bound falls below the current leader's
lower bound (normal approximation to the binomial) — this is the mechanism that actually
does the pruning. The originally-sketched fixed schedule (`Nm -> Nm^(3/4) -> Nm^(1/2) ->
Nm^(1/4)`, capped 30/10/4, at 100/300/700/1500 cumulative simulations) is layered on top
as hard survivor ceilings at those checkpoints, so the shipped agent still matches this
identity even where CI pruning alone would keep more candidates alive longer. Stops at
one survivor, 1500 cumulative sims/candidate, or 25000 total rollouts. "Rolls the dice a
thousand times before choosing" (names file).

## Measured result and diagnosis (2026-08-25)

Rating **35** — below the Borealis anchor (50), in the same tier as `A2`/`A4`, and
**not raised by more compute**: a budget-vs-rating sweep (1.0x/1.75x/2.3x the rollout
count, 1500 games/point vs `borealis`) measured statistically indistinguishable ratings
(35.4%, 38.2%, 35.9%, all overlapping 95% CIs). That flat curve is the signature of a
biased estimator, not an under-searched one — confirmed by playtracing three losses vs
`borealis`: the agent repeatedly favours resource-building moves (cash exchange, draw,
recall) over committing champions against a strategic opponent that keeps attacking,
because `mc_playout()` models *both* seats as `AI_STRATEGY_RANDOM` for every rollout
regardless of who the real opponent is (as this design's own stub specified: "randomly
... make moves 2+"). That makes every candidate's win-probability estimate answer "what
happens if the opponent now plays randomly forever" — accurate against `rand`,
systematically biased against a real strategic opponent that converts tempo into damage
more reliably than a random continuation predicts. Full sweep numbers, playtrace
excerpts, and the code-level confirmation: `doc/changelog.md`'s 2026-08-25 entry.

**Shipped as-is at rating 35, not chased higher.** This agent was never intended to be a
strong opponent — the original intent (restated directly when this result was reviewed)
is a tool for probing the value of every legal action from curated positions, to help
mine new heuristics for future agents. A cheap, non-tree heuristic rollout policy is the
lever that could plausibly raise the rating without more compute budget; whether/how to
pursue that without drifting this agent toward `A9`/`A10`/`A11`'s own territory is an
open design question, not resolved here.

## Deliberately out of scope

- Building a tree of any kind — a node per candidate move with no children is what makes
  this "simple." Tree search is `A10` IS-MCTS's job.
- **Superseded, 2026-08-25**: this used to also list "reasoning about hidden information
  via determinization" as `A10`'s job. It shipped as part of this agent instead — the
  design-comment stub's own `clone_and_randomize_gamestate()` call won over this
  document's earlier "out of scope" framing (see `ai_strat_playout.h`'s
  `mc_determinize()`). `A10`'s distinguishing contribution over this agent narrows to
  the tree itself plus *reshuffle-aware* determinization (a player's remaining deck
  composition becomes partially known after a reshuffle; this agent's
  `mc_determinize()` deliberately does not track that) — see
  `../A10 ai agent is-mcts (the omniscient)/about.md` and
  `../A11 ai agent is-mcts + nn (alphaoracle prime)/local_training_plan.md`'s
  narrowing note.
- Exact/closed-form expected-value computation of dice rolls as a *replacement* for
  simulation — this agent's whole point is sampling; see `A10`'s
  `mcts_depth_strategy.md` for where closed-form dice statistics *do* apply (tree agents,
  not this one).
- The "Interactive Mode AI assistant" display mode the original stub sketched (printing
  the top-4 candidate moves with their win rates) — deliberately deferred, a UI feature
  rather than part of making the agent play.

## Design sources

- `src/ai_strat/ai_strat_simplemc1.h` — the original design-comment stub, preserved
  verbatim as provenance, plus the full implementation summary of every decision made
  against it.
- `src/ai_strat/ai_strat_simplemc_search.{c,h}` — the progressive-pruning search itself.
- `src/actions/move_gen.h`, `src/ai_strat/ai_strat_playout.h` — the shared engine
  infrastructure this agent needed built first (legal-move enumeration; forked-RNG-stream
  cloning/determinization/rollout), reusable by `A9`-`A11`.
- `aicalibsrc/simplemc/README.md` — this agent's twenty parameters are a different shape
  than every other agent's (compute-budget dials vs decision-quality weights); read this
  before assuming a `sweep`/`optimize` result here means what it would for `A1`-`A7`.
- `../A10 ai agent is-mcts (the omniscient)/mcts_depth_strategy.md` — dice-statistics
  background relevant to the family, and how this agent's flat structure differs from a
  real tree.
- `doc/changelog.md`'s 2026-08-25 entry — full playtrace excerpts, the budget-sweep
  numbers, and the code-level root cause.
