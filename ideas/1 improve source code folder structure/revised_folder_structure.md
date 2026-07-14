# STDA Source Code Folder Structure v4

## Conceptual Model

Modes are defined as **(role, ui)** tuples:

- **Roles**: `stda` (standalone), `client`, `server`
- **UIs**: `cli`, `tui`, `gui`, `simauto`, `simtui`, `servercli`

## Key Architectural Decisions

### Unified State Machine

All modes use the same **pollable state machine** (`core/game_engine.c`):

- **Blocking modes** (CLI/TUI): Game owns loop, calls `engine_run_until_input()`
- **Event-driven modes** (GUI): UI owns loop, calls `engine_step()` per frame
- **Server mode**: Blocking loop with network I/O

### Action-Based Architecture

- All player decisions produce `Action*` objects
- Actions are validated before submission
- Engine processes actions in RESOLVE phases
- Prepares for network serialization

### Callback System

- Engine emits events during state transitions
- UIs implement callbacks to respond to events
- Same pattern for CLI, TUI, GUI, and server

---

## Complete Directory Structure

```
src/
├── core/                      # Platform-agnostic game engine
│   ├── game_types.h          # All enums and struct definitions
│   ├── game_constants.c/h    # Full deck, static data, enums
│   ├── game_state.c/h        # Game initialization, setup
│   ├── game_engine.c/h       # Unified state machine (~300 lines)
│   ├── turn_logic.c/h        # Turn flow helpers (execute_begin_turn, etc.)
│   ├── card_actions.c/h      # Card playing mechanics
│   ├── combat.c/h            # Combat resolution
│   ├── combo_bonus.c/h       # Combo calculations
│   └── game_context.c/h      # Context pattern implementation
│
├── structures/                # Data structure implementations
│   ├── deckstack.c/h         # Fixed-size stack for decks
│   ├── hdcll.c/h            # Circular linked list (to be deprecated)
│   └── fixed_arrays.c/h      # Future: fixed-size array replacements
│
├── util/                      # Utility functions
│   ├── rnd.c/h               # RNG wrapper functions
│   ├── mtwister.c/h          # Mersenne Twister PRNG
│   ├── prng_seed.c/h         # Seed management
│   ├── debug.h               # Debug macros
│   └── logger.c/h            # Logging system (future)
│
├── ai_strat/                # AI strategy implementations
│   ├── ai_strategy.c/h          # Strategy framework (function pointers)
│   ├── aistrat_random.c/h      # Random AI
│   ├── aistrat_balanced.c/h    # Balanced rules AI
│   ├── aistrat_heuristic.c/h   # Heuristic AI
│   ├── aistrat_hybrid.c/h      # Hybrid AI
│   ├── aistrat_simplemc.c/h    # Simple Monte Carlo AI
│   ├── aistrat_ismcts.c/h      # IS-MCTS AI
│   └── ai_params.c/h         # AI parameter management
│
├── deck_formats/              # Deck construction methods
│   ├── deck_random.c/h       # Random deck distribution
│   ├── deck_monochrome.c/h   # Monochrome deck builder
│   ├── deck_custom.c/h       # Custom deck builder
│   ├── deck_solomon.c/h      # Solomon 7×7 draft
│   ├── deck_draft12x8.c/h    # Draft 12×8
│   ├── deck_draft123.c/h     # Draft 1-2-3
│   └── deck_draft_common.c/h      # Shared draft utilities
│
├── game_rules/                # Optional game depth additions
│   ├── abilities.c/h         # Champion abilities (Berserker, Vampire)
│   ├── momentum.c/h          # Momentum token system
│   └── order_powers.c/h      # Order ultimate powers
│
├── roles/                     # Role-specific orchestration
│   ├── stda/
│   │   ├── stda_main.c/h     # Standalone entry point dispatcher
│   │   └── stda_game.c/h     # Standalone game loops (~380 lines)
│   │                         # Functions: stda_game_loop_cli(), _tui(), _gui(), _simauto(), _simtui()
│   │                         # Uses engine_run_until_input() for blocking modes
│   │                         # Uses engine_step() for event-driven GUI
│   │
│   ├── client/
│   │   ├── client_main.c/h   # Client entry point dispatcher
│   │   ├── client_game.c/h   # Client game loops (network → engine)
│   │   │                     # Functions: client_game_loop_cli(), _tui(), _gui()
│   │   ├── client_state.c/h  # Visible state management
│   │   └── client_network.c/h # Network communication
│   │
│   └── server/
│       ├── server_main.c/h   # Server entry point
│       ├── server_game.c/h   # Server game loop (full engine)
│       ├── server_state.c/h  # Full game state management
│       ├── session_manager.c/h # Game session handling
│       └── matchmaking.c/h   # Matchmaking logic
│
├── ui/                        # UI implementations (role-agnostic)
│   ├── shared/
│   │   ├── ui_callbacks.h        # Callback interface definition (~50 lines)
│   │   ├── ui_context.c/h        # Context pattern for callbacks
│   │   ├── localization.h        # I18n support
│   │   ├── player_selection.c/h  # Player type selection
│   │   └── player_config.c/h     # Player name/strategy config
│   │
│   ├── cli/                   # Text-based interface (interactive)
│   │   ├── cli_display.c/h   # Output formatting (~340 lines)
│   │   ├── cli_input.c/h     # Input parsing (~290 lines)
│   │   └── cli_callbacks.c/h # Event handlers (~180 lines)
│   │
│   ├── tui/                   # Terminal UI (ncurses)
│   │   ├── tui_display.c/h   # ncurses rendering (~400 lines)
│   │   ├── tui_input.c/h     # ncurses input handling (~300 lines)
│   │   ├── tui_callbacks.c/h # Event handlers (~200 lines)
│   │   └── tui_windows.c/h   # Window management (helper)
│   │
│   ├── gui/               # Game GUI (SDL3 for gameplay)
│   │   ├── gui_display.c/h    # SDL3 rendering coordinator (~350 lines)
│   │   ├── gui_input.c/h      # SDL3 event handling (~250 lines)
│   │   ├── gui_callbacks.c/h  # Event handlers (~200 lines)
│   │   ├── card_renderer.c/h      # Card compositing (helper)
│   │   ├── font_manager.c/h       # Font loading (helper)
│   │   ├── texture_cache.c/h      # Image caching (helper)
│   │   └── layout.c/h             # Responsive layout (helper)
│   │
│   ├── simulation/                # Simulation and analysis
│   │   ├── sim_engine.c/h        # core simulation engine
│   │   ├── sim_export.c/h        # CSV export
│   │   ├── sim_stats.c/h         # Statistics calculation
│   │   │
│   │   ├── simauto/                  # Non-interactive CLI (automation)
│   │   │   ├── simauto_display.c/h  # Minimal output (progress, summary)
│   │   │   └── simauto_callbacks.c/h # Event handlers (logging only)
│   │   │
│   │   └── simtui/                # Simulation TUI (ncurses visualization)
│   │       ├── simtui_display.c/h     # Stats/graph rendering
│   │       ├── simtui_input.c/h       # Control input (pause/resume/speed)
│   │       ├── simtui_callbacks.c/h   # Event handlers
│   │       └── simtui_stats_visualizer.c/h   # Real-time stats (helper)
│   │
│   └── servercli/             # Server admin CLI
│       ├── servercli_display.c/h  # Server status formatting
│       ├── servercli_input.c/h    # Admin command parsing
│       └── servercli_commands.c/h # Command handlers (list, show, stats, etc.)
│
├── interactive/               # Interactive player support
│   ├── mulligan.c/h          # Mulligan UI and logic
│   ├── discard.c/h           # Discard-to-7 UI and logic
│   ├── recall.c/h            # Recall card selection
│   └── deck_builder.c/h      # Interactive deck building
│
├── actions/                   # Action system
│   ├── action.c/h            # Action structures and creation
│   ├── action_generator.c/h  # Generate legal moves
│   ├── action_validator.c/h  # Validate actions
│   └── action_processor.c/h  # Apply actions (server-side, ~400 lines)
│
├── visibility/                # Information hiding (for network)
│   ├── visible_state.c/h     # VisibleGameState conversion
│   └── state_filter.c/h      # Filter hidden information
│
├── network/                   # Protocol definitions (shared)
│   ├── protocol.c/h          # Message format
│   ├── serialization.c/h     # State serialization
│   ├── socket_utils.c/h      # Socket wrappers
│   └── crypto.c/h            # Authentication/checksums
│
├── rating/                    # Bradley-Terry rating system
│   ├── rating.c/h            # Core BT calculations
│   ├── rating_batch.c/h      # Batch processing
│   ├── rating_export.c/h     # CSV persistence
│   └── calibration.c/h       # Parameter optimization
│
├── config/                    # Configuration management
│   ├── config.c/h            # INI file parser
│   ├── ai_config.c/h         # AI parameter loading
│   └── config_defaults.c/h   # Default values
│
├── persistence/               # Save/load functionality
│   ├── save_game.c/h         # Game state persistence
│   └── load_game.c/h         # Game state restoration
│
├── platform/                  # Platform-specific code
│   ├── platform_windows.c/h  # Windows-specific
│   ├── platform_linux.c/h    # Linux-specific
│   ├── platform_ios.m/h      # iOS-specific
│   └── platform_android.c/h  # Android-specific
│
└── main/                      # Entry point
    ├── main.c                 # Main dispatcher
    ├── cmdline.c/h            # Command-line parsing
    └── version.h              # Version information
```

