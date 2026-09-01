# A13 — Cartographer · "The Cartographer" / "Le Cartographe" / "El Cartógrafo"

| | |
|---|---|
| Enum | **not registered** — `AI_STRATEGY_CARTOGRAPHER` was removed from `AIStrategyType` (`game_types.h`) when this agent was shelved; see "Measured: shelved" below |
| Shorthand | `carto` (retired, unused — not wired into `AI_STRATEGY_SHORTHANDS[]`) |
| Est. Borealis rating | 68 (design-intent estimate, never reached — see "Measured" below) |
| Source file | `src/ai_strat/ai_strat_a13.{c,h}` + `ai_strat_a13_belief.{c,h}` + `ai_strat_a13_state.{c,h}` + `ai_strat_a13_enum.{c,h}` — code remains on disk as reference; not compiled into any `STRATEGY_REGISTRY` entry, not selectable in the CLI/TUI, not part of `--stda.rating`. `ai_strat_a13_belief.{c,h}` (the closed-form unseen-pool/hypergeometric machinery) is kept specifically as reusable infrastructure for a future `A11` feature-extraction pass. |
| Status | **implemented, calibrated, and SHELVED (2026-08-31)** — every mechanism measured at parity with `A7` or worse, across four independent properly-powered searches. See "Measured: shelved" below for the full record. |

## Measured: shelved, nothing beat A7

Every one of the four new layers below was implemented, verified (the superset guarantee —
neutral config recovers `A7` bit-for-bit — held through every round of edits, confirmed via
whole-game bit-for-bit replay, not just unit-level checks), and calibrated. None produced a
measurable improvement over `A7`, at either the Borealis-relative metric or the actual ship
gate (head-to-head vs `hbt`, the bar this agent was always held to per Jonathan's
2026-08-31 direction — a good Borealis-relative rating does not by itself establish a win
against the specific agent a design is built on, as `A7`-vs-`A5` and `A9`-vs-`A7` both
already demonstrated on this roster):

| Configuration | vs `borealis` | vs `hbt` (ship gate) |
|---|---|---|
| Neutral (= `A7`) baseline | 65.10% [64.58, 65.62] | 50.00% [49.45, 50.55] |
| Layer R alone (properly-powered `optimize`) | 64.82% — parity | 49.93% — parity |
| Layer K-draw+D alone (bug-fixed value function) | 64.68% — parity | 49.93% — parity |
| Layer K-block `hplus_trust` (sweep, both opponents) | 17-64% monotonic decline | 20-50% monotonic decline |
| Layer K-block `belief_opp_block_trust` (sweep, live-wired) | flat, no direction | flat, no direction |
| Broad joint search, R+K-draw+block_trust (630 evals) | 65.18% — parity | 49.80% — parity |
| Stage 4 joint, + `defense_stdev_mult` free | 64.88% — parity | 49.91% — parity |

`hplus_trust` is the one mechanism that is not merely neutral: it is **conclusively,
severely harmful**, reproducing `A9`'s exact `reply_trust` failure signature (a clean
monotonic decline vs both opponents, tight non-overlapping confidence intervals) even more
sharply. Every other configuration — isolated, staged, and jointly searched, including a
630-evaluation search specifically designed to rule out a coordinate-descent trap across
layers — lands within noise of doing nothing.

