# Handout — Implement the Combo Threshold AI Agent ("The Showboat")

**Status:** implemented and calibrated (2026-08-22) — see `doc/changelog.md` and this
folder's `about.md` for the shipped parameters and measured strength. This document is
kept as implementation history/rationale; a few details below (marked inline) went stale
between when this was written and when `A1` Value Based landed (2026-08-21) — the code is
authoritative where they conflict.
**History:** this document was originally titled "Oracle — AI Agent Roadmap & Borealis
Benchmark" and specified the rating-scale benchmark agent. The benchmark role was
**reassigned to Greedy Power** (`A3 ai agent greedy power (borealis)/`) — see
`../G1 AI agent general info/oracle_ai_agent_names.md`, "What Changed and Why". This
document was rewritten in place (2026-08-21) to specify what the displaced design became:
**Combo Threshold**, roster name **The Showboat**, a non-benchmark roster agent that sits
between Value Based and Borealis.
**Canonical roster reference for names/ratings/ordering:**
`../G1 AI agent general info/oracle_ai_agent_names.md` — treat that file as the tie-breaker
if anything below conflicts with it.

---

## 1. Purpose

Specifies **Combo Threshold**, a combo-chasing agent that pursues champion combinations
whose combo bonus clears a tunable threshold, and defends probabilistically rather than by
a fixed rule. It is *not* the benchmark — see §3 for why, and for how that same design
reads as a legible, beatable roster rung instead.

---

## 2. Decisions summary

