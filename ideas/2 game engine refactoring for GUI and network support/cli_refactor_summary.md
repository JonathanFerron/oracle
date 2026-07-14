# CLI Refactoring Architecture Summary

## File Structure

```
src/
├── ui/
│   ├── shared/
│   │   └── ui_callbacks.h          # Generic callback interface (~50 lines)
│   └── cli/
│       ├── cli_callbacks.c/h       # CLI event handlers (~180 lines)
│       ├── cli_display.c/h         # Display functions (~340 lines)
│       └── cli_input.c/h           # Input parsing (~290 lines)
│
├── roles/
│   └── stda/
│       └── stda_game.c/h           # CLI game orchestration (~380 lines)
│
└── core/
    └── game_engine.c/h             # Unified state machine (~300 lines)
```

**Total**: ~1,540 lines (was ~800 in single file)

- Additional code is infrastructure for future features
- Each module is independently testable
- Clear separation enables easy extension

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│                   stda_game.c                           │
│              (Game Loop Orchestrator)                   │
│                                                         │
│  - run_mode_stda_cli() - entry point                   │
│  - stda_game_loop_cli() - main blocking loop           │
│  - Player configuration                                 │
│  - AI vs Human routing                                  │
└──────────┬──────────────────────────────────┬───────────┘
           │                                  │
           ▼                                  ▼
    ┌──────────────┐                  ┌──────────────┐
    │ cli_input.c  │                  │ cli_display.c│
    │              │                  │              │
    │ Parse user   │                  │ Format and   │
    │ commands     │                  │ print info   │
    │              │                  │              │
    │ Return       │                  │ Pure output  │
    │ Action*      │                  │ (no state)   │
    └──────┬───────┘                  └──────▲───────┘
           │                                 │
           │         ┌──────────────┐        │
           └────────►│game_engine.c │────────┘
                     │              │
                     │ State machine│◄───────┐
                     │ engine_step()│        │
                     │              │        │
                     └──────┬───────┘        │
                            │                │
                            ▼                │
                     ┌──────────────┐        │
                     │cli_callbacks │────────┘
                     │              │
                     │ Event hooks  │
                     │ Display via  │
                     │ cli_display  │
                     └──────────────┘
```

---

## Data Flow: Blocking Mode (CLI/TUI)

### Human Player Turn

```
1. stda_game_loop_cli()
   └─> engine_run_until_input(engine, ctx)
       └─> State machine steps until PHASE_*_REQUEST

2. engine_needs_input() == TRUE
   └─> get_human_action(engine, player, cfg)
       └─> cli_get_attack_action(gs, player, cfg)  [BLOCKS HERE]
           └─> User types command
           └─> Parse input
           └─> Validate
           └─> Return Action*

3. engine_submit_action(engine, action)
   └─> advance_to_resolve_phase(engine)

4. Loop back to step 1
```

### AI Player Turn

```
1. stda_game_loop_cli()
   └─> engine_run_until_input(engine, ctx)

2. engine_needs_input() == TRUE
   └─> get_ai_action(engine, player, strategies, ctx)
       └─> strategy->attack_strategy(vgs, ctx)  [NON-BLOCKING]
       └─> Return Action* immediately

3. engine_submit_action(engine, action)
   └─> advance_to_resolve_phase(engine)

4. Loop back to step 1
```

### Event Callbacks (During Resolution)

```
engine_step() in RESOLVE phase
   ├─> apply_action(gs, action, ctx)
   │   └─> play_champion(gs, player, card_id, ctx)
   │       └─> callbacks->on_card_played(player, card_id, ACTION_PLAY_CHAMPION, ui_ctx)
   │           └─> cli_on_card_played()
   │               └─> cli_display_card_played()
   │                   └─> printf(...)
   │
   └─> resolve_combat(gs, ctx)
       └─> callbacks->on_combat_resolved(result, ui_ctx)
           └─> cli_on_combat_resolved()
               └─> cli_display_combat_resolution()
                   └─> printf(...)
```

---

## Key Design Decisions

### 1. **Game Owns Loop (Blocking)**

```c
void stda_game_loop_cli(config_t* cfg, GameContext* ctx,
                        StrategySet* strategies) {
    while (phase != GAME_OVER) {
        engine_run_until_input(engine, ctx);  // Steps until blocked

        if (engine_needs_input(engine)) {
            Action* action = get_human_or_ai_action(...);  // May block
            engine_submit_action(engine, action);
        }
    }
}
```

**Why:** CLI/TUI don't need continuous UI updates. Blocking on input is natural and efficient.

### 2. **Actions as First-Class Objects**

```c
// Old way (direct mutation):
play_champion(gstate, player, card_idx, ctx);

