# STDA Source Code Folder Structure v3

## Conceptual Model

Modes are defined as **(role, ui)** tuples:

- **Roles**: `stda` (standalone), `client`, `server`
- **UIs**: `cli`, `tui`, `gamegui`, `auto`, `simtui`, `servercli`

## Complete Directory Structure

```
src/
├── core/                      # Platform-agnostic game engine
│   ├── game_types.h          # All enums and struct definitions
│   ├── game_constants.c/h    # Full deck, static data, enums
│   ├── game_state.c/h        # Game initialization, setup
│   ├── turn_logic.c/h        # Turn flow, phase management
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
├── strategies/                # AI strategy implementations
│   ├── strategy.c/h          # Strategy framework (function pointers)
│   ├── strat_random.c/h      # Random AI
│   ├── strat_balanced.c/h    # Balanced rules AI
│   ├── strat_heuristic.c/h   # Heuristic AI
│   ├── strat_hybrid.c/h      # Hybrid AI
│   ├── strat_simplemc.c/h    # Simple Monte Carlo
│   ├── strat_ismcts.c/h      # IS-MCTS
│   └── ai_params.c/h         # AI parameter management
│
├── deck_formats/              # Deck construction methods
│   ├── deck_random.c/h       # Random deck distribution
│   ├── deck_monochrome.c/h   # Monochrome deck builder
│   ├── deck_custom.c/h       # Custom deck builder
│   ├── deck_solomon.c/h      # Solomon 7×7 draft
│   ├── deck_draft12x8.c/h    # Draft 12×8
│   ├── deck_draft123.c/h     # Draft 1-2-3
│   └── draft_common.c/h      # Shared draft utilities
│
├── game_rules/                # Optional game depth additions
│   ├── abilities.c/h         # Champion abilities (Berserker, Vampire)
│   ├── momentum.c/h          # Momentum token system
│   └── order_powers.c/h      # Order ultimate powers
│
├── roles/                     # Role-specific orchestration
│   ├── stda/
│   │   ├── stda_main.c/h     # Standalone entry point dispatcher
│   │   └── stda_game.c/h     # Standalone game loops (direct engine access)
│   │                         # Functions: stda_game_loop_cli(), _tui(), _gamegui(), _auto(), _simtui()
│   │
│   ├── client/
│   │   ├── client_main.c/h   # Client entry point dispatcher
│   │   ├── client_game.c/h   # Client game loops (network → actions)
│   │   │                     # Functions: client_game_loop_cli(), _tui(), _gamegui()
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
│   │   ├── ui_context.c/h        # Context pattern for callbacks
│   │   ├── ui_callbacks.h        # Callback interface definition
│   │   ├── localization.h        # I18n support
│   │   ├── player_selection.c/h  # Player type selection
│   │   └── player_config.c/h     # Player name/strategy config
│   │
│   ├── cli/                   # Text-based interface (interactive)
│   │   ├── cli_display.c/h   # Output formatting (~250-300 lines)
│   │   ├── cli_input.c/h     # Input parsing (~200-250 lines)
│   │   └── cli_callbacks.c/h # Event handlers (~150-200 lines)
│   │
│   ├── tui/                   # Terminal UI (ncurses)
│   │   ├── tui_display.c/h   # ncurses rendering
│   │   ├── tui_input.c/h     # ncurses input handling
│   │   ├── tui_callbacks.c/h # Event handlers
│   │   └── tui_windows.c/h   # Window management (helper)
│   │
│   ├── gamegui/               # Game GUI (SDL3 for gameplay)
│   │   ├── gamegui_display.c/h    # SDL3 rendering coordinator
│   │   ├── gamegui_input.c/h      # SDL3 event handling
│   │   ├── gamegui_callbacks.c/h  # Event handlers
│   │   ├── card_renderer.c/h      # Card compositing (helper)
│   │   ├── font_manager.c/h       # Font loading (helper)
│   │   ├── texture_cache.c/h      # Image caching (helper)
│   │   └── layout.c/h             # Responsive layout (helper)
│   │
│   ├── auto/                  # Non-interactive CLI (automation)
│   │   ├── auto_display.c/h  # Minimal output (progress, summary)
│   │   └── auto_callbacks.c/h # Event handlers (logging only)
│   │
│   ├── simtui/                # Simulation TUI (ncurses visualization)
│   │   ├── simtui_display.c/h     # Stats/graph rendering
│   │   ├── simtui_input.c/h       # Control input (pause/resume/speed)
│   │   ├── simtui_callbacks.c/h   # Event handlers
│   │   └── stats_visualizer.c/h   # Real-time stats (helper)
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
├── actions/                   # Action system (for network/MCTS)
│   ├── action.c/h            # Action structures
│   ├── action_generator.c/h  # Generate legal moves
│   ├── action_validator.c/h  # Validate actions
│   └── action_processor.c/h  # Apply actions (server-side)
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
├── simulation/                # Simulation and analysis
│   ├── sim_engine.c/h        # Simulation runner
│   ├── sim_export.c/h        # CSV export
│   └── sim_stats.c/h         # Statistics calculation
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
│   ├── platform_ios.m/h      # iOS-specific (Objective-C)
│   └── platform_android.c/h  # Android-specific
│
└── main/                      # Entry point
    ├── main.c                 # Main dispatcher
    ├── cmdline.c/h            # Command-line parsing
    └── version.h              # Version information
```

