# Handout — Implement the Borealis Benchmark AI Agent

**Display name:** Borealis (identical in EN / FR / ES)
**Technique:** Greedy Power — one-ply subset enumeration with cost-penalised
scoring, plus epsilon tie-break randomisation and lethal-combo holding.
**Role:** This is the **Bradley-Terry anchor** for the entire rating system.
Its strength is fixed at rating 50 by definition; every other agent and every
human player is measured against it.
**Scope:** standalone. Do not depend on `strat_lib` or on any other agent.

---

## 1. Why This Agent Is Special

Borealis is not just another strategy in the roster. Because it defines the
zero point of the rating scale, three properties matter more than raw strength:

1. **Calibratable.** Its strength must be adjustable along a single, monotone
   axis so it can be tuned to a target band during playtesting.
2. **Reproducible.** Identical seed must produce identical play, so ratings
   can be re-measured and regressions detected.
3. **Not memorisable.** A repeat human opponent must not be able to learn a
   fixed exploit, which is why it is deliberately stochastic (§6).

Calibration target: children aged 11–14 with moderate to frequent experience
should average a 45–55% win rate against it.

---

## 2. Concept

One-ply, no simulation, no opponent modelling. Borealis enumerates every legal
champion play of size 0–3, scores each by expected combat contribution net of
luna cost, and plays the best — breaking near-ties at random and holding back
very large combos for a finishing blow.

Deliberately **out of scope** (these belong to stronger agents):

- resource targets tied to opponent energy (Balanced Rules)
- game-phase or aggression modelling (Tactical)
- advantage functions over energy/cards/cash (ε-γ-δ)
- any lookahead or simulation

---

## 3. Parameters

```c
typedef struct
{ float luna_value;             // lambda: damage-units per luna. Default 0.5
  float tiebreak_epsilon;       // score window for random tie-break. Default 0.5
  bool  hold_lethal_combos;     // enable combo holding. Default true
  int   lethal_combo_bonus;     // combo bonus considered "big". Default 16
  int   lethal_hold_ceiling;    // stop holding at/below this opp energy. Default 25
  int   min_hand_size_target;   // draw-card trigger. Default 4
} BorealisParams;

void            borealis_set_params(const BorealisParams* params);
BorealisParams  borealis_get_default_params(void);
```

**Note (2026-08-21):** `borealis_set_params()` above is a single global setter,
not per-player. A1 Value Based's calibration work found that self-play
(parameter set 1 vs. parameter set 2, head-to-head in one game) is a
significantly more discriminating calibration signal than vs-Random, once
vs-Random win rates saturate near a ceiling — see `doc/changelog.md`,
2026-08-21. That requires a *per-player* override, which
`value_based_set_params(PlayerID player, ...)` provides for A1
(`src/ai_strat/ai_strat_valuebased.c/h`) and which this signature does not.
When implementing A3, adapt to `borealis_set_params(PlayerID player, const
BorealisParams* params)` following that same pattern (file-static per-player
override arrays, calibration-only, not threaded through the general strategy
framework) rather than porting this single-setter signature as written.

**`luna_value` (λ) is the primary calibration dial.** It is the only parameter
whose effect on strength is monotone and well understood — see §8. The others
are behavioural switches, not strength knobs. Do not attempt to calibrate on
more than one axis at a time.

Public strategy API, matching `AttackStrategyFunc` / `DefenseStrategyFunc`:

```c
void borealis_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void borealis_defense_strategy(struct gamestate* gstate, GameContext* ctx);
```

Everything else is `static`.

---

## 4. Scoring Model

One scoring function serves both phases. For a candidate set `S` of champion
card indices:

```
score(S) = Σ_{c∈S} contribution(c) + combo_bonus(S) − λ · Σ_{c∈S} cost(c)
```

`contribution(c)` is expected attack (attack phase) or expected defence
(defence phase).

- The empty set scores exactly `0`. This makes "play nothing" a first-class
  candidate — no special-case pass logic, no arbitrary comparison multipliers.
- All terms are in damage units, including the cost penalty. Keep them
  commensurable; this is what makes λ interpretable.