// New way (action-based):
Action* action = action_create_play_champions(player, card_ids, count);
if (validate_action(action, gs)) {
    engine_submit_action(engine, action);
}
```

**Why:** 

- Validation separate from execution
- Easy to serialize for network
- Enables undo/replay
- Clear separation of concerns

### 3. **Callbacks for Events**

```c
// Engine emits events during state transitions:
callbacks->on_card_drawn(player, card_id, ui_ctx);
callbacks->on_combat_resolved(result, ui_ctx);

// CLI implements:
void cli_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx) {
    cli_display_card_drawn(player_name, card);  // Display immediately
}

// TUI would implement:
void tui_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx) {
    queue_animation(player, card_id);  // Add to animation queue
}

// Server would implement:
void server_on_card_drawn(PlayerID player, uint8_t card_id, void* ctx) {
    broadcast_event(ALL_CLIENTS, EVENT_CARD_DRAWN, player, card_id);
}
```

**Why:**

- UI can respond to events without tight coupling
- Same engine works for CLI, TUI, GUI, server
- Easy to add logging, replay, etc.

### 4. **State Machine is Pollable**

```c
// Returns TRUE if advanced, FALSE if blocked
bool engine_step(GameEngine* engine, GameContext* ctx) {
    if (waiting_for_input) return false;

    switch (phase) {
        case PHASE_ATTACK_REQUEST:
            waiting_for_input = true;
            return false;  // Stop here

        case PHASE_ATTACK_RESOLVE:
            apply_action(gs, pending_action, ctx);
            phase = PHASE_DEFENSE_REQUEST;
            return true;  // Keep going
    }
}
```

**Why:**

- Blocking mode: `while (engine_step(engine, ctx)) {}`
- Event-driven mode: `if (engine_step(engine, ctx)) { render(); }`
- Same engine, different usage patterns

---

## Comparison with Current Code

### Current (stda_cli.c)

```c
// Monolithic function (~100 lines)
static int handle_interactive_attack(struct gamestate* gstate, 
                                     PlayerID player, GameContext* ctx,
                                     config_t* cfg) {
    // Display code mixed in
    printf("Your hand:\n");
    display_player_hand(player, gstate, cfg);

    // Input parsing mixed in
    char input[256];
    fgets(input, sizeof(input), stdin);

    // Validation mixed in
    if (total_cost > gstate->current_cash_balance[player]) {
        printf("Error: Not enough lunas\n");
        return NO_ACTION;
    }

    // State mutation mixed in
    play_champion(gstate, player, card_idx, ctx);

    // Everything in one place - hard to test, hard to extend
}
```

### Refactored (4 modules)

```c
// cli_display.c - Pure output
void cli_display_hand(PlayerID player, const GameState* gs) {
    printf("Your hand:\n");
    // ... just formatting and printing
}

// cli_input.c - Parse and validate
Action* cli_get_attack_action(const GameState* gs, PlayerID player,
                              config_t* cfg) {
    char input[256];
    fgets(input, sizeof(input), stdin);

    ParsedInput parsed;
    parse_input_line(input, &parsed);

    Action* action = create_champion_action(gs, player, ...);

    if (validate_champion_action(action, gs, cfg)) {
        return action;  // Valid action, ready to submit
    }

    free_action(action);
    return NULL;  // Try again
}

// stda_game.c - Orchestration
void stda_game_loop_cli(...) {
    engine_run_until_input(engine, ctx);

    if (engine_needs_input(engine)) {
        cli_display_hand(player, gs);  // Display
        Action* action = cli_get_attack_action(gs, player, cfg);  // Input
        engine_submit_action(engine, action);  // Execute
    }
}

// cli_callbacks.c - Event response
void cli_on_card_played(PlayerID player, uint8_t card_id, ...) {
    cli_display_card_played(player_name, card);  // Notify user
}
```

**Benefits:**

- Each function < 35 lines
- Each file < 500 lines
- Easy to test in isolation
- Clear responsibilities
- Easy to swap implementations

---

## Future Extensions

### TUI (Terminal UI with ncurses)

Replace only 2 files:

```
ui/tui/
├── tui_display.c    # ncurses rendering (instead of printf)
└── tui_input.c      # ncurses input (instead of fgets)
```

Keep:

- `tui_callbacks.c` (similar to CLI)
- `stda_game.c` (same orchestration)
- `game_engine.c` (same state machine)

### GUI (Event-driven, UI owns loop)

```c
// UI main loop
void run_gamegui_loop(GUIContext* gui) {
    while (running) {
        handle_sdl_events();           // Non-blocking
        engine_step(engine, ctx);      // Non-blocking
        update_animations(dt);         // Always runs
        render();                      // Always runs
    }
}

