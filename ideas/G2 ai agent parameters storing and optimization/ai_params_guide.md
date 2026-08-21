# Oracle AI Parameter Management System

## Overview

This system provides a comprehensive framework for managing, loading, calibrating, and optimizing AI strategy parameters across all deterministic and stochastic strategies.

---

## Key Components

### 1. **ai_params.h/c** - Parameter Structures
- Defines parameter structs for each AI strategy type
- Provides default values via macros
- Handles loading/saving from/to INI files
- Generic parameter access by name

### 2. **ai.ini** - Configuration File
- INI-style format with one section per strategy
- Comments supported with `#` or `;`
- Boolean values: `true`/`false`, `yes`/`no`, `1`/`0`
- Easily human-editable

### 3. **ai_calibration.h** - Optimization Framework
- Multiple optimization methods (grid search, random, genetic)
- Parameter range specification
- Evaluation with confidence intervals
- CSV export for analysis

### 4. **calibration_example.c** - Usage Examples
- Five complete examples showing different use cases
- Integration guide for existing code
- Best practices for parameter tuning

---

## Parameter Naming Convention

**Format**: `{category}_{subcategory}_{parameter_name}`

### Categories

| Prefix | Purpose | Range | Example |
|--------|---------|-------|---------|
| `behavior_*` | Probability/threshold | 0.0-1.0 | `behavior_defend_prob` |
| `target_*` | Resource target formulas | varies | `target_cash_slope` |
| `weight_*` | Advantage function weights | 0.0-5.0 | `weight_energy_advantage` |
| `threshold_*` | Decision thresholds | varies | `threshold_mulligan_power` |
| `limit_*` | Hard limits (counts) | integer | `limit_max_simulations` |
| `rollout_*` | MC simulation params | varies | `rollout_random_depth` |
| `search_*` | Tree search params | varies | `search_exploration_constant` |
| `prior_*` | Prior probabilities (MCTS) | 0.0-1.0 | `prior_use_heuristic` |

### Naming Examples

**Good**:
- `behavior_defend_prob` - Clear: probability of defending
- `target_cash_slope` - Clear: slope in cash target formula
- `weight_energy_advantage` - Clear: weight for energy delta
- `threshold_mulligan_power` - Clear: power threshold for mulligan
- `limit_max_simulations` - Clear: max simulation count
- `rollout_random_depth` - Clear: depth before random rollout

**Avoid**:
- `defend_rate` - Missing category
- `cash_target` - Ambiguous (target amount? formula?)
- `energy_weight` - Missing context (weight for what?)
- `mulligan_threshold` - What kind of threshold?

---

## Strategy-Specific Parameters

### Random Strategy
```c
typedef struct {
    float behavior_defend_prob;           // 0.47 default
    float threshold_mulligan_power;       // 4.98 default
    bool enabled;
    char description[64];
} RandomParams;
```

**Tunable Parameters**:
- `behavior_defend_prob`: Probability of defending when attacked
  - Range: 0.0 (never defend) to 1.0 (always defend)
  - Default: 0.47 (balanced)
  - Typical optimal: 0.40-0.55 depending on opponent

### Balanced Rules Strategy
```c
typedef struct {
    // Resource formulas: target = slope * (opp_energy - 8) + intercept
    float target_cash_slope;              // 0.2088 (19/91)
    float target_cash_intercept;          // 8.0
    float target_cards_slope;             // 0.0549 (5/91)
    float target_cards_intercept;         // 3.0
    float threshold_defense_beta;         // 1.0 (variance multiplier)
    float behavior_late_game_aggro;       // 1.2
    float threshold_mulligan_power;       // 4.98
    bool enabled;
    char description[64];
} BalancedRulesParams;
```

**Tunable Parameters**:
- `target_cash_slope`: Controls how cash reserves scale with opponent energy
- `threshold_defense_beta`: How conservative to be on defense (lower = more aggressive)
- `behavior_late_game_aggro`: Aggression multiplier when opponent low energy

