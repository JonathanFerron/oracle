

## Refactoring of stda_cli in 4 modules and redesign of game loop architecture



## The Problem

In your current design, `stda_game_loop_cli()` is **synchronous** - it blocks waiting for input:

```c
// Current (blocking) approach
void stda_game_loop_cli() {
    while (!game_over) {
        render_game_state();
        Action* action = wait_for_user_input();  // BLOCKS HERE
        apply_action_to_engine(action);
    }
}
```

This works fine for CLI/TUI but is **terrible** for GUI because:

- GUI freezes while waiting for input
- No animations, hover effects, or window updates
- User expects immediate feedback to mouse movement

## Solution: Event-Driven Architecture

The GUI should **own the main loop** and call into the game engine, not vice versa. Here's how:

### Architecture A: GUI-Driven Flow (Recommended)

```
Main Loop (gamegui)
    ↓
Event Handling → Input Translation → Action Creation
    ↓                                      ↓
Game State Query ← stda_game.c or client_game.c
    ↓
Rendering (always responsive)
```

### Implementation## Comparison: Blocking vs Event-Driven

### Blocking (CLI/TUI)

```c
// Game owns the loop
void stda_game_loop_cli() {
    while (!game_over) {
        display();
        action = wait_for_input(); // BLOCKS
        process(action);
    }
}
```

**Pros:**

- Simple, linear flow
- Easy to reason about
- Fine for text-based UIs

**Cons:**

- UI freezes during input
- No animations or updates
- Poor for GUI/mobile

### Event-Driven (GUI)

```c
// UI owns the loop
void run_gamegui_loop() {
    while (running) {
        handle_events();        // Immediate
        update_game(dt);        // Non-blocking
        update_ui(dt);          // Always runs
        render();               // Always runs
    }
}
```

**Pros:**

- UI always responsive
- Animations/hover effects work
- Proper GUI behavior
- Scales to mobile touch

**Cons:**

- More complex architecture
- Need state management

## Client/Server Consideration

For **network mode**, the architecture becomes even more important:## Recommended Architecture

**Use event-driven architecture with GUI owning the main loop:**

### Benefits

1. **UI always responsive** - No freezing during AI turns or network delays
2. **Role-agnostic UI** - Same GUI code works for standalone and client
3. **Scales to mobile** - Touch events work naturally
4. **Network-friendly** - Client can poll network without blocking
5. **Animations work** - Update loop runs continuously

### File Organization

```
ui/gamegui/
├── gamegui_display.c/h    # Main loop, rendering (~400 lines)
├── gamegui_input.c/h      # Event handling (~300 lines)
├── gamegui_callbacks.c/h  # UI callbacks (~200 lines)
└── card_renderer.c/h      # Card compositing (~350 lines)

roles/stda/
├── stda_main.c/h          # Entry point
├── stda_game.c/h          # Game loop contexts (~250 lines)
└── stda_cli.c/h           # CLI blocking loops (~300 lines)

roles/client/
├── client_main.c/h        # Entry point
└── client_game.c/h        # Network game loop (~300 lines)
```

### Summary

- **CLI/TUI**: Use blocking loops (game owns loop)
- **GUI**: Use event-driven (UI owns loop)
- **Both share**: Same game engine, action system, validation
- **Network**: Client polls network in update loop, never blocks

The key insight is that **different UIs need different control flow**, but they can all use the same underlying game engine through a thin abstraction layer.



Yes! You can absolutely create a **single unified state machine** that works elegantly with both approaches. The key is designing the state machine to be **poll-able** rather than **blocking**.

## Unified State Machine Design## Usage in Different Modes## Key Design Principles

### 1. **State Machine is Poll-able**

```c
// Returns TRUE if advanced, FALSE if blocked
bool engine_step(GameEngine* engine, GameContext* ctx);
```

- **Blocking modes** call in loop: `while (engine_step(engine, ctx)) {}`
- **Event-driven modes** call once per frame: `engine_step(engine, ctx)`

### 2. **Clear Input Request States**

```c
PHASE_ATTACK_REQUEST   // Waiting
PHASE_ATTACK_RESOLVE   // Processing
```

- **REQUEST phases** set `waiting_for_input = true`
- **RESOLVE phases** apply the action and advance

### 3. **Action Submission is Separate**

```c
engine_submit_action(engine, action);  // Queue action
advance_to_resolve_phase(engine);      // Trigger processing
```

