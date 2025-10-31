AI Idea 2

Excellent question! Let me analyze the three strategies comparatively:

## Strategic Comparison

### 1. **Tactical Strategy vs Balanced Rules 1**

**Balanced Rules 1 Strengths:**
- Clean mathematical formulas for resource targets
- Explicit target cash: `(Enemy_Energy - 8) * 19/91 + 8`
- Explicit target cards: `(Enemy_Energy - 8) * 5/91 + 3`
- Defense uses variance-based decision: `E[Defense] <= E[Attack] - β*SD[Attack]`
- Simple, predictable behavior

**Tactical Strategy Advantages:**
- **More adaptive aggression** - uses multiple factors (energy diff, hand power, cash surplus) vs single opponent energy metric
- **Better combo evaluation** - considers combos before committing to number of cards
- **Opportunistic play** - can be more aggressive when detecting weakness
- **Defense value optimization** - weighs cost vs damage reduction, not just variance

**Winner: Probably a tie or slight edge to Tactical**

The Balanced Rules approach is more principled and systematic, which could be stronger in average cases. However, Tactical's adaptability might win in extreme situations (huge leads, desperate positions).

**Suggested Hybrid:** Combine them!

```c
// strat_balanced_tactical.c
void balanced_tactical_attack_strategy(struct gamestate* gstate, GameContext* ctx) {
    PlayerID attacker = gstate->current_player;
    PlayerID opponent = 1 - attacker;
    
    // 1. BALANCED RULES: Calculate target resources
    int opp_energy = gstate->current_energy[opponent];
    float target_cash = (opp_energy - 8) * 19.0 / 91.0 + 8.0;
    float target_cards = (opp_energy - 8) * 5.0 / 91.0 + 3.0;
    
    // 2. TACTICAL: Assess strategic state
    StrategicState state = evaluate_strategic_state(gstate, attacker);
    
    // 3. HYBRID: Adjust targets based on aggression factor
    target_cash *= (1.0 + (state.aggression_factor - 0.5) * 0.4);
    target_cards *= (1.0 + (state.aggression_factor - 0.5) * 0.3);
    
    // 4. Calculate spending budget
    int current_cash = gstate->current_cash_balance[attacker];
    int current_cards = calculate_effective_cards(gstate, attacker);
    
    int cash_to_spend = oraclemax(0, current_cash - (int)target_cash);
    int cards_to_play = oraclemax(0, current_cards - (int)target_cards);
    
    // 5. Apply Balanced Rules priorities with Tactical selection
    
    // Priority 1: Play 0-cost champions (free efficiency)
    if(play_zero_cost_champions(gstate, attacker, &cash_to_spend, &cards_to_play, ctx))
        return;
    
    // Priority 2: Hand building (when hand is small and not in critical phase)
    if(state.my_phase != PHASE_CRITICAL && 
       gstate->hand[attacker].size < 7) {
        if(should_play_draw_card_balanced(&state, gstate, attacker, 
                                          current_cash, target_cash)) {
            uint8_t draw_card = select_best_draw_card(gstate, attacker);
            if(draw_card != 255) {
                play_draw_card(gstate, attacker, draw_card, ctx);
                return;
            }
        }
    }
    
    // Priority 3: Champion attacks with combo awareness
    int num_attackers = oraclemin(3, oraclemin(cards_to_play, 
                                   estimate_affordable_champions(gstate, attacker, cash_to_spend)));
    
    if(num_attackers > 0) {
        uint8_t attackers[3];
        select_best_attackers_with_budget(gstate, attacker, num_attackers, 
                                          attackers, cash_to_spend, &state);
        
        for(int i = 0; i < num_attackers; i++)
            play_champion(gstate, attacker, attackers[i], ctx);
    }
}

void balanced_tactical_defense_strategy(struct gamestate* gstate, GameContext* ctx) {
    PlayerID defender = 1 - gstate->current_player;
    PlayerID attacker = gstate->current_player;
    
    // BALANCED RULES: Calculate attack statistics
    float expected_attack = 0.0;
    float attack_variance = 0.0;
    
    struct LLNode* current = gstate->combat_zone[attacker].head;
    for(uint8_t i = 0; i < gstate->combat_zone[attacker].size; i++) {
        uint8_t card_idx = current->data;
        expected_attack += fullDeck[card_idx].expected_attack;
        
        // V[Dn] = (n² - 1) / 12
        uint8_t n = fullDeck[card_idx].defense_dice;
        attack_variance += (n * n - 1) / 12.0;
        
        current = current->next;
    }
    
    // Add combo bonus to expected attack
    CombatCard combat_cards[3];
    current = gstate->combat_zone[attacker].head;
    for(uint8_t i = 0; i < gstate->combat_zone[attacker].size; i++) {
        combat_cards[i].species = fullDeck[current->data].species;
        combat_cards[i].color = fullDeck[current->data].color;
        combat_cards[i].order = fullDeck[current->data].order;
        current = current->next;
    }
    int combo_bonus = calculate_combo_bonus(combat_cards, 
                                           gstate->combat_zone[attacker].size, 
                                           DECK_RANDOM);
    expected_attack += combo_bonus;
    
    float attack_stdev = sqrtf(attack_variance);
    
    // BALANCED RULES: Defense constraint
    float beta = 1.0;  // Tunable parameter
    float attack_threshold = expected_attack - beta * attack_stdev;
    
    // TACTICAL: Resource targets
    int opp_energy = gstate->current_energy[attacker];  // Attacker's energy
    float target_cash = (opp_energy - 8) * 19.0 / 91.0 + 8.0;
    float target_cards = (opp_energy - 8) * 5.0 / 91.0 + 3.0;
    
    int current_cash = gstate->current_cash_balance[defender];
    int current_cards = calculate_effective_cards(gstate, defender);
    
    int cash_available = oraclemax(0, current_cash - (int)target_cash);
    int cards_available = oraclemax(0, current_cards - (int)target_cards);
    
    // Find best defense within constraints
    uint8_t best_defenders[3];
    int best_num = 0;
    float best_expected_def = 0.0;
    
    uint8_t affordable[40];
    int affordable_count = 0;
    uint8_t* hand_array = HDCLL_toArray(&gstate->hand[defender]);
    
    for(uint8_t i = 0; i < gstate->hand[defender].size; i++) {
        uint8_t card_idx = hand_array[i];
        if(fullDeck[card_idx].card_type == CHAMPION_CARD &&
           fullDeck[card_idx].cost <= cash_available)
            affordable[affordable_count++] = card_idx;
    }
    
    // Prioritize D4+0 (0-cost) defenders
    for(int i = 0; i < affordable_count; i++) {
        if(fullDeck[affordable[i]].cost == 0 && 
           fullDeck[affordable[i]].attack_base == 0) {
            best_defenders[best_num++] = affordable[i];
            if(best_num >= 3) break;
        }
    }
    
    // Evaluate defense options
    for(int num = best_num; num <= oraclemin(3, oraclemin(affordable_count, cards_available)); num++) {
        uint8_t defenders[3];
        float expected_def = 0.0;
        
        if(num > best_num) {
            select_best_defenders_by_efficiency(affordable, affordable_count, 
                                               num - best_num, defenders, gstate);
            
            // Calculate expected defense with combo
            for(int i = 0; i < num - best_num; i++)
                expected_def += fullDeck[defenders[i]].expected_defense;
            
            // Add previously selected 0-cost defenders
            for(int i = 0; i < best_num; i++) {
                defenders[num - best_num + i] = best_defenders[i];
                expected_def += fullDeck[best_defenders[i]].expected_defense;
            }
        } else {
            for(int i = 0; i < best_num; i++) {
                defenders[i] = best_defenders[i];
                expected_def += fullDeck[best_defenders[i]].expected_defense;
            }
        }
        
        // Add combo bonus
        if(num >= 2) {
            CombatCard def_combat[3];
            for(int i = 0; i < num; i++) {
                def_combat[i].species = fullDeck[defenders[i]].species;
                def_combat[i].color = fullDeck[defenders[i]].color;
                def_combat[i].order = fullDeck[defenders[i]].order;
            }
            expected_def += calculate_combo_bonus(def_combat, num, DECK_RANDOM);
        }
        
        // BALANCED RULES constraint: E[Def] <= E[Attack] - β*SD[Attack]
        if(expected_def <= attack_threshold || num == 0) {
            // This defense level satisfies the constraint
            best_num = num;
            for(int i = 0; i < num; i++)
                best_defenders[i] = defenders[i];
            best_expected_def = expected_def;
        } else {
            break;  // Don't overdefend
        }
    }
    
    // Play defenders
    for(int i = 0; i < best_num; i++)
        play_champion(gstate, defender, best_defenders[i], ctx);
    
    free(hand_array);
}
```

