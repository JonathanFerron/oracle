# A12 — Clairvoyant · "The Clairvoyant" / "Le Voyant" / "El Clarividente"

| | |
|---|---|
| Enum | `AI_STRATEGY_CLAIRVOYANT` |
| Shorthand | `clairvoy` |
| Borealis rating | **31 — measured, 2026-08-25** |
| Source files | `src/ai_strat/ai_strat_clairvoyant1.{c,h}`; reuses `ai_strat_simplemc_search.{c,h}` and `SimpleMcParams` verbatim (see `ideas/A8 .../about.md`) |
| Status | **implemented, lightly calibrated** (2026-08-25) — see `doc/changelog.md` |

## Not part of the `A1`-`A11` ladder's authoritative order

This agent's enum ordinal sits after `AI_STRATEGY_ISMCTS_NN` (i.e. after `A11`), but it
is **not** the next rung on the ladder -- `A9` HBT 2-Ply remains next (see
`doc/oracle_roadmap.md`'s "Next Up"). It was built as a side exploration of `A8` Simple
Monte Carlo's own diagnosed ceiling (`ideas/A8 .../about.md`, `doc/changelog.md`'s
2026-08-25 A8 entry), not as a scheduled ladder step.

## The one thing this agent does

`A8`'s identical progressive-pruning search (same file, same `SimpleMcParams`, same
budget/pruning shape) with exactly one change: rollouts keep this agent's own future
moves uniformly random (still "a simple MC approach, no tree" -- the search itself
doesn't change) but give the *opponent's* simulated replies a cheap heuristic instead of
`A8`'s pure-random policy for both seats. The heuristic is deliberately not a search: a
single fixed candidate (the top up to 3 affordable champions by `power`, a cheap O(n)
partial selection, not an enumeration over subsets) scored by one closed-form formula
and committed only if it clears a threshold, else pass/decline.

Targets `A8`'s own diagnosed root cause directly: `mc_playout()`'s rollouts model *both*
seats as `AI_STRATEGY_RANDOM` regardless of who the real opponent is, so `A8`'s
win-probability estimates are calibrated against `rand` specifically and systematically
biased against a real strategic opponent. This agent's opponent-side heuristic is an
attempt at a more realistic (if still very cheap) proxy for that opponent.

## Two real defects found by playtracing (not the aggregate number alone)

The first smoke test measured 26.6% -- *below* `A8`'s 35, the wrong direction entirely.
Playtracing (not just the win-rate number) found two genuine bugs in the heuristic
formula itself, both the same shape:

- **Attack never weighed cost.** `expected_attack` is always positive, so the original
  `score > 0` threshold was a near-tautology: 100.0% of sampled attack decisions
  committed. The simulated opponent wasn't deciding anything -- it threw every
  affordable champion into the fight on every turn, modeling a strawman always-all-in
  opponent, not a sharper one.
- **Defense had the identical gap.** Once attack's cost term was added and defense's
  wasn't, the asymmetry showed up as a large, unusual seat-order gap (16.4 percentage
  points, well outside this project's normal range). Applying the same fix to defense
  (mirroring Borealis's own defense evaluation exactly: cap raw defense value at the
  incoming threat, then subtract `cost_weight * total_cost`) resolved the seat
  asymmetry as a side effect (down to 6.0pp, back in normal range) without a separate
  investigation being necessary.

Full traces: `doc/changelog.md`'s 2026-08-25 entry.

## `cost_weight` -- reused Borealis's lambda as a starting point, then swept it

Both the attack and defense formulas subtract `cost_weight * total_cost`. The starting
estimate was Borealis's own calibrated `luna_value`/lambda (4.5846, "damage-units per
luna", `ai_strat_borealis.h`) -- the same quantity, already tuned via real calibration
for this game. But it was tuned for a different job (helping Borealis choose *its own*
moves well), not this one (what makes the rollout's opponent-model produce value
estimates that lead *this* agent to its best decisions), so it was checked directly: a
14-point sweep (0.5 to 10.0, n=700 games/point vs `borealis`) found a genuinely unimodal
response (quadratic fit R²=0.73, implied optimum ≈3.86), with the borrowed value
measuring near the *low* edge of a fairly flat 1-7 plateau rather than its center.
Shipped at **3.0**, a real but modest gain over the borrowed default.

## Deliberately not pursued further

- **`ROLLOUT_ENERGY_WEIGHT`** (a small energy-differential nudge in the attack formula
  only, not in defense) was considered for a similar sweep and deliberately skipped: it
  contributes at most ~5-20% of the magnitude of the terms that actually decide most
  commits, so a same-scale sweep was assessed at roughly 15-20% odds of a >2-3
  percentage-point gain -- not worth chasing given `A9`-`A11` are expected to be
  materially stronger agents by design.
- **A wider-range energy-weight sweep**, or extending it to defense, treating energy
  differential as a real aggression dial rather than a minor nudge, was also considered
  and not pursued -- higher potential upside, but a bigger, more open-ended change than
  a constant sweep, closer in spirit to what `A9`'s phase/aggression work will cover.
- **`aicalibsrc/clairvoyant/`** (a full calibration harness matching `A1`-`A8`'s
  pattern) was deliberately not built -- the lightweight scratch-probe sweep above
  answered the only question that mattered (is this worth more investment), and the
  answer was "modest, not worth chasing further right now."

**Final measured rating: 31** (n=1500, both seats, vs `borealis`) -- close to but
consistently a few points below `A8`'s own 35 across every measurement in this
investigation. Shipped as-is: kept in the roster, selectable from every mode, as a
deliberately modest "fun, easy to beat" sibling of `A8`, not chased toward parity or
beyond.

## Design sources

- `ideas/A8 ai agent simple monte carlo (the soothsayer)/about.md` -- the search this
  agent reuses verbatim, and the diagnosis this agent is a response to.
- `src/ai_strat/ai_strat_clairvoyant1.h` -- the full implementation summary.
- `doc/changelog.md`'s 2026-08-25 entry -- full playtrace excerpts, the sweep table, and
  every measured number in this investigation.