Two genuine implementation bugs were found and fixed along the way, independent of the
calibration verdict: (1) the original `pool_mean_power()` averaged over the *whole* unseen
pool including non-champion cards, diluting `draw_value` against its `AVERAGE_POWER_FOR_
MULLIGAN` comparison baseline by a large, roughly-constant amount unrelated to real pool
depletion — this alone produced a spurious catastrophic-looking collapse (34.7% win rate!)
at strongly positive `belief_draw_weight` in an early sweep, a bug signature rather than a
real finding; (2) after Jonathan clarified that `power`/`expected_attack`/`expected_defense`/
the two efficiency fields are all his own early heuristic guesses (not authoritative), the
value function was rebuilt around the *empirically measured* champion attack:defense
play-role split (78.21%/21.79%, counted directly from ~8000 real games) applied to
`attack_efficiency`/`defense_efficiency` instead of the naive 50/50 `power` blend — the
result was flatter, not better, ruling out "wrong value function" as the explanation for
Layer K-draw's null result. See the `project_a13_cartographer` memory for the full,
turn-by-turn calibration record, including a driver bug (`defense_stdev_mult` silently
staying pinned in Stage 4's first attempt despite being explicitly requested) found and
fixed in the same session.

**Verdict (Jonathan's call, 2026-08-31):** shelve rather than register. A second `A7` under
a new name at best (and a net-negative agent if `hplus_trust` had shipped) would pollute the
Bradley-Terry fit — the same call already made for `A9`'s own below-target 2026-08-28
re-attempt, applied here to a fully-null rather than partially-successful result. The
sections below are preserved as the original design record.

## Where this sits in the roadmap

`doc/oracle_roadmap.md`'s "Next Up" (agreed 2026-08-28) makes this agent the active item:
*"a new deterministic AI agent with no design constraint beyond determinism: a synthesis of
whichever techniques from `A1`-`A12` have proven out, plus room for new ideas, aiming for the
strongest deterministic play the project can produce."* Jonathan clarified in the design
conversation that new ideas are explicitly in scope, not just recombination — this agent is
not merely "`A7` again."

**The bar.** `A7` Hybrid HBT = rating **65**, the best deterministic agent on the roster. `A10`
IS-MCTS = **69**, the roster ceiling, but stochastic (it re-determinizes randomly every
iteration). This agent must beat 65 to justify shipping; 69+ would move the roster ceiling to
deterministic play.

## The one thing this agent does

`A7`'s exact three-layer synthesis, unmodified, plus four new deterministic mechanisms layered
on top — each independently sweepable, each pinned to a neutral value that recovers `A7`
bit-for-bit. The unifying idea across all four: **model hidden information as a distribution
computed in closed form, never as a point estimate, and never simulate the opponent's
decision.** That is a direct, deliberate answer to `A9`'s failure (see below).

### Why closed-form belief, not a surrogate hand

`A9` HBT 2-Ply added a *deterministic* one-ply opponent reply against a single fabricated
"surrogate hand," blended by `reply_trust`. Win rate vs `A7` declined **monotonically** with
trust — 47.6% / 43.4% / 39.4% / 37.3% / 31.2% at trust 0/0.25/0.5/0.75/1.0 — so `reply_trust=0`,
which provably recovers `A7` exactly, was the optimum on the entire curve
(`../A9 ai agent hbt 2 ply (grandmaster ii)/about.md`). `A8`/`A12`'s rollout policies (35, 31)
showed the same class of defect from a different angle: an estimator with systematic bias that
more computation cannot correct.

This agent's answer: never fabricate one hidden hand and simulate a decision against it.
Instead, derive the exact unseen-card pool by subtraction from the known 120-card
`fullDeck[]`, and compute closed-form hypergeometric expectations over it. The engine already
does the equivalent by sampling — `ai_strat_playout.c`'s `mc_determinize()` (`A8`'s
determinization) re-deals the opponent's hand, the observer's own deck, and the opponent's
deck from exactly this pool. This agent computes analytically what that function samples.

**The caveat that bounds this mechanism.** `setup_game()` deals only 80 of the 120 cards — 40
per player — so **40 cards never enter play at all**. With `|unseen| ≈ 90-114` and an opponent
hand size `h ≤ 12`, `P(card ∈ opponent hand) ≈ 5-8%`: the opponent-hand belief is genuinely
diffuse. The sharply-known quantity is **the pool's own mean value**, which drifts as
high-power cards are observed leaving it. Expect the draw-valuation half of this design to pay
off before the opponent-modelling half — see the risk ranking below.

### The four new layers

**Layer R — race arithmetic.** Deterministic turns-to-kill both ways
(`opp_energy / my_sustainable_damage`, `own_energy / opp_sustainable_damage`, continuous, not
`ceil()`-based) turns `A7`'s *fixed* signed `defense_stdev_mult` into a *state-dependent* one:
ahead in the race → deflate variance (protect the win, block more); behind → inflate it (seek
variance). This is the direct completion of `A7`'s own sign-unification of `A4`'s deflation and
`A6`'s inflation into one dial (`../A7 .../about.md`'s "Implementation note"). Lowest risk of
the four: belief-independent, touches no `A7` file (`incoming` is already a float parameter to
`evaluate_defense_subset()`).

