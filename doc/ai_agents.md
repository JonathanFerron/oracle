# Oracle AI Agents — Consolidated Reference

One section per implemented agent (`A1`-`A13`, enum order), consolidated
2026-09-04 from each agent's separate `ideas/A#.../about.md` (the living,
dated status record), its pre-implementation design handout where one existed,
and `ideas/G1 AI agent general info/oracle_ai_agent_names.md` (the canonical
roster/naming file, folded in below as this document's table of contents).
Each per-agent section keeps: the identity card (with its **real measured**
Borealis rating, not a design-intent estimate, once one exists), what the
agent does and deliberately doesn't, and a math-oriented account of its
decision formula — named constants from the actual source file included as
breadcrumbs, since those are what you'd grep for. Dropped: purely procedural
pre-implementation scaffolding that's now moot (files-to-create checklists,
"verify before coding" notes, exact test commands, integration-point
checklists) — the agent is built; see the source file and `testsrc/` for
that. The original per-agent folders and the G1 roster file are deleted; git
history has them if ever needed.

For the dated history of what was measured/fixed/calibrated, see
`doc/changelog.md`. For the Bradley-Terry rating system itself, see
`doc/bt_rating_system/`.

---

## Roster (Table of Contents)

Ratings are on the Borealis scale (1-99), where the rating *is* the expected
win percentage against the Borealis benchmark agent (`doc/bt_rating_system/rating_system.md`).
`Measured` is the real fitted/measured rating; `Est.` is the original
design-intent guess made before the agent existed. Roster order matches
`AIStrategyType`'s declaration order (`src/core/game_types.h`).

| # | Tech/Math Name | Flavour (EN / FR / ES) | Shorthand | Measured | Est. |
|---|---|---|---|---|---|
| — | Random | The Gambler / Le Parieur / El Apostador | `rand` | 2 | 5 |
| A1 | Value Based | The Apprentice / L'Apprenti / El Aprendiz | `value` | 24 | 15 |
| A2 | Combo Threshold | The Showboat / Le Frimeur / El Fanfarrón | `combo` | 30 | 37 |
| A3 | Borealis *(benchmark)* | Borealis / Borealis / Borealis | `borealis` | 50 | 50 |
| A4 | Balanced Rules | Bean Counter / Compteur de Fèves / Contador de Frijoles | `balanced` | 36 | 62 |
| A5 | Heuristic | ε-γ-δ / ε-γ-δ / ε-γ-δ | `heuristic` | 64 | 70 |
| A6 | Tactical | Pressure Cooker / Cocotte-Minute / Olla a Presión | `tactical` | 52 | 74 |
| A7 | Hybrid HBT | The Grandmaster / Le Grand Maître / El Gran Maestro | `hbt` | 65 | 78 |
| A8 | Simple Monte Carlo | The Soothsayer / Le Devin / El Adivino | `simplemc` | 35 | 82 |
| A9 | HBT 2-Ply | Grandmaster II / Grand Maître II / Gran Maestro II | `hbt2ply` | 62 | 85 |
| A10 | IS-MCTS | The Omniscient / L'Omniscient / El Omnisciente | `ismcts` | 69 | 92 |
| A11 | IS-MCTS + NN | AlphaOracle Prime (all languages) | `ismctsnn` | 74 | 97 |
| A12 | Clairvoyant *(`A8`'s sibling, off-ladder)* | The Clairvoyant / Le Voyant / El Clarividente | `clairvoy` | 31 | — |
| A13 | Cartographer *(registered for its character, not its strength — see below)* | The Cartographer / Le Cartographe / El Cartógrafo | `carto` | 65 | 68 |

Measured values mirror `AI_STRATEGY_RATINGS[]` (`src/ui/shared/player_config.c`),
which the interactive AI strategy menu reads to print each agent's rating —
`~`-prefixed for a design-intent estimate, bare for measured. When an agent's
rating changes, update that table too (`doc/oracle_todo.md`'s new-agent
checklist).

**How the "Measured" numbers were obtained**: `Random`/`A1`-`A7` from the
2026-08-23 through 2026-08-25 `--stda.rating` round-robin fits
(`src/rating/`, `doc/changelog.md`); `A8`/`A9` from a large-sample direct
pairwise measurement against `borealis` specifically (1,500 and 20,000 games)
rather than a roster-wide fit — `A8` costs ~100x more per game than any
closed-form agent, making a full round-robin featuring it impractical, and
since this project's rating *is defined as* win rate vs Borealis, a direct
measurement against exactly that opponent is arguably more direct anyway, not
a lesser substitute; `A10`/`A11` similarly from direct pairwise measurement
(10,008 and 4,110 games) for the same reason (see each section below).

**Design-intent estimates missed by a wide, informative margin**: `A4` was
designed to sit above Borealis (est. 62) but measured **36**, below the
anchor, in the same neighborhood as `A2` (30) — the largest miss on this
table, a legitimate calibrated result, not a bug (`doc/changelog.md`,
2026-08-24). Every agent `A5` onward measured *below* its own estimate but
several measured comfortably *above* the anchor anyway (`A5` 64, `A7` 65,
`A10` 69, `A11` 74) — the estimates were systematically optimistic across the
whole ladder, not just for `A4`.

**Gaps are deliberately non-uniform** — the scale is non-linear (§3 of
`doc/bt_rating_system/rating_system.md`): a 10-point gap near 50 is a much
smaller strength difference than a 10-point gap near 90.

**Calibration target** (design intent, not yet fully realized): children aged
11-14 with moderate-to-frequent experience should average a 45-55% win rate
against Borealis, bracketed by a measured weaker neighbor near 40-45 and a
measured stronger one — the stronger half exists (`A5` at 64), the weaker half
does not (`A4` landed at 36, below Borealis rather than between `A2` and
Borealis as designed). Tune `A3`'s `λ` (`greedy_power_borealis_handout`'s
lambda, the value of a luna in damage units) against playtest data, not
simulation win rates — simulation finds the λ that maximizes strength, not
necessarily the λ that hits the target band.

### Ordering constraints established during design

`A2` sits between `A1` and Borealis; `A8` is weaker than `A9`; `A1` is only
marginally stronger than Random.

### ASCII-safe variants

For byte-oriented display layers (`get_strategy_display_name()`'s values are
already accent-stripped; `print_ai_agent_shorthand_list()` pads with `%-16s`,
counting bytes not glyphs): `Le Frimeur`→`Le Frimeur` (no accent), `El
Fanfarrón`→`El Fanfarron`, `Compteur de Fèves`→`Compteur de Feves`, `ε-γ-δ`→
`Eps-Gam-Del` (alternatives, decreasing terseness: `E-G-D` · `Eps-Gam-Del` ·
`Epsilon-Gamma-Delta`), `Grand Maître`→`Grand Maitre`, `Le Grand Maître`→`Le
Grand Maitre`, `Olla a Presión`→`Olla a Presion`, `El Cartógrafo`→`El
Cartografo`.

### Retired names

**The Hoarder** / L'Accapareur / El Acaparador — previously assigned to
Borealis's spec before it was renamed from "Greedy Power"; freed and unused.
`greedy` and `showboat` were considered as shorthand *aliases* for
`borealis`/`combo` but dropped (2026-08-21) in favor of exactly one canonical
shorthand per agent.

### What changed and why (Borealis's own origin)

The original plan had a threshold-gated, probabilistic-defense agent as the
*benchmark* and a cost-penalized subset-scoring agent below it — swapped
before either was built. A benchmark needs a single monotone strength dial so
it can be calibrated to a target band: Borealis has one (`λ`, unimodal in win
rate), where the older design had seven interacting parameters with unclear
individual effects (one, `aggression_level`, was never even referenced by its
own algorithm) and was weakened primarily by a ~45% chance of declining a
correct defense — *exploitable* rather than *diffuse* suboptimality, the kind
a child notices and plays around within a dozen games. The displaced design
survives as `A2` The Showboat: chases high combo bonuses, hoards them, blocks
unreliably — legible enough that losing to it still teaches something.

### Naming rationale (progression narrative)

Flavour text for a difficulty-select screen, reading as a climb from chaos to
near-superhuman play: (1) **The Gambler** — pure chance, no plan; (2) **The
Apprentice** — knows one thing (efficiency) and nothing else; (3) **The
Showboat** — chases the spectacular play, forgets to block; (4) **Borealis** —
the yardstick everything else is measured against; (5) **Bean Counter** —
obsessive resource accounting; (6) **ε-γ-δ** — reduces the game to a weighted
advantage function; (7) **Pressure Cooker** — reads the position and turns up
the heat; (8) **The Grandmaster** — synthesis of the three approaches above;
(9) **The Soothsayer** — rolls the dice a thousand times before choosing;
(10) **Grandmaster II** — the Grandmaster, now anticipating your reply;
(11) **The Omniscient** — deep tree search over hidden information;
(12) **AlphaOracle Prime** — search plus learned intuition.

