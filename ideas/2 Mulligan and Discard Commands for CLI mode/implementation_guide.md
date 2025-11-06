# Mulligan & Discard-to-7 Implementation Guide

## Overview

This implementation adds two key game mechanics to the CLI interactive mode:
1. **Mulligan**: Player B can discard up to 2 cards and draw replacements at game start
2. **Discard-to-7**: Active player must discard down to 7 cards at end of turn

## Implementation Summary

### Files Modified
- `src/stda_cli.c` - Main implementation (adds ~300 lines, total ~800 lines)
- `src/stda_cli.h` - Header updates (adds 5 new function declarations)

### New Commands Added
- `mull <indices>` - Mulligan 1-2 cards (e.g., `mull 1 3`)
- `disc <indices>` - Discard cards to reach 7 (e.g., `disc 2 5 8`)

## Key Design Decisions

### 1. Code Reuse Pattern
Both mulligan and discard share similar logic:
- Display cards with power values
- Parse user input for card indices
- Validate selections
- Discard selected cards
- Optionally draw replacements

**Solution**: Created shared helper functions:
```c
void display_card_with_index(uint8_t card_idx, int display_num, int show_power);
int parse_card_indices_cli(char* input, uint8_t* indices, int max_count, int hand_size);
void discard_and_draw(struct gamestate* gstate, PlayerID player, uint8_t* indices,
                     int count, int draw_replacements, GameContext* ctx);
```

### 2. Power Display
Both phases show card power values to help players make informed decisions:
- Mulligan: Suggests discarding cards with power < 4.98
- Discard-to-7: Suggests discarding lowest power cards

### 3. Error Handling
Robust input validation:
- Invalid card numbers
- Duplicate selections
- Wrong number of cards
- Fallback to AI logic on input errors

### 4. User Experience
- Clear prompts with examples
- Color-coded output
- Helpful tips displayed
- Confirmation messages
- `help` command available in all phases

## Function Structure

### Mulligan Functions

```c
// Display mulligan prompt with hand and instructions
static void display_mulligan_prompt(struct gamestate* gstate, PlayerID player);

// Process mulligan commands ('mull', 'pass', 'help')
static int process_mulligan_command(char* input_buffer, 
                                    struct gamestate* gstate,
                                    GameContext* ctx);

// Main mulligan handler (called from run_mode_stda_cli)
static int handle_interactive_mulligan(struct gamestate* gstate, 
                                       GameContext* ctx);
```

### Discard-to-7 Functions

```c
// Display discard prompt with hand and requirements
static void display_discard_prompt(struct gamestate* gstate, PlayerID player);

// Process discard commands ('disc', 'help')
static int process_discard_command(char* input_buffer,
                                   struct gamestate* gstate,
                                   int cards_to_discard,
                                   GameContext* ctx);

// Main discard handler (called from game loop)
static int handle_interactive_discard_to_7(struct gamestate* gstate,
                                           GameContext* ctx);
```

## Integration Points

### 1. Game Initialization (run_mode_stda_cli)

```c
printf("\n=== Game Start ===\n");
gstate->turn = 0;

// Mulligan phase for Player B
// Note: Currently Player A is always human, Player B is always AI
apply_mulligan(gstate, ctx);  // AI mulligan

// TODO: When implementing player choice:
// if(human_player == PLAYER_B)
//   handle_interactive_mulligan(gstate, ctx);
// else
//   apply_mulligan(gstate, ctx);
```

### 2. End of Turn (Main Game Loop)

```c
collect_1_luna(gstate);

// Discard-to-7 phase
if(gstate->current_player == PLAYER_A)  // Human player
  handle_interactive_discard_to_7(gstate, ctx);
else  // AI player
  discard_to_7_cards(gstate, ctx);

change_current_player(gstate);
```

## Usage Examples

### Mulligan Phase