---

## Architecture Principles

### 1. Unified State Machine (NEW)

**Single engine for all modes** (`core/game_engine.c`):

```c
typedef enum {
    PHASE_GAME_START,
    PHASE_BEGIN_TURN,
    PHASE_DRAW_CARD,
    PHASE_ATTACK_REQUEST,      // Waiting for input
    PHASE_ATTACK_RESOLVE,      // Processing action
    PHASE_DEFENSE_REQUEST,
    PHASE_DEFENSE_RESOLVE,
    PHASE_COMBAT_RESOLVE,
    PHASE_DISCARD_REQUEST,
    PHASE_DISCARD_RESOLVE,
    PHASE_END_TURN,
    PHASE_GAME_OVER
} GamePhase;

typedef struct {
    GameState* game;
    GamePhase phase;
    Action* pending_action;
    bool waiting_for_input;
    PlayerID active_player;
} GameEngine;

// Core API
bool engine_step(GameEngine* engine, GameContext* ctx);  // Non-blocking step
void engine_run_until_input(GameEngine* engine, GameContext* ctx);  // Blocking helper
bool engine_submit_action(GameEngine* engine, Action* action);
```

**Usage patterns:**

```c
// Blocking mode (CLI/TUI) - game owns loop
void stda_game_loop_cli(config_t* cfg, GameContext* ctx, StrategySet* strategies) {
    while (phase != GAME_OVER) {
        engine_run_until_input(engine, ctx);  // Blocks until needs input

        if (engine_needs_input(engine)) {
            Action* action = get_human_or_ai_action(...);
            engine_submit_action(engine, action);
        }
    }
}

// Event-driven mode (GUI) - UI owns loop
void run_gui_loop(GUIContext* gui) {
    while (running) {
        handle_sdl_events();        // Non-blocking
        engine_step(engine, ctx);   // Non-blocking
        update_animations(dt);      // Always runs
        render();                   // Always runs
    }
}
```

