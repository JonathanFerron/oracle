# Oracle — AI Agent Roadmap & Borealis Benchmark

**Status:** design decisions, not yet implemented
**Suggested location:** `ideas/ai agents deterministic/borealis_handout.md`
**Scheduling:** belongs to the AI strategies work item (A1–A4), after TUI Milestone 2.

---

## 1. Purpose

Defines the AI agent progression for Oracle, and specifies **Borealis**, the combo-aware agent that will serve as the fixed benchmark ("Keeper") for the Bradley-Terry rating system.

---

## 2. Decisions summary

| Item | Decision |
|---|---|
| Benchmark agent | **Borealis** — combo-aware, tunable |
| Benchmark AI is NOT | Random (too weak — see §3) |
| Borealis rating anchor | 50 by definition (BT strength = 1.0) |
| File naming | `strat_borealis.c/h` (follows `strat_*.c` convention; earlier working title `strat_combo.c` is **dropped**) |
| Development order | ValueBased → Borealis → HBT → HBT2Ply → SimpleMC → IS-MCTS |
| Rating estimates below | **Hypotheses to be measured, not targets to engineer toward** |

---

## 3. Why Borealis and not Random as benchmark

Playtest evidence (physical cards + `stda.cli`): children aged ~8 learn combo awareness within 10–15 minutes and then beat the Random AI **≥75%** of the time.

Consequence: with Random as benchmark, human ratings would cluster in the 70–95 band, wasting most of the 1–99 scale. A combo-aware benchmark should recentre the human distribution near 50 and spread it roughly 25–90.

This is a hypothesis based on one playtest observation. Validate against real human data (§8) before freezing the rating scale.

---

## 4. Agent roster and naming

| Strategy | Agent name | Character | Est. rating |
|---|---|---|---|
| `strat_random` | Random | baseline, no evaluation | 20–25 |
| `strat_valuebased` | *(unnamed)* | card-value greedy | 40–45 |
| `strat_borealis` | **Borealis** | combo-aware, tunable | **50 (anchor)** |
| `strat_hbt` | *(open)* | heuristic + balanced + tactical | 58–65 |
| `strat_hbt2ply` | *(open — shortlist below)* | HBT + opponent-response lookahead | 68–75 |
| `strat_simplemc` | *(open)* | progressive-pruning Monte Carlo | 75–82 |
| `strat_ismcts` | *(open)* | information-set MCTS | 85–92+ |

**All ratings above are untested predictions.** Search-based agents are expected to outrank static-evaluation agents given sufficient compute, and HBT2Ply is expected to sit between HBT and SimpleMC — but nothing here is measured.

### HBT2Ply naming shortlist (open)

- **Equinox** — pairs thematically with Borealis (both natural/astronomical phenomena); "balance" reads onto HBT's three-layer design.
- **Cassandra** — foresight; maps onto 2-ply lookahead.
- **Luminary** — ties into the "Five Lights of Arcadia" lore already in `game_rules_doc.md`.

No decision made. Recommend deferring until HBT2Ply actually exists.

---

## 5. Corrections to earlier design output

**Read this section before implementing.** The following issues exist in the draft `strat_borealis.c` sketch produced in chat and must not be carried forward.

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

Ranking candidate plays by `power` would make Borealis prefer a d4+0 over a d20+5. Any scoring model must use:

- **Combat value:** `expected_attack` (attacking) / `expected_defense` (defending), plus combo bonus.
- **Efficiency (`power`, `*_efficiency`):** only as a cash-constrained tiebreaker, never as primary strength.

**Related, out of scope but worth logging:** `discard_to_7_cards()` (`card_actions.c`) and `apply_mulligan()` (`stda_auto.c`) both discard the *lowest `power`* card, i.e. they preferentially throw away expensive high-strength champions and keep cheap weak ones. That may be intentional (cash-constrained games favour efficiency) or may be a long-standing bug. Review under the `strat_lib` work item, not here. `AVERAGE_POWER_FOR_MULLIGAN 4.98` sits near the cost-1/cost-2 efficiency band, which suggests the threshold was tuned to efficiency semantics deliberately.

### 5.2 `should_defend()` had an invalid cast

The draft contained `genRand(&((GameContext*)gstate)->rng)` — casting a `struct gamestate*` to `GameContext*`. Undefined behaviour. `should_defend()` must take `GameContext* ctx` and use `ctx->rng`, or use `RND_randn()` from `rnd.h`.

### 5.3 `find_best_attack_play()` was ~95 lines

Violates the ~35-line target. Decompose into:

```
static void eval_single_champions(...);
static void eval_two_card_combos(...);
static void eval_three_card_combos(...);
static void eval_cash_fallback(...);
```

Each takes the running `PlayCandidate* best` and updates it in place.