**Layer K (draw) — deck-aware draw valuation.** `A5`/`A7`'s `gamma` treats every card in a
DRAW-card's yield as equally likely; the pool mean is not constant, it drifts as the game
progresses. Values a DRAW candidate against the live pool mean instead of a flat average.

**Layer D — reshuffle-boundary awareness.** `deck[player].top + 1` is public pile height. When
a player's deck empties, *their own discard pile* — visible to them exactly, card by card — is
reshuffled back in (`card_actions.c`). Near that boundary, `E[next draw]` is not the diffuse
pool mean but a sharp, exactly-known number over the discard. Makes Draw-card *timing*
genuinely skillful. New to the roster; ~25 lines.

**Implementation correction (2026-08-31):** an original third dial here, `belief_opp_power_trust`
(blending A6-style `estimate_opponent_power()` toward `pool_mean_power` for the same opponent-
capability quantity `race_use_belief_opp` already selects between), was dropped while writing
`ai_strat_a13_state.c`: it would have been a redundant third tier on a quantity
`race_use_belief_opp` already makes a clean two-tier choice for, and it had a real units
mismatch (the non-belief fallback is naturally in `expected_attack` units, `pool_mean_power` is
in `power` units — blending them directly would have been dimensionally wrong). `race_use_belief_opp`
is this agent's only opponent-capability-estimate toggle; the dial count is 10, not 11.

**Layer K (block) — Jensen-corrected expected block.** The version that *doesn't* replay `A9`'s
failure. A naive `max(raw(S) − E[block], 0)` is a monotone transform of `raw(S)` (`E[block]`
does not depend on the chosen subset `S`), so it only discourages attacking uniformly relative
to pass/draw/cash — `A9`'s exact signature, and already foreclosed by `A5`'s own header (a
constant-fraction block model is absorbed into `epsilon`). The corrected version is
`E[net(S)] = Σ_k P(K=k) · min(max(raw(S) − E[block|k], 0), opp_energy)`, conditioned on the
hypergeometric distribution over how many affordable champions the opponent holds. `max(·,0)`
is convex, so Jensen's gap is strictly positive and **shrinks as `raw(S)` grows** — penalising
small, easily-absorbed attacks relative to committed 2-3 champion subsets. Subset-dependent,
and the opposite sign to `A9`'s mechanism. Highest risk of the four (see below).

### The superset guarantee

There exists one parameter vector — all 10 new dials at their neutral value (0, or `false` for
the one boolean) — under which this agent is **`A7` bit-for-bit**: same move, same tie-breaks,
same mulligan, same discard. Enforced by short-circuit (`a13_belief_needed()` skips the whole
belief computation when every belief-consuming dial is neutral), not by multiplying by zero, so
recovery is exact regardless of any degenerate-pool arithmetic and cost collapses to `A7`'s in
the neutral configuration. This is the same property that made `A9`'s failure cleanly
diagnosable, and it gives every one of the four layers a safe floor: any layer that doesn't
calibrate is pinned to neutral and the agent silently degrades toward `A7` rather than toward
something worse.

## On the estimate (historical — written before calibration; see "Measured: shelved" above for what actually happened)