## Architecture Principles

### 1. Role-UI Separation

**Roles** orchestrate the game flow:
- **stda**: Direct engine access, local game state
- **client**: Network communication, visible state only
- **server**: Full engine access, manages multiple games

**UIs** handle presentation and input:
- Pure rendering and input parsing
- Role-agnostic (same UI code works for stda and client)
- No game logic

### 2. Data Flow Patterns

#### Standalone (stda role)
```
User Input → UI Input Parser → Actions → Game Engine → Game State → UI Display
```

#### Client Role
```
User Input → UI Input Parser → Actions → Network → Server
Server → Network → Visible State → UI Display
```

#### Server Role
```
Network → Actions → Game Engine → Game State → Network (broadcast)
Admin CLI → Server Commands → Server State Display
```

### 3. UI Module Structure

Each interactive UI follows a consistent 3-component pattern:

- **display.c/h**: Rendering/presentation only (~250-300 lines)
- **input.c/h**: Input parsing and validation (~200-250 lines)
- **callbacks.c/h**: Engine event handlers (~150-200 lines)

**Note**: `auto` UI has only display and callbacks (no interactive input).

### 4. Action System Integration

UI input parsers produce **Action structures** (defined in `actions/action.h`):

```c
// ui/cli/cli_input.c
Action* parse_user_command(const char *input, GameState *state) {
    Action *action = create_action(...);
    
    // Validate using actions/action_validator.c
    if (!validate_action(action, state)) {
        free_action(action);
        return NULL;
    }
    
    return action;
}
```

This allows:
- Same validation logic for UI input and network messages
- Actions can be serialized for network transmission
- MCTS can use action generation/validation

## Valid Mode Combinations

| UI        | stda | client | server |
|-----------|------|--------|--------|
| cli       | ✓    | ✓      | ✗      |
| tui       | ✓    | ✓      | ✗      |
| gamegui   | ✓    | ✓      | ✗      |
| auto      | ✓    | ✗      | ✗      |
| simtui    | ✓    | ✗      | ✗      |
| servercli | ✗    | ✗      | ✓      |

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
```

## Example Server CLI Commands

```
Server Admin CLI Commands:
  list              - List active games
  show <game_id>    - Show detailed game state
  stats             - Server statistics (uptime, games, connections)
  config            - Show/modify server configuration
  kick <player>     - Kick a player from the server
  shutdown          - Graceful server shutdown
  help              - Display help text
```

## File Size Compliance

All modules respect the coding guidelines:
- ≤35 lines per function (documentation/comments excluded)
- ≤100 lines per function (firm limit)
- ≤500 lines per file (ideal: ≤400)
- ≤1000 lines per file (firm limit)

Modularization ensures each file stays focused and maintainable.

## Benefits

1. **Conceptual Clarity**: (role, ui) model matches mental model
2. **Maximum Code Reuse**: UI code shared across roles
3. **Clean Separation**: Orchestration vs presentation
4. **Easy Testing**: Test UIs and roles independently
5. **Extensibility**: New UIs or roles fit naturally into structure
6. **Network Ready**: Action system prepares for client/server architecture
7. **Maintainability**: Small, focused files with clear responsibilities

## Migration Strategy

1. **Phase 1**: Create directory structure
2. **Phase 2**: Move existing files to appropriate locations
3. **Phase 3**: Split oversized files (especially `stda_cli.c` → CLI triplet)
4. **Phase 4**: Extract role orchestration to `roles/` directory
5. **Phase 5**: Update `#include` paths
6. **Phase 6**: Update Makefile with new structure
7. **Phase 7**: Test compilation and functionality
8. **Phase 8**: Add new features incrementally
