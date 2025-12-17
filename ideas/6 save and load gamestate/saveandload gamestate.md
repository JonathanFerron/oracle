## Save/Load Game State - Sequencing Analysis

### Recommended Position: **Between Folders 5 and 6** (new folder 5.5 or 6)


## Why After Folder 5 (Cash Card Selection)?

### 1. **Completes Interactive Play Foundation**
By folder 5, you have:
- ✅ Full game loop (mulligan, discard-to-7)
- ✅ All card types playable interactively
- ✅ Complete CLI experience for human players

Save/load is a natural **quality-of-life feature** that enhances this complete interactive experience.

### 2. **Enables Debugging and Testing**
Once you can save/load, you can:
- Create specific game states for testing combat
- Test edge cases without playing full games
- Save interesting positions for AI benchmarking
- Resume interrupted development testing sessions

This becomes **very valuable** when implementing AI strategies (folders 8+).

### 3. **Before Refactoring Makes Implementation Easier**
The current game state structure is relatively simple. Implementing save/load **before** major refactoring (folders 6-7) means:
- You're working with the familiar current structure
- You don't have to refactor the save/load code later
- The save format can inform architectural decisions during refactoring

---

## Why Before Folder 6 (Code Restructuring)?

### 1. **Save Format Reveals Architecture Issues**
Implementing serialization forces you to think about:
- Which fields are truly essential vs. derived
- What constitutes "game state" vs. "UI state"
- Data dependencies and invariants

These insights are **extremely valuable** when planning your refactoring.

### 2. **Testing the Refactoring**
Having save/load lets you:
```c
// Before refactoring
save_game("test_state_before.sav", &old_gstate);

// After refactoring
load_game("test_state_before.sav", &new_gstate);
verify_games_equivalent(&old_gstate, &new_gstate);
```

This provides **regression testing** during the refactoring process.

---

## Implementation Scope

### What to Save (Minimum Viable):

```c
typedef struct {
    // Version info
    uint32_t format_version;
    uint32_t prng_seed;
    
    // Core game state (from struct gamestate)
    PlayerID current_player;
    uint16_t current_cash_balance[2];
    uint8_t current_energy[2];
    uint16_t turn;
    TurnPhase turn_phase;
    PlayerID player_to_move;
    GameStateEnum game_state;
    
    // Card locations (by index into fullDeck[])
    uint8_t deck_A[40];
    uint8_t deck_A_size;
    uint8_t hand_A[12];
    uint8_t hand_A_size;
    uint8_t discard_A[40];
    uint8_t discard_A_size;
    uint8_t combat_A[3];
    uint8_t combat_A_size;
    
    // Same for player B...
    
    // Player configuration
    PlayerConfig player_config;
    
    // Strategy selection (for AI vs AI)
    AIStrategyType ai_strategies[2];
    
} SaveGameData;
```

### Format Options:

**Option 1: Binary Format (Recommended First)**
```c
// Simple, fast, works immediately
bool save_game_binary(const char* filename, struct gamestate* gs, 
                     config_t* cfg);
bool load_game_binary(const char* filename, struct gamestate* gs, 
                     config_t* cfg);
```

**Pros:**
- Fast to implement (~200 lines total)
- Compact files
- Direct struct serialization

**Cons:**
- Not human-readable
- Version compatibility issues
- Endianness concerns (but you're not targeting different architectures)

**Option 2: Text Format (Future Enhancement)**
```c
// JSON or custom text format
bool save_game_text(const char* filename, struct gamestate* gs, 
                   config_t* cfg);
```

**Pros:**
- Human-readable
- Easy to manually create test cases
- Version-tolerant (can skip unknown fields)

**Cons:**
- More code (~500 lines with parsing)
- Larger files
- Slower to parse

---

## Implementation Strategy

### Phase 1: Binary Save/Load (Folder 5.5/6)

Create `src/save_load.c` and `src/save_load.h`:

```c
// save_load.h
#ifndef SAVE_LOAD_H
#define SAVE_LOAD_H

#include "game_types.h"

#define SAVE_FORMAT_VERSION 1
#define SAVE_FILE_MAGIC 0x4F52434C  // "ORCL" in hex

typedef enum {
    SAVE_OK = 0,
    SAVE_ERR_FILE_OPEN,
    SAVE_ERR_FILE_WRITE,
    SAVE_ERR_INVALID_STATE,
    SAVE_ERR_VERSION_MISMATCH,
    SAVE_ERR_CORRUPTED
} SaveLoadError;

// Save current game state
SaveLoadError save_game(const char* filename, 
                        struct gamestate* gstate,
                        config_t* cfg,
                        GameContext* ctx);

// Load game state
SaveLoadError load_game(const char* filename,
                        struct gamestate* gstate,
                        config_t* cfg,
                        GameContext* ctx);

// Utility: get error message
const char* save_load_error_string(SaveLoadError err);

#endif
```

### Estimated Implementation Time:
- **Binary format**: 4-6 hours
- **CLI integration**: 2 hours
- **Testing**: 2-3 hours
- **Total**: ~8-11 hours

Much less than any of the AI strategies or UI implementations.

---

## Alternative Position: After Folder 9 (TUI)

### Argument FOR Later Position:

If you implement save/load **after TUI**, you get:
- Save from both CLI and TUI
- More polished UI for selecting save files
- File browser integration in TUI

### Argument AGAINST Later Position:

Waiting until after TUI means:
- 🔴 No save/load for ~6-8 weeks of development
- 🔴 Can't use it for debugging during AI development
- 🔴 Can't create standardized test positions
- 🔴 Miss the architectural insights during refactoring

---

## Recommendation: Create Folder 5.5

```
1. Mulligan and Discard Commands
2. Display Discard Pile
3. Recall Card Functionality
4. Combat Results Display
5. Cash Card Selection
5.5. Save/Load Game State  ← INSERT HERE
6. Source Code Structure Refactoring
7. Engine Refactoring for GUI/Network
8.x AI Strategies...
```

### Rationale:

1. ✅ **Immediate value** - helps with testing everything else
2. ✅ **Low complexity** - binary format is straightforward
3. ✅ **Enables better development** - save interesting game states
4. ✅ **Informs refactoring** - reveals architecture issues
5. ✅ **Quick implementation** - ~8-11 hours total
6. ✅ **No major dependencies** - works with current structure

---

## Integration with CLI

Add commands to `stda_cli.c`:

```c
// During game
"save <filename>" - Save current game state
"load <filename>" - Load game state (confirmation dialog)

// At startup
"--resume <filename>" - Resume saved game
"--load <filename>"   - Alternative syntax
```

Example:
```
Player A (Alice) > save interesting_position.sav
[OK] Game saved to 'interesting_position.sav'

Player A (Alice) > load test_case_42.sav
Warning: This will replace the current game. Continue? (y/n): y
[OK] Game loaded from 'test_case_42.sav'
  Turn: 47
  Player A: HP:23 L:5
  Player B: HP:67 L:8
```

---

## Future Enhancements (Post-Folder 9)

Once TUI exists, add:
- File browser in TUI
- Auto-save on exit
- Save state "slots" (save1.sav, save2.sav, etc.)
- Text format for human-readable saves
- Save file metadata (timestamp, turn number, preview)

But none of these are needed for the **core save/load functionality** to be useful.