### 2. Action-Based Communication

**All decisions produce `Action*` objects:**

```c
// ui/cli/cli_input.c
Action* cli_get_attack_action(const GameState* gs, PlayerID player, config_t* cfg) {
    // Parse user input
    // Validate
    return action_create_play_champions(player, card_ids, count);
}

// actions/action_processor.c (server-side execution)
void apply_action(GameState* gs, const Action* action, GameContext* ctx) {
    switch (action->type) {
        case ACTION_PLAY_CHAMPIONS:
            for (int i = 0; i < action->data.play_champions.num_cards; i++) {
                play_champion(gs, action->player_id, 
                            action->data.play_champions.card_ids[i], ctx);
            }
            break;
        // ... other action types
    }
}
```

### 3. Event-Driven Callbacks

**Engine emits events, UIs respond:**

```c
// ui/shared/ui_callbacks.h
typedef struct {
    void (*on_card_drawn)(PlayerID player, uint8_t card_id, void* ui_ctx);
    void (*on_card_played)(PlayerID player, uint8_t card_id, 
                          ActionType action_type, void* ui_ctx);
    void (*on_combat_resolved)(const CombatResult* result, void* ui_ctx);
    void (*on_phase_changed)(GamePhase old_phase, GamePhase new_phase, 
                            void* ui_ctx);
    void* ui_context;
} UICallbacks;

// ui/cli/cli_callbacks.c
void cli_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx) {
    cli_display_card_drawn(player_name, card);  // Display immediately
}

// ui/gui/gui_callbacks.c
void gui_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx) {
    queue_card_draw_animation(player, card_id);  // Queue for rendering
}

// roles/server/server_callbacks.c
void server_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx) {
    broadcast_event(ALL_CLIENTS, EVENT_CARD_DRAWN, player, card_id);
}
```

