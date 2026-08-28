# A10 — IS-MCTS · "The Omniscient" / "L'Omniscient" / "El Omnisciente"

| | |
|---|---|
| Enum | `AI_STRATEGY_ISMCTS` |
| Shorthand | `ismcts` |
| Est. Borealis rating | 92 — design intent; **measured 69** (n=10008 vs `Borealis`, 95% CI 67.6-69.5), the roster ceiling -- also 63.2% head-to-head vs `A7` (n=10008) |
| Source file | `src/ai_strat/ai_strat_ismcts1.c`, `ai_strat_ismcts_tree.c`, `ai_strat_ismcts_search.c`, `ai_strat_ismcts_flat.c` |
| Status | implemented and calibrated (2026-08-27), shipped `limit_iterations=4000` |

## The one thing this agent does

Information Set Monte Carlo Tree Search: repeatedly determinizes the opponent's hidden
hand/deck consistent with what's publicly known, builds/grows a UCT tree over that sample
(select → expand → simulate → backpropagate), and aggregates across determinizations.
"Deep tree search over hidden information" (names file).

## Phase 6 finding (2026-08-27): the rollout policy, not the tree, was the bottleneck

A budget→rating scaling curve (1k/4k/16k/64k iterations, ~2000 games/level vs `Borealis`,
uniformly-random rollout policy on both seats) showed a real rise from 1k (41.9%) to
4k-16k (~46-48%), then flattened all the way to 64k (46.2%) -- the same "more search
can't fix it" signature `A8`'s own diagnosis found, though not as severe (A8 was flat
from the start). A controlled A/B test isolating the rollout policy (everything else
identical, 16k iterations, n≈2000) confirmed the cause: swapping the rollout policy from
`AI_STRATEGY_RANDOM` to `AI_STRATEGY_HEURISTIC` (`A5`, with the
`project_a5_a7_defense_pass_dominance` fix applied) took the same measurement from 47.6%
to **63.0%** -- a ~15-point jump, dwarfing every other effect measured on this agent so
far. Shipped as `ai_strat_ismcts1.c`'s `heuristic_rollout_strategy_set()` (and the
matching change in `ai_strat_ismcts_flat.c` for mulligan/discard-to-7 scoring, which
scores candidates by rollout too and is subject to the identical bias). This is
consistent with the ISMCTS literature's own reported experience: a purely random rollout
policy's adequacy is domain-dependent (Powley/Cowling/Whitehouse's Spades player was
strong without heuristic knowledge; Oracle evidently is not -- random self-play games run
~34 turns on average vs ~7-20 for any real strategy, and are the only matchup that ever
reshuffles a deck, so random play here is qualitatively unlike skilled play, not just a
noisier version of it).

## Phase 6 finding #2 (2026-08-27): with the fixed rollout policy, more search past a
point actively hurts, so the shipped budget is 4000, not ~100000