### Defence phase — cap at incoming threat

There is no value in blocking more damage than is being dealt:

```
value(S) = min( Σ expected_defence + combo_bonus(S), E[incoming attack] )
           − λ · Σ cost(S)
```

`E[incoming attack]` is directly computable — the attacker's combat zone is
fully visible. Sum expected attack over those cards and call the real combo
bonus function on them. **Do not estimate it.**

This cap is the single most important line in the defence logic. Without it
the agent over-defends and bleeds lunas.

---

## 5. Enumeration — No Pruning

Hand size is capped at 7 by the discard-to-7 rule, so the affordable-champion
list holds at most 7 entries and subsets of size 0–3 number at most
`1 + 7 + 21 + 35 = 64`. Enumerate exhaustively with three nested loops.

**Do not add loop caps, sampling, or combo-bonus thresholds.** There is nothing
to prune, and threshold-gated pruning is precisely the flaw that the earlier
Combo Threshold design (now "The Showboat") exists to demonstrate. Pruning on
combo bonus when the decision criterion is total score discards strong plays —
two powerful champions with a small bonus can beat any single champion.

Enforce cumulative affordability inside the loops: a set is legal only if
`Σ cost(S) ≤ current_cash_balance[player]`. Per-card cost filtering is not
sufficient for sets of size 2–3.

---

## 6. Epsilon Tie-Break Randomisation

After scoring all candidates:

1. Find `best_score`.
2. Collect every candidate with `score >= best_score − tiebreak_epsilon`.
3. Choose uniformly at random from that set using `ctx->rng`.

This is where Borealis's unpredictability lives. It is deliberately placed at
the point of **near-indifference**, so the strength cost is close to zero while
still denying a repeat human opponent a fixed, memorisable script.

Requirements:

- All randomness goes through `ctx->rng`. No `rand()`, no static RNG state.
  Follow the pattern in the Random agent (`RND_randn(count, ctx)`).
- With `tiebreak_epsilon = 0`, exact ties must still be broken randomly.
- Play is reproducible under a fixed `--prng.seed`, but note that RNG
  consumption differs between agent versions, so a given seed will not produce
  identical games across a code change. That is expected; do not treat it as a
  regression.

---

## 7. Lethal Combo Holding

Ported from the earlier design as genuine forward-looking behaviour. When
`hold_lethal_combos` is set, exclude a candidate set `S` from consideration if
**all** of the following hold:

- `combo_bonus(S) >= lethal_combo_bonus`
- `S` is **not** lethal now — i.e. expected damage from `S` is less than the
  opponent's current energy
- opponent energy `> lethal_hold_ceiling`

Read as: hoard the big combo while the opponent is comfortably healthy, but
release it the moment it either finishes them or they drop close enough that a
finishing window is imminent.

Two things to watch:

- **Interaction with discard-to-7.** Holding combo pieces raises the chance
  they are discarded at end of turn, wasting the whole plan. If simulation
  shows held combos are frequently discarded rather than played, this feature
  is net-negative and should be disabled by default.
- Never let holding produce a stall. The empty set is always a legal candidate,
  so this cannot deadlock — but confirm the agent still attacks with non-combo
  plays while holding.

---

## 8. Calibration — λ Is the Dial

Win rate against any fixed opponent is **unimodal in λ**:

- `λ = 0` — lunas are free. Overspends, empties hand, runs dry.
- `λ` optimal — balanced.
- `λ` large — hoards, never commits, never closes out a game.

To make Borealis weaker, move λ off-optimum in **either** direction. Both
produce a coherent, human-legible playing style rather than visible stupidity.
This is the key property that makes it usable as a tunable benchmark.

**Do not weaken Borealis by making it randomly skip correct plays.** Diffuse
suboptimality keeps a benchmark feeling like a real opponent; exploitable
suboptimality does not. A child who notices "it sometimes just doesn't block"
has found a strategy, not a challenge.

---

## 9. Draw / Cash Cards (Attack Phase Only)

Minimal, clearly marked as placeholder heuristics outside the scoring model:

