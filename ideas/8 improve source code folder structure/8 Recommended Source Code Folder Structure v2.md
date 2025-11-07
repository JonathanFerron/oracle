# Recommended Source Code Folder Structure

Based on the analysis of your current codebase, planned features, and architectural goals, here's a comprehensive folder structure recommendation:

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
├── modes_ui/                 # Game Mode Implemention with User interface
│   ├── cli/
│   │   ├── cli_display.c/h   # CLI output formatting (stda and client roles)
│   │   ├── cli_input.c/h     # CLI input parsing (stda and client roles)
│   │   ├── cli_game_stda.c/h # CLI game loop (stda role only as the game engine is the responsibility of the server in client/server mode)
│   │   └── cli_callbacks.c/h # CLI event callbacks (stda and client roles)
│   ├── tui/
│   │   ├── tui_main.c/h      # TUI initialization (stda role only)
│   │   ├── tui_render.c/h    # ncurses rendering (stda and client roles)
│   │   ├── tui_input.c/h     # TUI input handling (stda and client roles)
│   │   └── tui_windows.c/h   # Window management (stda and client roles)
│   ├── gui/
│   │   ├── gui_main.c/h      # SDL3 initialization (stda role only)
│   │   ├── card_renderer.c/h # Card compositing (stda and client roles)
│   │   ├── font_manager.c/h  # Font loading/caching (stda and client roles)
│   │   ├── texture_cache.c/h # Image caching (stda and client roles)
│   │   ├── layout.c/h        # Responsive layout (stda and client roles)
│   │   └── input.c/h         # Input abstraction (stda and client roles)
│   ├── stda_auto.c/h         # Standalone automated simulation
│   └── stda_sim.c/h          # Standalone interactive simulation
│   ├── localization.h         # I18n support
│   ├── player_selection.c/h   # Player type selection
│   └── player_config.c/h      # Player name/strategy config
│
├── interactive/               # Interactive player support
│   ├── mulligan.c/h          # Mulligan UI and logic
│   ├── discard.c/h           # Discard-to-7 UI and logic
│   ├── recall.c/h            # Recall card selection
│   ├── deck_builder.c/h      # Interactive deck building
│   └── ui_callbacks.h        # Callback definitions
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
├── network/                   # Network protocol (future)
│   ├── protocol.c/h          # Message format
│   ├── serialization.c/h     # State serialization
│   ├── socket_utils.c/h      # Socket wrappers
│   └── crypto.c/h            # Authentication/checksums
│
├── server/                    # Server-specific code (future)
│   ├── server_main.c/h       # Server entry point
│   ├── server_state.c/h      # Full game state management
│   ├── session_manager.c/h   # Game session handling
│   └── matchmaking.c/h       # Matchmaking logic
│
├── client/                    # Client-specific code (future)
│   ├── client_main.c/h       # Client entry point (this may be best done in cli_game_client, tui_main_client, and gui_main_client)
│   ├── client_state.c/h      # Visible state management (helper to cli, tui and gui client mode code)
│   └── client_network.c/h    # Network communication
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
└── main/                      # Entry points
    ├── main.c/h              # Main entry point
    ├── cmdline.c/h           # Command-line parsing
    └── version.h             # Version information
```

## Key Design Rationale

### 1. **Core** - Game Engine Purity

- Contains only platform-agnostic game logic
- No UI, network, or platform-specific code
- Can be compiled standalone for testing
- Used by all modes (standalone, client, server)

### 2. **Strategies** - AI Encapsulation

- Each AI in its own file pair
- Strategy framework provides abstraction
- AI parameters in dedicated file
- Easy to add new strategies without touching core

### 3. **Deck Formats** - Extensibility

- One file pair per deck construction method
- Shared utilities in `draft_common.c/h`
- Interactive drafting uses callbacks to UI
- Clean separation from core game rules

### 4. **Game Rules** - Optional Features

- Champion abilities, momentum, order powers
- Can be compiled out if not used
- Integrates with core via function pointers
- Configuration flag controls inclusion

### 5. **Modes** / UI ('Standalone' and 'Client' roles, each with three UI options)

- 'game' and 'main' code orchestrate core + UI + strategies

- `cli/`, `tui/`, `gui/` directories for each interface with most code shared between 'client' and 'standalone' roles

- Hook from UI to game engine in
  
  - 'cli_game_stda' / 'tui_main_stda' / 'gui_main_stda' code for standalone role (direct hook to game engine)
  - 'cli_game_client' / 'tui_main_client' / 'gui_main_client' code for client role (communicate with server via client_network.c)

- Split by concern: display, input, game loop

- Shared localization and configuration

- Callbacks allow core to notify UI of events

### 6. **Interactive** - Player Agency

- Mulligan, discard, recall, deck building
- Separated from AI logic
- Used by CLI/TUI/GUI modes
- UI callbacks for choice presentation

### 7. **Actions** - Network & MCTS Foundation

- Action enumeration and generation
- Validation separated from application
- Used by both network and tree search
- State machine preparation

### 8. **Visibility** - Information Hiding

- VisibleGameState for clients and MCTS
- Filter functions for security
- Determinization for MCTS
- Foundation for network play

### 9. **Network** - Client/Server Split

- Protocol in shared directory
- Server and client each have own folders
- Socket utilities abstracted
- Authentication built-in

### 10. **Simulation** - Analysis Tools

- Export engine separated from game engine
- Statistics calculation utilities
- CSV format for R/Python analysis
- Simparam generation

### 11. **Rating** - Self-Contained System

- Bradley-Terry implementation
- Batch and incremental methods
- CSV persistence
- Calibration tools

### 12. **Config** - Centralized Settings

- INI file parsing
- AI parameters from file
- Runtime and persistent config
- Override hierarchy

### 13. **Platform** - Portability

- One file per platform
- SDL3 abstractions in GUI code
- Conditional compilation
- No platform code in core

### 14. **Main** - Minimal Coordinator

- Command-line parsing
- Mode selection
- Cleanup orchestration
- Version management

## Migration Strategy

1. **Phase 1**: Create directory structure (empty)
2. **Phase 2**: Move existing files to appropriate locations
3. **Phase 3**: Split oversized files (`stda_cli.c` → _cli quadruplet)
4. **Phase 4**: Update `#include` paths
5. **Phase 5**: Update Makefile with new structure
6. **Phase 6**: Test compilation and functionality
7. **Phase 7**: Add new features incrementally

## Benefits

- **Clarity**: Each directory has a clear purpose
- **Scalability**: Easy to find and add new components
- **Modularity**: Clean separation of concerns
- **Testability**: Core can be tested independently
- **Maintainability**: Small files, clear organization
- **Extensibility**: New features have obvious homes

This structure supports your current needs while providing a clear path for all planned features.