`ε-γ-δ` deliberately breaks the flavour-name pattern — the one agent whose
entire identity is its weights, so naming it after them is a conscious
in-joke, not an oversight. `A13` The Cartographer was designed to sit
*outside* this progression rather than extend it as a 13th rung — the
strongest *deterministic* play the roster could produce via closed-form
deck/race arithmetic on top of `A7`'s synthesis, not a further step toward
"near-superhuman." The strength goal didn't survive calibration (see its
section below) — shelved 2026-08-31, then registered anyway 2026-09-04 for
its distinct playing character rather than for strength.

---

## A1 — Value Based · "The Apprentice" / "L'Apprenti" / "El Aprendiz"

|                      |                                                                                                                                                                                                                                                                   |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Enum                 | `AI_STRATEGY_VALUE_BASED`                                                                                                                                                                                                                                         |
| Shorthand            | `value`                                                                                                                                                                                                                                                           |
| Borealis rating      | **24** (measured, `--stda.rating` round-robin, `doc/changelog.md`'s 2026-08-23 entry) — original design-intent estimate was 15; calibrated with `VB_COST_FLOOR=1.3`, `VB_DEFEND_THRESHOLD=0.8` against a ~92.4% win rate vs Random (2026-08-21) before Borealis existed to benchmark against directly |
| Source file          | `src/ai_strat/ai_strat_valuebased.c` (implemented 2026-08-21)                                                                                                                                                                                                     |

**The one thing this agent does**: ranks individual champion cards by an
efficiency ratio, `contribution / (cost + k)`, and greedily takes the top ones
(cap 2) under the cash budget. No subset enumeration, no lookahead — a pure
per-card ranking.

**Deliberately out of scope**: touching the combo bonus in any way — the
omission *is* the agent, it "knows one thing (efficiency) and nothing else"
(roster naming rationale); subset/combination enumeration of any kind (that's
`A3` Borealis); a pass option in the attack phase — ratio scoring has no
natural zero to compare against, so this agent always commits at least its top
card; resource targets, phase modelling, aggression modelling, any simulation.

**Decision formula**: for a single champion card `c`, in whichever phase is
active,

```
efficiency(c) = contribution(c) / (cost(c) + VB_COST_FLOOR)
```

where `contribution(c)` is expected attack (attack phase) or expected defense
(defense phase). Champions are ranked by `efficiency` descending and taken
greedily under the cash budget, cap `VB_MAX_ATTACK_CARDS = 2` in the attack
phase. `VB_COST_FLOOR` (shipped 1.3) does two jobs: prevents division by zero
on free cards, and shrinks the ratio advantage of very cheap cards toward the
population mean, so a low-cost/tiny-contribution card can't dominate the
ranking purely on denominator effects.

Defense adds a threshold gate on top of the same ranking — play the top-ranked
defender iff:

```
E[incoming attack]  >=  VB_DEFEND_THRESHOLD × expected_defense(candidate)
```

(shipped `VB_DEFEND_THRESHOLD = 0.8`). Both sides are in damage units, so the
rule is scale-free: "don't spend a champion unless the incoming threat
justifies at least an 80% fraction of what it can absorb."

**Design rationale**: the risk with a low-tier agent like this is collapsing
into "Borealis minus combos" with different constants — that would only measure
an ablation, not add a distinct roster character. Value Based differs from
Borealis on three structural axes at once, not one:

| Axis      | Borealis                                            | Value Based                                   |
| --------- | --------------------------------------------------- | --------------------------------------------- |
| Scoring   | additive, λ-penalised: `Σcontrib + combo − λ·Σcost` | ratio: `contrib / (cost + k)` per card        |
| Selection | exhaustive subsets 0–3, argmax                      | rank + greedy take under budget, cap 2        |
| Defense   | play argmax subset, benefit capped at `E[incoming]` | play ≤1 champion, gated by a threat threshold |
| Combos    | aware                                               | blind                                         |

The ratio scoring is deliberately **scale-invariant** — it has no λ penalty
term, so it cannot express "lunas are worth more than damage right now." That's
a genuine weakness and a genuine difference in character, not an oversight.

Note the asymmetry with Borealis: that agent caps *benefit* at incoming damage
to avoid over-committing, while this one gates *entry* on incoming damage and
then commits fully. Both address over-defending, but fail differently — an
intended contrast, not a bug to reconcile.

The attack phase's missing pass option was a deliberate ship decision: it
makes the head-to-head loss against Borealis interpretable as "combos +
restraint" rather than combos alone, at the cost of muddying attribution
slightly. Revisit only if the vs-Borealis gap ever turns out much larger than
~40/60 — add a minimum-efficiency floor as the pass mechanism rather than
porting Borealis's empty-set scoring, which would pull the two agents' character
back together.

---

## A2 — Combo Threshold · "The Showboat" / "Le Frimeur" / "El Fanfarrón"

|                 |                                                                                                                                                                  |
| --------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_COMBO_THRESHOLD` (retired name: `AI_STRATEGY_COMBO_AWARE`)                                                                                          |
| Shorthand       | `combo` (sole canonical shorthand — `showboat` was never shipped as one)                                                                                        |
| Borealis rating | **30** (measured, `--stda.rating`) — original design-intent estimate was 37                                                                                     |
| Source file     | `src/ai_strat/ai_strat_combo_threshold.c` (implemented and calibrated 2026-08-22)                                                                                |

**The one thing this agent does**: chases champion combinations whose combo
bonus clears a tunable threshold, hoards big combos for a finishing blow, and
defends *probabilistically* — sometimes declining a defense it should take.
This was the original candidate for the rating-scale benchmark; it lost that
role to `A3` Borealis precisely because these traits are exploitable rather
than diffuse, which is exactly what makes it a legible, teachable rung below
the benchmark instead.

**Deliberately out of scope**: exhaustive subset enumeration or a single
monotone strength dial (that's `A3` Borealis); reliable defense — the
probabilistic decline is the point, not a bug to fix; resource targets tied
to opponent energy (`A4`), phase/aggression modelling (`A6`), advantage
functions over energy/cards/cash (`A5`), any lookahead or simulation.

**Decision formula**: candidates are single champions, pairs, and triples
subject to `total_cost <= cash`, but pairs/triples are only considered if
their combo bonus clears a threshold scaled by `aggression_level`:

```
effective_combo_bonus_threshold  = combo_bonus_threshold  / aggression_level
effective_combo3_bonus_threshold = combo3_bonus_threshold / aggression_level
score(candidate) = sum(expected_attack) + w * combo_bonus     (w = 1.0)
```

Higher `aggression_level` lowers both effective thresholds — chases combos
more eagerly. `save_big_combos_for_lethal` skips a triple whose combo bonus
is >=16 while the opponent's energy is above 25 (hold the finisher).

Defense is where the character lives:

```
expected_attack = sum(expected_attack over attacker's combat zone) + combo_bonus
if expected_attack < defend_damage_threshold: decline           (shipped: 8)
else: roll defend_probability_base (shipped 0.55), adjusted
      +0.15 if own energy < 20, +0.08 if < 40
      -0.20 if opp energy < 15, -0.10 if < 30
if defending: play the subset maximizing (sum(expected_defense)+combo_bonus)/(cost+1)
```

The ~45% baseline chance of declining a defense it should take is deliberate,
not a defect — replacing the roll with a deterministic rule would collapse
this agent's character into a weaker Borealis rather than a distinct rung.

**Design rationale**: this design was the original candidate for the
Bradley-Terry benchmark, rejected in favor of Borealis because a benchmark
needs a single monotone strength dial, and this design has seven interacting
parameters with unclear individual effects — `aggression_level` was originally
declared but never even wired into the algorithm. It was also weakened
primarily by that ~45% chance of declining a correct defense, which is
*exploitable* suboptimality (a human notices "it sometimes just doesn't
block" and has found a strategy, not a challenge, within a dozen games) —
Borealis's `λ` dial produces *diffuse* suboptimality instead, which is what a
calibratable benchmark needs. None of that disqualifies this design as a
roster rung: losing to a combo-chasing, unreliable defender still teaches
something.

Calibrated via differential evolution vs `A1` Value Based (vs Random
saturates near a ceiling the same way it did for `A1`), both seats, 80,000
games: uncalibrated defaults measured 49.94% vs `value`, calibrated shipped
**58.77%** [58.29%, 59.26%]; vs `rand`, 88.11% uncalibrated -> **92.78%**
[92.52%, 93.03%]. The optimizer's raw winner reached 77.3% vs `value` but was
rejected and hand-patched: it had pushed `aggression_level` to 2.21, which
collapsed the effective 2-card threshold low enough to admit every combo
bonus (including a plain color pair) — erasing this agent's "chases only the
spectacular" selectivity, the same failure mode `A4`'s free search hit later.
Shipped `aggression_level=1.3` keeps that selectivity identical to the
untuned defaults while keeping the optimizer's gains on the other eight
parameters.

---

## A3 — Greedy Power · "Borealis" (identical in EN / FR / ES)

|                 |                                                                                            |
| --------------- | ------------------------------------------------------------------------------------------ |
| Enum            | `AI_STRATEGY_BOREALIS` (retired name: `AI_STRATEGY_GREEDY_POWER`)                          |
| Shorthand       | `borealis` (sole canonical shorthand — no alias)                                          |
| Borealis rating | **50 — the scale anchor, by definition** (`s = 1.0` in the Bradley-Terry model)            |
| Source file     | `src/ai_strat/ai_strat_borealis.c` + `ai_strat_borealis_enum.c` (implemented and calibrated 2026-08-23) |

**The one thing this agent does**: exhaustively enumerates every legal
champion subset of size 0-3, scores each, and plays the best — breaking
near-ties at random (an epsilon window) and holding back very large combos
for a finishing blow. `λ` (`luna_value`) is the single, monotone strength
dial that makes this agent — and only this agent — fit to anchor the
Bradley-Terry rating scale at 50. See `doc/bt_rating_system/rating_system.md`
for the anchor mechanism itself.

**Deliberately out of scope**: pruning candidates on combo bonus before
scoring — that threshold-gated shortcut is precisely what `A2` Combo
Threshold does, and precisely why it isn't the benchmark; resource targets
tied to opponent energy (`A4`), phase/aggression modelling (`A6`), advantage
functions (`A5`), any lookahead or simulation; weakening via randomly
skipping correct plays — weaken only by moving `λ` off-optimum, which stays
diffuse and legible rather than exploitable.

**Decision formula**: for a candidate champion subset `S` (size 0-3, at most
`1+7+21+35=64` given the 7-card hand cap — exhaustive, no pruning):

```
score(S) = sum(contribution(c) for c in S) + combo_bonus(S) - lambda * sum(cost(c) for c in S)
```

`contribution(c)` is expected attack (attack phase) or expected defense
(defense phase). The empty set scores exactly 0, making "play nothing" a
first-class candidate with no special-case pass logic. **Defense caps value
at the incoming threat** — the single most important line in the defense
logic, without which the agent over-defends and bleeds lunas:

```
value(S) = min(sum(expected_defense) + combo_bonus(S), E[incoming attack]) - lambda * sum(cost(S))
```

**Epsilon tie-break**: collect every candidate with `score >= best_score -
tiebreak_epsilon`, choose uniformly at random via `ctx->rng` — placed at the
point of near-indifference, so the strength cost is close to zero while still
denying a repeat human opponent a fixed, memorizable script. **Lethal combo
holding**: exclude `S` if `combo_bonus(S) >= lethal_combo_bonus` AND `S` isn't
lethal right now AND `opp_energy > lethal_hold_ceiling` — hoard the big combo
while the opponent is comfortably healthy, release it the moment it either
finishes them or they're close enough that the window is closing.

**Why `λ` and only `λ`**: win rate against any fixed opponent is unimodal in
it — `λ=0` (lunas free) overspends and runs dry; `λ` optimal is balanced;
`λ` large hoards and never closes out a game. Moving `λ` off-optimum in
either direction produces a coherent, human-legible playing style rather than
visible stupidity, which is the property that makes it usable as a tunable
benchmark at all.

Calibrated via differential evolution vs `A2` Combo Threshold (vs `rand` is
ceiling-effected, vs `A1` was already near parity, so `A2` had the most
headroom), both seats: at the handout's untuned default (`luna_value=0.5`),
Borealis actually *lost* to `A2` (43.63%). Shipped **`luna_value=4.5846`**
(the true optimum, ~4.0-4.5, sits far above that original guess) measured
**69.13%** [68.67%, 69.58%] vs `combo` (40,000 games), **74.19%** [73.51%,
74.87%] vs `value` (16,000 games), **99.33%** [99.19%, 99.45%] vs `rand`
(16,000 games) — the unimodality property was re-confirmed by a quadratic fit
of a follow-up sweep (concave-down, other five parameters fixed).

---

## A4 — Balanced Rules · "Bean Counter" / "Compteur de Fèves" / "Contador de Frijoles"

|                 |                                                                                        |
| --------------- | ---------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_BALANCED`                                                                   |
| Shorthand       | `balanced`                                                                                |
| Borealis rating | **36** (measured, `--stda.rating`, 2026-08-24) — below the anchor, a legitimate result, not a defect; design-intent estimate was 62 |
| Source file     | `src/ai_strat/ai_strat_balanced_rules.c`/`.h` (implemented and calibrated 2026-08-24)     |

**The one thing this agent does**: obsessive resource accounting — derives a
target cash reserve and a target effective hand size directly from the
opponent's current energy (linear formulas), spends/holds to hit those
targets, and defends by a variance-aware rule rather than a flat threshold.

**Deliberately out of scope**: game-phase classification or an aggression
factor (`A6` Tactical) — this agent has one set of formulas, not a state
machine; combo bonus scoring as a primary signal (the shipped `combo_weight`
stays at 0.0 — calibration confirmed pushing it above 0.5 measures stronger
but erodes this exact boundary, echoing `A2`'s rejected extreme); a weighted
multi-factor advantage function (`A5` Heuristic) — this agent is principled
and formulaic, not a tunable weighted sum.

**Decision formula**: both resource targets share one shape,

```
target = slope * (opp_energy - 8) + intercept        (clamped >= 0)
target /= late_game_aggro   once opp_energy <= lethal_horizon
```

Defense: `E[Total Defense] <= E[Total Attack] - beta * sigma` (variance-aware,
`beta = defense_beta`) rather than a flat cap.

**Two corrections found while implementing, both real bugs, not just
untuned guesses**: (1) the original design's own numeric tables actually fit
`slope*(E-8)` with intercept **0** exactly, not the `+8`/`+3` an earlier
comparison doc stated (a misreading of the stub's inverse form) — shipped
intercepts (`target_cash_intercept=-2.73`, `target_cards_intercept=-0.99`)
are calibrated results, not a return to that reading; (2) the design's
`19`-luna cash-reserve constant was a fossil of an obsolete starting-cash
rule, and naively re-anchoring it to the current `INITIAL_CASH_DEFAULT=30`
(`30/91≈0.33`) left ~0 spendable surplus at full opponent energy, trapping
the agent unable to attack for several early turns — confirmed by playtrace
and a parameter sweep. Shipped `target_cash_slope=0.081`.

**Design rationale**: a free `differential_evolution` search eroded this
agent's identity — resource-target slopes drifting toward 0, `defense_beta`
toward "never defend" — while measuring *stronger*, mirroring `A2`'s
rejected `aggression_level=2.21`. Shipped values instead come from
`optimize --identity-safe`, a narrower search keeping both slopes
non-degenerate and `defense_beta` in `[0.25, 2.0]`. Measured (validated): vs
`borealis` 34.3%, vs `combo` ~59.7%, vs `value` ~58.1%, vs `rand` ~98.5% —
this agent measuring *below* the anchor despite a `~62` design-intent
estimate is the largest design-intent miss on the roster, a legitimate
calibrated result: the linear resource-target formulas simply don't buy as
much strength as the original design expected.

---

## A5 — Heuristic · "ε-γ-δ" (deliberately breaks the flavour-name pattern)

|                 |                                                                                    |
| --------------- | ------------------------------------------------------------------------------------ |
| Enum            | `AI_STRATEGY_HEURISTIC`                                                              |
| Shorthand       | `heuristic`                                                                          |
| Borealis rating | **64** (measured; originally 60 at first calibration, +4 from the 2026-08-27 PASS-dominance defense-formula fix shared with `A7`) — design-intent estimate was 70 |
| Source file     | `src/ai_strat/ai_strat_heuristic.c`/`.h` (implemented and calibrated 2026-08-25)     |

**The one thing this agent does**: reduces the whole game to one weighted
advantage function and picks the legal move (one-move lookahead, no
opponent-response simulation) that maximizes it after being applied. Its
entire identity is its weights — the reason it's named after them rather
than given a flavour name, a conscious exception to the naming pattern, not
an oversight.

**Deliberately out of scope**: dynamic/adaptive weights — a fixed epsilon,
gamma, delta per game is the point, not a state machine (that's `A6`
Tactical and `A7`'s synthesis); resource-target formulas as a first-class
mechanism (`A4`); subset enumeration or combo-bonus scoring as primary logic
(enumeration here is only the move generator, not the decision rule).

**Decision formula**:

```
taper     = (opp_energy / INITIAL_ENERGY_DEFAULT) ^ weight_taper_exponent
EnergyAdv = own_energy - opp_energy                (+/- HEUR_LETHAL_BONUS=100000 at 0 energy)
CardsAdv  = own_hand_size - opp_hand_size * opp_card_discount
CashAdv   = own_cash - opp_cash
Advantage = epsilon*EnergyAdv + taper*gamma*CardsAdv + taper*delta*CashAdv
```

Shipped (`HEURISTIC_DEFAULTS`): `epsilon=0.349`, `gamma=1.962` (pinned at its
own identity-safe search ceiling — an *unconstrained* search found
`gamma=9.815` measuring statistically indistinguishable and playtracing
confirmed it's still a fast, decisive strategy rather than a degenerate
"hoard forever" stall, but the identity-safe candidate shipped anyway per
protocol), `delta=1.0` (pinned during calibration: the argmax of a weighted
sum of three terms is invariant to a positive rescaling of all three
weights, so one is mathematically redundant — kept as a field for
readability, since the agent's stated identity is "its three weights", not
two), `weight_taper_exponent=0.101`, `opp_card_discount=0.987`.

No opponent block is modeled when scoring an attack — `predicted_damage()`
just clamps `sum(expected_attack)+combo_bonus` at `opp_energy`. A
constant-fraction block model would be a positive rescaling of the attack
term, already absorbed into `epsilon`; adding one would be a redundant,
unidentifiable parameter, not a real degree of freedom.

**Design rationale**: two apparent tensions between the original design
notes and this implementation were deliberately resolved, not left
ambiguous. The "no dynamic/adaptive weights" exclusion targets `A6`'s
game-phase *state machine*, not a smooth function of one public scalar — so
the taper ships as a single continuous dial (`weight_taper_exponent=0`
recovers strictly fixed weights). And "subset enumeration as primary logic"
being out of scope targets `A3`'s *decision rule* (maximize raw subset
value) — here enumeration is only the move generator the weighted sum scores
over.

Calibrated via `optimize --identity-safe` vs `borealis`, both seats: measured
**58.99%** [58.51%, 59.47%] (40,000 games) vs `borealis`, **99.85%**
[99.78%, 99.90%] vs `rand` (18,000 games), **77.09%** [76.47%, 77.70%] vs
`combo` (18,000 games), **81.81%** [81.24%, 82.37%] vs `value` (18,000
games), **74.24%** [73.59%, 74.87%] vs `balanced` (18,000 games) — the first
agent to measure above the Borealis anchor. That figure predates the
2026-08-27 PASS-dominance defense-formula fix (shared with `A7`), which moved
the shipped rating from 60 to 64.

---

## A6 — Tactical · "Pressure Cooker" / "Cocotte-Minute" / "Olla a Presión"

|                 |                                                                                |
| --------------- | ---------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_TACTICAL`                                                            |
| Shorthand       | `tactical`                                                                        |
| Borealis rating | **52** (measured) — design-intent estimate was 74                                |
| Source file     | `src/ai_strat/ai_strat_tactical.c`/`.h` (implemented and calibrated 2026-08-25)   |

**The one thing this agent does**: classifies the game into a phase (early /
mid / late / critical, by energy thresholds) and derives a single 0.0-1.0
aggression factor from energy difference, hand power, and cash surplus, then
scales how many champions to commit by that factor — reads the position and
turns up the heat as it sharpens.

**Deliberately out of scope**: a fixed, unchanging advantage function — that's
`A5` Heuristic, since this agent's whole point is that its weighting *moves*
with the position; resource-target formulas as primary logic (`A4` — can
consume those targets but doesn't replace them); any lookahead, simulation,
or tree search.

**Decision formula**:

```
GamePhase(energy) = EARLY    if energy >= phase_mid_threshold        (spec 75)
                   = MID      if phase_late_threshold <= energy < phase_mid_threshold  (spec 40)
                   = LATE     if phase_critical_threshold <= energy < phase_late_threshold (spec 15)
                   = CRITICAL if energy <  phase_critical_threshold

Aggression = 0.5                                             (neutral baseline)
  + (own_energy - opp_energy) * aggression_energy_diff_weight       (0.0008)
  + aggression_opp_critical_bonus if opp CRITICAL, else aggression_opp_late_bonus if opp LATE   (0.282 / 0.126)
  - aggression_self_critical_penalty if self CRITICAL, else aggression_self_late_penalty if self LATE (0.148 / 0.053)
  + aggression_hand_power_bonus if my_hand_power > opp_estimated_power * 1.5        (0.248)
  - aggression_hand_power_penalty if my_hand_power < opp_estimated_power * 0.7      (0.154)
  + aggression_cash_surplus_bonus if own_cash > aggression_cash_surplus_threshold=10 (0.230)
  clamped to [0.0, 1.0]
```

Champion count scales in fixed bands, not a proportional rounding:
`desired = 3 if aggression>=0.75, 2 if >=0.5, 1 if >=0.25, else 0`, then
`num_attackers = min(desired, min(3, affordable_champion_count))`.

**A real bug found via playtrace, fixed before shipping**: the original
sketch called `decide_num_attackers()` but never implemented it. A first
fill-in used proportional rounding (`round(aggression * max_playable)`),
which put `max_playable=1`'s *sole* decision boundary exactly on
aggression's own neutral baseline (0.5) — so routine negative signals (e.g.
the hand-power penalty) pushed aggression just below 0.5 often enough that
the agent passed on its only affordable champion far more than any other
implemented agent ever declines to attack, measuring a **loss to Random**
before the fix — the only implemented agent to do so. Fixed banding lands
the neutral baseline inside the `>=0.25` tier instead of exactly on a
boundary.

Defense is a standalone EV comparison, deliberately **independent** of
`aggression_factor`:

```
value = -(expected_damage * defense_damage_weight + defense_cost * defense_cash_weight)
attack_estimate = expected_attack + defense_conservative_stdev_mult * stdev(attack)
```

Note the sign: this *inflates* the attack estimate pessimistically before
comparing, the opposite of `A4`'s `E[Attack] - beta*sigma` cap — a
deliberate fidelity choice to the original design, not an oversight.

**Design rationale**: two gaps the design sketch left open were resolved
during implementation, not left ambiguous. The sketch's `GamePhase`
thresholds (75/40/15) and its aggression "smell blood" cutoffs
(independently 20/40) were two separate step functions over the same energy
axis — unified onto one shared 3-threshold set here, since "classify into a
phase, then read the position" is one coherent mechanism, not two competing
ones. Card selection within the chosen count is greedy combo-aware ranking
(the same shape as `A4`'s selection, not `A3`'s exhaustive enumeration) —
keeps this agent's complexity budget on the phase/aggression mechanism,
which is its actual identity. Combo awareness is unconditionally on here
(no `combo_weight` switch, unlike `A4`), matching the design intent that this
agent, unlike combo-blind `A4`, always looks for combos.

---

## A7 — Hybrid HBT · "The Grandmaster" / "Le Grand Maître" / "El Gran Maestro"

|                 |                                                                                       |
| --------------- | ----------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_HYBRID_HBT`                                                                  |
| Shorthand       | `hbt`                                                                                     |
| Borealis rating | **65** (measured; saga: 62 -> 58 (2026-08-27 PASS-dominance fix, a regression this agent alone suffered) -> 65 (2026-08-28 recalibration, see below)) — design-intent estimate was 78 |
| Source file     | `src/ai_strat/ai_strat_hbt.c` + `ai_strat_hbt_enum.{c,h}` + `ai_strat_hbt_cards.c` (implemented and calibrated 2026-08-25) |

**The one thing this agent does**: a *fixed* three-layer synthesis of three
already-implemented agents, in this order and no other — `A4` Balanced Rules
**weights** the advantage function via a soft resource-shortfall penalty, `A6`
Tactical **weights** the same advantage function dynamically by game
phase/aggression, and `A5` Heuristic **ranks** every enumerated move by the
resulting weighted advantage. Named "The Grandmaster" as a synthesis of the
three approaches below it on the ladder. `A7` and `A9` (HBT 2-Ply) are *by
roster design* syntheses — the personality **is** the fixed three-layer
combination itself, and nothing licenses bolting on a fourth mechanism.

**One deliberate exception, added on purpose**: this agent should feel
"combo-aware to a good extent" next to a human opponent, beyond what
`A4`/`A5`/`A6` provide on their own. `A5`'s enumeration already scores the
*realized* combo bonus for every candidate subset (genuine combo-aware
selection, inherited for free), but a one-move lookahead has no concept of a
combo worth *holding back* for a finishing blow — so `A3` Borealis's
`is_held_combo()` rule is ported verbatim (attack only), along with a local
port of its combo-protecting mulligan/discard-to-7 shape.

**Deliberately out of scope**: any mechanism not already present in `A3`
(combo hold/mulligan-discard only, the one exception above), `A4`, `A5`, or
`A6` — if an idea doesn't trace to one of those, it belongs in a different
agent; lookahead beyond the one-move evaluation `A5` already does (2-ply is
`A9`'s job); any sampling/simulation (`A8` and above).

**Decision formula — organized by layer, all 34 `HBTParams` fields shown**:
move generation and the advantage formula itself are `A5`'s verbatim
mechanism (closed-form one-move lookahead, argmax over pass / every
affordable 1-3 champion subset / every affordable draw card / every
affordable cash card). Fully deterministic — no epsilon tie-break.

*Layer T (from `A6`) modulates `A5`'s weights*, rather than gating an
attacker count the way `A6` itself does (`A5`'s enumerator already considers
every subset size, so there's no count decision left to gate):

```
a = calculate_aggression_factor(...)                    (A6's own formula, computed once/turn)
eps_eff   = epsilon * (1 + aggr_energy_gain   * 2*(a - 0.5))
gamma_eff = gamma   * (1 - aggr_resource_fade * 2*(a - 0.5))
delta_eff = delta   * (1 - aggr_resource_fade * 2*(a - 0.5))
if opp_phase == CRITICAL: eps_eff *= critical_epsilon_mult
```

*Layer B (from `A4`) enters as a soft penalty, not a hard filter* — a
deliberate deviation from the original design sketch, which called for `A4`
to *filter* (drop) moves outside a resource-derived budget before ranking.
`A4` is the weakest of the three source agents (rating 36) specifically
because a hard filter can starve the agent of any legal move at all (`A4`'s
own defaults comment documents a traced game with 4 of 5 early turns passing
outright) — reusing that failure mode here would have undermined this
agent's whole reason for existing:

```
penalty = penalty_cash_weight * max(0, target_cash - post_cash)
        + penalty_cards_weight * max(0, target_cards - post_hand)
advantage = heuristic_advantage(eps_eff, gamma_eff, delta_eff, ...) - penalty
```

The penalty applies to the *raw* post-move hand size/cash, not `A4`'s own
effective-hand-size estimate — `A5`'s enumerator already models a draw
card's real effect as a distinct candidate move, so stacking an
effective-hand estimate on top would double-count it (the identical argument
`A5` itself makes for using raw hand size). `target_cash`/`target_cards`
reuse `A4`'s own linear formulas, aggression-scaled with a **sign corrected**
from the original design notes: higher aggression scales targets *down*
(`(1 - scale * (aggression - 0.5))`), matching `A4`'s own late-game behavior
(spend more as a kill becomes reachable) rather than the notes' contradictory
"higher aggression, bigger cash reserve" sketch.

*Defense* unifies `A4`'s and `A6`'s threat estimates into one signed dial,
rather than picking one convention a priori:

```
incoming = expected_incoming_attack(...) + defense_stdev_mult * sqrt(sum(champion_variance(...)))
```

`defense_stdev_mult < 0` reproduces `A4`'s `E[Attack] - beta*sigma`
deflation (defend against less than the mean); `> 0` reproduces `A6`'s
inflation (defend against more, conservatively); `0` recovers `A5`'s plain
expected value. One signed dial spans both conventions; calibration decides
which this agent's identity needs — it landed at **+0.2156** (mild
inflation, `A6`-flavored), down from an original +0.711 fit to a dead code
path (see the PASS-dominance saga below).

**The 34 fields, grouped by which source agent they're inherited from**:
Layer H (`A5`'s weights) — `weight_energy_advantage`, `weight_cards_advantage`,
`weight_cash_advantage` (pinned 1.0, same scale-invariance redundancy as `A5`),
`weight_taper_exponent`, `opp_card_discount`; Layer T (`A6`'s phase/aggression) —
`phase_mid_threshold`, `phase_late_threshold`, `phase_critical_threshold`,
`aggression_energy_diff_weight`, `aggression_opp_late_bonus`,
`aggression_opp_critical_bonus`, `aggression_self_late_penalty`,
`aggression_self_critical_penalty`, `aggression_hand_power_bonus`,
`aggression_hand_power_penalty`, `aggression_cash_surplus_threshold`,
`aggression_cash_surplus_bonus`; the T->H coupling (new to this agent) —
`aggr_energy_gain`, `aggr_resource_fade`, `critical_epsilon_mult`; Layer B
(`A4`'s resource targets) — `target_cash_slope`, `target_cash_intercept`,
`target_cards_slope`, `target_cards_intercept`, `late_game_aggro`,
`lethal_horizon`, `target_aggr_cash_scale`, `target_aggr_cards_scale`; the
B->H coupling — `penalty_cash_weight`, `penalty_cards_weight`; the combo hold
(from `A3`) — `hold_lethal_combos`, `lethal_combo_bonus`,
`lethal_hold_ceiling`; and defense — `defense_stdev_mult`.

Mulligan/discard-to-7 are a *local port* of `A3`'s shape (find the best held
combo, protect it, discard/mulligan the lowest-value unprotected card, a
two-pass fallback so protection can never stall) rather than a call into
`A3`'s own functions — calling `A3`'s real function would make this agent's
discard behavior depend on `A3`'s own calibration, and `A3` is the
Bradley-Terry anchor for every agent measured so far, so refactoring it for
reuse is pure downside risk to every rating already shipped. Victim
valuation here uses the `A4`/`A6` efficiency ratio
`expected_attack/(cost+HBT_COST_FLOOR)` rather than `A3`'s
`expected_attack - lambda*cost`, so this agent introduces no new lambda dial.

**The PASS-dominance saga (2026-08-27/28)** — this agent's most significant
post-ship history. An `A9`-build diagnostic found that `A5`/`A7`'s shared
defense formula had a latent defect: the decline/PASS baseline scored the
*raw, undamaged* `own_energy` instead of `max(own_energy - incoming, 0)`,
which meant PASS mathematically dominated every blocking option under the
shipped weights. Fixing it (matching what `A9`'s own local reply oracle
already did correctly) measured both seats, n=4000 vs `borealis`: `A5`
improved 60->64 (shipped outright), but `A7` *regressed* 62->58 — worse, not
better, despite sharing what was believed to be an identical formula.
Shipped anyway (a deliberate call to keep focus elsewhere), with the
follow-up question left open: why did an identical fix help one agent and
hurt the other?

**Root cause** (2026-08-28): `defense_stdev_mult` was dead weight before the
fix — PASS strictly dominated every block regardless of the incoming-attack
estimate, so the shipped +0.711 (an `A6`-style inflation) had been fit to
noise. Post-fix it became live for the first time, and a univariate sweep
vs `borealis` confirmed it: win rate fell monotonically from 60.96% at -2.0
to 57.53% at +2.0 — the shipped +0.711 was now actively biasing toward
over-blocking. Re-optimizing the 12 originally-free parameters three ways,
the winner was `--identity-safe` restricted to the 10 of those 12 fields its
bounds table actually covers: **64.62%** [64.15%, 65.09%] (40,000 games), no
personality flags, and `weight_cards_advantage` landed at 1.95 — essentially
identical to `A5`'s own independently-shipped 1.96, no drift at all. Only 10
of the 34 fields changed from the original fit; `defense_stdev_mult` moved
0.711 -> 0.2156, the value shown above. **Shipped, closing the follow-up**:
rating 65, the highest measured value on the roster at the time, above all
three of its own source agents (`A4` 36, `A5` 64, `A6` 52) in the same fit.

---

## A8 — Simple Monte Carlo · "The Soothsayer" / "Le Devin" / "El Adivino"

|                 |                                                                                                                                        |
| --------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_SIMPLE_MC`                                                                                                                |
| Shorthand       | `simplemc`                                                                                                                              |
| Borealis rating | **35** (measured) — below the anchor, in the same tier as `A2`/`A4`, and *not raised by more compute* (see below); design-intent estimate was 82 |
| Source files    | `src/ai_strat/ai_strat_simplemc1.{c,h}` + `ai_strat_simplemc_search.{c,h}`; shared infra `src/actions/move_gen.{c,h}`/`move_apply.{c,h}`, `src/ai_strat/ai_strat_playout.{c,h}` (implemented and calibrated 2026-08-25) |

**The one thing this agent does**: flat rollouts with progressive pruning —
**no tree**. Enumerate every legal move (0-3 champion subsets, draw, recall,
cash; capped at 128 candidates), simulate each via a fresh determinized
clone rolled out with uniformly-random play on *both* seats to a terminal
win/loss/draw, and prune as evidence accumulates. This is the first agent
that actually *simulates* rather than scoring closed-form — the shared
infrastructure it needed built first (`move_gen`/`move_apply`,
`ai_strat_playout`'s forked-RNG-stream cloning/determinization/rollout) is
reused by every agent from here up the ladder.

**Decision formula — the pruning schedule** (two ideas from the original
design reconciled, not one chosen over the other): a small 7-rollout seed
round drops every 0-win candidate outright; repeated rounds then add 25 more
rollouts per survivor and drop anything whose confidence-interval upper
bound falls below the current leader's lower bound (a normal approximation
to the binomial) — this CI comparison is the mechanism that actually does
the pruning. A originally-sketched fixed schedule (survivor cap
`Nm -> Nm^(3/4) -> Nm^(1/2) -> Nm^(1/4)`, capped 30/10/4, at cumulative
100/300/700/1500 simulations) is layered on top as hard survivor ceilings at
those checkpoints, so the shipped agent still matches that identity even
where CI pruning alone would keep more candidates alive longer. Stops at one
survivor, 1500 cumulative sims/candidate, or 25,000 total rollouts.

**Deliberately out of scope**: building a tree of any kind — a node per
candidate move with no children is what makes this "simple" (tree search is
`A10`'s job); exact/closed-form expected-value computation of dice rolls as
a *replacement* for simulation — this agent's whole point is sampling.
**Superseded from an earlier "out of scope" claim**: reasoning about hidden
information via determinization used to be listed as `A10`'s job alone, but
`mc_determinize()` shipped as part of *this* agent instead — `A10`'s own
distinguishing contribution over this agent narrows to the tree itself plus
*reshuffle-aware* determinization specifically (see `A10`'s section below).

**Measured result and diagnosis — the defining finding of this agent**:
rating **35**, and *not raised by more compute*: a budget-vs-rating sweep at
1.0x/1.75x/2.3x the rollout count measured statistically indistinguishable
ratings (35.4%, 38.2%, 35.9%, all overlapping 95% CIs). A flat curve like
that is the signature of a **biased estimator**, not an under-searched one —
confirmed by playtracing three losses vs `borealis`: the agent repeatedly
favors resource-building moves (cash exchange, draw, recall) over committing
champions against a strategic opponent that keeps attacking, because
`mc_playout()` models *both* seats as `AI_STRATEGY_RANDOM` for every rollout
regardless of who the real opponent is (exactly as the original design
called for: "randomly ... make moves 2+"). Every candidate's win-probability
estimate therefore answers "what happens if the opponent now plays randomly
forever" — accurate against `rand` specifically, systematically biased
against a real strategic opponent that converts tempo into damage more
reliably than a random continuation predicts.

**Shipped as-is at rating 35, not chased higher** — this agent was never
intended to be a strong opponent; its original purpose is probing per-action
values from curated positions to help mine new heuristics for future agents.
A cheap, non-tree heuristic rollout policy is the lever that could plausibly
raise the rating without more compute budget — `A12` Clairvoyant is exactly
that experiment (see below), and it did not improve on this agent.

---

## A9 — HBT 2-Ply · "Grandmaster II" / "Grand Maître II" / "Gran Maestro II"

|                 |                                                                                                          |
| --------------- | ---------------------------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_HBT_2PLY`                                                                                     |
| Shorthand       | `hbt2ply`                                                                                                  |
| Borealis rating | **62** (measured; moved passively from `A7`'s own 2026-08-28 recalibration — this agent's own dials found no further gain, see below) — design-intent estimate was 85 |
| Source file     | `src/ai_strat/ai_strat_hbt2ply.c` + `ai_strat_hbt2ply_reply.{c,h}` (implemented and calibrated 2026-08-26)  |

**The one thing this agent does**: `A7` plus one opponent-response ply. For
each candidate champion-subset move: clone the position, commit the subset,
estimate the opponent's best reply against a *deterministic public-information
surrogate hand* (not sampling — a single constructed hand sized to the
opponent's real public hand size, drawn from the public unseen pool), then
blend that net-of-reply score against `A7`'s own undefended score:

```
score = (1 - reply_trust) * A7_undefended_score + reply_trust * net_of_reply_score
```

"The Grandmaster, now anticipating your reply." Deliberately **not**
sampling/rollouts (`A8`'s job) or a real search tree with backpropagation
(`A10`'s job) — this stays a fixed 2-ply minimax-on-expectations.

**Deliberately out of scope**: any evaluation mechanism not already in
`A4`/`A5`/`A6`/`A7` — the only new thing this agent adds over `A7` is the
second ply; fixing `A7`'s (or `A5`'s) own defense formula — deliberately
left untouched here even after being identified as this agent's own
blocker (see below).

**Measured: below the design target, root cause precisely isolated** — this
is the most instructive null-adjacent result on the roster. Validated
**47.2%** [46.7%, 47.7%] head-to-head vs `A7` (40,000 games), below the
`>55%` design target. Two independent calibration searches (`reply_trust`
against `borealis`, then directly against `hbt`) both converge to genuine,
non-degenerate values and both plateau in the same neighborhood — not a
tuning gap. A controlled test (both sides given a corrected, actually-
blocking defense so only the attack-side two-ply logic varies) found the
mechanism reaches **near-parity** with `A7` there (49.6% vs 50.5%) — the
model is sound in principle. The gap against the real `A7` opponent traces
instead to a pre-existing property of `A7`'s *own* shipped defense formula:
its PASS/decline baseline never charged the incoming attack, making
declining mathematically dominate every blocking option under `A7`'s
then-shipped weights (found while building *this* agent, not introduced by
it — see `A7`'s section above for the fix and its own follow-on saga). Since
`A7`'s real defense essentially never blocked at the time, this agent's
reply-anticipation ply had nothing real to correct for against the actual
opponent it was measured against.

After `A7`'s own defense fix and 2026-08-28 recalibration, this agent was
re-attempted against the new `A7`: **no further improvement was possible** —
a `reply_trust` sweep showed win rate declining *monotonically* as trust in
the ply increases, overturning the original diagnosis that `A7`'s broken
defense alone explained the gap. `HBT2PLY_DEFAULTS` shipped unchanged;
rating moved passively from 59 to 62 purely by inheriting `A7`'s own gain.
Shipped anyway as a playable, calibrated roster member — reporting a
below-target result honestly rather than withholding it, matching the
`A4`/`A8`/`A12` precedent.

---

## A10 — IS-MCTS · "The Omniscient" / "L'Omniscient" / "El Omnisciente"

|                 |                                                                                                                  |
| --------------- | ---------------------------------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_ISMCTS`                                                                                              |
| Shorthand       | `ismcts`                                                                                                          |
| Borealis rating | **69** (measured, n=10008 vs Borealis, 95% CI [67.6%, 69.5%]) — the roster ceiling until `A11`, also 63.2% head-to-head vs `A7` (n=10008); design-intent estimate was 92 |
| Source file     | `src/ai_strat/ai_strat_ismcts1.c`, `ai_strat_ismcts_tree.c`, `ai_strat_ismcts_search.c`, `ai_strat_ismcts_flat.c` (implemented and calibrated 2026-08-27) |

**The one thing this agent does**: Information Set Monte Carlo Tree Search —
repeatedly determinizes the opponent's hidden hand/deck consistent with
what's publicly known, builds/grows a UCT tree over that sample
(select -> expand -> simulate -> backpropagate), and aggregates across
determinizations. "Deep tree search over hidden information." The first
agent whose *tree selection* (not just its leaf evaluation) is a real search
mechanism rather than a closed-form formula.

**Deliberately out of scope**: a learned policy/value network guiding the
tree — that's `A11`'s addition on top of this agent; hand-written positional
heuristics as the tree's *own primary evaluator* (using `A5`'s heuristic as
the *rollout* policy is a different role than a closed-form scoring formula
like `A4`-`A7` use as their entire decision); sampling dice rolls via Monte
Carlo — dice have closed-form mean/variance, used directly instead of
rollout sampling for that component specifically; treating a player's whole
remaining deck as one uniform unknown pool once a reshuffle has happened
(see the correctness note below).

**Finding #1 (2026-08-27) — the rollout policy, not the tree, was the
bottleneck**: a budget-vs-rating scaling curve (1k/4k/16k/64k iterations,
uniformly-random rollout policy on both seats) rose from 41.9% at 1k to a
flat 46-48% plateau from 4k through 64k — the same "more search can't fix
it" signature `A8`'s own diagnosis found. A controlled A/B test isolating
just the rollout policy (everything else identical, 16k iterations)
confirmed the cause: swapping the rollout policy from `AI_STRATEGY_RANDOM`
to `AI_STRATEGY_HEURISTIC` (`A5`) took the same measurement from 47.6% to
**63.0%** — a ~15-point jump, dwarfing every other effect measured on this
agent. Shipped as `heuristic_rollout_strategy_set()` (attack/defense and the
matching mulligan/discard-to-7 flat-rollout scoring, subject to the
identical bias).

**Finding #2 (2026-08-27) — with the fixed rollout policy, more search past
a point actively hurts**: re-running the budget-vs-rating curve with the
heuristic rollout live surfaced a *different* shape: win rate vs `Borealis`
doesn't plateau, it **peaks then declines**. Across 1k-16k iterations the
curve is a noisy plateau (63-69%, no clean interior maximum distinguishable
from noise), but the decline beyond that is real and large: 58.5% at 64k,
55.2% at 100k — each several standard errors below the plateau. Working
hypothesis: since `A5`'s rollout policy is deterministic given a position,
more search lets the tree converge more confidently on lines that exploit
that specific simulated opponent's quirks rather than generalizing to a real
one. **Shipped `limit_iterations=4000`** (the most rigorously sampled point
in the plateau), a large, deliberate departure from an original
~100,000-iteration/~1s-per-decision calibration target that had been
calibrated against the since-replaced random rollout policy's own (much
cheaper) timing.

A real, separate bug was found and fixed along the way: `stda_auto.c`'s
`play_stda_auto_game()` called `apply_mulligan()` before initializing
`gstate.turn` — invisible to every earlier agent's mulligan hook (none of
them simulate forward from the mulligan point) but a genuine
uninitialized-stack-read once this agent's flat-rollout mulligan scoring
did (caught by valgrind, not by any test).

**Correctness note, deferred not dropped**: `card_actions.c`'s
`shuffle_discard_and_form_deck()` reshuffles a player's own discard back
into their own deck when it empties. After that reshuffle, that deck's
*composition* is exactly known (it's precisely the prior discard pile,
which was visible) — only the *draw order* is still hidden. A
determinization that samples card identities uniformly from "everything not
in hand, not yet played" throws away that composition information
post-reshuffle; the correct approach samples *permutations of the known
composition* for any reshuffled player-deck instead. Measured across 11,000
self-play games spanning every implemented agent: **reshuffles occur in 0 of
8,000 real games played by any non-random strategy** (only pure-random self-
play reshuffles meaningfully often, 5.1% of games) — confirming any agent
that plays with actual purpose finishes well before either 34-card deck
empties. **Conclusion: reshuffle-aware determinization stays deferred out of
this agent's v1** — the correctness gap it would close essentially never
fires in real play, though it remains a known, deliberate gap, not an
oversight.

---

## A11 — IS-MCTS + NN · "AlphaOracle Prime" (identical in EN / FR / ES)

|                 |                                                                                                    |
| --------------- | ---------------------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_ISMCTS_NN`                                                                              |
| Shorthand       | `ismctsnn`                                                                                            |
| Borealis rating | **74 — the roster ceiling** (measured, 58.44% [56.93%, 59.94%] head-to-head vs `A10`, the real ship gate; ~74.04% [72.68%, 75.36%] vs `borealis`) — design-intent estimate was 97 |
| Source file     | `src/ai_strat/ai_strat_ismctsnn.c`/`ai_strat_ismctsnn_net.c`/`ai_strat_ismctsnn_state.c` (implemented 2026-09-01/02, registered 2026-09-03) |

**The one thing this agent does**: `A10`'s exact SO-ISMCTS tree/search/
determinization code, unchanged — but leaf evaluation blends in a trained
value network instead of (or alongside) `A10`'s heuristic rollout:

```
leaf_value = (1 - nn_value_trust) * rollout_to_terminal + nn_value_trust * ismctsnn_net_value(state)
```

`nn_value_trust=0.0` is bit-for-bit `A10` (the superset guarantee — the
rollout is the exact same call `A10` always made); `1.0` skips the rollout
entirely (cheaper — no simulation to terminal) and shipped as the measured-
best point, since the sweep rose monotonically with trust.

The network: a small MLP (537-float information-set state -> 256 -> 128 ->
64 -> 1, BatchNorm + dropout during training) trained offline in PyTorch on
a curated self-play corpus (`A10` vs itself / `A7` / `A3`, deliberately
excluding `A5` since it's already `A10`'s own rollout policy), then compiled
into the game as a hand-written plain-C forward pass (no external ML
runtime) — verified to `2.4e-7` max diff against the live PyTorch model. The
537-float encoder is five 105-length card-type count vectors (own hand/
discard/combat-zone, opponent discard/combat-zone) plus 12 scalars (energy,
cash, turn, phase, deck-remaining counts, opponent hand size, a combo-bonus-
table one-hot) — deliberately omitting either player's deck contents
(anti-clairvoyance) and any explicit reshuffle-tracking feature (the header
argues discard+deck-remaining already carry it, and `A10`'s own Phase 0
finding that real non-random games essentially never reshuffle means the gap
rarely fires).

**Deliberately out of scope**: PUCT selection or a learned policy head
guiding tree *exploration* (a future Stage 4 agent, reserved name
"AlphaOracle Prime Plus I" — see below); training online/during play — all
training is offline PyTorch, compiled weights loaded once at startup.

**Ship gates, both measured on `--trust 1.0` vs `--trust 0.0` (the shipped
config vs. bit-for-bit `A10`)**, n=4,110 (137 games x 15 replicates x 2
seats): **Gate 2 (the real bar), vs `A10` directly**: baseline (trust=0.0)
measured exactly 50.00% (the superset guarantee reconfirmed); candidate
(trust=1.0) measured **58.44%** [56.93%, 59.94%] — a genuine, well-powered
win. **Gate 1 (context), vs `borealis`**: baseline 67.88% [66.44%, 69.29%]
(a free cross-check reproducing `A10`'s own documented 67.6-69.5%);
candidate **74.04%** [72.68%, 75.36%] — an estimated Borealis rating of ~74,
+5 over `A10`'s own 69.

**Cost**: ~16x `A10`'s own per-decision time (0.439s at `-O2` vs `A10`'s
~0.031s) — a naive, unvectorized C forward pass; `make release`'s `-O2` only
buys back ~11.5% of that.

**A methodological gotcha worth generalizing**: `A10` and `A11` share *one*
`ISMCTSParams` struct and *one* search function (unlike every other pair of
sibling agents, which use genuinely disjoint param structs) — a calibration
harness that copied another agent's "harmless to set unread params" pattern
silently invalidated an early round of measurements. Verify struct
disjointness, don't assume it, whenever two agents share state.

**Follow-up attempted and falsified, 2026-09-04**: a "bigger training
corpus" retrain was tested directly (a data-size learning curve on the
existing corpus, plus a recipe-diversity check with two new curated
opponents) before committing to a large generation run — both axes landed on
the same ~0.1705 val MSE floor as the shipped net, a genuine null result
matching `A9`/`A13`'s pattern. No retrain shipped; this agent's weights and
rating are unchanged. See `doc/changelog.md`'s 2026-09-04 entry for the full
record — the tooling built for that check remains as reusable
infrastructure for any future revisit. The reserved name for a future,
mechanism-different successor is **"AlphaOracle Prime Plus I"** (a distinct
"Plus" lineage, not "Prime II" — Stage 4/PUCT changes the search mechanism
itself, not one added lookahead ply the way `A7`->`A9` "Grandmaster"->
"Grandmaster II" did).

---

## A12 — Clairvoyant · "The Clairvoyant" / "Le Voyant" / "El Clarividente"

|                 |                                                                                                    |
| --------------- | ------------------------------------------------------------------------------------------------------ |
| Enum            | `AI_STRATEGY_CLAIRVOYANT`                                                                              |
| Shorthand       | `clairvoy`                                                                                              |
| Borealis rating | **31** (measured) — a few points below `A8`'s own 35, not a clear improvement                          |
| Source files    | `src/ai_strat/ai_strat_clairvoyant1.{c,h}`; reuses `ai_strat_simplemc_search.{c,h}` and `SimpleMcParams` verbatim (implemented, lightly calibrated, 2026-08-25) |

**Not part of the `A1`-`A11` ladder's authoritative order** — this agent's
enum ordinal sits after `A11`, but it was built as a side exploration of
`A8`'s own diagnosed ceiling, not as a scheduled ladder rung.

**The one thing this agent does**: `A8`'s identical progressive-pruning
search (same file, same params, same budget/pruning shape) with exactly one
change — rollouts keep this agent's own future moves uniformly random (the
search itself doesn't change), but give the *opponent's* simulated replies a
cheap heuristic instead of `A8`'s pure-random policy for both seats. The
heuristic is deliberately not a search: a single fixed candidate (the top up
to 3 affordable champions by `power`, a cheap O(n) partial selection, not a
subset enumeration) scored by one closed-form formula and committed only if
it clears a threshold, else pass/decline. Targets `A8`'s own diagnosed root
cause directly (rollouts modeling *both* seats as `AI_STRATEGY_RANDOM`, so
`A8`'s value estimates are calibrated against `rand` specifically).

**Two real defects found by playtracing** (not the aggregate number alone):
the first smoke test measured 26.6%, *below* `A8`'s 35 — the wrong
direction. (1) Attack never weighed cost, so the `score > 0` threshold was a
near-tautology (100% of sampled attack decisions committed) — the simulated
opponent modeled a strawman always-all-in play, not a sharper one. (2)
Defense had the identical gap, which showed up as an unusually large
seat-order asymmetry (16.4 percentage points) once attack's cost term was
added but defense's wasn't. Mirroring Borealis's own defense evaluation
exactly (cap raw defense value at the incoming threat, then subtract
`cost_weight * total_cost`) fixed both the formula and the seat asymmetry
(down to 6.0pp, back in normal range) as a side effect.

**`cost_weight`**: started from Borealis's own calibrated `luna_value`
(4.5846) as a reasonable prior, then swept directly (0.5 to 10.0, n=700
games/point vs `borealis`) since it was tuned for a different job there. Found
a genuinely unimodal response (quadratic fit R²=0.73, implied optimum ≈3.86),
with the borrowed value sitting near the *low* edge of a fairly flat 1-7
plateau. **Shipped at 3.0**, a real but modest gain over the borrowed
default.

**Final measured rating: 31** — close to but consistently a few points below
`A8`'s own 35 across every measurement in this investigation. Shipped as a
deliberately modest "fun, easy to beat" sibling of `A8`, not chased toward
parity or beyond — answering the open question of whether a cheap,
non-tree heuristic rollout policy could raise `A8`'s ceiling without more
compute budget: it did not.

---

## A13 — Cartographer · "The Cartographer" / "Le Cartographe" / "El Cartógrafo"

|                 |                                                                                                                              |
| --------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Enum            | `AI_STRATEGY_CARTOGRAPHER` (appended after `AI_STRATEGY_CLAIRVOYANT`, not restored to a "13th slot" mid-list — enum ordinal is a menu/table index everywhere) |
| Shorthand       | `carto`                                                                                                                      |
| Borealis rating | **65** (measured, statistically tied with `A7`'s own 65) — design-intent estimate was 68                                    |
| Source file     | `src/ai_strat/ai_strat_a13.{c,h}` + `ai_strat_a13_belief.{c,h}` + `ai_strat_a13_state.{c,h}` + `ai_strat_a13_enum.{c,h}` (implemented and calibrated 2026-08-31, shelved that date, registered 2026-09-04) |

**The one thing this agent does**: `A7`'s exact three-layer synthesis,
unmodified, plus four new deterministic mechanisms layered on top, each
independently pinned to a neutral value that recovers `A7` bit-for-bit (the
superset guarantee, enforced by short-circuit rather than multiplying by
zero, so recovery is exact). The unifying idea across all four: **model
hidden information as a distribution computed in closed form, never as a
point estimate, and never simulate the opponent's decision** — a direct,
deliberate answer to `A9`'s own failure mode (a *fabricated* single
surrogate hand). The unseen-card pool is derived by exact subtraction from
the known 120-card deck (`120 - own_hand - both_discards - both_combat_
zones`, deliberately not subtracting the observer's own deck — a player
doesn't know their own deck's contents either, matching `mc_determinize()`'s
own convention), and closed-form hypergeometric expectations are computed
over it — the engine already does the sampling equivalent of this in
`mc_determinize()`; this agent computes analytically what that function
samples. **The caveat that bounds the mechanism**: `setup_game()` deals only
80 of the 120 cards, so with `|unseen| ~ 90-114` and opponent hand size
`h <= 12`, `P(card in opponent hand) ~ 5-8%` — the opponent-hand belief is
genuinely diffuse; the sharply-known quantity is the pool's own *mean
value*, which drifts as high-power cards are observed leaving it.

**The four new layers**:

- **Layer R — race arithmetic.** Deterministic, continuous turns-to-kill
  both ways (`opp_energy / my_sustainable_damage`, `own_energy /
  opp_sustainable_damage`) turns `A7`'s *fixed* signed `defense_stdev_mult`
  into a *state-dependent* one: ahead in the race, deflate variance (protect
  the win, block more); behind, inflate it (seek variance). The direct
  completion of `A7`'s own sign-unification of `A4`'s deflation and `A6`'s
  inflation into one dial.
- **Layer K (draw) — deck-aware draw valuation.** Values a DRAW candidate
  against the *live* unseen-pool mean instead of a flat average — `A5`/`A7`'s
  `gamma` treats every card in a draw's yield as equally likely, but the
  pool mean drifts as the game progresses.
- **Layer D — reshuffle-boundary awareness.** Near the point where a
  player's own deck empties and their discard reshuffles back in, `E[next
  draw]` is not the diffuse pool mean but a sharp, exactly-known number over
  the (visible) discard — makes draw-card *timing* genuinely skillful.
- **Layer K (block) — Jensen-corrected expected block.** The version that
  *doesn't* replay `A9`'s failure: a naive `max(raw(S) - E[block], 0)` is a
  monotone transform of `raw(S)` (`E[block]` doesn't depend on `S`), so it
  only discourages attacking uniformly — `A9`'s exact signature. The
  corrected version, `E[net(S)] = sum_k P(K=k) * min(max(raw(S) -
  E[block|k], 0), opp_energy)`, conditions on the hypergeometric
  distribution over how many affordable champions the opponent holds; since
  `max(.,0)` is convex, Jensen's gap is strictly positive and *shrinks as
  raw(S) grows*, penalizing small, easily-absorbed attacks relative to
  committed 2-3 champion subsets — the opposite sign to `A9`'s mechanism.

**Calibrated: nothing beat `A7`, `hplus_trust` conclusively harmful.** Four
independent, properly-powered searches — including a 630-evaluation joint
search specifically testing for a coordinate-descent trap across layers —
all measured parity with `A7` or worse: neutral baseline 65.10% [64.58%,
65.62%] vs `borealis`; Layer R alone 64.82%; Layer K-draw+D alone 64.68%; the
broad joint search 65.18%; the Stage 4 joint (+`defense_stdev_mult` free)
64.88% [64.36%, 65.41%] (32,000 games) — the configuration this agent ships
with (see below). `hplus_trust` (Layer K-block's trust dial) is the one
mechanism that is not merely neutral: a clean monotonic decline vs both
opponents (17-64% vs `hbt`, tight non-overlapping CIs), `A9`'s exact
`reply_trust` failure signature, even sharper — pinned to 0, conclusively.
Two real implementation bugs were found and fixed along the way, independent
of the calibration verdict: a value-function scale mismatch in the belief
module (fixed using an empirically-measured 78.21%/21.79% attack/defense
champion play-role split rather than a naive 50/50 blend), and a calibration
driver bug that silently pinned `defense_stdev_mult` despite it being
explicitly requested.

**Shelved 2026-08-31, registered anyway 2026-09-04 for its character, not
its strength.** A second `A7` under a new name — and, had `hplus_trust`
shipped, a net-negative one — would pollute the Bradley-Terry fit, so the
original call was to shelve rather than register. Jonathan's later call: the
interesting part of this agent was never its strength but its distinct
*playing character* — race-aware risk appetite and deck-aware draw timing —
so it was registered anyway, deliberately **not** with the neutral/`A7`-
recovering configuration the superset guarantee still allows (which would
make it a literal `A7` clone under a new name), but with the **Stage 4
calibrated config**: `race_scale=8.147`, `race_stdev_ahead=0.868`,
`race_stdev_behind=-0.759`, `race_eps_gain=-0.0843`,
`belief_draw_weight=-1.102`, `belief_reshuffle_trust=0.431`,
`belief_opp_block_trust=0.225`, `defense_stdev_mult=1.290` (overriding `A7`'s
own 0.2156 — the one inherited field this agent ever re-fits) —
`hplus_trust`/`hplus_block_combo` stay pinned at neutral. Mechanically
distinct from `A7` even though statistically tied in strength (65 vs 65).
See `doc/changelog.md`'s 2026-09-04 entry for the full registration record
and verification.

**Deliberately out of scope**: any change to `A7`'s own 34 inherited
parameters beyond `defense_stdev_mult` — no `--identity-safe` escape hatch
for the other 33, since there is nothing of `A7`'s own tuning to erode;
sampling, rollouts, or re-determinization of any kind (`A8`, `A10`) — every
belief quantity here is closed-form and exact given public information;
reading `deck[*].card_indices` or `hand[opponent].cards` — the
anti-clairvoyance rule.
