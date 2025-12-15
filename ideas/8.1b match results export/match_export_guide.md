# Match Export System - Integration Guide

## Overview

This system automatically exports match results from interactive play modes (CLI, TUI, GUI) to CSV files for:

1. **Player Progress Tracking** - Monitor improvement over time
2. **AI Calibration** - Collect data for tuning AI parameters
3. **Statistical Analysis** - Export to Excel, R, Python for deeper insights

---

## Quick Start

### 1. Add to Your Game Loop

```c
// At end of stda_game_loop_cli() or stda_game_loop_tui()
void stda_game_loop_cli(config_t* cfg, GameContext* ctx, 
                        StrategySet* strategies) {
    // ... existing game loop ...
    
    while (engine_get_phase(engine) != PHASE_GAME_OVER) {
        // ... game turns ...
    }
    
    // After game ends, export match result
    match_export_from_gamestate(
        engine_get_state(engine),
        cfg,
        "stda_cli"  // Mode identifier
    );
    
    // Display final results
    cli_display_game_over(engine_get_state(engine), cfg);
}
```

### 2. Compile and Run

```bash
# Add to Makefile
SRCS += match_export.c

# Compile
make

# Run - match results automatically exported
./bin/oracle --stda.cli
```

### 3. View Results

```bash
# All matches
cat oracle_matches/matches_master.csv

# Alice's personal history
cat oracle_matches/players/alice_history.csv

# Matches against Balanced AI (for calibration)
cat oracle_matches/calibration/vs_AI_BALANCED.csv
```

---

## File Organization

### Directory Structure

```
oracle_matches/
├── matches_master.csv           # Central database (all matches)
├── players/
│   ├── alice_history.csv        # Alice's match history
│   ├── bob_history.csv          # Bob's match history
│   ├── charlie_history.csv
│   └── ...
└── calibration/
    ├── vs_AI_RANDOM.csv         # All matches vs Random AI
    ├── vs_AI_BALANCED.csv       # All matches vs Balanced AI
    ├── vs_AI_HEURISTIC.csv      # All matches vs Heuristic AI
    ├── vs_AI_HBT.csv            # All matches vs HBT Hybrid AI
    └── ...
```

### File Purpose

| File | Purpose | Use Case |
|------|---------|----------|
| `matches_master.csv` | Complete database | Overall statistics, cross-player analysis |
| `players/{name}_history.csv` | Individual player progress | Track improvement, personal stats |
| `calibration/vs_{AI}.csv` | Matches against specific AI | Calibrate AI parameters |

---

## CSV Format

### Header

```csv
match_id,timestamp,mode,player_a_name,player_a_type,player_a_strategy,
player_b_name,player_b_type,player_b_strategy,deck_type,initial_cash,
rng_seed,winner,turns_played,rounds_played,final_energy_a,final_energy_b,
final_cash_a,final_cash_b,final_hand_size_a,final_hand_size_b,
total_damage_a,total_damage_b,champions_played_a,champions_played_b,
draw_cards_a,draw_cards_b,mulligan_count_b
```

### Example Row

```csv
20251214_153045_0001,2025-12-14T15:30:45,stda_cli,Alice,HUMAN,"HUMAN",
BalancedAI,AI_BALANCED,"epsilon=1.0,gamma=0.15",RANDOM,30,12345,
PLAYER_A,48,24,35,0,15,8,4,2,65,99,14,18,2,3,1
```

---

## Use Cases

### 1. Track Player Progress

**Scenario**: Alice wants to see if she's improving against the Balanced AI.

```bash
# Get Alice's win rate over time
grep "Alice.*AI_BALANCED" oracle_matches/players/alice_history.csv | \
  awk -F',' '{if ($13=="PLAYER_A") wins++; total++} 
             END {print "Win Rate:", wins/total*100"%"}'

# Or use Python
import pandas as pd

df = pd.read_csv('oracle_matches/players/alice_history.csv')
df['match_number'] = range(1, len(df) + 1)

# Calculate rolling win rate (last 10 games)
df['is_win'] = (df['winner'] == 'PLAYER_A').astype(int)
df['rolling_wr'] = df['is_win'].rolling(10).mean()

# Plot progress
import matplotlib.pyplot as plt
plt.plot(df['match_number'], df['rolling_wr'] * 100)
plt.xlabel('Match Number')
plt.ylabel('Win Rate (%) - Last 10 Games')
plt.title("Alice's Progress vs Balanced AI")
plt.show()
```