### Heuristic Strategy
```c
typedef struct {
    // Advantage = ε*Energy + γ*Cards + δ*Cash
    float weight_energy_advantage;        // 1.0 (epsilon)
    float weight_cards_advantage;         // 0.15 (gamma)
    float weight_cash_advantage;          // 1.0 (delta)
    
    // Dynamic adjustments
    float weight_energy_critical_mult;    // 1.5
    float weight_cards_decay_rate;        // 0.01
    float weight_cash_decay_rate;         // 0.01
    
    float threshold_mulligan_power;       // 4.98
    bool enabled;
    char description[64];
} HeuristicParams;
```

**Tunable Parameters**:
- `weight_energy_advantage` (ε): Importance of energy delta
- `weight_cards_advantage` (γ): Importance of card advantage
- `weight_cash_advantage` (δ): Importance of cash advantage
- Key question: What is optimal (ε, γ, δ) vs different opponents?

### HBT Hybrid Strategy
```c
typedef struct {
    // Combines Balanced + Heuristic + Tactical
    
    // From Balanced
    float target_cash_slope;              // 0.2088
    float target_cash_intercept;          // 8.0
    float target_cards_slope;             // 0.0549
    float target_cards_intercept;         // 3.0
    float threshold_defense_beta;         // 1.0
    
    // From Heuristic
    float weight_energy_advantage;        // 1.0
    float weight_cards_advantage;         // 0.15
    float weight_cash_advantage;          // 1.0
    
    // From Tactical
    float behavior_aggression_base;       // 0.5
    float behavior_aggression_energy;     // 0.003
    float behavior_aggression_low_opp;    // 0.35
    float behavior_aggression_low_self;   // -0.4
    
    // Hybrid-specific
    float weight_energy_critical_mult;    // 1.5
    float threshold_combo_min_bonus;      // 5
    float threshold_mulligan_power;       // 4.98
    
    bool enabled;
    char description[64];
} HBTHybridParams;
```

**Most Important to Tune**:
1. `behavior_aggression_*` - Controls adaptive aggression
2. `weight_*_advantage` - Affects move evaluation
3. `threshold_defense_beta` - Defense strategy

### Simple Monte Carlo
```c
typedef struct {
    uint16_t limit_max_simulations;       // 100
    uint8_t rollout_random_depth;         // 5
    bool rollout_use_heuristic;           // false
    float threshold_combo_min_bonus;      // 5.0
    uint8_t limit_max_moves_to_eval;      // 30
    float threshold_mulligan_power;       // 4.98
    bool enabled;
    char description[64];
} SimpleMCParams;
```

**Key Trade-offs**:
- `limit_max_simulations`: Higher = better but slower
- `rollout_use_heuristic`: Better rollouts but more expensive

### Progressive Monte Carlo
```c
typedef struct {
    // Staged pruning
    uint16_t limit_stage1_simulations;    // 100
    uint16_t limit_stage2_simulations;    // 200
    uint16_t limit_stage3_simulations;    // 400
    uint16_t limit_stage4_simulations;    // 800
    
    float threshold_stage1_keep_ratio;    // 0.75 (N^0.75)
    float threshold_stage2_keep_ratio;    // 0.50 (N^0.50)
    float threshold_stage3_keep_ratio;    // 0.25 (N^0.25)
    
    float threshold_confidence_level;     // 1.96 (95% CI)
    uint8_t rollout_random_depth;         // 5
    bool rollout_use_heuristic;           // false
    float threshold_mulligan_power;       // 4.98
    bool enabled;
    char description[64];
} ProgressiveMCParams;
```

**Tuning Strategy**:
- Balance total simulations vs pruning aggressiveness
- More aggressive pruning = faster but might miss good moves