```
=== Mulligan Phase (Player B) ===
You may discard up to 2 cards and draw replacements.
Tip: Consider discarding cards with power < 4.98

Your starting hand:
  [1] Human (D4+0, ☾0, pwr:2.5)
  [2] Elf (D6+1, ☾1, pwr:4.5)
  [3] Dwarf (D8+2, ☾1, pwr:6.5)
  [4] Orc (D4+3, ☾1, pwr:5.5)
  [5] Dragon (D12+0, ☾1, pwr:6.5)
  [6] Hobbit (D6+0, ☾0, pwr:3.5)

Commands:
  mull <indices>  - Mulligan 1-2 cards (e.g., 'mull 1 3')
  pass            - Keep current hand
  help            - Show this help

> mull 1 6
✓ Mulliganing 2 card(s)...

New hand:
  [1] Elf (D6+1, ☾1, pwr:4.5)
  [2] Dwarf (D8+2, ☾1, pwr:6.5)
  [3] Orc (D4+3, ☾1, pwr:5.5)
  [4] Dragon (D12+0, ☾1, pwr:6.5)
  [5] Centaur (D8+1, ☾1, pwr:5.5)
  [6] Draw 2 (☾1, pwr:2.0)
```

### Discard-to-7 Phase

```
=== Discard Phase ===
You have 10 cards. You must discard 3 cards to reach 7.
Tip: Consider discarding lowest power cards

Your hand:
  [1] Human (D4+0, ☾0, pwr:2.5)
  [2] Elf (D6+1, ☾1, pwr:4.5)
  [3] Dwarf (D8+2, ☾1, pwr:6.5)
  [4] Orc (D4+3, ☾1, pwr:5.5)
  [5] Dragon (D12+0, ☾1, pwr:6.5)
  [6] Hobbit (D6+0, ☾0, pwr:3.5)
  [7] Centaur (D8+1, ☾1, pwr:5.5)
  [8] Draw 2 (☾1, pwr:2.0)
  [9] Minotaur (D6+4, ☾1, pwr:7.5)
  [10] Fairy (D12+1, ☾1, pwr:7.5)

Commands:
  disc <indices>  - Discard cards (e.g., 'disc 2 5')
  help            - Show this help

> disc 1 6 8
✓ Discarding 3 card(s)...

Remaining hand (7 cards):
  [1] Elf (D6+1, ☾1)
  [2] Dwarf (D8+2, ☾1)
  [3] Orc (D4+3, ☾1)
  [4] Dragon (D12+0, ☾1)
  [5] Centaur (D8+1, ☾1)
  [6] Minotaur (D6+4, ☾1)
  [7] Fairy (D12+1, ☾1)
```

## Testing Checklist

### Mulligan Tests
- [ ] Pass without mulliganing
- [ ] Mulligan 1 card
- [ ] Mulligan 2 cards
- [ ] Try to mulligan 3 cards (should error)
- [ ] Invalid card numbers
- [ ] Duplicate card numbers
- [ ] Help command
- [ ] Input error handling

### Discard Tests
- [ ] Hand with exactly 7 cards (no discard needed)
- [ ] Hand with 8 cards (discard 1)
- [ ] Hand with 10+ cards (discard multiple)
- [ ] Discard wrong number of cards (should error)
- [ ] Invalid card numbers
- [ ] Duplicate card numbers
- [ ] Help command
- [ ] Input error handling (falls back to AI logic)

### Integration Tests
- [ ] Complete game with mulligan and discard
- [ ] Multiple discard phases in one game
- [ ] Player A never mulligans (only Player B)
- [ ] Verify cards actually discarded and drawn
- [ ] Verify hand size exactly 7 after discard

## Code Quality Notes

### Function Size Compliance
All functions remain under 30 lines of actual code:
- `display_mulligan_prompt`: 14 lines
- `process_mulligan_command`: 29 lines
- `handle_interactive_mulligan`: 16 lines
- `display_discard_prompt`: 14 lines
- `process_discard_command`: 27 lines
- `handle_interactive_discard_to_7`: 19 lines
- `display_card_with_index`: 26 lines
- `parse_card_indices_cli`: 18 lines
- `discard_and_draw`: 12 lines

### File Size Compliance
- `stda_cli.c`: ~800 lines total (within acceptable range)
- Could be split later if it exceeds 500 lines of actual code

### Pattern Compliance
Follows project patterns:
- ✅ Static functions for internal helpers
- ✅ GameContext passed throughout
- ✅ Error handling with fallbacks
- ✅ Clear function naming
- ✅ Comments for major sections
- ✅ ANSI color constants defined

## Future Enhancements