- If hand size `< min_hand_size_target` **and** opponent energy `> 20` **and**
  an affordable draw card is held → play the cheapest one, return.
- If no champions are affordable and a cash card is held alongside at least one
  champion → play the cash card. Mirror the guard in the Random agent, which
  skips cash cards when the hand holds no champions.

Comment these as the first thing a stronger agent should replace.

---

## 10. Structural Constraints

Project limits: **≤35 lines of code per function** (hard limit 100),
**≤400 lines per file** (soft limit 500).

Suggested decomposition:

| Function | Responsibility |
|---|---|
| `build_affordable_champions()` | filter hand → array of card indices |
| `set_score()` | the formula in §4 |
| `is_held_combo()` | §7 exclusion test |
| `best_champion_set()` | enumerate 0–3, collect epsilon-band, random pick |
| `expected_incoming_attack()` | sum attack + real combo over combat zone |
| `try_play_draw_card()` | §9 placeholder |
| `borealis_attack_strategy()` | orchestration only |
| `borealis_defense_strategy()` | orchestration only |

`best_champion_set()` is shared between phases — parameterise it with a
contribution-function pointer or a phase flag rather than duplicating.

If the file exceeds ~400 lines after decomposition, split enumeration and
scoring into a separate translation unit rather than letting it grow.

---

## 11. Integration Points

1. **Enum rename.** `AI_STRATEGY_GREEDY_POWER` should become
   `AI_STRATEGY_BOREALIS`. Do this now, before the agent exists — the old name
   no longer matches the display name and will mislead.
2. **`get_strategy_display_name()`** — return "Borealis" for all three
   languages. Match the accent-stripping convention already used there.
3. **Interactive strategy menu** — drop the "not yet implemented" suffix.
4. **Shorthands** — add `borealis`; retain `greedy` as an alias.
5. **Strategy dispatch** — register both functions in the `AIStrategyType` →
   `StrategySet` mapping (currently only wires up the Random agent).
6. **`doc/changelog.md`** and the AI section of **`doc/oracle_todo.md`**.

---

## 12. Verify Before Coding

Assumed from design notes; **check against actual source**:

- Exact `fullDeck[]` field names for expected attack, expected defence, cost,
  card type. Confirm whether expected values are precomputed or derived.
- Combo bonus function name and signature, and **how the deck type is
  obtained** — read it from `gstate`/`ctx`, never hardcode `DECK_RANDOM`.
- The `CombatCard` struct (or equivalent) fed to the combo calculation.
- Combat-zone representation, for `expected_incoming_attack()`.
- `play_card()` / `play_champion()` signatures, and whether multiple champions
  are played by repeated calls or one batched call.
- `HDCLL_toArray()` allocates — every path must `free()`, including early
  returns. The Random agent is the reference for correct handling.

Flag any assumption that proves wrong rather than working around it silently.

---

## 13. Testing

```bash
./bin/oracle --ai borealis --ai rand -n 1000   # confirm exact flag syntax
```

- **vs Random:** should win clearly more than 50%. If not, the bug is almost
  certainly in cumulative affordability or the sign of the combo bonus.
- **Mirror match:** near 50%, any deviation attributable to first-player
  advantage. Also measure **variance across seeds** — this answers the open
  question about whether benchmark stochasticity is acceptable for BT
  anchoring. (It should be: a measured win rate is Bernoulli(p) regardless of
  where the randomness originates. Internal noise changes p, it does not make p
  harder to estimate.)
- **λ sweep:** 0.0, 0.25, 0.5, 1.0, 2.0 vs Random. Win rate should be unimodal.
  A flat curve means the cost term is not wired in.
- **`hold_lethal_combos` A/B:** run with the flag on and off. If it does not
  improve win rate, default it to false (see §7).

---

## 14. Deferred

Variance-aware defence (`E[attack] − β·SD[attack]`) is specified in the
Balanced Rules design notes and would be strictly better, but needs per-card
attack variance which may not exist in `fullDeck[]` yet. **Use the
expected-value version here.** Leave a comment marking the hook.