### Information Set MCTS
```c
typedef struct {
    uint16_t limit_max_iterations;        // 1000
    uint16_t limit_max_time_ms;           // 5000
    
    float search_exploration_constant;    // 1.414 (√2)
    bool search_use_puct;                 // true
    
    // Prior estimation
    bool prior_use_heuristic;             // true
    float weight_energy_advantage;        // 1.0
    float weight_cards_advantage;         // 0.15
    float weight_cash_advantage;          // 1.0
    
    uint8_t rollout_random_depth;         // 5
    bool rollout_use_heuristic;           // true
    
    bool search_reuse_tree;               // true
    uint16_t limit_max_tree_nodes;        // 10000
    uint8_t limit_determinizations;       // 1
    
    float threshold_mulligan_power;       // 4.98
    bool enabled;
    char description[64];
} ISMCTSParams;
```

**Critical Parameters**:
- `search_exploration_constant`: Balance exploration vs exploitation
- `prior_use_heuristic`: Use heuristic for better priors
- `limit_determinizations`: More = better but slower

---

## Usage Examples

### Example 1: Load and Use Parameters

```c
// In your strategy initialization
AIParams ai_params;
ai_params_init_default(&ai_params, STRAT_HBT_HYBRID);
ai_params_load_from_file(&ai_params, "ai.ini");

// In your strategy function
void hbt_attack_strategy(struct gamestate* gstate, 
                        GameContext* ctx,
                        AIParams* params)
{
    // Access parameters
    float epsilon = params->params.hbt.weight_energy_advantage;
    float gamma = params->params.hbt.weight_cards_advantage;
    float aggression = params->params.hbt.behavior_aggression_base;
    
    // Use in calculations
    float advantage = epsilon * energy_delta + 
                     gamma * cards_delta + 
                     cash_delta;
    
    // Adjust for aggression
    float threshold = base_threshold * aggression;
    // ...
}
```

### Example 2: Run Calibration

```c
// Optimize HBT Hybrid vs Random
CalibrationConfig config = {
    .method = OPT_GENETIC_ALGORITHM,
    .ga_population_size = 50,
    .ga_generations = 20,
    .games_per_evaluation = 100,
    .opponent_type = STRAT_RANDOM,
    .verbose = true
};

ai_params_init_default(&config.opponent_params, STRAT_RANDOM);

ParamRanges* ranges = calibration_create_default_ranges(STRAT_HBT_HYBRID);
calibration_add_float_param(ranges, "weight_energy_advantage",
                           0.5f, 2.0f, 0.1f, false);
calibration_add_float_param(ranges, "behavior_aggression_base",
                           0.2f, 0.8f, 0.05f, false);

CalibrationSession* session = calibration_create_session(&config, ranges);
calibration_run(session);

// Best parameters saved to hbt_best_params.ini
calibration_save_best(session, "hbt_best_params.ini");
```

---

## Migration Guide

### Step 1: Move Constants to ai_params.h

**Before** (in various files):
```c
#define AVERAGE_POWER_FOR_MULLIGAN 4.98
#define RANDOM_DEFEND_PROB 0.47
```

**After** (in ai_params.h):
```c
// Included in each strategy's param struct
float threshold_mulligan_power;  // Default: 4.98
float behavior_defend_prob;      // Default: 0.47
```

### Step 2: Update Strategy Function Signatures

**Before**:
```c
void random_attack_strategy(struct gamestate* gstate, GameContext* ctx);
```

**After**:
```c
void random_attack_strategy(struct gamestate* gstate, 
                           GameContext* ctx,
                           AIParams* params);
```

### Step 3: Update StrategySet

**Before**:
```c
typedef struct {
    AttackStrategyFunc attack_strategy[2];
    DefenseStrategyFunc defense_strategy[2];
} StrategySet;
```

**After**:
```c
typedef struct {
    AttackStrategyFunc attack_strategy[2];
    DefenseStrategyFunc defense_strategy[2];
    AIParams ai_params[2];  // Add this
} StrategySet;
```

### Step 4: Update Initialization

**Before**:
```c
StrategySet* strategies = create_strategy_set();
set_player_strategy(strategies, PLAYER_A,
                   random_attack_strategy, random_defense_strategy);
```