### 5.4 Other draft defects

- `aggression_level` declared in `BorealisParams` but never read. Either wire it in or remove it.
- `calculate_card_combo_potential()` declared and defined but never called. Remove or use.
- `borealis_get_default_params()` duplicated the static initializer — needs a single source of truth.
- `estimate_attack_strength()` summed `attack_base + expected_defense`; that is exactly `expected_attack`. Use the precomputed field.
- Brace style in the chat samples was K&R. **The repo uses run-in braces, 2-space indent, type-aligned pointers (`int* p`), formatted via astyle.** Match the repo, not the samples.

---

## 6. Borealis specification

### 6.1 Files

```
src/strat_borealis.h    ~80 lines
src/strat_borealis.c    target ≤400 lines
```

If the `.c` exceeds ~400 lines after decomposition, split enumeration/scoring into `strat_borealis_eval.c` rather than letting the file grow.

### 6.2 Public API

```c
void borealis_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void borealis_defense_strategy(struct gamestate* gstate, GameContext* ctx);

void           borealis_set_params(const BorealisParams* params);
BorealisParams borealis_get_default_params(void);
```

Signatures must match `AttackStrategyFunc` / `DefenseStrategyFunc` in `strategy.h`.

### 6.3 Tunable parameters

```c
typedef struct
{ float aggression_level;           // 1.0-3.0, target champions per attack
  int   combo_bonus_threshold;      // 7-12, min 2-card bonus to pursue
  int   combo3_bonus_threshold;     // 14+, min 3-card bonus to pursue
  float defend_probability_base;    // 0.40-0.70
  int   defend_damage_threshold;    // 6-10, ignore attacks below this
  int   min_hand_size_target;       // 3-5, trigger for draw cards
  bool  save_big_combos_for_lethal; // hold +16 combos while opp energy high
} BorealisParams;
```

Defaults: `1.8 / 10 / 14 / 0.55 / 8 / 4 / true`. Starting guesses for calibration, not tuned values.

### 6.4 Attack algorithm

1. If `hand_size < min_hand_size_target` and opponent energy > 20 and an affordable draw card is held → play it, return.
2. Enumerate candidate plays subject to `total_cost <= cash`:
   - all single champions
   - all champion pairs where `combo_bonus >= combo_bonus_threshold`
   - all champion triples where `combo_bonus >= combo3_bonus_threshold`
3. Score: `Σ expected_attack + w · combo_bonus` (start `w = 1.0`; `w` is a calibration knob).
4. If `save_big_combos_for_lethal` and the triple's bonus ≥ 16 and opponent energy > 25 → skip that candidate.
5. If best score is below a floor and a cash card + a champion are both held → play the cash card.
6. Otherwise play the best candidate, or pass.

Bound the triple loop (hand can reach 15 cards → 455 triples; acceptable, but check the profile once `stda.auto` runs 1000+ sims).

### 6.5 Defense algorithm

1. `expected_attack = Σ expected_attack` over cards in the attacker's combat zone, plus the combo bonus. The attacker's combat zone is fully visible, so call `calculate_combo_bonus()` directly rather than estimating — **do this; the draft's +7/+10 estimate was needless.**
2. If `expected_attack < defend_damage_threshold` → decline.
3. Compute defend probability from `defend_probability_base`, adjusted: +0.15 if own energy < 20, +0.08 if < 40; −0.20 if opponent energy < 15, −0.10 if < 30. Roll against it using `ctx->rng`.
4. If defending, pick the play maximising `(Σ expected_defense + combo_bonus) / (cost + 1)`.
5. Only consider 2-card defensive combos when `expected_attack >= 15`.

### 6.6 Determinism

Borealis is stochastic (the defend roll). This is deliberate — a fully deterministic benchmark is memorisable and exploitable by repeat human opponents. **All randomness must go through `ctx->rng`** so runs stay reproducible under a fixed `--prng.seed`. No `rand()`, no static RNG state.

Open question: whether benchmark variance is acceptable for BT anchoring, or whether the defend decision should become a deterministic threshold. Resolve during calibration by measuring mirror-match variance across seeds.

---

## 7. Integration points

### 7.1 `player_config.h`

Add to `AIStrategyType`, keeping `AI_STRATEGY_RANDOM = 0` and `AI_STRATEGY_COUNT` last:

```c
AI_STRATEGY_RANDOM = 0,
AI_STRATEGY_VALUEBASED,
AI_STRATEGY_BOREALIS,
AI_STRATEGY_HBT,
AI_STRATEGY_HBT2PLY,
AI_STRATEGY_SIMPLE_MC,
AI_STRATEGY_ISMCTS,
AI_STRATEGY_COUNT
```