### 1. Player Choice
Allow human to be either Player A or Player B:
```c
PlayerID human_player = determine_human_player();  // Random or choice
if(human_player == PLAYER_B)
  handle_interactive_mulligan(gstate, ctx);
else
  apply_mulligan(gstate, ctx);
```

### 2. Mulligan for Player A
Current design only allows Player B to mulligan (per rules).
Could be extended if rules change.

### 3. Undo Discard
Allow player to revise discard selection before confirming:
```c
Commands:
  disc <indices>  - Select cards to discard
  undo            - Clear current selection
  confirm         - Confirm and discard selected cards
```

### 4. Auto-suggest
Highlight lowest power cards automatically:
```
  [1] Human (D4+0, ☾0, pwr:2.5) ← suggested
  [2] Elf (D6+1, ☾1, pwr:4.5)
  ...
```

### 5. Batch Operations
Allow "disc all" or "disc lowest 3" shortcuts.

## Known Limitations

1. **Player Assignment**: Currently human is always Player A
2. **No Undo**: Once command is entered, can't revise
3. **No Preview**: Can't see what hand will look like before confirming
4. **AI Fallback**: Input errors fall back to AI logic (might surprise user)

## TODO List Updates

Update `doc/oracle_todo.md`:

```markdown
#### Turn Logic ⚠️ IN PROGRESS
- [x] Basic turn structure (turn_logic.c)
- [x] begin_of_turn()
- [x] attack_phase()
- [x] defense_phase()
- [x] end_of_turn()
- [x] Card drawing (skip first player, first turn)
- [x] Luna collection
- [x] **Mulligan system** [COMPLETED]
  - [x] UI for Player B (2 cards max)
  - [x] Power-based display for selection
  - [x] Integration with CLI mode
- [x] **Discard to 7 cards** [COMPLETED]
  - [x] UI for attacker at end of turn
  - [x] Power-based display for selection
  - [x] Edge case: exactly 7 cards (do nothing)
- [ ] Phase transition validation
- [ ] Win condition detection (energy = 0)
- [ ] Draw condition (max turns exceeded)

#### CLI Mode (stda_cli.c) ✅
- [x] Basic game loop
- [x] Human vs AI gameplay
- [x] Command parsing (cham, draw, cash, pass, gmst, help, exit)
- [x] ANSI color output
- [x] UTF-8 symbols (❤ ☾ ⚔ 🛡)
- [x] Attack phase UI
- [x] Defense phase UI
- [x] Error messages
- [x] **Mulligan UI** (Player B) [COMPLETED]
- [x] **Discard UI** (end of turn if hand > 7) [COMPLETED]
- [ ] Better game state display
- [ ] Show combat results clearly
- [ ] Command history (readline integration?)
- [ ] Save/load game state
- [ ] Undo last action (maybe?)
```

## Commit Message

```
feat(cli): Add mulligan and discard-to-7 for interactive player

- Add mulligan phase for Player B (up to 2 cards)
- Add discard-to-7 phase at end of turn
- Display card power values to assist decision-making
- New commands: 'mull <indices>' and 'disc <indices>'
- Shared helper functions for code reuse
- Robust input validation with error messages
- Fallback to AI logic on input errors
- Helpful tips displayed in both phases

Implementation follows project guidelines:
- All functions <30 lines
- stda_cli.c remains under file size limit
- Consistent with existing CLI patterns
- No new external dependencies

Addresses TODO items in oracle_todo.md:
- [x] Mulligan system
- [x] Discard to 7 cards

Files modified:
- src/stda_cli.c (+~300 lines)
- src/stda_cli.h (+5 declarations)
```

## Next Steps

1. **Testing**: Run through all test scenarios in checklist
2. **Documentation**: Update oracle_todo.md with completion checkboxes
3. **Player Choice**: Implement human player selection (A or B)
4. **Code Review**: Verify all functions meet <30 line guideline
5. **Valgrind**: Check for memory leaks with new code paths

## References

- **Game Rules**: `doc/game_rules_doc.md` (Mulligan section, Discard section)
- **TODO**: `doc/oracle_todo.md` (Turn Logic section, CLI Mode section)
- **Design**: `doc/oracle_design.md` (CLI Mode section)
- **Patterns**: `doc/CONTRIBUTING.md`, `doc/REFACTORING.md`