**After**:
```c
StrategySet* strategies = create_strategy_set();

// Load parameters
AIParams params_a, params_b;
ai_params_init_default(&params_a, STRAT_RANDOM);
ai_params_init_default(&params_b, STRAT_HBT_HYBRID);
ai_params_load_from_file(&params_a, "ai.ini");
ai_params_load_from_file(&params_b, "ai.ini");

set_player_strategy(strategies, PLAYER_A,
                   random_attack_strategy, random_defense_strategy,
                   &params_a);
set_player_strategy(strategies, PLAYER_B,
                   hbt_attack_strategy, hbt_defense_strategy,
                   &params_b);
```

---

## Best Practices

### 1. Always Use Default Values
- Initialize with `ai_params_init_default()` before loading from file
- This ensures all parameters have sensible values even if file is missing

### 2. Validate After Loading
- Call `ai_params_validate()` after loading to catch invalid ranges
- Example: `behavior_defend_prob` must be 0.0-1.0

### 3. Start with Coarse Grid, Then Refine
- First pass: Wide range, large step size (e.g., 0.0-1.0 by 0.2)
- Second pass: Narrow range around best, small step (e.g., 0.4-0.6 by 0.05)

### 4. Use Appropriate Opponent
- Optimize against the strategy you expect to face most often
- For general-purpose AI, optimize against itself or a strong baseline

### 5. Run Multiple Seeds
- Parameter effectiveness can vary with RNG seed
- Run calibration with 3-5 different seeds, average results

### 6. Document Optimal Values
- Save winning parameter sets with descriptive names
- Example: `hbt_vs_random_optimal.ini`, `hbt_vs_balanced_optimal.ini`

---

## Calibration Workflow

```
1. Define what to optimize
   ↓
2. Choose optimization method
   ↓
3. Set parameter ranges
   ↓
4. Run calibration (100-1000 games each)
   ↓
5. Analyze results (CSV export)
   ↓
6. Update ai.ini with best parameters
   ↓
7. Verify improvement with more games
   ↓
8. Repeat for other parameters
```

---

## FAQ

**Q: How many games per evaluation?**
A: 100 is good for quick iteration, 1000+ for final tuning. More games = better confidence but slower.

**Q: Which optimization method to use?**
A: 
- Grid Search: 1-3 parameters, want exhaustive
- Random Search: 3-5 parameters, faster than grid
- Genetic: 5+ parameters, complex interactions

**Q: Should all strategies have same mulligan threshold?**
A: No! Optimal mulligan threshold varies by strategy. An aggressive strategy might mulligan less (keep more cards), while a careful strategy mulligan more.

**Q: Can I optimize multiple strategies at once?**
A: Yes, but run separate calibration sessions. Each strategy should be optimized against its expected opponent.

**Q: How do I know if calibration found a real improvement?**
A: Check confidence intervals. If 95% CIs of old vs new don't overlap, improvement is statistically significant.

---

## Future Enhancements

1. **Multi-objective optimization**: Balance win rate vs game length
2. **Online learning**: Adjust parameters during play
3. **Opponent modeling**: Adapt parameters based on detected opponent strategy
4. **Parameter schedules**: Change parameters based on game phase (early/mid/late)
5. **Ensemble strategies**: Combine multiple parameter sets

---

## Files Summary

| File | Purpose | Lines |
|------|---------|-------|
| `ai_params.h` | Parameter structures | ~400 |
| `ai_params.c` | Load/save implementation | ~300 |
| `ai_calibration.h` | Optimization framework | ~200 |
| `ai_calibration.c` | Optimization implementation | ~500 |
| `calibration_example.c` | Usage examples | ~350 |
| `ai.ini` | Default configuration | ~200 |

**Total**: ~1950 lines (all within coding guidelines)

---

## Conclusion

This parameter system provides:
- ✅ Unified parameter management across all AI types
- ✅ Easy configuration via INI files
- ✅ Robust optimization framework
- ✅ Consistent naming conventions
- ✅ Complete integration guide
- ✅ Ready for both deterministic and stochastic strategies

The system scales from simple parameter tweaking to full genetic algorithm optimization, making it suitable for both quick experiments and rigorous AI development.