### 2. Calibrate AI Parameters

**Scenario**: Tune Heuristic AI's advantage weights (ε, γ, δ) based on Alice's play.

```python
import pandas as pd

# Load Alice's matches vs current Heuristic AI
df = pd.read_csv('oracle_matches/calibration/vs_AI_HEURISTIC.csv')
alice_matches = df[df['player_a_name'] == 'Alice']

# Analyze game characteristics
print("Alice's Performance vs Heuristic AI:")
print(f"Total Games: {len(alice_matches)}")
print(f"Win Rate: {(alice_matches['winner'] == 'PLAYER_A').mean():.2%}")
print(f"Avg Game Length: {alice_matches['turns_played'].mean():.1f} turns")
print(f"Avg Energy When Winning: {alice_matches[alice_matches['winner'] == 'PLAYER_A']['final_energy_a'].mean():.1f}")

# If Alice is winning >60%, AI needs better parameters
if (alice_matches['winner'] == 'PLAYER_A').mean() > 0.60:
    print("\n⚠️ AI is too weak - consider adjusting parameters:")
    print("  - Increase epsilon (energy weight)")
    print("  - Increase aggression in late game")
    print("  - Improve defense threshold")
```

### 3. Compare AI Strategies

**Scenario**: Which AI gives Alice the best challenge?

```python
import pandas as pd

df = pd.read_csv('oracle_matches/players/alice_history.csv')

# Filter for Alice as Player A (she played as protagonist)
alice_games = df[df['player_a_name'] == 'Alice']

# Group by opponent AI type
ai_stats = alice_games.groupby('player_b_type').agg({
    'match_id': 'count',  # Total games
    'winner': lambda x: (x == 'PLAYER_A').sum(),  # Alice wins
    'turns_played': 'mean'
}).rename(columns={'match_id': 'total_games', 'winner': 'alice_wins'})

ai_stats['alice_win_rate'] = ai_stats['alice_wins'] / ai_stats['total_games']

print(ai_stats)

# Output:
#                  total_games  alice_wins  turns_played  alice_win_rate
# AI_RANDOM               23          18          35.2          0.783
# AI_BALANCED             45          25          52.1          0.556
# AI_HEURISTIC            12           8          48.3          0.667
# AI_HBT                   8           3          61.5          0.375  ← Best challenge!
```

### 4. Statistical Analysis

**Scenario**: Does going first matter?

```python
import pandas as pd
from scipy import stats

df = pd.read_csv('oracle_matches/matches_master.csv')

# Separate by starting player
player_a_starts = df[df['turns_played'] % 2 == 0]  # Player A started
player_b_starts = df[df['turns_played'] % 2 == 1]  # Player B started

# Win rates
a_wr_when_first = (player_a_starts['winner'] == 'PLAYER_A').mean()
b_wr_when_first = (player_b_starts['winner'] == 'PLAYER_B').mean()

print(f"Player A win rate when going first: {a_wr_when_first:.2%}")
print(f"Player B win rate when going first: {b_wr_when_first:.2%}")

# Statistical test
chi2, p_value = stats.chi2_contingency([
    [player_a_starts['winner'].eq('PLAYER_A').sum(),
     player_a_starts['winner'].ne('PLAYER_A').sum()],
    [player_b_starts['winner'].eq('PLAYER_B').sum(),
     player_b_starts['winner'].ne('PLAYER_B').sum()]
])[:2]

if p_value < 0.05:
    print(f"First player advantage is statistically significant (p={p_value:.4f})")
else:
    print(f"No significant first player advantage (p={p_value:.4f})")
```

---

## Advanced Integration

### Option 1: Track In-Game Statistics

To export more detailed statistics (damage dealt, champions played, etc.), track them during gameplay:

```c
// Add to GameState struct (in game_types.h)
typedef struct {
    // ... existing fields ...
    
    // Statistics tracking
    uint16_t total_damage_dealt[2];
    uint16_t champions_played[2];
    uint16_t draw_cards_played[2];
    uint8_t mulligan_count;
} GameState;

// In combat resolution (combat.c)
void resolve_combat(GameState* gs, GameContext* ctx) {
    // ... existing combat code ...
    
    int16_t damage = attack_total - defense_total;
    if (damage > 0) {
        gs->current_energy[defender] -= damage;
        gs->total_damage_dealt[attacker] += damage;  // Track!
    }
}

// In card playing (card_actions.c)
void play_champion(GameState* gs, PlayerID player, 
                  uint8_t card_id, GameContext* ctx) {
    // ... existing code ...
    gs->champions_played[player]++;  // Track!
}

// Then match_create_record() will export these automatically
```