// Still uses same engine and callbacks!
```

### Client/Server

**Server:**

```c
void server_game_loop(Session* session) {
    while (phase != GAME_OVER) {
        engine_run_until_input(engine, ctx);

        if (engine_needs_input(engine)) {
            Action* action = server_receive_action(session, player);
            engine_submit_action(engine, action);

            // Broadcast result
            server_broadcast_state(session, engine);
        }
    }
}
```

**Client:**

```c
// Replace cli_input.c with:
Action* client_get_action_from_server(ClientContext* client) {
    return network_receive_action(client->connection);
}

// Orchestration stays similar!
```

---

## Testing Strategy

### Unit Tests

**cli_display.c:**

```c
void test_display_hand() {
    GameState gs = create_mock_gamestate();
    capture_stdout();

    cli_display_hand(PLAYER_A, &gs);

    char* output = get_captured_stdout();
    assert(strstr(output, "Your hand:") != NULL);
    assert(strstr(output, "[1]") != NULL);
}
```

**cli_input.c:**

```c
void test_parse_champion_command() {
    GameState gs = create_mock_gamestate();
    mock_stdin("cham 1 2 3\n");

    Action* action = cli_get_attack_action(&gs, PLAYER_A, &cfg);

    assert(action != NULL);
    assert(action->type == ACTION_PLAY_CHAMPIONS);
    assert(action->data.play_champions.num_cards == 3);
}
```

**cli_callbacks.c:**

```c
void test_callback_displays_card_drawn() {
    config_t cfg = create_mock_config();
    UICallbacks* callbacks = cli_create_callbacks(&cfg);
    capture_stdout();

    callbacks->on_card_drawn(PLAYER_A, 42, callbacks->ui_context);

    char* output = get_captured_stdout();
    assert(strstr(output, "draws") != NULL);
}
```

### Integration Tests

```c
void test_full_attack_phase() {
    config_t cfg = create_test_config();
    GameEngine* engine = init_cli_game(&cfg, &callbacks);

    // Simulate human input
    mock_stdin("cham 1 2\npass\n");

    // Run until game over
    stda_game_loop_cli(&cfg, ctx, strategies);

    // Verify final state
    assert(engine_get_phase(engine) == PHASE_GAME_OVER);
}
```

---

## Migration Checklist

- [ ] **Week 1: Structure**
  
  - [ ] Create directory structure
  - [ ] Create empty files with headers
  - [ ] Update Makefile
  - [ ] Verify compilation

- [ ] **Week 1-2: Display Module**
  
  - [ ] Move display functions to cli_display.c
  - [ ] Update function signatures
  - [ ] Remove code duplication
  - [ ] Test with mock GameState

- [ ] **Week 2: Input Module**
  
  - [ ] Define Action structures
  - [ ] Move parsing to cli_input.c
  - [ ] Convert to return Action*
  - [ ] Test validation logic

- [ ] **Week 2-3: Callback System**
  
  - [ ] Define UICallbacks interface
  - [ ] Implement CLI callbacks
  - [ ] Thread through engine
  - [ ] Verify events fire correctly

- [ ] **Week 3-4: State Machine Integration**
  
  - [ ] Refactor to use GameEngine
  - [ ] Replace direct calls with engine_step()
  - [ ] Update orchestration logic
  - [ ] Full integration testing

- [ ] **Week 4: Cleanup**
  
  - [ ] Remove old stda_cli.c
  - [ ] Update all includes
  - [ ] Final testing
  - [ ] Documentation

---

## Summary

This refactoring achieves:

✅ **Maintainability**: Each file < 500 lines, each function < 35 lines  
✅ **Testability**: Pure functions, easy to mock  
✅ **Extensibility**: TUI = swap 2 files, GUI = same engine different loop  
✅ **Network Ready**: Action-based, callbacks map to events  
✅ **Unified Architecture**: Same state machine for all modes  
✅ **Clear Separation**: Display, Input, Orchestration, Events are distinct  

The additional ~700 lines are **infrastructure investment** that pays off:

- Enables client/server with minimal refactoring
- TUI becomes trivial to add
- GUI uses same engine
- All future UIs benefit from this foundation