This **changes the existing enum ordering** (`AI_STRATEGY_BALANCED`, `AI_STRATEGY_HEURISTIC`, `AI_STRATEGY_HYBRID` are being renamed/reordered). Check every use site, including `get_strategy_display_name()`.

### 7.2 `player_config.c`

- `display_ai_strategy_menu()` — add Borealis, mark it available; localise EN/FR/ES.
- `get_ai_strategy_choice()` — currently forces `AI_STRATEGY_RANDOM` for any `choice > 1`. That gate must become a per-strategy availability check, not a hardcoded `> 1`.
- `get_strategy_display_name()` — add the Borealis case.

### 7.3 Strategy dispatch — prerequisite

There is currently **no mapping** from `AIStrategyType` to function pointers; `stda_auto.c` and `stda_cli.c` both hardcode `random_attack_strategy` / `random_defense_strategy`. Adding a second usable strategy requires a lookup, e.g. in `strategy.c`:

```c
void apply_strategy_by_type(StrategySet* set, PlayerID p, AIStrategyType t);
```

Without this, Borealis cannot be selected at all.

### 7.4 `strat_lib`

The combo-scoring helper (`CombatCard` construction + `calculate_combo_bonus()` wrapper) is shared AI heuristic logic and belongs in **`strat_lib.h`**, not in `strat_borealis.c`. Coordinate with the `strat_lib` work item; if it doesn't exist yet, create it there rather than duplicating later.

Borealis will inherit `discard_to_7_cards()` and `apply_mulligan()` behaviour, currently Random AI's heuristic hardcoded into `card_actions.c` / `stda_auto.c`. Known, accepted shortcut pending the `StrategySet` function-pointer extension. **Do not extend `StrategySet` as part of this work item.**

---

## 8. Calibration protocol

### Stage 1 — AI-vs-AI sanity checks (`stda.auto`, 1000 sims each)

| Matchup | Expectation | Failure means |
|---|---|---|
| Borealis vs Borealis | 50% ± 3% | asymmetry bug or first-player advantage leak |
| Borealis vs Random | 75–85% | combo logic not firing / scoring bug |
| Borealis vs ValueBased | 60–70% | combo awareness adds nothing |
| Borealis mirror, 10 seeds | low inter-seed variance | benchmark too noisy for BT anchoring |

### Stage 2 — human calibration

Recruit across age bands, ~10 games each vs Borealis in `stda.cli`:

- ages 8–10
- ages 11–14
- ages 15+

**Target: the 11–14 band averages 45–55% win rate.**

### Stage 3 — parameter adjustment

Humans winning too often (>65%) → strengthen: `aggression_level` 1.8→2.2, `combo_bonus_threshold` 10→7, `defend_probability_base` 0.55→0.60, `min_hand_size_target` 4→5.

Humans winning too rarely (<35%) → weaken: `aggression_level` 1.8→1.5, `combo_bonus_threshold` 10→12, `defend_probability_base` 0.55→0.50.

### Stage 4 — freeze

Once calibrated, lock `BorealisParams` and treat changes as a **rating-scale-breaking event**. Any later change requires rescaling all stored ratings by the strength ratio, or a full batch recompute.

---

## 9. Rating system decisions

- Borealis registers as the Keeper, BT strength fixed at 1.0, displayed rating 50.
- Rating = win probability vs Borealis as a percentage, clamped 1–99.
- Assert at startup: `keeper_id` resolves to Borealis and `rating == 50`.
- Other agents rate against Borealis, giving a stable ladder for measuring AI progress across versions (e.g. "HBT v1.2 → 61, v1.3 → 64").
- AI benchmarking match protocol: play in **pairs** so each agent takes the first-player position once, neutralising first-player advantage.

Rating system implementation itself lives in the existing `ideas/rating system/` work item and is out of scope here.

---

## 10. Open questions

1. HBT2Ply agent name — Equinox / Cassandra / Luminary, undecided.
2. Whether Borealis's defend roll stays stochastic or becomes a deterministic threshold (§6.6).
3. Whether `power`-based mulligan/discard heuristics are correct as-is (§5.1) — review under `strat_lib`.
4. Combo weight `w` in the attack scoring function — calibration output, no prior.
5. Whether `strat_valuebased` is worth building as a separate agent, or whether it collapses into an under-tuned Borealis.

---

## 11. Documentation to update on completion

- `doc/oracle_design.md` §7 Strategy Framework — add Borealis; update "Adding New Strategy" steps to include the dispatch table.
- `doc/oracle_design.md` Appendix code metrics — add new file sizes.
- `doc/oracle_todo.md` — check off Borealis; add HBT as next AI item.
- `README.md` AI Research Focus — mark combo-aware ✅, note Borealis as the rating benchmark.
- `doc/game_rules_doc.md` — no change required.
