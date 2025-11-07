# stda_cli.c Modularization Strategy

Given the upcoming features and architectural evolution toward client/server separation, I recommend a **4-module split** rather than the originally proposed 3-module approach. Here's the conceptual breakdown:

---

## Proposed Module Structure

### 1. **stda_cli_display.c** (~300-350 lines)

**Purpose:** Pure presentation layer - formats and displays information to the user

**Responsibilities:**

- Display player prompts with status (energy, lunas, hand size)
- Display hands with card details (champions, draw cards, cash cards)
- Display game status summaries
- Display combat state (attackers in combat zone)
- Display discard pile contents (summary and detailed views)
- Display combat resolution details (dice rolls, totals, damage)
- Display mulligan and discard-to-7 prompts
- Display help text and command lists
- Display final game summary
- **Key principle:** All functions receive data structures and cfg, return void, only call printf/localization macros

**Why separate:**

- Easy to adapt for TUI (replace printf with ncurses)
- Localization changes isolated here
- Display logic doesn't need game state mutation
- Can be tested by capturing stdout

---

### 2. **stda_cli_input.c** (~250-300 lines)

**Purpose:** Parse and validate user input, convert to validated actions

**Responsibilities:**

- Parse command strings (tokenization, argument extraction)
- Validate card indices (range checking, duplicate detection)
- Validate affordability (cost vs available cash)
- Validate card types for specific commands
- Parse mulligan selections
- Parse discard selections
- Parse recall selections (which champions to recall from discard)
- Parse deck building/drafting choices
- Command routing (identify which command was entered)
- **Key principle:** Functions receive input strings and game state, return validated action structs or error codes

**Why separate:**

- Input parsing is complex and error-prone
- Validation logic should be testable in isolation
- Preparing for action-based architecture (client sends validated actions to server)
- Can implement input history/autocomplete here later

---

### 3. **stda_cli_game.c** (~350-400 lines)

**Purpose:** Orchestrate game flow and player interaction loops

**Responsibilities:**

- Main CLI mode entry point (`run_mode_stda_cli`)
- Player configuration flow (type selection, names, strategies, assignment)
- Deck formation/drafting orchestration
  - Random deck setup
  - Monochrome deck building
  - Custom deck building
  - Solomon 7×7 drafting loop
  - Draft 12×8 drafting loop
  - Draft 1-2-3 drafting loop
- Mulligan phase orchestration (interactive or AI)
- Interactive attack phase loop (prompt → input → validate → execute)
- Interactive defense phase loop (prompt → input → validate → execute)
- Discard-to-7 phase orchestration
- Turn execution coordination (attack → defense → combat → end)
- Game initialization and cleanup
- **Key principle:** This is the "director" - calls display functions, calls input functions, calls game logic functions, manages state transitions

**Why separate:**

- Game flow logic is distinct from presentation and parsing
- Preparing for state machine architecture (phases as explicit states)
- Makes it clear where to inject AI vs human decision-making
- Isolates the "glue code" that coordinates other modules

---

### 4. **stda_cli_callbacks.c** (~150-200 lines) **[NEW]**

**Purpose:** Bridge between game engine and UI - handle engine events and display them

**Responsibilities:**

- Implement UI callback functions for game events:
  - `on_card_drawn(player, card_id, ui_ctx)` - Display which card was drawn
  - `on_card_played(player, card_id, action_type, ui_ctx)` - Display card play confirmation
  - `on_combat_resolved(combat_details, ui_ctx)` - Display detailed combat results
  - `on_phase_transition(old_phase, new_phase, ui_ctx)` - Display phase changes
  - `on_mulligan_complete(player, num_cards, ui_ctx)` - Display mulligan results
  - `on_discard_complete(player, num_cards, ui_ctx)` - Display discard results
- Callback registration and context management
- Format combat details structure for display
- **Key principle:** These are the hooks that let the game engine communicate events to the CLI without tight coupling

**Why add this module:**

- **Critical for client/server separation:** Server will need to send event notifications to clients
- **Prepares for action-response pattern:** Engine generates events → callbacks display them
- **Testability:** Can inject mock callbacks to verify engine generates correct events
- **Reusability:** Same callback pattern works for CLI, TUI, and GUI (just different implementations)
- **Debugging:** Can log all events without modifying game engine

