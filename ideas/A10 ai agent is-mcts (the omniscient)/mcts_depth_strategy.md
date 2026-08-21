# MCTS Search Depth Strategy for Oracle AI

**Status:** Idea / design notes for future AI strategy implementation
**Related:** `strat_simplemc1.c`, `strat_ismcts1.c`, future MCTS strategies
**Context:** Discussion on how deep MCTS/Monte Carlo search should go given Oracle's chance elements and branching factor

---

## Problem Statement

Oracle has a variable-depth decision tree with two competing pressures on search depth:

- **Shallow search** (1–2 ply) gives narrower statistical confidence intervals on the value of each action, since fewer stochastic events compound.
- **High branching factor** — especially from cash cards, and the draw/recall mechanics of draw/recall cards (including the recall option itself, which can offer a long list of champions to pull from discard) — makes exhaustive deep search expensive.

The naive assumption was that combat dice rolls require Monte Carlo sampling to estimate, which would force shallow searches to keep variance/cost manageable.

---

## Key Insight: Dice Rolls Don't Need Sampling

Dice rolls are a **discrete uniform distribution** with a known closed-form mean and variance. There's no need to simulate them — they can be computed exactly:

```c
typedef struct {
    double expected_value;
    double variance;
} DiceStats;

DiceStats get_dice_stats(uint8_t dice_type) {
    DiceStats stats;
    switch (dice_type) {
        case 4:  stats.expected_value = 2.5;  stats.variance = 1.25;   break; // (n²-1)/12
        case 6:  stats.expected_value = 3.5;  stats.variance = 2.917;  break;
        case 8:  stats.expected_value = 4.5;  stats.variance = 5.25;   break;
        case 12: stats.expected_value = 6.5;  stats.variance = 11.917; break;
        case 20: stats.expected_value = 10.5; stats.variance = 33.25;  break;
    }
    return stats;
}
```

Expected combat damage (attack − defense, plus combo bonuses) can be computed directly rather than sampled:

```c
double expected_damage(CombatState* combat) {
    double exp_attack = 0, var_attack = 0;
    for (int i = 0; i < combat->num_attackers; i++) {
        DiceStats ds = get_dice_stats(combat->attackers[i].dice);
        exp_attack += combat->attackers[i].base + ds.expected_value;
        var_attack += ds.variance;
    }
    exp_attack += combat->attack_combo_bonus;

    double exp_defense = 0, var_defense = 0;
    for (int i = 0; i < combat->num_defenders; i++) {
        DiceStats ds = get_dice_stats(combat->defenders[i].dice);
        exp_defense += ds.expected_value;
        var_defense += ds.variance;
    }
    exp_defense += combat->defense_combo_bonus;

    return exp_attack - exp_defense; // ignoring truncation at 0 for now
}
```

**Note:** truncation at 0 damage (P(attack < defense)) is a nonlinearity that the pure expected-value calc glosses over. Worth flagging as a refinement — either a normal approximation correction or an exact discrete convolution if precision matters near breakpoints.

### What's actually deterministic vs. stochastic

| Deterministic (exact calc)      | Stochastic (needs sampling)                  |
|----------------------------------|-----------------------------------------------|
| Dice roll mean/variance          | Card draws (unknown post-shuffle deck order)  |
| Combo bonuses                    | Opponent actions (hidden info / strategy)     |
| Luna costs                       | Damage truncation at 0 (boundary effect)      |
| Energy changes given a damage value | Recall choices (which champions are available in discard) |

This reclassification is the crux of the whole strategy: **combat itself is not where the real uncertainty lives.** Real uncertainty is in draws, opponent hidden information, and recall availability.

---

## Revised Approach: Expectimax over Monte Carlo Rollout

Since combat is deterministic, use **expectimax** rather than brute-force rollout sampling: deterministic max/min nodes for player choices, stochastic expectation nodes only at draw/recall points.

```c
double expectimax(GameState* state, int depth, bool is_max_player) {
    if (depth == 0 || is_terminal(state)) {
        return evaluate_heuristic(state);
    }

    if (state->phase == DRAW_PHASE) {
        // Stochastic node: sample possible draws
        double exp_value = 0;
        int num_samples = 10;
        for (int i = 0; i < num_samples; i++) {
            GameState sim = clone_state(state);
            simulate_draw(&sim);
            exp_value += expectimax(&sim, depth, is_max_player);
        }
        return exp_value / num_samples;
    }

    // Deterministic node: exact combat math, no sampling
    Action* actions = get_all_actions(state);
    double best = is_max_player ? -INFINITY : INFINITY;

    for (int i = 0; i < actions->count; i++) {
        GameState next = apply_deterministic(state, actions->list[i]);
        double value = expectimax(&next, depth - 1, !is_max_player);
        best = is_max_player ? max(best, value) : min(best, value);
    }
    return best;
}
```