Re-running the budget→rating curve with the heuristic rollout policy live surfaced a
second, different shape: win rate vs `Borealis` doesn't plateau, it **peaks then
declines**. Across 1k-16k iterations the curve is a noisy plateau (63-69%, no clean
interior maximum distinguishable from noise at n≈800-2000/level: 1k 63.4%, 2k 67.9%, 3k
68.1%, 4k 67.1%, 6k 66.7%, 8k 68.9%, 12k 64.8%, 16k 65.3%), but the decline beyond that is
real and large: 58.5% at 64k, 55.2% at 100k (quick n≈192 sample) -- each several standard
errors below the plateau. Working hypothesis: since `A5`'s heuristic rollout policy is
deterministic (not randomly varied) given a position, more search lets the tree converge
more confidently on lines that exploit that specific simulated opponent's quirks rather
than generalizing to a real one -- a different failure mode from finding #1's flat
plateau, and not yet root-caused beyond this hypothesis. **Shipped `limit_iterations =
4000`** (`ISMCTS_DEFAULTS`, `ai_strat_ismcts1.h`) -- the most rigorously sampled point in
the plateau (n=1992), a clean round number, and (confirmed afterward at n=10008) the
actual official rating: **69** vs `Borealis`, 63.2% head-to-head vs `A7`. This is a large,
deliberate departure from Phase 3's original ~100000-iteration/~1s-per-decision
calibration, which was calibrated against the since-replaced random rollout policy's own
timing -- see `ai_strat_ismcts1.h`'s header comment for the full note. A real, separate
bug was found and fixed along the way: `stda_auto.c`'s `play_stda_auto_game()` called
`apply_mulligan()` before initializing `gstate.turn` (only `begin_of_turn()` does, and
only when it's first called) -- invisible to every earlier agent's mulligan hook, since
none of them simulate forward from the mulligan point, but a genuine
uninitialized-stack-read once this agent's flat-rollout mulligan scoring did (caught by
valgrind, not by any test). Fixed by moving `gstate.turn = 0`/`turn_phase`/
`player_to_move` initialization before `apply_mulligan()`, matching the pattern
`stda_cli.c`/`stda_tui.c` already used.

## Deliberately out of scope

- A learned policy/value network guiding the tree — that's `A11`'s addition on top of this
  agent. This agent's tree selection is hand-computed (UCT), not learned; its rollout
  policy (as of 2026-08-27, see above) reuses `A5`'s existing heuristic rather than a
  bespoke or learned one.
- Hand-written positional heuristics as the tree's own primary evaluator — the tree's UCT
  selection/backprop is still what this agent adds over `A8`'s flat search; using `A5`'s
  heuristic as the *rollout* policy (leaf sampling, not tree-level scoring) is a different
  role than a closed-form scoring formula like `A4`-`A7` use as their entire decision.
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

## Phase 0 finding (2026-08-27): reshuffle-aware determinization deferred, not dropped

Measured across 11,000 self-play games (1000 each, fixed seed) spanning every implemented
agent -- `rand`, `value`, `combo`, `borealis`, `balanced`, `heuristic`, `tactical`, `hbt`,
`simplemc`, `hbt2ply`, `clairvoy` -- **reshuffles occur in 0 of 8000 real games played by any
of the 8 actual (non-random) strategies**, at turn counts up to max 42. Only pure-random play
(`rand` vs `rand`) reshuffles meaningfully often: 51/1000 real games (5.1%), average 34.1
turns, max 55. This confirms Jonathan's real-table experience and, more importantly, confirms
it against a much broader sample than the original single-matchup 500-game check: any agent
that plays with actual purpose finishes well before either 34-card deck empties.

**A nuance worth recording so it doesn't look like a contradiction later**: a single real
`simplemc` game (which triggers ~10-15 tree/rollout decisions) logged 736 reshuffle events in
its *internal* MC rollouts -- `clairvoy`, whose rollout policy is heuristic-guided rather than
uniform-random, logged only 15 for a comparable game. This is expected and *not* the gap this
document's correctness note targets: `mc_determinize()` fixes hidden information once, at the
start of each iteration, from the real (already-played) game history -- and real games never
reach a reshuffle there, per the finding above. Everything a rollout does *afterward* is
concrete simulated state, not hidden information, so an in-rollout reshuffle (a rollout's
random-ish policy simply playing long enough to empty a simulated deck) is handled correctly
by the engine's real `shuffle_discard_and_form_deck()` inside the fork -- there is no sampling
step left to get wrong. `A10`'s planned rollout policy is the same `AI_STRATEGY_RANDOM`
default as `A8`, so its rollouts will show the same pattern as `simplemc`'s; this is
informative about rollout *length/realism* (a random-policy tail is not very representative of
how the real game would actually end), not about the determinization gap.

**Conclusion: Phase 7 (reshuffle-aware determinization) stays deferred out of `A10` v1**, per
the plan's gate -- the correctness gap it would close essentially never fires in real play.

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