---

## Dependency Flow

```
stda_cli_game.c (orchestrator)
    ↓ calls
    ├─→ stda_cli_display.c (presents info)
    ├─→ stda_cli_input.c (validates input)
    ├─→ stda_cli_callbacks.c (receives engine events)
    ├─→ game engine (turn_logic, card_actions, combat, etc.)
    └─→ player_config.c, player_selection.c

stda_cli_callbacks.c
    ↓ calls
    └─→ stda_cli_display.c (formats event data for display)

game engine
    ↓ calls
    └─→ stda_cli_callbacks.c (notifies of events)
```

---

## Benefits of This Structure

### For Current Features

1. **Mulligan/Discard-to-7:** Game orchestrator calls display prompt → input parser → validator → engine, all cleanly separated
2. **Combat Details:** Engine calls callback → callback formats data → display renders it
3. **Discard Pile Display:** Game orchestrator detects "shod" command → calls display function directly
4. **Recall Cards:** Input parser handles "draw vs recall" choice → validates champion selection → returns validated action

### For Future Features (Client/Server)

1. **Server:** Uses same game engine, but no callbacks (or logging callbacks)
2. **Network Client:** Replaces `stda_cli_input.c` with network message receiver, keeps display and game orchestration
3. **Action-Response Pattern:** Input parser already produces validated actions → easy to serialize for network
4. **Event Notifications:** Callbacks already structured for event-driven architecture → easy to send over network

### For Future Features (TUI/GUI)

1. **TUI:** Replace display.c with ncurses version, keep input/game/callbacks
2. **GUI:** Replace display.c with SDL3 rendering, replace input.c with event handling, keep game orchestration pattern

### For Testability

1. **Display:** Capture stdout, verify formatting
2. **Input:** Feed test strings, verify parsing/validation
3. **Game:** Inject mock display/input, verify orchestration logic
4. **Callbacks:** Inject mock display, verify correct events triggered

---

## File Size Estimates

| Module               | Estimated Lines | Current Violations            |
| -------------------- | --------------- | ----------------------------- |
| stda_cli_display.c   | 300-350         | Within 500 limit ✓            |
| stda_cli_input.c     | 250-300         | Within 500 limit ✓            |
| stda_cli_game.c      | 350-400         | Within 500 limit ✓            |
| stda_cli_callbacks.c | 150-200         | Within 500 limit ✓            |
| **Total**            | **1050-1250**   | Down from ~800 in single file |

**Note:** Total increases because:

- Additional infrastructure for callbacks
- Clearer separation requires some interface code
- But each module is maintainable and focused
- Prepares for much larger future expansion

---

## Migration Strategy

### Phase 1: Extract Callbacks (Immediate)

- Create `stda_cli_callbacks.c` with stub implementations
- Thread callback context through game engine functions
- Verify no behavior change

### Phase 2: Extract Display (Next)

- Move all display functions to `stda_cli_display.c`
- Update includes and function signatures
- Test display in isolation

### Phase 3: Extract Input (Then)

- Move parsing/validation to `stda_cli_input.c`
- Define action structures
- Test input parsing independently

### Phase 4: Refactor Game Orchestration (Finally)

- Clean up `stda_cli_game.c` to only coordinate
- Remove inline display/input logic
- Verify full integration

---

## Key Design Principles Maintained

1. **≤500 lines per file:** All modules comply
2. **≤35 lines per function:** Splitting makes this easier
3. **Separation of concerns:** Each module has one job
4. **Testability:** Each module can be tested in isolation
5. **Extensibility:** Easy to add new display modes, input methods, game features
6. **Client/Server ready:** Callback architecture maps directly to network events

---

## Conclusion

The **4-module split** (display, input, game, callbacks) is superior to the original 3-module proposal because:

- **It explicitly prepares for client/server architecture** via the callback module
- **It maintains strict separation** between presentation, validation, and orchestration
- **It makes the action-response pattern explicit** through the input validator
- **It stays within file size limits** while being more maintainable than a single file
- **It provides clear extension points** for TUI, GUI, network modes, and new game features

This structure will save significant refactoring work when implementing the server separation, while making the current CLI mode more maintainable and testable immediately.