| Item | Decision |
|---|---|
| Roster position | Between Value Based (`A1`) and Borealis (`A3`) |
| Est. rating | **37** — design-intent estimate, unmeasured (per the names file) |
| Enum | `AI_STRATEGY_COMBO_THRESHOLD` (retired name: `AI_STRATEGY_COMBO_AWARE`), declared in `src/core/game_types.h` as of `A1` (was `ui/shared/player_config.h` when this was drafted) |
| Shorthand | `combo` (**stale below**: the line used to read "`showboat`, alias `combo`" — `A1`'s one-shorthand-per-agent cleanup, 2026-08-21, retired `showboat` before this agent shipped; `combo` is the sole canonical shorthand) |
| File naming | `src/ai_strat/ai_strat_combo_threshold.c/h`, following the repo's `ai_strat_*` convention (not the `strat_*.c` naming this document originally proposed) |
| Character | chases and hoards combo bonuses above a threshold; defends unreliably — see §8 |

---

## 3. Why this design lost the benchmark role

Playtest evidence (physical cards + `stda.cli`): children aged ~8 learn combo awareness
within 10–15 minutes and then beat the Random AI **≥75%** of the time, so *some* combo-aware
agent was always going to anchor the rating scale rather than Random.

This design was the original candidate for that anchor, but was rejected in favour of
Greedy Power (`A3`) because a benchmark needs a single monotone strength dial, and this
design has seven interacting parameters with unclear individual effects (`aggression_level`
was never wired into its own algorithm — see §5.4). It was also weakened primarily by a
~45% chance of declining a correct defence, which is *exploitable* suboptimality — a human
opponent notices "it sometimes just doesn't block" and has found a strategy, not a
challenge, within a dozen games. Greedy Power's λ dial produces *diffuse* suboptimality
instead (`../A3 ai agent greedy power (borealis)/greedy_power_borealis_handout.md` §8),
which is what a calibratable benchmark needs.

None of that disqualifies this design as a **roster rung**. Legibility is a liability in a
benchmark and an asset in a mid-tier opponent: losing to a combo-chasing, unreliable
defender teaches something. That is the case for keeping it as Combo Threshold / The
Showboat, one step below Borealis.

---

## 4. Roster context

Full roster, naming, and rating table now live in the canonical
`../G1 AI agent general info/oracle_ai_agent_names.md` — not duplicated here to avoid a
second copy drifting out of sync. This agent's neighbours: `A1` Value Based below,
`A3` Greedy Power (Borealis) above.

---

## 5. Corrections to earlier design output

**Read this section before implementing.** The following issues exist in the original
draft sketch and must not be carried forward.

### 5.1 `power` is an efficiency ratio, not card strength — CRITICAL

Verified against `game_constants.c`:

```
attack_efficiency  = expected_attack  / cost      (cost 0 treated as ~0.25)
defense_efficiency = expected_defense / cost
power              = (attack_efficiency + defense_efficiency) / 2
```

Examples:

| Card | Dice | Cost | expected_attack | power |
|---|---|---|---|---|
| idx 0 | d4+0 | 0 | 2.5 | **10.0** |
| idx 34 | d20+5 | 3 | 15.5 | **4.33** |

Ranking candidate plays by `power` would make this agent prefer a d4+0 over a d20+5. Any
scoring model must use:

- **Combat value:** `expected_attack` (attacking) / `expected_defense` (defending), plus combo bonus.
- **Efficiency (`power`, `*_efficiency`):** only as a cash-constrained tiebreaker, never as primary strength.

**Related, out of scope but worth logging:** `discard_to_7_cards()` (`card_actions.c`) and
`apply_mulligan()` (`stda_auto.c`) both discard the *lowest `power`* card, i.e. they
preferentially throw away expensive high-strength champions and keep cheap weak ones. That
may be intentional (cash-constrained games favour efficiency) or may be a long-standing bug.
Review under the `strat_lib` work item
(`../G1 AI agent general info/strat_lib_refactor_handout.md`), not here.
`AVERAGE_POWER_FOR_MULLIGAN 4.98` sits near the cost-1/cost-2 efficiency band, which
suggests the threshold was tuned to efficiency semantics deliberately.

### 5.2 `should_defend()` had an invalid cast

The draft contained `genRand(&((GameContext*)gstate)->rng)` — casting a
`struct gamestate*` to `GameContext*`. Undefined behaviour. `should_defend()` must take
`GameContext* ctx` and use `ctx->rng`, or use `RND_randn()` from `rnd.h`.

### 5.3 `find_best_attack_play()` was ~95 lines

Violates the ~35-line target. Decompose into:

```
static void eval_single_champions(...);
static void eval_two_card_combos(...);
static void eval_three_card_combos(...);
static void eval_cash_fallback(...);
```

Each takes the running `PlayCandidate* best` and updates it in place.

### 5.4 `aggression_level` — resolved

The draft declared `aggression_level` in the params struct but never read it — flagged by
name in the names file's "What Changed and Why" as part of why this design was unfit as a
benchmark dial (one of seven params with unclear effect). **Decision: wire it in**, scoped
narrowly so it doesn't turn into a second uncontrolled dial. Use it as a single multiplier
on both threshold parameters:

```
effective_combo_bonus_threshold  = combo_bonus_threshold  / aggression_level
effective_combo3_bonus_threshold = combo3_bonus_threshold / aggression_level
```

Higher `aggression_level` → lower effective thresholds → chases combos more eagerly. This
keeps the "showboat" personality (chases combos, sometimes at the expense of a solid single
play) tunable along one axis without resurrecting the multi-parameter ambiguity that got
this design bumped from benchmark to roster rung.

### 5.5 Other draft defects

- `calculate_card_combo_potential()` declared and defined but never called. Remove or use.
- `combo_threshold_get_default_params()` (originally `borealis_get_default_params()`)
  duplicated the static initializer — needs a single source of truth.
- `estimate_attack_strength()` summed `attack_base + expected_defense`; that is exactly
  `expected_attack`. Use the precomputed field.
- Brace style in the original draft samples was K&R. **The repo uses run-in braces,
  2-space indent, type-aligned pointers (`int* p`), formatted via astyle.** Match the repo,
  not the samples.

---

## 6. Combo Threshold specification

### 6.1 Files

```
src/ai_strat/ai_strat_combo_threshold.h    ~80 lines
src/ai_strat/ai_strat_combo_threshold.c    target ≤400 lines
```

If the `.c` exceeds ~400 lines after decomposition, split enumeration/scoring into
`ai_strat_combo_threshold_eval.c` rather than letting the file grow.

### 6.2 Public API

```c
void combo_threshold_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void combo_threshold_defense_strategy(struct gamestate* gstate, GameContext* ctx);

void                  combo_threshold_set_params(const ComboThresholdParams* params);
ComboThresholdParams  combo_threshold_get_default_params(void);
```

**Note (2026-08-22, shipped differently):** `combo_threshold_set_params()` above is a
single global setter, not per-player — the same issue `A3`'s handout flagged for
`borealis_set_params()` after `A1`'s calibration work showed self-play (parameter set 1
vs. parameter set 2, head-to-head in one game) is a more discriminating calibration
signal than vs-Random once vs-Random saturates near a ceiling. That requires a
*per-player* override. Shipped:
`combo_threshold_set_params(PlayerID player, const ComboThresholdParams* params)` /
`combo_threshold_reset_params(void)`, file-static per-player override arrays, same
pattern as `value_based_set_params()` (`src/ai_strat/ai_strat_valuebased.c/h`) —
calibration-only, not threaded through the general strategy framework.

