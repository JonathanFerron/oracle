I've created a comprehensive revision of the folder structure document that incorporates all the architectural decisions we made in this conversation. The key updates include:

## Major Changes from v3 to v4

### 1. **Unified State Machine Architecture**

- Added detailed explanation of `core/game_engine.c`
- Documents both blocking (`engine_run_until_input()`) and event-driven (`engine_step()`) patterns
- Shows how the same engine serves CLI, TUI, GUI, and server modes

### 2. **Action-Based Communication**

- Expanded `actions/` directory documentation
- Shows how actions flow from UI input → validation → submission → processing
- Prepares for network serialization

### 3. **Callback System**

- Added `ui/shared/ui_callbacks.h` as the interface
- Documents callback implementations for each UI type
- Shows how callbacks bridge engine events to UI responses

### 4. **CLI Module Breakdown**

- Detailed description of the 4-file split:
  - `cli_display.c` (~340 lines) - Pure presentation
  - `cli_input.c` (~290 lines) - Parse/validate → `Action*`
  - `cli_callbacks.c` (~180 lines) - Event handlers
  - `stda_game.c` (~380 lines) - Orchestration
- Complete function signatures for each module

### 5. **Updated Data Flow Diagrams**

- Shows blocking mode flow (CLI/TUI)
- Shows event-driven flow (GUI)
- Shows client/server patterns

### 6. **Migration Strategy**

- 7-phase plan with time estimates
- Practical steps for refactoring existing code
- Example of adding new UI modes

The document now serves as a comprehensive blueprint for the entire codebase architecture, showing how all pieces fit together while maintaining clean separation and code reuse.
