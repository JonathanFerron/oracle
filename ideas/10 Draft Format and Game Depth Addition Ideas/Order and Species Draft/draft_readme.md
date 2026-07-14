# Oracle Draft Calibration Simulation

## Overview

This program calibrates the buffer size (n) for Order and Species draft formats in the Oracle tactical card game. It simulates thousands of draft scenarios to determine the optimal number of cards to set aside as a buffer, ensuring fair deck construction.

## Quick Start

```bash
# Build the program
make

# Run with default parameters (n1=5000, n2=500)
./bin/draft_calibration

# Run with custom parameters
./bin/draft_calibration --n1=10000 --n2=1000 --seed=42

# Quick test run
make run-quick
```

## Output Files

The simulation generates two CSV files:

- `order_draft_calibration_n1_5000_n2_500_seed_XXXXXX.csv`
- `species_draft_calibration_n1_5000_n2_500_seed_XXXXXX.csv`

Each file contains 35 rows (buffer sizes n=0 to 34) and 13 columns:
- Column 1: Buffer size (n)
- Columns 2-7: Goal 1 failures (minimum draft < 34 cards)
- Columns 8-13: Goal 2 failures (maximum draft > 68 cards)

## Parameters

- `--n1=NUM`: Outer loop iterations (default: 5000)
  - Higher values = more statistical confidence
  - Recommended range: 1000-10000

- `--n2=NUM`: Inner loop iterations for random strategy (default: 500)
  - Higher values = better variance capture
  - Recommended range: 200-1000

- `--seed=NUM`: Random seed (default: current time)
  - Use specific seed for reproducible results

## Calibration Goals

**Goal 1**: Minimum draft size ≥ 34 cards (80% success rate)
- Players can build a 40-card deck without needing buffer access

**Goal 2**: Maximum draft size ≤ 68 cards (99% success rate)
- Special compensation rule rarely needed

## Interpreting Results

Look for rows where **all** failure columns meet their thresholds:
- Goal 1: ≤ 20% of n1 (e.g., ≤1000 failures if n1=5000)
- Goal 2: ≤ 1% of n1 (e.g., ≤50 failures if n1=5000)

The smallest n value meeting both goals is optimal.

### Example Analysis

```csv
n,Goal1_Random_min,...,Goal2_Greedy_S/L
0,4523,...,0
12,856,...,0    <- Meets Goal 1 threshold!
13,623,...,0    <- Better!
14,412,...,0    <- Even better!
15,198,...,0    <- Optimal n = 15
16,87,...,0
```

## File Structure

```
src/
├── draft_calibration_main.c   - Main program
├── draft_types.h              - Type definitions
├── draft_champion.c/h         - Champion deck operations
├── draft_pile.c/h             - Pile sorting and selection
├── draft_draft.c/h            - Draft simulation logic
├── draft_statistics.c/h       - Statistical calculations
├── draft_simulation.c/h       - Main simulation loop
├── draft_output.c/h           - CSV output generation
├── mtwister.c/h               - Mersenne Twister RNG
├── game_constants.c/h         - Game constants (from Oracle)
└── game_types.h               - Game types (from Oracle)
```

## Algorithm Overview

For each buffer size n (0 to 34):
1. Generate 102-champion deck
2. Shuffle deck
3. Set aside top n cards as buffer
4. Sort remaining (102-n) cards into piles (5 or 15)
5. Simulate pile selection with 3 strategies:
   - Random: Both players pick randomly (n2 iterations)
   - Greedy L/S: Player A picks largest, B picks smallest
   - Greedy S/L: Player A picks smallest, B picks largest
6. Record failures for both calibration goals
7. Repeat n1 times

## Selection Pattern

### Order Draft (5 piles)
- A picks 1
- B picks 2, A picks 2 (alternating)
- Result: A gets 3 piles, B gets 2 piles

### Species Draft (15 piles)
- A picks 1
- B picks 2, A picks 2 (alternating until exhausted)
- Result: A gets 7 piles, B gets 8 piles

## Performance

**Expected Runtime** (~10 minutes):
- Order Draft: ~6 minutes (5 piles, faster iterations)
- Species Draft: ~15 minutes (15 piles, slower iterations)

**Optimization Tips**:
- Reduce n2 if runtime too long (less critical than n1)
- Increase n1 for better confidence (if time permits)

## Dependencies

- C11 compiler (GCC recommended)
- Math library (-lm)
- Standard C library

**No external dependencies required!**

## Building

### Linux/MSYS2
```bash
make
```

### Manual compilation
```bash
gcc -Wall -Wextra -O2 -std=c11 -Isrc \
    src/draft_calibration_main.c \
    src/draft_champion.c \
    src/draft_pile.c \
    src/draft_draft.c \
    src/draft_statistics.c \
    src/draft_simulation.c \
    src/draft_output.c \
    src/mtwister.c \
    -o bin/draft_calibration \
    -lm
```

## Makefile Targets

- `make` or `make all` - Build executable
- `make clean` - Remove build artifacts and CSV files
- `make run` - Build and run with defaults
- `make run-quick` - Quick test (n1=1000, n2=200)
- `make run-full` - Full simulation (n1=10000, n2=1000)
- `make help` - Show help message

## Troubleshooting

**Problem**: Compilation fails with "undefined reference to sqrt"
**Solution**: Make sure `-lm` flag is included (links math library)

**Problem**: Segmentation fault during execution
**Solution**: Check memory allocation - may need to reduce n1 or n2

**Problem**: Results show no viable n values
**Solution**: The draft format may not be viable with current card distribution

## Next Steps After Calibration

1. Review both CSV files
2. Identify optimal n for Order Draft
3. Identify optimal n for Species Draft
4. Document findings in draft format specification
5. Implement draft format in main Oracle game if viable

## Credits

Part of the Oracle tactical card game project.
See: https://github.com/JonathanFerron/oracle

## License

GPL v3 (same as Oracle project)