### Option 2: Custom Export Filters

```c
// Only export matches that meet criteria
bool match_should_export(const GameState* gs, const config_t* cfg) {
    // Only export vs AI (skip human vs human)
    if (cfg->player_types[PLAYER_A] == INTERACTIVE_PLAYER &&
        cfg->player_types[PLAYER_B] == INTERACTIVE_PLAYER) {
        return false;
    }
    
    // Only export games longer than 20 turns
    if (gs->turn < 20) {
        return false;
    }
    
    return true;
}

// Use in game loop
if (match_should_export(gs, cfg)) {
    match_export_from_gamestate(gs, cfg, "stda_cli");
}
```

### Option 3: Batch Analysis Script

```python
#!/usr/bin/env python3
# analyze_matches.py - Batch analysis of match exports

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def analyze_player_progress(player_name):
    """Generate progress report for a player"""
    df = pd.read_csv(f'oracle_matches/players/{player_name}_history.csv')
    
    # Calculate metrics
    total_games = len(df)
    win_rate = (df['winner'] == 'PLAYER_A').mean()
    avg_turns = df['turns_played'].mean()
    
    print(f"\n=== {player_name}'s Progress Report ===")
    print(f"Total Games: {total_games}")
    print(f"Overall Win Rate: {win_rate:.1%}")
    print(f"Average Game Length: {avg_turns:.1f} turns")
    
    # Win rate by opponent
    print("\nWin Rate by Opponent:")
    wr_by_opp = df.groupby('player_b_type').apply(
        lambda x: (x['winner'] == 'PLAYER_A').mean()
    )
    for ai, wr in wr_by_opp.items():
        print(f"  vs {ai}: {wr:.1%}")
    
    # Trend over time (last 20 games)
    recent = df.tail(20)
    recent_wr = (recent['winner'] == 'PLAYER_A').mean()
    print(f"\nRecent Form (last 20 games): {recent_wr:.1%}")
    
    # Visualizations
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    
    # Win rate over time (rolling average)
    df['match_num'] = range(1, len(df) + 1)
    df['is_win'] = (df['winner'] == 'PLAYER_A').astype(int)
    df['rolling_wr'] = df['is_win'].rolling(10, min_periods=1).mean()
    
    axes[0, 0].plot(df['match_num'], df['rolling_wr'] * 100)
    axes[0, 0].axhline(50, color='gray', linestyle='--', alpha=0.5)
    axes[0, 0].set_xlabel('Match Number')
    axes[0, 0].set_ylabel('Win Rate (%)')
    axes[0, 0].set_title('Win Rate Trend (10-game rolling average)')
    
    # Game length distribution
    axes[0, 1].hist(df['turns_played'], bins=20, edgecolor='black')
    axes[0, 1].axvline(df['turns_played'].mean(), 
                       color='red', linestyle='--', 
                       label=f'Mean: {df["turns_played"].mean():.1f}')
    axes[0, 1].set_xlabel('Turns Played')
    axes[0, 1].set_ylabel('Frequency')
    axes[0, 1].set_title('Game Length Distribution')
    axes[0, 1].legend()
    
    # Win rate by opponent
    wr_by_opp.plot(kind='bar', ax=axes[1, 0])
    axes[1, 0].set_ylabel('Win Rate')
    axes[1, 0].set_title('Win Rate by Opponent AI')
    axes[1, 0].set_ylim(0, 1)
    axes[1, 0].axhline(0.5, color='gray', linestyle='--', alpha=0.5)
    
    # Energy correlation (does more energy → wins?)
    wins_df = df[df['winner'] == 'PLAYER_A']
    losses_df = df[df['winner'] != 'PLAYER_A']
    
    axes[1, 1].hist(wins_df['final_energy_a'], 
                    bins=15, alpha=0.5, label='Wins', color='green')
    axes[1, 1].hist(losses_df['final_energy_a'], 
                    bins=15, alpha=0.5, label='Losses', color='red')
    axes[1, 1].set_xlabel('Final Energy (Player A)')
    axes[1, 1].set_ylabel('Frequency')
    axes[1, 1].set_title('Final Energy Distribution')
    axes[1, 1].legend()
    
    plt.tight_layout()
    plt.savefig(f'{player_name}_progress_report.png', dpi=150)
    print(f"\n📊 Visualizations saved to {player_name}_progress_report.png")

if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1:
        analyze_player_progress(sys.argv[1])
    else:
        print("Usage: python analyze_matches.py <player_name>")
        print("Example: python analyze_matches.py Alice")
```