### 4. UI Module Structure (Consistent Pattern)

Each interactive UI follows a **3-component pattern**:

```
ui/<ui_name>/
├── <ui>_display.c/h   # Rendering/presentation (~300-400 lines)
├── <ui>_input.c/h     # Input parsing (~250-300 lines)
└── <ui>_callbacks.c/h # Event handlers (~180-200 lines)
```

**Non-interactive UIs** (simauto, simtui) have only display and callbacks.

### 5. Role Orchestration

**Roles coordinate game flow:**

- **`roles/stda/stda_game.c`**: Direct engine access, local state
  
  - Blocking loops for CLI/TUI: `engine_run_until_input()`
  - Event-driven for GUI: `engine_step()` per frame

- **`roles/client/client_game.c`**: Network → engine
  
  - Receives actions from server
  - Displays visible state only
  - Same UI modules as standalone

- **`roles/server/server_game.c`**: Full engine, manages sessions
  
  - Receives actions from clients
  - Validates and executes
  - Broadcasts results

---

## Data Flow Patterns

### Standalone (CLI/TUI - Blocking)

```
User Input → UI Input Parser → Action*
                                   ↓
                         engine_submit_action()
                                   ↓
                         engine_run_until_input()
                                   ↓
                              Game Engine
                                   ↓
                              Callbacks
                                   ↓
                             UI Display
```

### Standalone (GUI - Event-Driven)

```
SDL Events → UI Event Handler → Action* → engine_submit_action()
                                               ↓
                                         engine_step() (per frame)
                                               ↓
                                          Game Engine
                                               ↓
                                           Callbacks
                                               ↓
                                      Update animations
                                               ↓
                                            Render
```

### Client Role

```
User Input → UI Input Parser → Action* → Network → Server
                                                      ↓
Server → Network → VisibleGameState → Callbacks → UI Display
```

### Server Role

```
Network → Action* → Validate → engine_submit_action()
                                        ↓
                                  Game Engine
                                        ↓
                                   Callbacks
                                        ↓
                              Broadcast to clients
```

---

## Valid Mode Combinations

| UI        | stda | client | server | Loop Ownership |
| --------- | ---- | ------ | ------ | -------------- |
| cli       | ✓    | ✓      | ✗      | Game owns      |
| tui       | ✓    | ✓      | ✗      | Game owns      |
| gui       | ✓    | ✓      | ✗      | UI owns        |
| simauto   | ✓    | ✗      | ✗      | Game owns      |
| simtui    | ✓    | ✗      | ✗      | Game owns      |
| servercli | ✗    | ✗      | ✓      | Game owns      |

---

## Main Entry Point Logic

```c
// main/main.c
int main(int argc, char **argv) {
    GameConfig cfg;
    parse_cmdline(argc, argv, &cfg);

    // Dispatch based on (role, ui) tuple
    switch (cfg.role) {
        case ROLE_STDA:
            return run_stda_mode(cfg.ui, &cfg);

        case ROLE_CLIENT:
            return run_client_mode(cfg.ui, &cfg);

        case ROLE_SERVER:
            return run_server_mode(cfg.ui, &cfg);
    }
}

// roles/stda/stda_main.c
int run_stda_mode(UIType ui, GameConfig* cfg) {
    switch (ui) {
        case UI_CLI:
            return run_mode_stda_cli(cfg);  // Blocking loop
        case UI_TUI:
            return run_mode_stda_tui(cfg);  // Blocking loop
        case UI_GUI:
            return run_mode_stda_gui(cfg);  // Event-driven loop
        case UI_SIMAUTO:
            return run_mode_stda_simauto(cfg);  // Fast batch loop
        case UI_SIMTUI:
            return run_mode_stda_simtui(cfg);  // Blocking with visualization
    }
}
```