---

### 2. **Tactical Strategy vs Heuristic 1**

**Heuristic 1 Strengths:**
- **One-move look-ahead** - evaluates each possible move
- **Explicit advantage function**: `Advantage = ε*Energy_Adv + γ*Cards_Adv + Cash_Adv`
- **Systematic evaluation** of all moves
- **Calibratable** - optimize ε and γ empirically
- **More principled** than pure heuristics

**Tactical Strategy Weaknesses vs Heuristic:**
- Tactical is more "greedy" - doesn't enumerate all moves
- No explicit advantage function
- Harder to tune systematically

**Winner: Heuristic 1 should be stronger**

The Heuristic approach is more sophisticated because it:
1. Considers **all possible moves**
2. Uses a **quantifiable advantage metric**
3. Can be **optimized** through parameter tuning

**However**, Heuristic 1 needs implementation details:

```c
// strat_heuristic1.c - Full implementation sketch

typedef struct {
    uint8_t move_type;  // 0=pass, 1=champion(s), 2=draw, 3=cash
    uint8_t num_champions;
    uint8_t champion_indices[3];
    uint8_t draw_card_index;
    uint8_t cash_card_index;
} Move;

// Tunable parameters (optimize via AI vs AI tournaments)
#define EPSILON_ENERGY 1.0   // Weight for energy advantage
#define GAMMA_CARDS 0.15     // Weight for card advantage

float calculate_advantage(struct gamestate* gstate, PlayerID player) {
    PlayerID opponent = 1 - player;
    
    float energy_adv = gstate->current_energy[player] - 
                       gstate->current_energy[opponent];
    
    // Huge bonus for winning position
    if(gstate->current_energy[opponent] == 0)
        energy_adv += 100000;
    
    float cards_adv = calculate_effective_cards(gstate, player) - 
                      estimate_opponent_effective_cards(gstate, opponent);
    
    float cash_adv = gstate->current_cash_balance[player] - 
                     gstate->current_cash_balance[opponent];
    
    // Reduce weights on cards/cash as opponent energy decreases
    float opp_energy_ratio = gstate->current_energy[opponent] / 99.0;
    float cards_weight = GAMMA_CARDS * opp_energy_ratio;
    float cash_weight = 1.0 * opp_energy_ratio;
    
    return EPSILON_ENERGY * energy_adv + 
           cards_weight * cards_adv + 
           cash_weight * cash_adv;
}

void heuristic_attack_strategy(struct gamestate* gstate, GameContext* ctx) {
    PlayerID attacker = gstate->current_player;
    
    // Generate all possible moves
    Move moves[100];  // Max possible moves
    int num_moves = generate_all_attack_moves(gstate, attacker, moves);
    
    // Evaluate each move
    float best_advantage = -1e9;
    int best_move_idx = -1;
    
    for(int i = 0; i < num_moves; i++) {
        // Clone gamestate
        struct gamestate* clone = clone_gamestate(gstate);
        
        // Apply move to clone
        apply_move(clone, attacker, &moves[i], ctx);
        
        // Simulate expected defense response (use average or simple heuristic)
        simulate_expected_defense_response(clone, ctx);
        
        // Calculate advantage after move + expected response
        float advantage = calculate_advantage(clone, attacker);
        
        if(advantage > best_advantage) {
            best_advantage = advantage;
            best_move_idx = i;
        }
        
        // Free clone
        free_gamestate_clone(clone);
    }
    
    // Execute best move
    if(best_move_idx >= 0)
        apply_move(gstate, attacker, &moves[best_move_idx], ctx);
}
```