### Dominance pruning

Exact expected values also enable cheap dominance pruning before/during search — no sampling needed to compare two actions:

```c
bool is_dominated_action(GameState* state, Action a1, Action a2) {
    double exp_dmg_1 = expected_damage_if_play(state, a1);
    double exp_dmg_2 = expected_damage_if_play(state, a2);
    double cost_1 = get_cost(a1);
    double cost_2 = get_cost(a2);

    // a1 dominated if strictly worse damage and not cheaper
    return (exp_dmg_1 < exp_dmg_2 && cost_1 >= cost_2);
}
```

This is the main lever against the high branching factor from cash cards / recall options: prune dominated actions deterministically instead of paying sampling cost to discover they're bad.

---

## Depth Recommendations by Strategy Tier

| Strategy | Tree depth | Rollout depth | Total plies | Sampling |
|---|---|---|---|---|
| **Heuristic AI** (deterministic only) | — | — | 2–4 | None — pure exact eval |
| **Simple MC** (`strat_simplemc1.c`) | — | 10–12 | 10–12 | 20–30 samples at draw nodes only |
| **MCTS / IS-MCTS** (`strat_ismcts1.c`) | 6–8 | 6–8 | 12–16 | 10 determinizations, samples only at draw/recall nodes |

Rough ply-to-turn mapping: 1 turn cycle ≈ 2 plies (one action per player), so 12 plies ≈ 6 turn cycles.

### Why deeper is now affordable

**Before** (naive full simulation, sampling combat too):
- 50 actions × 100 samples × 6 plies ≈ 30,000 game states evaluated
- Budget mostly spent re-deriving dice statistics that were already known in closed form

**After** (deterministic combat, sample only true uncertainty):
- 50 actions → pruned to ~15 by dominance → 10 samples (draws) × 12 plies ≈ 1,800 game states
- **~16x fewer states evaluated, for a ~2x deeper search**

This reverses the original conclusion (favor shallow/narrow search) — once dice are handled exactly, the branching factor becomes the binding constraint rather than dice variance, and dominance pruning directly attacks branching factor. Depth can increase because sampling budget is no longer wasted re-discovering dice statistics.

---

## Where Real Sampling Budget Should Go

1. **Card draws** — deck composition unknown after shuffle; genuinely needs sampling (10–30 draws depending on tier).
2. **Opponent hidden information** — determinization (IS-MCTS style), sample plausible opponent hands/strategies.
3. **Recall choices** — which champions are available in discard is state-dependent and can be large; may need its own pruning/sampling treatment rather than full enumeration.
4. **Damage truncation boundary** — only matters when expected attack ≈ expected defense; could special-case a correction near this boundary rather than sampling broadly.

---

## Open Questions / Follow-ups for Implementation

- [ ] Formalize the damage-truncation correction (normal approximation vs. exact discrete convolution) for near-boundary combat.
- [ ] Decide how recall-option branching gets pruned/sampled specifically (it was flagged as the worst-case branching factor contributor but not fully modeled above).
- [ ] Define the heuristic evaluation function used at leaf/cutoff nodes (energy_adv, luna_adv, hand_adv weights above are placeholders from discussion, not tuned).
- [ ] Benchmark actual game-state throughput on target hardware to calibrate real depth/sample budgets rather than the illustrative numbers above.
- [ ] Determine per-tier time budget target (e.g., Simple MC interactive vs. MCTS potentially offline/background).
- [ ] Validate against `StrategySet` deferral note — this design assumes mulligan/discard/exchange heuristics are still hardcoded Random-AI-style per current `strat_lib.c` scope; revisit once `StrategySet` extension lands.

---

## Testing Protocol (for whenever this is implemented)

Measure, per strategy tier:
1. Win rate vs. baseline (Random AI, Balanced AI)
2. Decision time per move
3. Effective branching factor at each depth (pre/post dominance pruning)
4. Variance in evaluation outcomes (should shrink as depth/samples increase)

Illustrative targets discussed (unvalidated, revisit once real benchmarking is possible):
- Simple MC: 60–70% win rate vs. Random
- MCTS: 70–80% win rate vs. Random, 55–60% vs. Simple MC
- Decision time: <5s per move for interactive tiers