This allows validation before commitment.

### 4. **No Mode-Specific Code in Engine**

The engine doesn't know or care about:

- Whether it's CLI, GUI, or network
- Whether player is human or AI
- What UI is displaying

## Benefits

| Feature                  | CLI (Blocking) | GUI (Event-Driven) | Server | Auto-Sim |
| ------------------------ | -------------- | ------------------ | ------ | -------- |
| **Same engine**          | ✅              | ✅                  | ✅      | ✅        |
| **No code duplication**  | ✅              | ✅                  | ✅      | ✅        |
| **UI can be responsive** | N/A            | ✅                  | N/A    | N/A      |
| **Can run fast**         | ✅              | ❌                  | ✅      | ✅        |
| **Network compatible**   | ❌              | ✅                  | ✅      | ❌        |

## Visual Comparison

```
┌─────────────────────────────────────────┐
│         CLI Mode (Game Owns Loop)       │
├─────────────────────────────────────────┤
│  while (!game_over) {                   │
│    engine_run_until_input(engine);      │ ◄── Blocks here
│    display_state();                     │
│    action = wait_for_input();          │ ◄── Blocks here
│    engine_submit_action(action);        │
│  }                                      │
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│        GUI Mode (UI Owns Loop)          │
├─────────────────────────────────────────┤
│  while (running) {                      │
│    handle_events();         ◄── Never blocks
│    engine_step(engine);     ◄── Non-blocking
│    if (needs_input && is_ai) {          │
│      auto_submit_ai_action();           │
│    }                                    │
│    update_animations();     ◄── Always runs
│    render();                ◄── Always runs
│  }                                      │
└─────────────────────────────────────────┘
```

## File Organization

```
core/
├── game_engine.c/h        # State machine (~300 lines)
├── turn_logic.c/h         # execute_begin_turn(), etc. (~250 lines)
├── action_processor.c/h   # apply_action() (~400 lines)
└── combat.c/h             # resolve_combat() (~300 lines)

roles/stda/
├── stda_game.c/h          # Blocking wrappers for CLI/TUI (~200 lines)
└── stda_game_gui.c/h      # Non-blocking wrappers for GUI (~150 lines)

roles/client/
└── client_game.c/h        # Network + state machine (~300 lines)

roles/server/
└── server_game.c/h        # Network + state machine (~300 lines)
```

## Summary

**Yes, one state machine works for both!** The key is:

1. ✅ **Fine-grained phases** (REQUEST/RESOLVE pairs)
2. ✅ **Non-blocking step function** returns bool
3. ✅ **Separate input submission** from processing
4. ✅ **Helper functions** for different usage patterns:
   - `engine_step()` - one step (GUI)
   - `engine_run_until_input()` - batch steps (CLI)
   - `engine_run_turn()` - complete turn (auto-sim)

The state machine itself is **mode-agnostic** - it's just a clean implementation of your game rules. The **different modes** are just different **wrappers** around the same core engine.



Now let me create a visual summary document showing the complete architecture:## Summary

The refactoring breaks `stda_cli.c` (800 lines) into **4 focused modules** (~1,540 lines total):

### The 4 Modules

1. **`cli_callbacks.c`** (~180 lines) - Bridge between engine events and display
2. **`cli_display.c`** (~340 lines) - Pure presentation layer (printf only)
3. **`cli_input.c`** (~290 lines) - Parse/validate input → return `Action*`
4. **`stda_game.c`** (~380 lines) - Orchestrate game flow using state machine

### Key Integration Points

**With Unified State Machine:**

- Uses `engine_run_until_input()` for blocking behavior
- Calls `engine_needs_input()` to check for human turns
- Submits actions via `engine_submit_action()`
- State machine handles all phase transitions

**With Action System:**

- Input parsing returns validated `Action*` objects
- Actions are submitted, not executed directly
- Prepares for network serialization

**With Callbacks:**

- Engine fires events during state transitions
- CLI callbacks immediately display results
- Same pattern works for TUI, GUI, server

### Benefits

✅ All files < 500 lines (well within limits)  
✅ Blocking mode works naturally (game owns loop)  
✅ Same engine enables event-driven GUI later  
✅ Ready for client/server with minimal changes  
✅ Each module independently testable  
✅ TUI = swap 2 files, keep rest

The architecture gracefully supports both "game owns loop" (CLI/TUI) and "UI owns loop" (GUI) using the same unified state machine!