Every other row on this roster's naming table carries a naive pre-implementation guess, and
every one of them overshot what was later measured (`A4` 62→36, `A6` 74→52, `A8` 82→35, `A9`
85→59, `A10` 92→69 — see `../G1 AI agent general info/oracle_ai_agent_names.md`). This entry's
68 is not that kind of guess: it is the honest post-mortem discount already applied, landing
just above `A7`'s 65. Beating `A7` by 1-3 points without reaching `A10`'s 69 is the modal
outcome and is still a success — it would make this agent the best deterministic agent on the
roster and close the roadmap item. If every new dial calibrates to neutral, this agent is
shelved rather than registered (Jonathan's call, 2026-08-31) — a second `A7` under a new name
would pollute the Bradley-Terry fit, the same call the project made for `A9`'s 2026-08-28
re-attempt.

## Risk, ranked most → least likely to fail (historical prediction — confirmed accurate: `hplus_trust`, ranked #1, was the one mechanism that actually failed catastrophically; the other three ranks 2-4 all landed at the predicted "flat/no signal" rather than "declining", exactly as anticipated)

1. **Jensen-corrected block (K-block) — high.** Signature: monotone win-rate decline as its
   trust dial rises, mirroring `A9`. Likely cause if so: the correction is only ~1-3 energy,
   small next to `eps_eff · ΔEnergy`. *Fallback:* pin to 0.
2. **Opponent-hand belief (K-block's distribution input) — medium-high.** Signature is
   **flat** (no information), not *declining* (misinformation) — that distinction is the
   empirical test of this design's central hypothesis. A flat curve confirms the hypothesis
   while showing no headroom in *this* game, because the 40 undealt cards dilute the belief.
   A cheap kill-test (empirical σ of the expected-block estimate over 1000 games) is run before
   any full calibration budget is spent on this layer.
3. **K-draw + Layer D — medium-low.** The pool mean genuinely drifts as high-power cards leave
   it, and the reshuffle signal is exact, not diffuse.
4. **Layer R — lowest risk, highest novelty.** Signatures: the race-scale dial saturating at
   its ceiling, or the two variance dials converging to each other (state-dependence collapsed
   back to `A7`'s constant).

## Deliberately out of scope

- Any change to `A7`'s own 34 inherited parameters beyond `defense_stdev_mult` (which Layer R
  turns from *the* value into a *baseline*, and which is therefore the one field re-fit in an
  optional final joint stage) — no `--identity-safe` escape hatch for the other 33, since there
  is nothing of `A7`'s own tuning to erode (`A9`'s reasoning, `../A9 .../about.md`).
- Sampling, rollouts, or re-determinization of any kind (`A8`, `A10`) — every belief quantity
  here is closed-form and exact given public information, computed once per decision.
- Reading `deck[*].card_indices` or `hand[opponent].cards` — the anti-clairvoyance rule. The
  unseen pool is `120 − own_hand − both_discards − both_combat_zones`; it deliberately does
  **not** subtract the observer's own deck, matching `ai_strat_playout.c`'s
  `mc_determinize()` — a player does not know their own deck's contents either.

## Design sources

- `../A7 ai agent hybrid hbt (the grandmaster)/about.md`,
  `../A7 .../hbt_design_notes.md` — the synthesis this agent inherits verbatim as `.base`.
- `../A9 ai agent hbt 2 ply (grandmaster ii)/about.md` — the cautionary precedent this design
  is built to avoid repeating.
- `src/ai_strat/ai_strat_playout.c` — `mc_determinize()`, the canonical (sampling) definition
  of the unseen pool this agent computes over analytically instead.
- `src/core/game_state.c`'s `setup_game()` — the 80-of-120 deal that bounds the opponent-belief
  mechanism's ceiling.
- `../G1 AI agent general info/oracle_ai_agent_names.md` — roster naming and rating context.