**Usage:**
```bash
python analyze_matches.py Alice
```

---

## Configuration Options

### Enable/Disable Export

```c
// Globally enable/disable (in main.c or config)
match_export_set_enabled(true);   // Enable (default)
match_export_set_enabled(false);  // Disable for private sessions

// Check status
if (match_export_is_enabled()) {
    printf("Match results will be exported to CSV\n");
}
```

### Custom Paths

```c
// Use custom directory structure
MatchExporter* exporter = match_export_init_custom(
    "my_data/matches.csv",          // Master log
    "my_data/players",              // Player histories
    "my_data/calibration"           // AI calibration data
);
```

### Verbose Mode

```c
// Print match summary after export
MatchExporter* exporter = match_export_init("stda_cli");
exporter->verbose = true;  // Enable console output

// Output:
// === Match Exported ===
// Match ID: 20251214_153045_0001
// Alice (HUMAN) vs BalancedAI (AI_BALANCED)
// Winner: PLAYER_A
// Duration: 48 turns (24 rounds)
// ======================
```

---

## Privacy Considerations

### Anonymous Mode

For privacy-conscious users, implement anonymous export:

```c
// In match_create_record(), hash player names
void anonymize_player_name(const char* input, char* output, size_t max_len) {
    // Use MD5 hash or similar
    uint32_t hash = hash_string(input);
    snprintf(output, max_len, "Player_%08X", hash);
}

// Usage:
if (cfg->anonymous_mode) {
    anonymize_player_name(pconfig->player_names[PLAYER_A],
                         record->player_a_name, MAX_PLAYER_NAME);
}
```

### Opt-In vs Opt-Out

```c
// In cmdline.c, add flag
--export-matches    Enable match export (default: off)
--no-export         Disable match export (if default on)

// In config_t
bool export_matches;  // User preference

// Apply
match_export_set_enabled(cfg->export_matches);
```

---

## Troubleshooting

### Issue: No files created

**Cause:** Directory doesn't exist or no write permissions

**Solution:**
```bash
# Check directory exists
ls -la oracle_matches/

# Check permissions
chmod 755 oracle_matches/
chmod 644 oracle_matches/matches_master.csv
```

### Issue: Empty CSV files

**Cause:** Header written but no data

**Solution:** Ensure `match_export_from_gamestate()` is called after game ends:

```c
// Make sure game is in terminal state
if (gs->game_state == PLAYER_A_WINS ||
    gs->game_state == PLAYER_B_WINS ||
    gs->game_state == DRAW) {
    match_export_from_gamestate(gs, cfg, "stda_cli");
}
```

### Issue: Duplicate entries

**Cause:** Export called multiple times

**Solution:** Add guard flag:

```c
static bool match_exported = false;

if (!match_exported) {
    match_export_from_gamestate(gs, cfg, "stda_cli");
    match_exported = true;
}
```

---

## Performance Considerations

- **File I/O**: Append-only writes are fast (< 1ms per match)
- **Memory**: MatchRecord is ~500 bytes, negligible
- **Disk Space**: ~150 bytes per match, 1000 matches ≈ 150 KB

**Optimization for high-volume export:**
```c
// Buffer multiple matches before writing
#define MATCH_BUFFER_SIZE 10

static MatchRecord buffer[MATCH_BUFFER_SIZE];
static int buffer_count = 0;

void match_export_buffered(MatchRecord* record) {
    buffer[buffer_count++] = *record;
    
    if (buffer_count >= MATCH_BUFFER_SIZE) {
        // Flush buffer to disk
        for (int i = 0; i < buffer_count; i++) {
            match_append_to_master(&buffer[i], "matches_master.csv");
        }
        buffer_count = 0;
    }
}
```

---

## Summary

The match export system provides:

✅ **Automatic** - No manual intervention required  
✅ **Universal** - Works with CLI, TUI, GUI modes  
✅ **Organized** - Separate files for different purposes  
✅ **Analyzable** - Standard CSV format for all tools  
✅ **Lightweight** - Minimal performance impact  
✅ **Privacy-Aware** - Can be disabled or anonymized  

**Integration is simple:**
1. Add `match_export.c` to build
2. Call `match_export_from_gamestate()` after each game
3. Analyze results with Python, R, or Excel

Perfect for tracking player progress and calibrating AI strategies!
    