---

## **Overall Ranking (Strongest to Weakest)**

1. **Heuristic 1** (with proper implementation)
   - Most sophisticated decision-making
   - Evaluates all options systematically
   - Tunable parameters
   - **Estimated win rate vs Random: 75-85%**

2. **Balanced-Tactical Hybrid** (my suggested combination)
   - Combines principled resource management with adaptability
   - Variance-aware defense
   - Combo optimization
   - **Estimated win rate vs Random: 70-80%**

3. **Balanced Rules 1** (your original proposal)
   - Solid mathematical foundation
   - Predictable and reliable
   - **Estimated win rate vs Random: 65-75%**

4. **Tactical Strategy** (my original suggestion)
   - Good adaptability
   - May miss optimal plays
   - **Estimated win rate vs Random: 65-75%**

5. **Random Strategy** (current implementation)
   - Baseline
   - **Win rate: 50%** (by definition vs itself)

---

## **Recommendation: Multi-Stage Development**

```c
// Stage 1: Implement Balanced Rules 1 (simplest, good foundation)
// Files: strat_balanced.c, strat_balanced.h

// Stage 2: Add Heuristic 1 (most sophisticated)
// Files: strat_heuristic.c, strat_heuristic.h

// Stage 3: Create parameter optimization framework
// Run tournaments: Balanced vs Heuristic with different parameters
// Files: optimize_params.c

// Stage 4: Implement best hybrid based on empirical results
```

Would you like me to provide a complete implementation of any of these strategies?
