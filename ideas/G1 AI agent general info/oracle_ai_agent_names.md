# Oracle — AI Agent Names & Borealis Ratings

Canonical naming reference for the AI selection / difficulty-select screen
across CLI, TUI and GUI front ends.

Ratings are on the Borealis scale (1–99), where the rating equals the expected
win percentage against the Borealis benchmark agent.

---

## Agent Roster

The last column is the original design-intent guess made before any agent existed;
`Measured` (2026-08-23, `--stda.rating` round-robin, `src/rating/`; `A4` added
2026-08-24, `A5`/`A6` added 2026-08-25, `A7` added 2026-08-25) is the actual
Bradley-Terry fit now that `Random`/`A1`/`A2`/`A3`/`A4`/`A5`/`A6`/`A7` are all
implemented. See `doc/changelog.md`'s 2026-08-23, 2026-08-24, and 2026-08-25 entries
for the runs these came from. `A4`'s measured rating (36) landed well below its own
design-intent estimate (62) and below the Borealis anchor -- a legitimate, calibrated
result (`doc/changelog.md`), the largest design-intent miss on this table so far.
`A5`'s and `A6`'s measured ratings both landed below their own estimates (70, 74) but
*above* the Borealis anchor. `A7`'s original measured rating (62, as of this
2026-08-25 fit) was the highest on this table so far, above all three of the agents
it synthesizes (`A4`/`A5`/`A6`) in the same roster-wide fit -- despite measuring a
clear pairwise loss to `A5` specifically (26.0%, see `doc/changelog.md`), a result
reported rather than tuned away. (`A7`'s rating later moved twice more -- see the
2026-08-27 and 2026-08-28 entries in `doc/changelog.md`; the roster table above
reflects the current 65, not this original 62.) `A8`'s and
`A9`'s measured ratings (35, added 2026-08-25; 59, added 2026-08-26) are **not** from a
roster-wide `--stda.rating` fit like the others above -- `A8` costs roughly 100x more per
game than any closed-form agent (~5.6ms/decision, ~100ms/game), which makes a full
round-robin featuring it impractical regardless of any other agent's own cost, so both
use a large-sample, both-seats direct pairwise measurement against `borealis`
specifically instead (`A8`: 1500 games; `A9`: 20,000 games). Since this project's rating
is defined as "the expected win percentage against Borealis" (top of this file) and both
were measured directly against exactly that opponent, this is not a lesser substitute
for the roster-wide fit -- for the specific quantity "rating vs Borealis," it's arguably
more direct. `A8`'s rating also did not improve under a budget-vs-rating sweep
(1.0x/1.75x/2.3x the rollout count, statistically indistinguishable results) -- see
`doc/changelog.md` for the full diagnosis (a rollout-policy bias, not a search-depth
shortfall). `A9`'s 59 is a genuinely respectable Borealis-scale rating -- above the
anchor, and above `A6`'s 52 -- despite the agent failing its OWN specific design
target (beating `A7` head-to-head, measured 47.2%, see `doc/changelog.md`): the two
questions ("how does this agent do against the whole roster/Borealis" vs "does this
agent's own specific mechanism achieve what it set out to do") can and did come apart,
the same way `A6` rated 52 overall despite a wide pairwise loss to `A5` (39.30%).

| Tech/Math Name | English | Français | Español | Measured | Est. Borealis Rating |
|---|---|---|---|---|---|
| Random | The Gambler | Le Parieur | El Apostador | 2 | 5 |
| Value Based | The Apprentice | L'Apprenti | El Aprendiz | 24 | 15 |
| Combo Threshold | The Showboat | Le Frimeur | El Fanfarrón | 30 | 37 |
| Greedy Power *(benchmark)* | Borealis | Borealis | Borealis | 50 | 50 |
| Balanced Rules | Bean Counter | Compteur de Fèves | Contador de Frijoles | 36 | 62 |
| Heuristic | ε-γ-δ | ε-γ-δ | ε-γ-δ | 64 | 70 |
| Tactical | Pressure Cooker | Cocotte-Minute | Olla a Presión | 52 | 74 |
| Hybrid (HBT) | The Grandmaster | Le Grand Maître | El Gran Maestro | 65 | 78 |
| Simple Monte Carlo | The Soothsayer | Le Devin | El Adivino | 35 | 82 |
| HBT 2-Ply | Grandmaster II | Grand Maître II | Gran Maestro II | 62 | 85 |
| IS-MCTS | The Omniscient | L'Omniscient | El Omnisciente | 69 | 92 |
| IS-MCTS + NN | AlphaOracle Prime | AlphaOracle Prime | AlphaOracle Prime | — | 97 |
| Clairvoyant *(A8's sibling, not on the A1-A11 ladder)* | The Clairvoyant | Le Voyant | El Clarividente | 31 | — |
| Cartographer *(implemented, calibrated, and SHELVED 2026-08-31 — see below, not registered)* | The Cartographer | Le Cartographe | El Cartógrafo | shelved | 68 |

---

## ASCII-Safe Variants

Use these if the display layer is byte-oriented. The current
`get_strategy_display_name()` values are already accent-stripped, and
`print_ai_agent_shorthand_list()` pads with `%-16s`, which counts bytes rather
than glyphs.

| Tech/Math Name | English | Français | Español |
|---|---|---|---|
| Random | The Gambler | Le Parieur | El Apostador |
| Value Based | The Apprentice | L'Apprenti | El Aprendiz |
| Combo Threshold | The Showboat | Le Frimeur | El Fanfarron |
| Greedy Power | Borealis | Borealis | Borealis |
| Balanced Rules | Bean Counter | Compteur de Feves | Contador de Frijoles |
| Heuristic | Eps-Gam-Del | Eps-Gam-Del | Eps-Gam-Del |
| Tactical | Pressure Cooker | Cocotte-Minute | Olla a Presion |
| Hybrid (HBT) | The Grandmaster | Le Grand Maitre | El Gran Maestro |
| Simple Monte Carlo | The Soothsayer | Le Devin | El Adivino |
| HBT 2-Ply | Grandmaster II | Grand Maitre II | Gran Maestro II |
| IS-MCTS | The Omniscient | L'Omniscient | El Omnisciente |
| IS-MCTS + NN | AlphaOracle Prime | AlphaOracle Prime | AlphaOracle Prime |
| Clairvoyant | The Clairvoyant | Le Voyant | El Clarividente |
| Cartographer | The Cartographer | Le Cartographe | El Cartografo |

Alternative ASCII forms for the Heuristic agent, in decreasing terseness:
`E-G-D` · `Eps-Gam-Del` · `Epsilon-Gamma-Delta`

---

## Mapping to Enum & Shorthands

Roster order matches strength ordering, which should match `AIStrategyType`
declaration order.

| Enum Constant | Shorthand | Display Name (EN) |
|---|---|---|
| `AI_STRATEGY_RANDOM` | `rand` | The Gambler |
| `AI_STRATEGY_VALUE_BASED` | `value` | The Apprentice |
| `AI_STRATEGY_COMBO_THRESHOLD` | `combo` | The Showboat |
| `AI_STRATEGY_BOREALIS` | `borealis` | Borealis |
| `AI_STRATEGY_BALANCED` | `balanced` | Bean Counter |
| `AI_STRATEGY_HEURISTIC` | `heuristic` | ε-γ-δ |
| `AI_STRATEGY_TACTICAL` | `tactical` | Pressure Cooker |
| `AI_STRATEGY_HYBRID_HBT` | `hbt` | The Grandmaster |
| `AI_STRATEGY_SIMPLE_MC` | `simplemc` | The Soothsayer |
| `AI_STRATEGY_HBT_2PLY` | `hbt2ply` | Grandmaster II |
| `AI_STRATEGY_ISMCTS` | `ismcts` | The Omniscient |
| `AI_STRATEGY_ISMCTS_NN` | `ismctsnn` | AlphaOracle Prime |
| `AI_STRATEGY_CLAIRVOYANT` | `clairvoy` | The Clairvoyant |

No `AI_STRATEGY_CARTOGRAPHER` row: implemented, calibrated, and shelved 2026-08-31 (see
"Naming Rationale" below and `../A13 .../about.md`) — the enum constant was removed from
`AIStrategyType` when the agent was shelved, `carto` is not wired into
`AI_STRATEGY_SHORTHANDS[]`, and this project's actual `AIStrategyType` declaration order
therefore still ends at `AI_STRATEGY_CLAIRVOYANT`.

### Required renames

Two existing enum constants no longer match what they name. Rename before
either agent is implemented, not after:

| Old | New | Reason |
|---|---|---|
| `AI_STRATEGY_GREEDY_POWER` | `AI_STRATEGY_BOREALIS` | This spec is now the benchmark |
| `AI_STRATEGY_COMBO_AWARE` | `AI_STRATEGY_COMBO_THRESHOLD` | "Combo Aware" no longer discriminates — Borealis computes combo bonuses too |

`greedy` and `showboat` (the retired pre-rename tech name and a flavour name,
respectively) were considered as shorthand aliases but dropped in favor of one
canonical shorthand per agent (2026-08-21) -- see `doc/changelog.md`. Neither
had an implemented agent behind it yet, so this is not a breaking change to
any working script.

### Retired names

- **The Hoarder** / L'Accapareur / El Acaparador — previously assigned to
  Greedy Power. Freed when that spec became Borealis. Unused.

---

## What Changed and Why

The original plan had a threshold-gated, probabilistic-defence agent as the
benchmark and a cost-penalised subset-scoring agent below it. These were
swapped.

**Reason:** a benchmark agent needs a single monotone strength dial so it can
be calibrated to a target band. Greedy Power has one — λ, the value of a luna
in damage units — and win rate is unimodal in it, so the agent can be detuned
in either direction while still playing coherently. The older design had seven
interacting parameters with unclear individual effects (one of which,
`aggression_level`, was never referenced by its own algorithm), and was weakened
primarily by a ~45% chance of declining a correct defence. That produces
*exploitable* rather than *diffuse* suboptimality, which a child notices and
plays around within a dozen games.

The displaced design survives as **The Showboat**: chases high combo bonuses,
hoards them, and blocks unreliably. Its behaviour is legible enough that losing
to it teaches something, which makes it a reasonable rung below the benchmark.

---

## Notes on the Ratings

- **Borealis = 50 by definition.** It is the scale anchor (`s = 1.0` in the
  Bradley-Terry model); its rating never moves.
- All other values are **placeholder design-intent estimates**, not measured
  results. Replace each with a measured rating from simulated games against
  Borealis once the agent is implemented.
- This roster's `Measured`/`Est. Borealis Rating` columns are mirrored in code by
  `AI_STRATEGY_RATINGS[]` (`src/ui/shared/player_config.c`), which the interactive
  AI strategy menu (`display_ai_strategy_menu()`, CLI and TUI) reads to print each
  agent's rating -- `~`-prefixed for a design-intent estimate, bare for a measured
  one. When an agent goes from estimate to measured here, update that table too
  (see "Checklist: Adding a New AI Strategy", `doc/oracle_todo.md`).
- Ordering constraints established during design:
  - Combo Threshold sits between Value Based and Borealis.
  - Simple Monte Carlo is weaker than HBT 2-Ply.
  - Value Based is only marginally stronger than Random.
- Gaps are deliberately non-uniform. The scale is non-linear: a 10-point gap
  near 50 is a much smaller strength difference than a 10-point gap near 90.

### Calibration target

Children aged 11–14 with moderate to frequent experience should average a
45–55% win rate against Borealis.

That is a wide age range for a single agent to serve. The difficulty screen was
meant to mitigate this by bracketing Borealis with a weaker and a stronger
neighbour so players self-select, easing the calibration burden on Borealis
alone. **The weaker-neighbour half of that bracketing did not survive
measurement**: Bean Counter (`A4`) was designed to sit above Borealis (est. 62)
but measured at **36** — below the anchor, in the same neighbourhood as Combo
Threshold (30) rather than above Borealis (`doc/changelog.md`, 2026-08-24). The
stronger-neighbour half arrived the following day: Eps-Gam-Del (`A5`) measured
**60** — above the anchor, though below its own 70 estimate
(`doc/changelog.md`, 2026-08-25) — so Borealis now has a genuine measured
stronger neighbour, just not (yet) a measured weaker one nearer 40-45 than
`A4`'s 36. Tune λ against playtest data, not against simulation win rates —
simulation finds the λ that maximises strength, which is not necessarily the λ
that hits the target band.

---

## Naming Rationale (Progression Narrative)

The roster reads as a climb from chaos to near-superhuman play, usable as
flavour text on the difficulty-select screen:

1. **The Gambler** — pure chance, no plan
2. **The Apprentice** — knows one thing (efficiency) and nothing else
3. **The Showboat** — chases the spectacular play, forgets to block
4. **Borealis** — the yardstick everything else is measured against
5. **Bean Counter** — obsessive resource accounting
6. **ε-γ-δ** — reduces the game to a weighted advantage function
7. **Pressure Cooker** — reads the position and turns up the heat
8. **The Grandmaster** — synthesis of the three approaches above
9. **The Soothsayer** — rolls the dice a thousand times before choosing
10. **Grandmaster II** — the Grandmaster, now anticipating your reply
11. **The Omniscient** — deep tree search over hidden information
12. **AlphaOracle Prime** — search plus learned intuition

Note that **ε-γ-δ** deliberately breaks the flavour-name pattern. It is the one
agent whose entire identity is its weights, so naming it after them is a
reasonable in-joke — but it is a conscious exception, not an oversight.

**The Cartographer** (`A13`, implemented, calibrated, and shelved 2026-08-31 — see
`../A13 ai agent cartographer (the cartographer)/about.md`) was designed to sit outside this
progression narrative rather than extending it as a 13th rung: it was not meant as a step
further toward "near-superhuman," but the Grandmaster's synthesis (`A7`) plus closed-form
deck-and-race arithmetic, aimed at the strongest *deterministic* play the roster could
produce. The name fit the theme anyway — mapping unseen territory (the unseen-card pool)
rather than reading minds (`The Soothsayer`, `The Omniscient`, `The Clairvoyant`), which was
exactly the distinction the design leaned on: computing an exact distribution over what's
hidden, never fabricating a single guess at it. **The idea didn't survive calibration**:
every mechanism measured at parity with `A7` or worse (one, `hplus_trust`, conclusively
harmful) across four independent properly-powered searches, so this agent was shelved rather
than registered — no enum slot, no shorthand, not part of the roster above. "The Hoarder"
(see "Retired names" above) remains the only freed-but-unused flavour name on this table.
