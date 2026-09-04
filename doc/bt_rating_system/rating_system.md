# Oracle Rating System — Bradley-Terry Model Reference

This document explains the theory and design rationale behind Oracle's rating
system: why Bradley-Terry, how the scale and update rule work, and how the
pieces fit together mathematically. For the actual API, read `src/rating/rating.h`
(thoroughly commented, always current). For the dated history of what was
built/fixed on porting this design into real code, see `doc/changelog.md`'s
2026-08-23 entry. For a runnable visual exploration of the scale/curve
properties below, see `rating_strength_analysis.html` in this same folder
(a standalone Chart.js page, predates the "Borealis" naming but uses no
scale-specific labels that would make it stale).

Consolidated 2026-09-04 from three superseded pre-implementation design
iterations (`ideas/5 rating system/v0/v1/v2`, since deleted and this whole
folder moved from `ideas/` to `doc/` — git history has the originals if
needed): the underlying Bradley-Terry math didn't change across those
iterations, only the benchmark's name ("Keeper" → "Borealis", `doc/changelog.md`'s
2026-07-27 rename) and the display scale (0-1000 → 1-99, this doc's §3). Where
those old drafts described a prototype `oracle_rating.h` API that was never
merged, this doc omits that material entirely — the real API in `src/rating/`
diverged from it in several ways (e.g. `uint32_t` win counts, not the
`uint8_t` that silently overflowed past 255 games).

---

## 1. Why Bradley-Terry

The Bradley-Terry model assigns every entrant a **strength** parameter `s > 0`.
The probability that entrant *i* beats entrant *j* is a simple ratio:

```
P(i beats j) = s_i / (s_i + s_j)
```

**Key properties:**
- Symmetric: `P(i beats j) + P(j beats i) = 1`
- Equal strength → `P = 0.5`
- `s_i >> s_j` → `P → 1`; `s_i << s_j` → `P → 0`

**Why this over Elo?** Elo's win probability is a logistic function of a
rating *difference* (`1 / (1 + 10^(-Δ/400))`) on an arbitrary scale. Bradley-Terry's
probability is a direct *ratio* of strengths, which:
- has a clear probabilistic interpretation (no arbitrary constants like Elo's 400),
- relates directly to win ratios rather than an abstract rating gap,
- scales naturally to round-robin comparisons among many entrants at once
  (exactly Oracle's `--stda.rating` use case — see `src/roles/stda/stda_rating.c`),
- extends cleanly to draws (weighted 0.5 to each side).

## 2. Borealis as the Fixed Anchor

Bradley-Terry strengths are only defined up to a positive scale factor: multiplying
every entrant's strength by the same constant `α` leaves every probability
unchanged (`P = s_i/(s_i+s_j) = αs_i/(αs_i+αs_j)`). Something has to break that
ambiguity, so one entrant is fixed as the **anchor**:

```
s_Borealis = 1.0   (by definition, always)
```

`AI_STRATEGY_BOREALIS` (`A3`) is the anchor — a deliberately simple, deterministic,
calibrated agent chosen as a stable reference point, the same role "0°C" plays for
the Celsius scale. Every batch fit and every incremental update rescales
(`rating_rebalance_to_borealis()`) so Borealis's strength returns to exactly 1.0
afterward — this is what prevents **rating inflation/deflation** drifting the whole
scale over time.

For any entrant with strength `s`:

```
P(beat Borealis) = s / (s + 1)
```

| `s` | `P(beat Borealis)` |
|---|---|
| 0.001 | ≈0.1% (nearly hopeless) |
| 1.0 | 50% (equal to Borealis) |
| 99.0 | 99% (nearly unbeatable) |

## 3. Rating Scale (1-99)

The whole point of the scale is that **rating = win probability against Borealis**,
not an abstract number:

```
R = 100 × P(beat Borealis) = 100 × s / (s + 1)
```

| Rating | Strength | Meaning |
|---|---|---|
| 1 | 0.0101 | ~1% win vs Borealis |
| 25 | 0.333 | 25% win vs Borealis |
| **50** | **1.0** | **equal to Borealis, by construction** |
| 75 | 3.0 | 75% win vs Borealis |
| 99 | 99.0 | ~99% win vs Borealis |

Clamped to `[1, 99]` (`RATING_MIN`/`RATING_MAX`, `rating.h`) rather than `[0, 100]`:
at the true extremes the strength formula becomes numerically unstable (a rating
of exactly 0 or 100 implies infinite or zero strength), and 1%-99% win rates
already cover every practically meaningful case.

**Deriving the inverse** (rating → strength), useful whenever a target win
probability needs converting back to a strength for a fit or a sanity check:
let `P = R/100`. Then `P(s+1) = s`, so `Ps + P = s`, so `P = s(1-P)`, giving:

```
s = R / (100 - R)
```

Sanity check: `R = 50` → `s = 50/50 = 1` ✓ (equals Borealis).
`R = 75` → `s = 75/25 = 3` (3× stronger than Borealis).

**Win probability between any two (non-anchor) entrants** follows directly —
convert both ratings to strengths, then apply the core formula:

```
s_i = R_i / (100 - R_i),   s_j = R_j / (100 - R_j)
P(i beats j) = s_i / (s_i + s_j)
```

*Worked example*: entrant A at rating 74 (`A11`), entrant B at rating 69 (`A10`).
`s_A = 74/26 = 2.846`, `s_B = 69/31 = 2.226`.
`P(A beats B) = 2.846 / (2.846+2.226) = 56.1%` — a rough BT-implied estimate;
`A11`'s actual measured head-to-head vs `A10` was 58.44% [56.93%, 59.94%]
(`doc/changelog.md`'s 2026-09-03 entry), reasonably close, the residual being
real matchup-specific effects a single scalar strength can't fully capture.

## 4. Adaptive Learning Rate

Incremental updates (`rating_update_match()`, real-time/no-history-needed) use a
per-game multiplier `A` that **decays with games played**, so new entrants adapt
fast while established ones stay stable against noise:

```
A(games) = a_min + (a_max - a_min) × exp(-games / a_decay_rate)
```

Shipped defaults (`rating_default_config()`, `rating_core.c`): `a_max = 1.30`,
`a_min = 1.08`, `a_decay_rate = 150`.

| Games played | `A` | Behavior |
|---|---|---|
| 0 | 1.30 | large rating swings — fast convergence for a new entrant |
| 150 | ≈1.13 | moderate |
| 500+ | →1.08 | small, stable adjustments |

**Per-game update** (one game at a time, wins/losses/draws interleaved in the
order they occurred — see below for why order matters here):

```
expected = s_i / (s_i + s_j)
actual   = 1.0 (win) / 0.5 (draw) / 0.0 (loss)
delta    = actual - expected

s_i' = s_i × A^delta
s_j' = s_j × A^(-delta)
```

The exponential form guarantees strength stays positive no matter how large
`delta` gets — a plain additive update could drive `s` negative.

**Path dependence**: because each game's `A` depends on games-played-so-far,
incremental updates are *order-dependent* — applying 10 wins then 10 losses
does not land at the same strength as interleaving them, since the multiplier
decays as it goes. `rating_update_match()` interleaves wins/losses/draws
proportionally through the batch specifically to avoid the systematic bias an
all-wins-then-all-losses ordering would introduce (see `doc/changelog.md`'s
defect-fix list). This path-dependence is exactly why a second, order-independent
method exists — see §5.

## 5. Two Ways to Compute a Rating

**Incremental** (`rating_update_match()`): fast, real-time, no history needed,
but path-dependent (an approximation, per §4).

**Batch** (`rating_batch_compute()`): an order-independent maximum-likelihood fit
over every accumulated match at once. Two solvers, selected by
`RatingConfig.batch_method`:

- **`RATING_BATCH_MM`** (default) — the Zermelo/Newman/Hunter multiplicative
  fixed-point iteration, `s_i ← W_i / Σ_j N_ij/(s_i+s_j)`. Parameter-free and
  monotone in log-likelihood (each iteration cannot decrease it).
- **`RATING_BATCH_GRADIENT`** — gradient ascent on the same log-likelihood,
  normalized by total game count so a fixed `gradient_learning_rate` (default
  0.5) stays stable regardless of dataset size. Kept mainly as a cross-check
  against MM, not as the primary method.

Both converge to the same maximum-likelihood strengths (`convergence_threshold`,
default `1e-6`, capped at `max_iterations`, default 1000) and both rescale to the
Borealis anchor at the end. `--stda.rating`'s round-robin benchmark mode uses the
batch fit; live/interactive play uses the incremental path (`stda_rating_track.c`).

**Log-likelihood being maximized** (both methods, same objective):

```
L(s_1, ..., s_n) = Σ_(i,j) W_ij × log(s_i / (s_i + s_j))
```

where `W_ij` is the number of times *i* beat *j* (draws split 0.5/0.5 to each side).

## 6. Confidence Intervals

With few games, a rating is uncertain. `rating_confidence_interval()` computes
a proper **Wilson score interval** (not the simpler normal/Wald approximation
older drafts of this design used, which becomes unreliable at small `n` or
extreme win rates) on an entrant's own overall win rate across whatever
opponents it has actually played, returned as a rating-point-equivalent
half-width for a caller-chosen `z` (1.96 for ~95%).

As games played grows, the interval narrows — intuitively, standard error
shrinks like `1/√n`, so quadrupling the sample roughly halves the half-width.
This is why the project's own ship-gate measurements (see `doc/changelog.md`)
use `n` in the 4,000-40,000 range depending on how small an effect needs
resolving: distinguishing a 74 from a 75 rating (about a 1 percentage-point
win-rate difference) needs a far larger sample than confirming a clear ~8-point
gap.

## 7. Mathematical Properties

**Scale invariance**: multiplying every strength by the same `α > 0` leaves every
probability unchanged — the reason an anchor is needed at all (§2) to pin down an
otherwise-arbitrary overall scale.

**Not perfectly transitive in probability**: `P(A beats B) = 0.7` and
`P(B beats C) = 0.7` does *not* imply `P(A beats C) = 0.49` — transitivity holds
in *strength ratios*, not probabilities: `(s_A/s_B) × (s_B/s_C) = s_A/s_C`.

**Rating gaps mean more at the extremes**: because the mapping from rating to
strength is nonlinear, the same rating difference implies a bigger strength
ratio near the edges of the scale than in the middle:

| Gap | Ratings | Strength ratio |
|---|---|---|
| 100 (0-1000 scale) / 10 (1-99 scale) | 50 vs 40 | 1.25× |
| | 70 vs 60 | 1.75× |
| | 90 vs 80 | 4.5× |

## 8. Comparison with Other Rating Systems

| System | Scale | Update | Interpretation |
|---|---|---|---|
| **Oracle** | 1-99 | `s' = s × A^delta` | Direct: rating = win% vs anchor |
| Elo | Arbitrary (1500 typical) | Linear, `R' = R + K(S-E)` | Abstract rating difference |
| Glicko | Arbitrary | Bayesian | Abstract, with uncertainty (`RD`) |
| TrueSkill | `μ ± σ` | Bayesian | Full skill distribution, not a point estimate |

Oracle's specific tradeoff: simpler than Glicko/TrueSkill (no explicit uncertainty
tracked per-entrant beyond the Wilson interval computed on demand, §6), but every
rating is directly interpretable as a win probability against a fixed, known
opponent — nothing needs a lookup table or a second entrant's rating to interpret.

## FAQ

**Why clamp to 1-99 instead of 0-100?** At the true extremes the strength
formula is numerically unstable (rating 0 or 100 implies zero or infinite
strength); 1%-99% covers every practically meaningful win rate.

**How many games for an accurate rating?** Rough guide from this project's own
ship-gate practice (`doc/changelog.md`): tens of games for a clear signal,
thousands for a headline number, tens of thousands to resolve a 1-2 point gap
near an existing rating (§6).

**Why can batch and incremental give different numbers for the same data?**
Incremental is a fast, path-dependent approximation (§4); batch is the
order-independent maximum-likelihood fit (§5). They should agree closely on
large, well-mixed datasets and can diverge more on small or lopsided ones.

**Does a stronger opponent pool inflate everyone's rating?** No — precisely
because Borealis is rescaled back to strength 1.0 after every update (§2),
the scale can't drift regardless of who else plays.