Signatures must match `AttackStrategyFunc` / `DefenseStrategyFunc` in `ai_strategy.h`.

### 6.3 Tunable parameters

```c
typedef struct
{ float aggression_level;           // 1.0-3.0, scales both thresholds — see §5.4
  int   combo_bonus_threshold;      // 7-12, min 2-card bonus to pursue
  int   combo3_bonus_threshold;     // 14+, min 3-card bonus to pursue
  float defend_probability_base;    // 0.40-0.70
  int   defend_damage_threshold;    // 6-10, ignore attacks below this
  int   min_hand_size_target;       // 3-5, trigger for draw cards
  bool  save_big_combos_for_lethal; // hold +16 combos while opp energy high
} ComboThresholdParams;
```

Defaults: `1.0 / 10 / 14 / 0.55 / 8 / 4 / true`. `aggression_level` defaults to 1.0 (no
scaling) so the other defaults reproduce the originally-drafted behaviour; treat all seven
as starting guesses for calibration, not tuned values. Unlike Borealis's λ (see `A3`'s
handout §8), **this parameter set has no single agreed strength dial** — that absence is
the defining, deliberate difference from Borealis, not a gap to fill.

### 6.4 Attack algorithm

This is the threshold-gated pruning behaviour that defines the agent — see §8 for why it's
kept rather than "fixed" to match Borealis's exhaustive enumeration.

1. If `hand_size < min_hand_size_target` and opponent energy > 20 and an affordable draw
   card is held → play it, return.
2. Enumerate candidate plays subject to `total_cost <= cash`:
   - all single champions
   - all champion pairs where `combo_bonus >= effective_combo_bonus_threshold` (§5.4)
   - all champion triples where `combo_bonus >= effective_combo3_bonus_threshold` (§5.4)
3. Score: `Σ expected_attack + w · combo_bonus` (start `w = 1.0`; `w` is a calibration knob).
4. If `save_big_combos_for_lethal` and the triple's bonus ≥ 16 and opponent energy > 25 →
   skip that candidate.
5. If best score is below a floor and a cash card + a champion are both held → play the
   cash card.
6. Otherwise play the best candidate, or pass.

Bound the triple loop (hand can reach 15 cards → 455 triples; acceptable, but check the
profile once `stda.auto` runs 1000+ sims).

### 6.5 Defense algorithm — the probabilistic decline

1. `expected_attack = Σ expected_attack` over cards in the attacker's combat zone, plus the
   combo bonus. The attacker's combat zone is fully visible, so call
   `calculate_combo_bonus()` directly rather than estimating.
2. If `expected_attack < defend_damage_threshold` → decline.
3. Compute defend probability from `defend_probability_base`, adjusted: +0.15 if own energy
   < 20, +0.08 if < 40; −0.20 if opponent energy < 15, −0.10 if < 30. Roll against it using
   `ctx->rng`.
4. If defending, pick the play maximising `(Σ expected_defense + combo_bonus) / (cost + 1)`.
5. Only consider 2-card defensive combos when `expected_attack >= 15`.

Step 3's roll is the mechanism §3 above calls out as exploitable at the benchmark tier —
here it is the point, not a defect: **keep it.** Do not replace it with a deterministic
rule; that would collapse this agent's character into a weaker Borealis rather than a
distinct roster rung.

### 6.6 Determinism

Stochastic by design (the defend roll and the eventual tie-break, if added). **All
randomness must go through `ctx->rng`** so runs stay reproducible under a fixed
`--prng.seed`. No `rand()`, no static RNG state.

---

## 7. Integration points

