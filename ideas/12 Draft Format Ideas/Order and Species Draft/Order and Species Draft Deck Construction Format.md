# Oracle Tactical Card Game: Order / Species Draft Format Specification (Experimental)

## Document Purpose

This document provides a complete specification for implementing a simulation to calibrate the Order / Species Draft format for the Oracle TacCG. The simulation will generate two calibration tables (Order Draft and Species Draft) to determine optimal buffer (n) sizes.

**Target Audience:** This document is for implementing C code to run the calibration simulation.

---

## Table of Contents

1. [Overview](#overview)
2. [Draft Rules](#draft-rules)
3. [Calibration Goals](#calibration-goals)
4. [Simulation Requirements](#simulation-requirements)
5. [Algorithm Specification](#algorithm-specification)
6. [Results Tables Structure](#results-tables-structure)
7. [Implementation Details](#implementation-details)
8. [Analysis Questions](#analysis-questions)
9. [Game Context](#game-context)

---

## Overview

### Format Description

An experimental two-player draft format inspired by Magic: The Gathering's Color Draft. Players select piles of champions organized by Order (5 piles) or Species (15 piles).

**Key Innovation:** Uses a buffer of n cards to compensate for uneven pile distributions, ensuring fair deck construction. Ideally n would be between 12 and 18.

### Format Variants

1. **Order Draft:** 5 piles (one per Order: A, B, C, D, E)
2. **Species Draft:** 15 piles (one per species: Human, Elf, Dwarf, Orc, Goblin, Minotaur, Dragon, Cyclops, Fairy, Hobbit, Faun, Centaur, Aven, Koatl, Lycan)

### Inspiration

Based on Magic: The Gathering's Color Draft format (reference: goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html)

---

## Draft Rules

### Setup Phase

1. **Prepare Champion Pool:**
   
   - Remove all 18 non-champion cards from one 120-card deck
   - Shuffle the 102 champion cards thoroughly
   - Cut the top n cards and set aside face-down (buffer pool)
   - Retain the bottom (102-n) cards for drafting

2. **Create Piles (Face-Up):**
   
   - **Order Draft:** Sort (102-n) cards into 5 face-up piles by Order (A through E)
   - **Species Draft:** Sort (102-n) cards into 15 face-up piles by Species
   - **Important:** Piles are face-up and visible to both players

3. **Determine First Player (Drafter):** Randomly (coin flip, die roll)

### Draft Process

**Selection Pattern:**

**Complete Pattern:**

- A picks 1
- B picks 2
- A picks 2
- B picks 2
- A picks 2
- B picks 2
- ... (continue alternating 2-2 until no piles remain)

**Note:** The final selection may involve only one pile if only one remain (if the starting number of piles is even).

**Pile Selection Rules:**

- Players can see all face-up piles before selecting
- Selected piles immediately go into player's draft pool
- Players cannot look at the n buffer cards during pile selection

### Deck Construction Phase

1. Each player counts their drafted champions
2. If a player has fewer than 34 champions:
   - That player reveals the n buffer cards
   - That player selects cards from the buffer until they have exactly 34 champions, or if that's not possible:
     - That player may select additional cards from opponent's draft pool (face up)
     - Selects only enough cards to reach exactly 34 champions
   - Unused buffer cards are set aside
3. Each player discards down to exactly 34 champions (if they have more)
4. Add 5 Draw/Recall cards (3× Draw 2, 2× Draw 3) to each deck
5. Add 1 Exchange card to each deck

**Note:** It is currently unproven whether the case where a player drafts more than 68 champions can occur in Species Draft or in Order Draft. Mathematical proof or algorithmic testing is required to confirm.

### Combo Bonuses

**Primary Option:** Use Monochrome combo bonuses (no color combos)

**Alternative to Consider:** Use Draft combo bonuses from Solomon 7×7 format

- This would provide intermediate power level
- May better match the draft environment
- Requires playtesting to determine optimal choice

### Game Start

- Shuffle final 40-card decks
- Play using chosen combo bonus system
- Follow standard Oracle game rules

---

## Calibration Goals

### Objectives

Find optimal value of n for each format such that:

1. **Goal 1 (Minimum Draft Size):** 
   
   - Probability(either player has <34 champions before buffer access) ≤ 20%
   - This ensures players rarely need to access the buffer

2. **Goal 2 (Maximum Draft Size):** 
   
   - Probability(one player drafts more than 68 champions before buffer access) ≤ 1%
   - This ensures the special compensation rule, where the player with less than 34 cards can select cards from the draft of the player with more than 68 cards, is rarely needed

### Why These Goals Matter

**Goal 1:**

- Too small n → Players frequently cannot reach 34 champions
- Sweet spot n → Rare buffer access, preserves draft decisions
- Failure indicates format may not be viable

**Goal 2:**

- The >68 champion case requires the special compensation rule
- Minimizing this case keeps the format simple and intuitive
- Optimal n should make this case extremely rare (<1%)

### Important Notes

- Optimal n may differ between Order Draft and Species Draft
- Each format will have its own calibration table
- The >68 champion case may or may not be possible (requires verification)

---

## Simulation Requirements

### Parameters

**n1:** Number of outer loop iterations (per n value)

- Recommended: 1000-10000
- Focus: Statistical confidence in probability estimates
- Trade-off with n2 based on 10-minute runtime target

**n2:** Number of inner loop iterations (for Random strategy only)

- Recommended: 100-1000
- Purpose: Capture variance in random selection
- Used to compute min, avg-2σ, avg-1σ, avg statistics

**Performance Target:** ≤10 minutes runtime on single CPU core

### Selection Strategies

Three strategies to test:

1. **Random Selection:**
   
   - Both players randomly select from available piles
   - Provides baseline with maximum variance, perhaps close to how players may draft in real life
   - Requires n2 iterations to compute statistics

2. **Greedy Large/Small:**
   
   - Player A selects largest available pile
   - Player B selects smallest available pile
   - Deterministic (single run per outer iteration)
   - Stress Test the probability that Player A drafts more than 68 cards, or that Player B drafts less than 34 cards

3. **Greedy Small/Large:**
   
   - Player A selects smallest available pile
   - Player B selects largest available pile
   - Deterministic (single run per outer iteration)
   - Stress Test the probability that Player B drafts more than 68 cards, or that Player A drafts less than 34 cards

---

## Algorithm Specification

### Main Algorithm Structure

```
INPUT:
  n1 = number of outer loop iterations (e.g., 5000)
  n2 = number of random selection iterations (e.g., 500)

DEFINE:
  pile_methods = ["Order", "Species"]
  selection_strategies = ["Random", "Greedy_Large_Small", "Greedy_Small_Large"]

FOR pile_method IN pile_methods:
  OUTPUT: "Testing", pile_method, "draft format"

  # Initialize results table: 35 rows (n=0 to 34), 13 columns
  results_table = initialize_table(rows=35, columns=13)

  FOR n FROM 0 TO 34:
    # Track calibration goal failures for this n value
    goal1_failures = {
      "Random_min": 0,
      "Random_avg_minus_2sd": 0,
      "Random_avg_minus_1sd": 0,
      "Random_avg": 0,
      "Greedy_Large_Small": 0,
      "Greedy_Small_Large": 0
    }

    goal2_failures = {
      "Random_min": 0,
      "Random_avg_minus_2sd": 0,
      "Random_avg_minus_1sd": 0,
      "Random_avg": 0,
      "Greedy_Large_Small": 0,
      "Greedy_Small_Large": 0
    }

    FOR iteration FROM 1 TO n1:
      # Setup
      champion_pool = generate_102_champion_deck()
      shuffled_pool = shuffle(champion_pool)
      buffer_cards = top_n_cards(shuffled_pool, n)
      draft_pool = bottom_cards(shuffled_pool, 102-n)

      # Sort into piles
      IF pile_method == "Order":
        piles = sort_by_order(draft_pool)  # 5 piles
      ELSE:
        piles = sort_by_species(draft_pool)  # 15 piles

      # Test Strategy 1: Random (requires n2 inner simulations)
      piles_copy = deep_copy(piles)
      random_min_drafts = []
      random_max_drafts = []

      FOR sim FROM 1 TO n2:
        piles_copy2 = deep_copy(piles)
        pA_draft, pB_draft = simulate_pile_selection(piles_copy2, "Random")
        min_draft = min(count(pA_draft), count(pB_draft))
        max_draft = max(count(pA_draft), count(pB_draft))
        random_min_drafts.append(min_draft)
        random_max_drafts.append(max_draft)

      # Compute statistics from n2 runs (for minimum draft sizes)
      stat_min = minimum(random_min_drafts)
      stat_avg = average(random_min_drafts)
      stat_sd = standard_deviation(random_min_drafts)
      stat_avg_minus_1sd = stat_avg - stat_sd
      stat_avg_minus_2sd = stat_avg - 2 * stat_sd

      # Check Goal 1 failures (min_draft < 34)
      IF stat_min < 34:
        goal1_failures["Random_min"] += 1
      IF stat_avg_minus_2sd < 34:
        goal1_failures["Random_avg_minus_2sd"] += 1
      IF stat_avg_minus_1sd < 34:
        goal1_failures["Random_avg_minus_1sd"] += 1
      IF stat_avg < 34:
        goal1_failures["Random_avg"] += 1

      # Compute statistics from n2 runs (for maximum draft sizes)
      max_stat_min = minimum(random_max_drafts)
      max_stat_max = maximum(random_max_drafts)
      max_stat_avg = average(random_max_drafts)
      max_stat_sd = standard_deviation(random_max_drafts)

      # Check Goal 2 failures (max_draft > 68)
      IF max_stat_max > 68:
        goal2_failures["Random_min"] += 1
      IF max_stat_avg + 2 * max_stat_sd > 68:
        goal2_failures["Random_avg_minus_2sd"] += 1
      IF max_stat_avg + max_stat_sd > 68:
        goal2_failures["Random_avg_minus_1sd"] += 1
      IF max_stat_avg > 68:
        goal2_failures["Random_avg"] += 1

      # Test Strategy 2: Greedy Large/Small (deterministic)
      piles_copy = deep_copy(piles)
      pA_draft, pB_draft = simulate_pile_selection(piles_copy, "Greedy_Large_Small")
      min_draft = min(count(pA_draft), count(pB_draft))
      max_draft = max(count(pA_draft), count(pB_draft))

      IF min_draft < 34:
        goal1_failures["Greedy_Large_Small"] += 1
      IF max_draft > 68:
        goal2_failures["Greedy_Large_Small"] += 1

      # Test Strategy 3: Greedy Small/Large (deterministic)
      piles_copy = deep_copy(piles)
      pA_draft, pB_draft = simulate_pile_selection(piles_copy, "Greedy_Small_Large")
      min_draft = min(count(pA_draft), count(pB_draft))
      max_draft = max(count(pA_draft), count(pB_draft))

      IF min_draft < 34:
        goal1_failures["Greedy_Small_Large"] += 1
      IF max_draft > 68:
        goal2_failures["Greedy_Small_Large"] += 1

    # Record results for this n value in table
    results_table[n] = {
      "n": n,
      "Goal1_Random_min": goal1_failures["Random_min"],
      "Goal1_Random_avg-2sd": goal1_failures["Random_avg_minus_2sd"],
      "Goal1_Random_avg-1sd": goal1_failures["Random_avg_minus_1sd"],
      "Goal1_Random_avg": goal1_failures["Random_avg"],
      "Goal1_Greedy_L/S": goal1_failures["Greedy_Large_Small"],
      "Goal1_Greedy_S/L": goal1_failures["Greedy_Small_Large"],
      "Goal2_Random_min": goal2_failures["Random_min"],
      "Goal2_Random_avg-2sd": goal2_failures["Random_avg_minus_2sd"],
      "Goal2_Random_avg-1sd": goal2_failures["Random_avg_minus_1sd"],
      "Goal2_Random_avg": goal2_failures["Random_avg"],
      "Goal2_Greedy_L/S": goal2_failures["Greedy_Large_Small"],
      "Goal2_Greedy_S/L": goal2_failures["Greedy_Small_Large"]
    }

  # Output complete table for this pile method
  OUTPUT: results_table, pile_method
  SAVE: results_table to CSV file

OUTPUT: "Analysis complete. Review tables to determine optimal n values."
```

---

## Pile Selection Simulation

### Procedure: simulate_pile_selection(piles, strategy)

```
INPUT:
  piles = list of champion piles (either 5 or 15)
  strategy = "Random" | "Greedy_Large_Small" | "Greedy_Small_Large"

OUTPUT:
  playerA_draft = list of champion cards
  playerB_draft = list of champion cards

ALGORITHM:
  playerA_draft = []
  playerB_draft = []
  available_piles = piles.copy()
  is_first_pick = true

  WHILE available_piles is not empty:

    IF is_first_pick:

      # Player A picks 1
      IF available_piles is not empty:
        selected = select_pile(available_piles, strategy, "PlayerA")
        playerA_draft.append_all(selected)
        remove_from(available_piles, selected)

      is_first_pick = false

    ELSE:
      # B picks 2, A picks 2 (alternating)

      # Player B picks 2
      FOR pick FROM 1 TO 2:
        IF available_piles is not empty:
          selected = select_pile(available_piles, strategy, "PlayerB")
          playerB_draft.append_all(selected)
          remove_from(available_piles, selected)

      # Player A picks 2
      FOR pick FROM 1 TO 2:
        IF available_piles is not empty:
          selected = select_pile(available_piles, strategy, "PlayerA")
          playerA_draft.append_all(selected)
          remove_from(available_piles, selected)

  RETURN playerA_draft, playerB_draft
```

### Procedure: select_pile(available_piles, strategy, player)

```
INPUT:
  available_piles = list of remaining piles
  strategy = selection strategy identifier
  player = "PlayerA" | "PlayerB"

OUTPUT:
  selected_pile = chosen pile

ALGORITHM:
  IF strategy == "Random":
    RETURN random_choice(available_piles)

  ELSE IF strategy == "Greedy_Large_Small":
    IF player == "PlayerA":
      RETURN largest_pile(available_piles)
    ELSE:
      RETURN smallest_pile(available_piles)

  ELSE IF strategy == "Greedy_Small_Large":
    IF player == "PlayerA":
      RETURN smallest_pile(available_piles)
    ELSE:
      RETURN largest_pile(available_piles)
```

**Helper Functions:**

```
largest_pile(piles):
  RETURN pile with maximum card count

smallest_pile(piles):
  RETURN pile with minimum card count
  # If multiple piles tied for smallest, return any one (e.g., first)

random_choice(piles):
  RETURN uniformly random pile from piles
```

---

## Results Tables Structure

### Required Output

Two CSV files, one for each pile method:

1. `order_draft_calibration_n1_{n1}_n2_{n2}.csv`
2. `species_draft_calibration_n1_{n1}_n2_{n2}.csv`

### Table Structure

**Dimensions:** 35 rows × 13 columns

**Row Labels:** n = 0, 1, 2, ..., 34

**Column Labels:**

| Column               | Description                              |
| -------------------- | ---------------------------------------- |
| n                    | Buffer size (0-34)                       |
| Goal1_Random_min     | Count where min(n2 runs) < 34            |
| Goal1_Random_avg-2sd | Count where (avg - 2σ) < 34              |
| Goal1_Random_avg-1sd | Count where (avg - 1σ) < 34              |
| Goal1_Random_avg     | Count where avg < 34                     |
| Goal1_Greedy_L/S     | Count where min_draft < 34 (Large/Small) |
| Goal1_Greedy_S/L     | Count where min_draft < 34 (Small/Large) |
| Goal2_Random_min     | Count where max(n2 runs) > 68            |
| Goal2_Random_avg-2sd | Count where (avg + 2σ) > 68              |
| Goal2_Random_avg-1sd | Count where (avg + 1σ) > 68              |
| Goal2_Random_avg     | Count where avg > 68                     |
| Goal2_Greedy_L/S     | Count where max_draft > 68 (Large/Small) |
| Goal2_Greedy_S/L     | Count where max_draft > 68 (Small/Large) |

### Example Table (Order Draft)

```
n,Goal1_Random_min,Goal1_Random_avg-2sd,Goal1_Random_avg-1sd,Goal1_Random_avg,Goal1_Greedy_L/S,Goal1_Greedy_S/L,Goal2_Random_min,Goal2_Random_avg-2sd,Goal2_Random_avg-1sd,Goal2_Random_avg,Goal2_Greedy_L/S,Goal2_Greedy_S/L
0,4523,3201,2145,1032,4891,4756,0,0,0,0,0,0
1,3876,2534,1678,723,4234,4098,0,0,0,0,0,0
...
34,0,0,0,0,0,0,0,0,0,0,0,0
```

### Success Criteria

**Goal 1 (Minimum Draft Size):**

- Failure count should be ≤ 0.20 × n1 (20% threshold)
- Example: If n1=5000, accept ≤1000 failures

**Goal 2 (Maximum Draft Size):**

- Failure count should be ≤ 0.01 × n1 (1% threshold)
- Example: If n1=5000, accept ≤50 failures

**Optimal n:**

- Any n value where all 12 count columns meet their respective thresholds

---

## Implementation Details

### Champion Card Structure

```c
// Simplified Champion card representation
typedef struct {
    int species_id;     // 0-14 (15 species)
    int order_id;       // 0-4 (5 orders)
    int color_id;       // 0-2 (3 colors)
} SimplifiedChampion;
```

### Species and Order Mappings

See fullDeck array in game_constants.c/h, select only the 102 champions, and keep only the information required for the SimplifiedChampion struct. See game_types.h for the card struct details and recon that the SimplifiedChampion struct is a much simplified version of the card struct.

### Deck Composition

**102 Champion Cards Total:**

- 34 cards per color

**Per Species (approximately 6-7 cards each):**

- Each species appears in roughly equal numbers
- Exact distribution varies by cost and power level

### Generating the 102-Champion Deck

```c
// Pseudo-code for deck generation
SimplifiedChampion* generate_102_champion_deck() {
    SimplifiedChampion* deck = allocate_array(102);
    int index = 0;

    // Extract the information from the fullDeck array here

    return deck;
}
```

**Note:** For simulation purposes, exact champion stats (defense_die, attack_base) are not critical. The key attributes are species_id and order_id for pile sorting.

### Pile Structures

```c
// Pile representation
typedef struct {
    SimplifiedChampion** cards;     // Array of pointers to champion cards
    int count;            // Number of cards in pile
    int capacity;         // Allocated capacity
    int identifier;       // Order ID (0-4) or Species ID (0-14)
} Pile;

// Create piles from champion list
Pile* sort_by_order(SimplifiedChampion* champions, int num_champions) {
    Pile* piles = allocate_array(5);

    // Initialize 5 empty piles
    for (int i = 0; i < 5; i++) {
        piles[i].cards = allocate_array(30);  // Max ~30 per order
        piles[i].count = 0;
        piles[i].capacity = 30;
        piles[i].identifier = i;
    }

    // Sort champions into piles
    for (int i = 0; i < num_champions; i++) {
        int order = champions[i].order_id;
        add_to_pile(&piles[order], &champions[i]);
    }

    return piles;
}

Pile* sort_by_species(SimplifiedChampion* champions, int num_champions) {
    Pile* piles = allocate_array(15);

    // Initialize 15 empty piles
    for (int i = 0; i < 15; i++) {
        piles[i].cards = allocate_array(10);  // Max ~10 per species
        piles[i].count = 0;
        piles[i].capacity = 10;
        piles[i].identifier = i;
    }

    // Sort champions into piles
    for (int i = 0; i < num_champions; i++) {
        int species = champions[i].species_id;
        add_to_pile(&piles[species], &champions[i]);
    }

    return piles;
}
```

### Draft Structures

```c
// Draft pool for a player
typedef struct {
    SimplifiedChampion** cards;     // Array of pointers to drafted champions
    int count;            // Number of cards drafted
    int capacity;         // Allocated capacity
} Draft;

// Initialize empty draft
Draft create_draft() {
    Draft draft;
    draft.cards = allocate_array(102);  // Max possible
    draft.count = 0;
    draft.capacity = 102;
    return draft;
}

// Add all cards from a pile to draft
void add_pile_to_draft(Draft* draft, Pile* pile) {
    for (int i = 0; i < pile->count; i++) {
        draft->cards[draft->count++] = pile->cards[i];
    }
}
```

### Statistical Functions

```c
// Calculate minimum of array
int minimum(int* values, int count) {
    int min = values[0];
    for (int i = 1; i < count; i++) {
        if (values[i] < min) min = values[i];
    }
    return min;
}

// Calculate maximum of array
int maximum(int* values, int count) {
    int max = values[0];
    for (int i = 1; i < count; i++) {
        if (values[i] > max) max = values[i];
    }
    return max;
}

// Calculate average of array
double average(int* values, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}

// Calculate standard deviation
double standard_deviation(int* values, int count) {
    double avg = average(values, count);
    double sum_sq_diff = 0.0;

    for (int i = 0; i < count; i++) {
        double diff = values[i] - avg;
        sum_sq_diff += diff * diff;
    }

    return sqrt(sum_sq_diff / count);
}
```

### Random Number Generation

```c
// Fisher-Yates shuffle
void shuffle_array(SimplifiedChampion* array, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);  // use oracle's Mersenne-Twister implementation (mtwister.c) whenever a rand() is required
        SimplifiedChampion temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

// Random selection from available piles
int random_pile_index(Pile* piles, int num_piles) {
    return rand() % num_piles; // use mtwister.c
}

// Set random seed for reproducibility
void init_random(unsigned int seed) {
    srand(seed);
}
```

### File Output

```c
// Write results table to CSV
void write_results_table(const char* filename, int results[35][13], int n1) {
    FILE* fp = fopen(filename, "w");

    // Write header
    fprintf(fp, "n,Goal1_Random_min,Goal1_Random_avg-2sd,");
    fprintf(fp, "Goal1_Random_avg-1sd,Goal1_Random_avg,");
    fprintf(fp, "Goal1_Greedy_L/S,Goal1_Greedy_S/L,");
    fprintf(fp, "Goal2_Random_min,Goal2_Random_avg-2sd,");
    fprintf(fp, "Goal2_Random_avg-1sd,Goal2_Random_avg,");
    fprintf(fp, "Goal2_Greedy_L/S,Goal2_Greedy_S/L\n");

    // Write data rows
    for (int n = 0; n < 35; n++) {
        fprintf(fp, "%d", n);
        for (int col = 0; col < 12; col++) {
            fprintf(fp, ",%d", results[n][col]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
}
```

### Optimization Tips

1. **Pre-allocation:** Allocate all necessary memory upfront
2. **Cache pile sizes:** Store pile counts to avoid recounting
3. **Avoid unnecessary copying:** Use pointers where possible
4. **Random seed:** Use deterministic seed for reproducible results
5. **Progress reporting:** Print progress every 100 outer iterations
6. **Profiling:** Measure time for each major step to identify bottlenecks

---

## Selection Pattern Analysis

### Order Draft (5 piles)

**Total Picks:** 5 piles

**Distribution:**

```
  - A picks 1 pile
  - B picks 2 piles
  - A picks 2 piles

Result: A has 3 piles, B has 2 piles
```

**Final Count:**

- Player A: 3 piles (60% of piles)
- Player B: 2 piles (40% of piles)

**Expected Champion Counts (if piles even):**

- With 102 cards: ~20.4 per pile (for the case n=0)
- Player A: ~61 champions (3 × 20.4)
- Player B: ~41 champions (2 × 20.4)

**Note:** Player A gets more piles, but pile sizes vary significantly based on Order distribution in shuffled deck.

---

### Species Draft (15 piles)

**Total Picks:** 15 piles

**Distribution:**

```
Round 1: A(1) B(2) A(2) → 5 selected, 10 remain
Round 2: B(2) A(2) → 4 selected, 6 remain
Round 3: B(2) A(2) → 4 selected, 2 remain
Round 4: B(2) → 2 selected, 0 remain
```

**Final Count:**

- Player A: 1 + 2 + 2 + 2 = 7 piles (46.7%)
- Player B: 2 + 2 + 2 + 2 = 8 piles (53.3%)

**Expected Champion Counts (if piles even):**

- With 102 cards: ~6.8 per pile
- Player A: ~48 champions (7 × 6.8)
- Player B: ~54 champions (8 × 6.8)

**Note:** Player B gets slightly more piles, which may compensate for Player A's first-pick advantage.

---

## Analysis Questions

After simulation completes, analyze tables to answer:

### 1. Optimal n Values

**Order Draft:**

- Find all the n where all Goal1 columns ≤ 20% failure rate
- Verify all Goal2 columns ≤ 1% failure rate

**Species Draft:**

- Same criteria as Order Draft
- May differ from Order Draft value

### 2. Strategy Comparison

**Goal 1 Analysis (Minimum Draft Size):**

- Which strategy has fewest failures at optimal (acceptable) n values?
- Does Random strategy have higher variance than Greedy strategies?
- Are Greedy strategies too much of an edge case that we would neve4 see in real life?

**Goal 2 Analysis (Maximum Draft Size):**

- Are any violations observed in Order Draft? In Species Draft?

### 3. Random Statistic Selection

**Compare four Random columns:**

- **min:** may require large n
- **avg-2σ:** Conservative, ~95% confidence interval
- **avg-1σ:** Balanced, ~68% confidence interval
- **avg:** Optimistic, mean performance

**Recommendation:**

- Use avg-1σ for balance of safety and efficiency
- Provides reasonable confidence without over-calibrating n

### 4. Verification Questions

**Mathematical Analysis:**

- Can max(draft_A, draft_B) > 68 occur in Order Draft? How about in Species Draft?
- Check Goal2 columns
- If all zeros across all n and strategies → extremely rare
- If non-zero → possible, requires special rule documentation

**Practical Viability:**

- Do both formats have viable optimal n values?

---

## Code Structure Recommendations

### Modular Organization

```c
// File: champion.h / champion.c
- SimplifiedChampion struct definition
- generate_102_champion_deck()
- shuffle_deck()

// File: pile.h / pile.c
- Pile struct definition
- sort_by_order()
- sort_by_species()
- largest_pile()
- smallest_pile()
- random_pile()

// File: draft.h / draft.c
- Draft struct definition
- simulate_pile_selection()
- select_pile()

// File: statistics.h / statistics.c
- minimum()
- maximum()
- average()
- standard_deviation()

// File: simulation.h / simulation.c
- run_outer_loop()
- run_inner_loop()
- check_goal_failures()

// File: output.h / output.c
- initialize_results_table()
- write_results_table()
- print_progress()

// File: main.c
- Command-line argument parsing
- Main simulation loop
- Results summary
```

### Function Size Guidelines

- Keep each C function to ≤35 lines of actual code (preferred)
- Apply firm limit of ≤100 lines of code per function
- Comments and whitespace don't count toward limit
- Long switch statements are acceptable

### File Size Guidelines

- Target: ≤400 lines of actual code per file
- Soft limit: 500 lines of actual code
- Firm limit: 1000 lines of actual code
- Split larger files into logical modules

### Compilation Environment

**Platforms:**

- MSYS2 (Windows)
- Arch Linux

**Compiler:**

- GCC on both platforms

**Editor:**

- Geany (on both platforms)

**Build System:**

- Makefile for cross-platform compilation
- Python available for scripting if needed

---

## Game Context

### Oracle Game Overview

**Game Type:** Strategic two-player card game

**Objective:** Reduce opponent's Energy to 0

**Key Mechanics:**

- 40-card constructed decks
- Luna resource system (starts at 30, +1 per turn)
- Champion combat with dice rolls
- Combo bonuses for matching species/order/color

**Typical Game:**

- Duration: ~20 minutes
- Rounds: ~30 (60 turns)
- Energy: 99 starting, 0 to lose

---

## Combo Bonus Systems Reference

### Monochrome Combo Bonuses

Used when each player has single-color deck (or custom deck):

**Priority 1: Species Matches**

- 2 of same species: +7
- 3 of same species: +12
- 2 of same species + 1 of same Order: +9

**Priority 2: Order Matches (if no species match)**

- 2 of same Order: +4
- 3 of same Order: +6

**Note:** Only highest applicable bonus applied (no stacking)

### Draft Combo Bonuses (Solomon 7×7)

Used in Solomon, Draft 12×8, Draft 1-2-3 formats:

**Priority 1: Species Matches**

- 3 of same species: +12
- 2 of same species + 1 of same Order: +9
- 2 of same species + 1 of same Color: +8
- 2 of same species: +7

**Priority 2: Order Matches (if no species match)**

- 3 of same Order: +6
- 2 of same Order + 1 of same Color: +5
- 2 of same Order: +4

**Priority 3: Color Matches (if no species or Order match)**

- 3 of same Color: +4
- 2 of same Color: +3

### Random Deck Combo Bonuses

Used when decks distributed randomly:

**Priority 1: Species Matches (Highest)**

- 2 of same species: +10
- 3 of same species: +16
- 2 of same species + 1 of same Order: +14
- 2 of same species + 1 of same Color: +13

**Priority 2: Order Matches (if no species match)**

- 2 of same Order: +7
- 3 of same Order: +11
- 2 of same Order + 1 of same Color: +9

**Priority 3: Color Matches (if no species or Order match)**

- 2 of same Color: +5
- 3 of same Color: +8

---

## Verification Tasks

### Mathematical Analysis

**Question:** Can `max(draft_A, draft_B) > 68` occur in Order Draft? Can it occur in Species Draft?

**Simulation will provide empirical evidence for both cases.**

---

## Additional Implementation Notes

### Progress Reporting

```c
void print_progress(int n, int iteration, int n1) {
    if (iteration % 100 == 0) {
        printf("n=%d: %d/%d iterations (%.1f%%)\n", 
               n, iteration, n1, 100.0 * iteration / n1);
        fflush(stdout);
    }
}
```

### Memory Management

```c
// Free allocated memory
void cleanup_deck(Champion* deck) {
    free(deck);
}

void cleanup_piles(Pile* piles, int num_piles) {
    for (int i = 0; i < num_piles; i++) {
        free(piles[i].cards);
    }
    free(piles);
}

void cleanup_draft(Draft* draft) {
    free(draft->cards);
}
```

### Error Handling

```c
// Check for allocation failures
Champion* safe_alloc_champions(int count) {
    Champion* ptr = malloc(count * sizeof(Champion));
    if (ptr == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for %d champions\n", count);
        exit(1);
    }
    return ptr;
}
```

### Command-Line Interface

```c
// Example usage:
// ./orderspecies_draft_sim --n1=5000 --n2=500 --seed=42

int main(int argc, char* argv[]) {
    int n1 = 5000;   // Default values
    int n2 = 500;
    unsigned int seed = time(NULL);

    // Parse arguments (implementation needed)
    parse_arguments(argc, argv, &n1, &n2, &seed);

    printf("Order and Soecies Draft Calibration Simulation\n");
    printf("n1 (outer loop): %d\n", n1);
    printf("n2 (inner loop): %d\n", n2);
    printf("Random seed: %u\n", seed);
    printf("\n");

    init_random(seed);

    // Run Order Draft simulation
    printf("=== ORDER DRAFT ===\n");
    int order_results[35][13];
    run_simulation("Order", n1, n2, order_results);
    write_results_table("order_draft_calibration.csv", order_results, n1); // add info on n1, n2 and seed to file name

    // Run Species Draft simulation  
    printf("\n=== SPECIES DRAFT ===\n");
    int species_results[35][13];
    run_simulation("Species", n1, n2, species_results);
    write_results_table("species_draft_calibration.csv", species_results, n1); // add info on n1, n2 and seed to file name

    printf("\nSimulation complete. Results written to CSV files.\n");
    return 0;
}
```

---

## Summary Checklist

### Implementation Requirements

- [ ] Champion struct with species_id and order_id
- [ ] Generate 102-champion deck function
- [ ] Shuffle function (Fisher-Yates)
- [ ] Sort into Order piles (5 piles)
- [ ] Sort into Species piles (15 piles)
- [ ] Pile selection simulation (3 strategies)
- [ ] Draft tracking (count champions per player)
- [ ] Statistical functions (min, max, avg, std dev)
- [ ] Goal failure checking (Goal 1 and Goal 2)
- [ ] Results table storage (35×13 integers)
- [ ] CSV file output
- [ ] Progress reporting
- [ ] Command-line argument parsing

### Output Requirements

- [ ] Two CSV files (Order Draft and Species Draft)
- [ ] 35 rows each (n = 0 to 34)
- [ ] 13 columns each (n + 12 failure counts)
- [ ] Clear column headers
- [ ] Integer failure counts (not percentages)

### Analysis Requirements

- [ ] Identify acceptable n values for Order Draft
- [ ] Identify acceptable n values for Species Draft
- [ ] Compare Random statistics (min, avg-2σ, avg-1σ, avg)
- [ ] Evaluate Greedy strategy realism
- [ ] Verify Goal 2 violations (>68 champions case)
- [ ] Document findings and recommendations

---

## Expected Runtime Analysis

**Per Outer Iteration:**

- Shuffle: O(102)
- Sort into piles: O(102)
- Random strategy: n2 × O(num_piles²) pile selections
- Greedy strategies: 2 × O(num_piles²) pile selections  
- Statistics: O(n2) calculations

**For Order Draft (5 piles):**

- Per iteration: ~1-2 ms (estimated)
- n1=5000, n=0-34: 35 × 5000 × 2ms = 350 seconds ≈ 6 minutes

**For Species Draft (15 piles):**

- Per iteration: ~3-5 ms (estimated)
- n1=5000, n=0-34: 35 × 5000 × 5ms = 875 seconds ≈ 15 minutes

**Total Estimated Runtime:** ~20 minutes (both formats)

**Optimization Targets:**

- If too slow: Reduce n2 (less critical than n1)
- If too fast: Increase n1 for better confidence
- Goal: ~10 minutes total runtime

---

## Document Version

**Version:** 1.0  
**Date:** November 2025  
**Status:** Ready for Implementation  
**Target:** C code implementation for calibration simulation

**Next Steps:**

1. Implement simulation in C
2. Run calibration for both formats
3. Analyze results tables
4. Determine optimal n values
5. Write formal draft rules if format is viable

---