---

## CLI Module Breakdown (Example)

### File: `ui/cli/cli_display.c` (~340 lines)

**Purpose:** Pure presentation - format and print information

```c
void cli_display_player_status(PlayerID player, const GameState* gs,
                               const char* player_name, bool is_defense);
void cli_display_hand(PlayerID player, const GameState* gs);
void cli_display_combat_zone(const GameState* gs, config_t* cfg);
void cli_display_game_status(const GameState* gs, config_t* cfg);
void cli_display_available_commands(GamePhase phase, config_t* cfg);
void cli_display_card_drawn(const char* player_name, const struct card* card);
void cli_display_card_played(const char* player_name, const struct card* card,
                             ActionType action_type);
void cli_display_combat_resolution(const CombatResult* result, config_t* cfg);
void cli_display_game_over(const GameState* gs, config_t* cfg);
```

**Key principle:** All functions receive data, return void, only call printf/localization.

### File: `ui/cli/cli_input.c` (~290 lines)

**Purpose:** Parse and validate input, return `Action*`

```c
Action* cli_get_attack_action(const GameState* gs, PlayerID player,
                              config_t* cfg);
Action* cli_get_defense_action(const GameState* gs, PlayerID player,
                               config_t* cfg);
Action* cli_get_discard_action(const GameState* gs, PlayerID player,
                               config_t* cfg);

// Internal helpers
static bool parse_input_line(const char* input, ParsedInput* parsed);
static bool validate_card_indices(const int* indices, int count,
                                  int hand_size, config_t* cfg);
static Action* create_champion_action(const GameState* gs, PlayerID player,
                                      const int* indices, int count);
static bool validate_champion_action(const Action* action, const GameState* gs,
                                     config_t* cfg);
```

**Key principle:** Functions return validated `Action*` or NULL. Blocking is acceptable.

### File: `ui/cli/cli_callbacks.c` (~180 lines)

**Purpose:** Bridge between engine events and CLI display

```c
UICallbacks* cli_create_callbacks(config_t* cfg);
void cli_destroy_callbacks(UICallbacks* callbacks);

void cli_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx);
void cli_on_card_played(PlayerID player, uint8_t card_id,
                        ActionType action_type, void* ui_ctx);
void cli_on_combat_resolved(const CombatResult* result, void* ui_ctx);
void cli_on_phase_changed(GamePhase old_phase, GamePhase new_phase,
                          void* ui_ctx);
void cli_on_discard_complete(PlayerID player, uint8_t num_cards,
                             void* ui_ctx);
void cli_on_energy_changed(PlayerID player, int16_t old_energy,
                           int16_t new_energy, void* ui_ctx);
```

**Key principle:** Callbacks receive events, call display functions, no state mutation.

### File: `roles/stda/stda_game.c` (~380 lines)

**Purpose:** Orchestrate CLI game flow

```c
void stda_game_loop_cli(config_t* cfg, GameContext* ctx,
                        StrategySet* strategies);
int run_mode_stda_cli(config_t* cfg);

// Internal helpers
static GameEngine* init_cli_game(config_t* cfg, UICallbacks** callbacks_out);
static void display_turn_header(const GameState* gs, PlayerID player,
                                config_t* cfg);
static Action* get_human_action(GameEngine* engine, PlayerID player,
                               config_t* cfg);
static Action* get_ai_action(GameEngine* engine, PlayerID player,
                            StrategySet* strategies, GameContext* ctx);
```

**Key principle:** Orchestrates flow, delegates to display/input/callbacks, manages engine.

---

## Benefits of This Architecture

### 1. Code Reuse

- **Same engine** for CLI, TUI, GUI, server, client
- **Same UI modules** work with standalone and client roles
- **Same action system** for human input, AI decisions, and network

### 2. Separation of Concerns

- **Display**: Pure output, no state mutation
- **Input**: Validation only, returns actions
- **Callbacks**: Event response, delegates to display
- **Orchestration**: Coordinates components, manages flow
- **Engine**: Game rules only, mode-agnostic