**Status (2026-08-22): all six items below are done.** Kept as a record of the plan;
the "stale" notes describe how reality diverged from what this section originally said
by the time implementation actually happened (`A1` landed in between, 2026-08-21).

1. **Enum.** ~~`AI_STRATEGY_COMBO_THRESHOLD` already exists in
   `src/ui/shared/player_config.h`, currently under the retired name
   `AI_STRATEGY_COMBO_AWARE` — see Part 5 of the folder-sort plan for the rename.~~
   **Stale**: `A1` moved `AIStrategyType` to `src/core/game_types.h` and completed the
   `AI_STRATEGY_COMBO_AWARE` → `AI_STRATEGY_COMBO_THRESHOLD` rename before this agent's
   own implementation started; nothing left to do here by the time it did.
2. **`get_strategy_display_name()`** — flavour name "The Showboat" / "Le Frimeur" /
   "El Fanfarrón", per the names file. Done (already present pre-implementation).
3. **Interactive strategy menu** — drop the "not yet implemented" suffix once implemented.
   Done, and automatic: `ai_strategy_is_implemented()` (`ai_strategy.c`) derives the label
   from the registry (item 5) rather than a hardcoded per-strategy check.
4. **Shorthands.** ~~`showboat` primary, `combo` retained as alias so existing scripts and
   saved configs keep working (per the names file).~~ **Stale**: `A1`'s one-shorthand-
   per-agent cleanup retired `showboat` before this agent existed. Shipped: `combo` is the
   sole shorthand, no alias.
5. **Strategy dispatch.** ~~register both functions in the `AIStrategyType` →
   `StrategySet` mapping (currently only Random is wired).~~ **Stale**: by
   implementation time `A1` was wired too; done with one line added to `ai_strategy.c`'s
   `STRATEGY_REGISTRY[]`.
6. **`strat_lib`.** ~~the combo-scoring helper (`CombatCard` construction +
   `calculate_combo_bonus()` wrapper) is shared AI heuristic logic; coordinate with
   `../G1 AI agent general info/strat_lib_refactor_handout.md` rather than duplicating it
   here.~~ **Stale**: `strat_lib` is scoped to *non-turn* heuristics (mulligan,
   discard-to-N, cash-exchange selection) — this combo-scoring helper is turn-strategy
   logic, so it went into `ai_strat_common.c/h` instead
   (`combo_bonus_for_selection()`), alongside `build_affordable_champions()`/
   `expected_incoming_attack()`/`try_play_draw_card()`, all reused as-is from `A1`.

---

## 8. Character — why the weaknesses are the point

Read together with §3: this agent's threshold-gated pruning and probabilistic decline are
not implementation shortcuts to eventually fix. They are what makes it **The Showboat**
rather than a second Borealis. Concretely:

- **Chases the spectacular, forgets to block** (names file, "Naming Rationale" §3) — it
  will pass up a strong single play to wait for a combo above threshold, and will
  sometimes decline a defence it should take.
- This is legible: a human opponent can learn "it likes big combos and skips blocks
  sometimes," which is a real, teachable pattern — appropriate for a rung below the
  benchmark, wrong for the benchmark itself (§3).

**Deliberately out of scope for this agent** (these belong to other rungs):

- exhaustive subset enumeration / a single monotone strength dial — that's `A3` Borealis
- resource targets tied to opponent energy — `A4` Balanced Rules
- game-phase or aggression modelling — `A6` Tactical
- advantage functions over energy/cards/cash — `A5` Heuristic
- any lookahead or simulation

---

## 9. Open questions

1. Combo weight `w` in the attack scoring function — calibration output, no prior.
2. Whether `power`-based mulligan/discard heuristics are correct as-is (§5.1) — review
   under `strat_lib`.
3. Whether the `aggression_level` wiring in §5.4 is the right scope, or whether it should
   also touch `min_hand_size_target` — defer until the agent exists and can be measured.

---

## 10. Documentation to update on completion

- `doc/oracle_design.md` §7 Strategy Framework — add Combo Threshold; update "Adding New
  Strategy" steps to include the dispatch table.
- `doc/oracle_design.md` Appendix code metrics — add new file sizes.
- `doc/oracle_todo.md` — check off Combo Threshold.
- `README.md` AI Research Focus — mark combo-threshold agent ✅.
- `doc/game_rules_doc.md` — no change required.