### 3. Testability

- **Display**: Capture stdout, verify formatting
- **Input**: Mock stdin, verify parsing/validation
- **Callbacks**: Mock display, verify events
- **Engine**: Unit test state transitions
- **Integration**: Mock UI, test full game flow

### 4. Extensibility

- **New UI**: Implement display/input/callbacks (3 files)
- **New role**: Implement orchestration (1 file)
- **New feature**: Add to engine, all UIs benefit
- **Network mode**: Action system ready for serialization

### 5. Flexibility

- **Blocking modes**: Natural for CLI/TUI/server
- **Event-driven**: Smooth for GUI/mobile
- **Same codebase**: Both patterns coexist

---

## Migration Strategy

### Phase 1: Core Engine

1. Create `core/game_engine.c` with pollable state machine
2. Extract phase logic from `turn_logic.c`
3. Define `GamePhase` enum with REQUEST/RESOLVE pairs
4. Implement `engine_step()` and `engine_run_until_input()`
5. Test with existing game loop

### Phase 2: Action System

1. Define `Action` structures in `actions/action.h`
2. Implement action creators in `actions/action.c`
3. Move validation logic to `actions/action_validator.c`
4. Implement `apply_action()` in `actions/action_processor.c`
5. Test action validation and execution

### Phase 3: Callback System

1. Define `UICallbacks` interface in `ui/shared/ui_callbacks.h`
2. Thread callback context through engine functions
3. Fire callbacks at appropriate points
4. Test with mock callbacks

### Phase 4: CLI Refactoring

1. Extract display functions to `ui/cli/cli_display.c`
2. Extract input parsing to `ui/cli/cli_input.c`
3. Implement CLI callbacks in `ui/cli/cli_callbacks.c`
4. Refactor orchestration in `roles/stda/stda_game.c`
5. Test full CLI mode

### Phase 5: Verification

1. Verify all modes still work
2. Performance testing
3. Integration testing
4. Update documentation

### Phase 6: TUI Implementation

1. Create `ui/tui/` with ncurses versions
2. Reuse callback patterns
3. Test with same engine

### Phase 7: GUI Foundation

1. Implement event-driven loop in `roles/stda/stda_game.c`
2. Create SDL3 display/input in `ui/gui/`
3. Verify engine works in both blocking and event-driven modes
4. Implement GUI callbacks

---

## Example: Adding a New UI Mode

To add **GTK4 GUI** mode:

```
1. Create directory structure:
   ui/gtkgui/
   ├── gtkgui_display.c/h     # GTK4 widgets and rendering
   ├── gtkgui_input.c/h       # GTK4 event handlers
   └── gtkgui_callbacks.c/h   # Engine event → GTK updates

2. Implement display functions:
   - Create GTK widgets for game board
   - Render cards using Cairo
   - Display text with Pango

3. Implement input parsing:
   - Handle button clicks → Action*
   - Handle key presses → Action*
   - Validate before returning

4. Implement callbacks:
   - on_card_drawn → Update card widget
   - on_combat_resolved → Show animation
   - on_phase_changed → Update UI state

5. Add orchestration to stda_game.c:
   void stda_game_loop_gtkgui(...) {
       // Event-driven pattern (like SDL3)
       while (gtk_running) {
           gtk_main_iteration();
           engine_step(engine, ctx);
           update_gtk_widgets();
       }
   }

6. Register in stda_main.c:
   case UI_GTKGUI:
       return run_mode_stda_gtkgui(cfg);
```

**Total work**: ~3 new files, ~1 function in existing file. Engine unchanged.

---

## Summary

This architecture provides:

✅ **Unified state machine** for all modes  
✅ **Action-based** communication (network-ready)  
✅ **Event-driven callbacks** (UI-agnostic)  
✅ **Flexible loop ownership** (blocking or event-driven)  
✅ **Clean separation** of display, input, orchestration, events  
✅ **Maximum code reuse** across modes  
✅ **Easy extensibility** for new UIs and features  
✅ **File size compliance** (all files ≤500 lines)  
✅ **Testability** at every level  

The structure naturally supports both "game owns loop" (CLI/TUI/server) and "UI owns loop" (GUI/mobile) patterns using the same core engine and UI modules.
