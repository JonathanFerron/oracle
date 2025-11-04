# Oracle: Implementation Roadmap & TODO

## Overview

This document outlines the recommended implementation order for the Oracle card game project, organized by phases with clear milestones and dependencies.


**Target Platform**: Windows (MSYS2) & Linux (Arch) with GCC
**Constraints**: Max 30 lines per function, max 500 lines per file

---

## Phase 1: Core Foundation

### 1.1 Data Structures & Basic Game Logic

**Priority**: CRITICAL

- [x] **Card Data System** (`card_data.c/h`)
  
  - [x] Define `Card` structure
  - [x] Create full deck of 120 cards with all attributes
  - [x] Helper functions: `get_card_by_id()`, `is_champion_card()`
  - [x] Card species/color/rank lookup tables

- [ ] **Game State Structure** (`gamestate.c/h`)
  
  - [ ] Define `PlayerState` structure
  - [x] Define `GameState` structure (full server-side state)
  - [ ] Define `VisibleGameState` structure (filtered client-side state)
  - [x] Basic initialization: `gamestate_init()`
  - [x] Memory management functions

- [ ] **Card Visibility System** (`visibility.c/h`)
  
  - [ ] Define `CardVisibility` enum
  - [ ] Define `CardVisibilityState` structure
  - [ ] Implement `update_card_visibility_draw()`
  - [ ] Implement `update_card_visibility_play()`
  - [ ] Implement `update_card_visibility_discard()`
  - [ ] Implement `get_card_visibility_for_player()`

**Deliverable**: Compile-able core structures with initialization functions

---

### 1.2 Action System

**Priority**: CRITICAL

- [ ] **Action Definitions** (`action.c/h`)
  
  - [ ] Define `ActionType` enum (all action types)
  - [ ] Define `CardPlayData` structure
  - [ ] Define `Action` structure with union for different action types
  - [ ] Implement action constructors:
    - [ ] `action_create_draw()`
    - [ ] `action_create_play_champions()`
    - [ ] `action_create_play_draw_recall()`
    - [ ] `action_create_play_exchange()`
    - [ ] `action_create_pass()`
    - [ ] `action_create_defend()`
    - [ ] `action_create_discard()`

- [ ] **Action Serialization** (`action.c`)
  
  - [ ] `action_serialize()` - convert Action to byte buffer
  - [ ] `action_deserialize()` - parse byte buffer to Action
  - [ ] `action_to_text()` - convert Action to text command
  - [ ] `action_from_text()` - parse text command to Action

**Deliverable**: Complete action system with serialization

---

### 1.3 Game State Logic & Action Generation

**Priority**: CRITICAL

- [ ] **State Visibility** (`gamestate_logic.c/h`)
  
  - [ ] `gamestate_get_visible()` - filter full state to player-specific visible state
  - [ ] `gamestate_serialize_visible()` - serialize visible state
  - [ ] `gamestate_deserialize_visible()` - deserialize visible state

- [ ] **Action Generation** (`gamestate_logic.c/h`)
  
  - [ ] `get_list_of_possible_actions()` - **CENTRAL FUNCTION** for all AIs
  - [ ] `get_attack_actions()` - generate all valid attack actions
  - [ ] `get_defense_actions()` - generate all valid defense actions
  - [ ] `get_draw_recall_actions()` - generate draw/recall options
  - [ ] `get_discard_actions()` - generate discard combinations
  - [ ] Helper: `can_afford_cards()` - check luna cost

- [ ] **Action Validation** (`gamestate_logic.c/h`)
  
  - [ ] `action_is_valid()` - validate action against visible state
  - [ ] `validate_play_champions()` - check champion play legality
  - [ ] `validate_card_cost()` - verify player can afford cards

**Deliverable**: Central action generation system usable by all AIs

---

## Phase 2: Combat & Game Loop

### 2.1 Combat Resolution

**Priority**: HIGH

- [ ] **Combo Calculation** (`combat.c/h`)
  
  - [x] `calculate_combo_bonus()` - implement combo bonus logic
    - [x] Random deck combos
    - [x] Monochrome deck combos
    - [x] Custom deck combos
  - [ ] Test all combo scenarios

- [x] **Combat System** (`combat.c/h`)
  
  - [x] `roll_dice()` - dice rolling with RNG
  - [x] `calculate_attack_total()` - sum attack + combo + dice
  - [x] `calculate_defense_total()` - sum defense + combo + dice
  - [x] `resolve_combat()` - apply damage, update energy
  - [x] `apply_damage()` - reduce defender energy

**Deliverable**: Working combat resolution system

---

### 2.2 Turn Logic & Game Flow

**Priority**: HIGH

- [ ] **Turn Management** (`turn_logic.c/h`)
  
  - [ ] `advance_turn()` - move to next turn/player
  - [ ] `process_draw_phase()` - handle card drawing
  - [ ] `process_action_phase()` - handle main action
  - [ ] `process_defense_phase()` - handle defense response
  - [ ] `process_combat_phase()` - call combat resolution
  - [ ] `process_collect_phase()` - collect luna
  - [ ] `process_discard_phase()` - discard to 7 cards
  - [ ] `check_win_condition()` - detect game end

- [x] **Deck Management** (`gamestate_logic.c`)
  
  - [x] `shuffle_deck()` - shuffle player deck
  - [x] `draw_card()` - draw card from deck
  - [x] `reshuffle_discard()` - shuffle discard into deck when empty
  - [x] `discard_card()` - move card to discard pile
  - [x] `recall_champion()` - move champion from discard to hand

- [x] **Special Actions** (`gamestate_logic.c`)
  
  - [x] `apply_exchange_card()` - exchange champion for 5 lunas
  - [x] `apply_draw_recall_card()` - draw cards or recall champions
  - [x] `process_mulligan()` - handle second player mulligan

**Deliverable**: Complete turn-based game loop

---

## Phase 3: Standalone Mode with Basic AI

### 3.1 Basic AI Agents

**Priority**: HIGH

- [ ] **AI Interface** (`ai_interface.h`)
  
  - [ ] Define `AIAgent` structure
  - [ ] Define `AISelectActionFunc` function pointer type
  - [ ] AI agent registration system

- [ ] **Random AI** (`ai_random.c/h`)
  
  - [ ] `ai_random_select_action()` - select random valid action
  - [ ] Call `get_list_of_possible_actions()`
  - [x] Random selection from list
  - [x] Test against itself (should be ~50% win rate)

- [ ] **Balanced Rules AI** (`ai_balanced.c/h`)
  
  - [ ] Port existing balanced strategy logic
  - [ ] Use `get_list_of_possible_actions()`
  - [ ] Implement decision heuristics for:
    - [ ] Attack phase (when to play champions vs draw/recall)
    - [ ] Defense phase (when to defend vs decline)
    - [ ] Card selection (which cards to play)
    - [ ] Discard phase (which cards to keep)

**Deliverable**: Two working AI agents that play complete games that can outperform the random AI



Old TODO notes: 

    Build a first set of 'smart' AI playing agents:
    
    enhance decision rules to mimic what a smart player would do, and find what could be more optimal 
       decision rules, probably using heuristics to keep things simple. See notes in balanced rules strategy source file: strat_balancedrules1.c and in heuristics strategy
       source file. Don't forget that a call to a strategy function should also be done when mulliganing and discarting (to 7 cards).
    
      Need to put in place, in the module that calls the simulation code (from main()), an optimization mechanism that can be used to automate the fine tuning of AI agent
        heuristics / parameters with the goal of maximizing that agent's win rate, or eventually finding the parameters that will make a given agent's win rate equal to x%
        against a pre-determined 'benchmark' agent. This would allow a human user to select an AI agent with a given 'strength' to play against. 
    
      Work on implementing a correct 'power' for non-champion cards to allow better 'power heuristic'-based choices. Model multiple simulations with varying power from 2 to 15
      for the non-champion cards for player A, and keep the same card's power to a fixed value for player B. Which of the values between 2, 3, 4, ..., 15 yield the best
      win percentage for player A? Say that's 5.00. Use 5.00 as the new default 'power' value for the card, and do the simulation again, keeping the default value of 5.00
      for player B's decisions but iterating from 2 to 15 for player A to confirm that it now yields a better win percentage for player B for all of A's values except when
      A also uses the 'optimal' value 5.00. When the 'cash card' is implemented, use the same approach to calculate a 'power heuristic' for it: calibrate the heuristic
      parameter to maximize the chances of winning.
    
       To Add New Strategies:
         - Copy strat_random.c as template
         - Implement attack/defense functions
         - Register in main.c

---

### 3.2 Standalone Automated Mode

**Priority**: HIGH

- [ ] **Simulation Engine** (`simulation.c/h`)
  
  - [x] `run_single_game()` - play one complete game
  - [x] `run_simulation_batch()` - play N games
  - [x] `collect_statistics()` - track wins, avg turns, etc.
  - [ ] `export_results_to_file()` - save results to text file

- [x] **Main Standalone** (`main_standalone.c`)
  
  - [x] Parse command-line args: `oracle -sa`
  - [x] Initialize both AI agents
  - [x] Run default 1000 simulations
  - [x] Display results (25-row console output)
  - [x] Clean exit

**Deliverable**: Working `oracle -sa` mode

---

### 3.3 Heuristic AI

**Priority**: MEDIUM

- [ ] **Heuristic AI** (`ai_heuristic.c/h`)
  - [ ] Implement power heuristic for card evaluation
  - [ ] `calculate_card_power()` - estimate card value
  - [ ] `evaluate_combat_outcome()` - predict combat results
  - [ ] `select_best_action_by_heuristic()` - greedy action selection
  - [ ] Decision rules:
    - [ ] Play highest-power cards for attack
    - [ ] Defend only if likely to reduce damage significantly
    - [ ] Use draw/recall when hand power is low
    - [ ] Mulligan low-power cards

**Deliverable**: Heuristic AI that outperforms Random AI

---

## Phase 4: Text User Interface

### 4.1 ncurses TUI Foundation

**Priority**: HIGH

**Reference**: https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/

- [ ] **TUI Core** (`ui_tui.c/h`)
  
  - [ ] `tui_init()` - initialize ncurses, create windows
  - [ ] `tui_shutdown()` - cleanup ncurses
  - [ ] `tui_create_windows()` - create game_win and console_win
  - [ ] `tui_refresh_all()` - refresh all windows

- [ ] **Game Display** (`ui_tui.c`)
  
  - [ ] `tui_display_opponent_info()` - show opp energy/lunas/hand count
  - [ ] `tui_display_combat_zone()` - show attack/defense cards
  - [ ] `tui_display_my_hand()` - show player's hand with card details
  - [ ] `tui_display_my_info()` - show energy/lunas/deck count
  - [ ] `tui_display_phase()` - show current phase/turn

- [ ] **Console System** (`ui_tui.c`)
  
  - [ ] `tui_console_print()` - add message to console
  - [ ] `tui_console_get_input()` - get command from user
  - [ ] `tui_console_clear()` - clear console window

**Deliverable**: Basic TUI displaying game state

---

### 4.2 Interactive TUI Gameplay

**Priority**: HIGH

- [ ] **Input Handling** (`ui_tui.c`)
  
  - [ ] `tui_get_player_action()` - main input function
  - [ ] `tui_select_cards_from_hand()` - card selection interface
  - [ ] `tui_confirm_action()` - ask for confirmation
  - [ ] Parse commands: help, quit, simmode, play, pass, etc.

- [ ] **Action Display** (`ui_tui.c`)
  
  - [ ] `tui_display_action()` - show action taken
  - [ ] `tui_display_combat_result()` - show dice rolls and damage
  - [ ] `tui_display_error()` - show error messages
  - [ ] Animation helpers (optional): card movement

- [ ] **Game Flow Integration** (`main_standalone.c`)
  
  - [ ] Mode: `oracle -st`
  - [ ] Initialize TUI
  - [ ] Game loop:
    - [ ] Display state
    - [ ] Get action (from human or AI)
    - [ ] Apply action
    - [ ] Show results
    - [ ] Check win condition
  - [ ] Human vs AI gameplay
  - [ ] Human vs Human gameplay

**Deliverable**: Working `oracle -st` mode with full gameplay

---

### 4.3 Mode Switching & Polish

**Priority**: MEDIUM

- [ ] **Mode Switching** (`ui_tui.c`)
  
  - [ ] `tui_switch_to_sim_mode()` - transition to simulation UI (simmode)
  - [ ] Save current game state before switch
  - [ ] Clean transition without screen corruption

- [ ] **UI Polish**
  
  - [ ] Color coding (energy bars, card types)
  - [ ] Box drawing characters for clean borders
  - [ ] Status indicators (waiting, your turn, etc.)
  - [ ] Help text display

**Deliverable**: Polished TUI with mode switching

---

## Phase 5: Simulation UI & Calibration

### 5.1 Interactive Simulation UI

**Priority**: MEDIUM

- [ ] **Simulation UI** (`ui_sim.c/h`)
  
  - [ ] Layout: results (left), console (right), parameters (bottom left)
  - [ ] `sim_ui_init()` - initialize simulation UI
  - [ ] `sim_ui_display_results()` - show win rates, stats
  - [ ] `sim_ui_display_progress()` - progress bar for running sims
  - [ ] `sim_ui_get_parameters()` - get simulation parameters
  - [ ] `sim_ui_export_results()` - export to file

- [ ] **Commands** (`ui_sim.c`)
  
  - [ ] `run <N>` - run N simulations
  - [ ] `export <filename>` - export results
  - [ ] `tuimode` - switch to TUI mode
  - [ ] `set strat_a <name>` - change strategy A
  - [ ] `set strat_b <name>` - change strategy B

- [ ] **Main Integration** (`main_standalone.c`)
  
  - [ ] Mode: `oracle -ss`
  - [ ] Run simulation UI
  - [ ] Support mode switching (tuimode / simmode)

**Deliverable**: Working `oracle -ss` mode

---

### 5.2 Heuristic Calibration System

**Priority**: MEDIUM

- [ ] **Calibration Framework** (`calibration.c/h`)
  
  - [ ] `calibrate_parameter()` - test parameter values 2-15
  - [ ] `run_calibration_sweep()` - systematic testing
  - [ ] `find_optimal_value()` - determine best parameter
  - [ ] `verify_optimality()` - reverse testing

- [ ] **Non-Champion Card Power** (`calibration.c`)
  
  - [ ] Calibrate draw 2 / recall 1 card power
  - [ ] Calibrate draw 3 / recall 2 card power
  - [ ] Calibrate exchange card power
  - [ ] Document optimal values

- [ ] **Calibration Script** (`scripts/calibrate_heuristics.py`)
  
  - [ ] Automate calibration runs
  - [ ] Generate graphs of win rate vs parameter value
  - [ ] Export optimal values to C header file

**Deliverable**: Calibrated heuristic values, improved AI performance

---

## Phase 6: Advanced AI Agents

### 6.1 Hybrid AI

**Priority**: MEDIUM

- [ ] **Hybrid AI** (`ai_hybrid.c/h`)
  - [ ] Combine balanced rules with heuristic evaluation
  - [ ] `ai_hybrid_select_action()` - decision function
  - [ ] Use rules for high-confidence situations
  - [ ] Use heuristics for ambiguous situations
  - [ ] Implement situational awareness:
    - [ ] Early game vs late game
    - [ ] Leading vs trailing in energy
    - [ ] High luna vs low luna situations

**Deliverable**: Hybrid AI stronger than both Balanced and Heuristic

---

### 6.2 Monte Carlo AI (Simple)

**Priority**: MEDIUM

- [ ] **Simple Monte Carlo** (`ai_simplemc.c/h`)
  - [ ] `ai_simplemc_select_action()` - MC-based decision
  - [ ] For each possible action:
    - [ ] Play N random rollouts (e.g., 50)
    - [ ] Track win rate
  - [ ] Select action with highest win rate
  - [ ] Implement pruning after 4 episodes
  - [ ] Optimize for performance (keep under reasonable time)

**Deliverable**: Monte Carlo AI

---

### 6.3 Advanced AI (Future)

**Priority**: LOW | **Time**: Future phases

- [ ] **Progressive Pruning MC** (`ai_progressive_mc.c/h`)
  
  - [ ] Gradually eliminate weak actions during rollouts
  - [ ] More sophisticated than simple pruning

- [ ] **UCB1 AI** (`ai_ucb1.c/h`)
  
  - [ ] Upper Confidence Bound for action selection
  - [ ] Balance exploration vs exploitation

- [ ] **PUCB1 AI** (`ai_puct.c/h`)
  
  - [ ] Predictor + UCT for better action selection

- [ ] **MCTS (UCT)** (`ai_mcts.c/h`)
  
  - [ ] Full Monte Carlo Tree Search implementation
  - [ ] Tree reuse between turns

- [ ] **Information Set MCTS** (`ai_ismcts.c/h`)
  
  - [ ] Handle imperfect information properly
  - [ ] Determinization for hidden information
- [ ] **MCTS with prior predictor**

- [ ] **Neural Network MCTS** (`ai_nn_mcts.c/h`)
  
  - [ ] NN-based prior predictor (e.g. DQN)
  - [ ] Requires training infrastructure
  - [ ] Potentially strongest AI

---

## Phase 7: Network Foundation

### 7.1 Protocol Implementation

**Priority**: HIGH

- [ ] **Protocol Core** (`protocol.c/h`)
  
  - [ ] Define `MessageType` enum
  - [ ] Define `Message` structure
  - [ ] `message_serialize()` - convert Message to bytes
  - [ ] `message_deserialize()` - parse bytes to Message
  - [ ] `message_create_action()` - wrap Action in Message
  - [ ] `message_create_gamestate()` - wrap VisibleGameState in Message
  - [ ] `message_create_event()` - create event notification

- [ ] **Text Protocol** (`protocol.c`)
  
  - [ ] `text_command_to_action()` - parse "cham 01,04,06"
  - [ ] `action_to_text_command()` - convert Action to text
  - [ ] Command parsing for: cham, draw, recl, exch, pass, defe, decl, disc, quit, stat

**Deliverable**: Complete protocol layer with text and binary support

---

### 7.2 Server Implementation

**Priority**: HIGH

- [ ] **Server Core** (`server.c/h`)
  
  - [ ] `server_init()` - initialize server, bind to port
  - [ ] `server_accept_client()` - accept client connections
  - [ ] `server_wait_for_players()` - wait for 2 clients
  - [ ] `server_shutdown()` - cleanup

- [ ] **Message Handling** (`server.c`)
  
  - [ ] `server_receive_message()` - receive from client
  - [ ] `server_send_message()` - send to specific client
  - [ ] `server_broadcast()` - send to all clients

- [ ] **Game Management** (`server.c`)
  
  - [ ] `server_initialize_game()` - setup game, deal cards
  - [ ] `server_apply_action()` - validate and apply client action
  - [ ] `server_process_combat()` - roll dice, resolve combat
  - [ ] `server_send_visible_state()` - send filtered state to client
  - [ ] `server_broadcast_action()` - notify all clients of action
  - [ ] `server_game_loop()` - main server game loop

- [ ] **Main Server** (`main_server.c`)
  
  - [ ] Mode: `oracle -sv`
  - [ ] Parse port argument
  - [ ] Run server
  - [ ] Handle multiple games (future enhancement)

**Deliverable**: Working server that manages game state

---

## Phase 8: Client Implementation

### 8.1 Client Core

**Priority**: HIGH

- [ ] **Client Core** (`client.c/h`)
  
  - [ ] `client_connect()` - connect to server
  - [ ] `client_disconnect()` - graceful disconnect
  - [ ] `client_send_action()` - send Action to server
  - [ ] `client_receive_update()` - receive state/event from server
  - [ ] `client_request_gamestate()` - request state resync

- [ ] **Client State Management** (`client.c`)
  
  - [ ] Maintain local `VisibleGameState`
  - [ ] Update state from server messages
  - [ ] Track connection status

**Deliverable**: Client library

---

### 8.2 Human Client with TUI

**Priority**: HIGH

- [ ] **Client TUI** (`main_client.c`)
  - [ ] Mode: `oracle -ct`
  - [ ] Parse server address and port
  - [ ] Connect to server
  - [ ] Integrate with existing TUI code
  - [ ] Game loop:
    - [ ] Receive state from server
    - [ ] Display state
    - [ ] Get action from human
    - [ ] Send action to server
  - [ ] Handle disconnection gracefully

**Deliverable**: Working `oracle -ct` mode

---

### 8.3 AI Client

**Priority**: HIGH

- [ ] **AI Client** (`main_ai_client.c`)
  - [ ] Mode: `oracle -ai`
  - [ ] Parse server address, port, and AI strategy
  - [ ] Connect to server
  - [ ] Game loop:
    - [ ] Receive visible state from server
    - [ ] Call AI agent to select action
    - [ ] Send action to server
  - [ ] Minimal console output (log actions)

**Deliverable**: Working `oracle -ai` mode

---

## Phase 9: Polish & Testing

### 9.1 Testing Infrastructure

**Priority**: HIGH

- [ ] **Unit Tests**
  
  - [ ] Test action serialization/deserialization
  - [ ] Test combo bonus calculations (all scenarios)
  - [ ] Test `get_list_of_possible_actions()` for all phases
  - [ ] Test visibility filtering
  - [ ] Test card drawing and shuffling

- [ ] **Integration Tests**
  
  - [ ] Test full game simulation (AI vs AI)
  - [ ] Test client/server communication
  - [ ] Test reconnection handling
  - [ ] Test malformed message handling

- [ ] **Network Tests**
  
  - [ ] Test with artificial latency
  - [ ] Test with packet loss
  - [ ] Test simultaneous connections

- [ ] **Test Scripts** (`scripts/`)
  
  - [ ] `test_protocol.py` - test message parsing
  - [ ] `test_full_game.py` - automated game testing
  - [ ] `stress_test_server.py` - load testing

**Deliverable**: Comprehensive test suite

---

### 9.2 Documentation

**Priority**: MEDIUM

- [ ] **API Documentation** (`doc/API.md`)
  
  - [ ] Document all public functions with Doxygen comments
  - [ ] Generate HTML documentation
  - [ ] Include usage examples

- [ ] **Protocol Documentation** (`doc/PROTOCOL.md`)
  
  - [ ] Document message format
  - [ ] Document text command syntax
  - [ ] Include wire format diagrams

- [ ] **User Guide** (`doc/USER_GUIDE.md`)
  
  - [ ] How to compile
  - [ ] How to run different modes
  - [ ] Gameplay instructions
  - [ ] Troubleshooting

- [ ] **Strategy Guide** (`doc/STRATEGY_GUIDE.md`)
  
  - [ ] Describe each AI agent
  - [ ] Tips for playing against each AI
  - [ ] Combo explanations

**Deliverable**: Complete documentation

---

## Phase 10: GUI & Advanced Features (Future)

### 10.1 Graphical User Interface

**Priority**: LOW | **Future phase**

- [ ] **GUI Implementation** (`ui_gui.c/h`)
  
  - [ ] Basic window and rendering
  - [ ] Card art display
  - [ ] Drag-and-drop card playing
  - [ ]   - [ ] Chat window for networked play

- [ ] **Main GUI** (`main_standalone.c`, `main_client.c`)
  
  - [ ] Mode: `oracle -sg` and `oracle -cg`

**Deliverable**: Working GUI for standalone and client modes

---

### 10.2 Advanced Features

**Priority**: LOW | **Future phases**

- [ ] **Game State Persistence**
  
  - [ ] Save game to file
  - [ ] Load game from file
  - [ ] Resume interrupted games

- [ ] **Replay System**
  
  - [ ] Record all actions in a game
  - [ ] Replay game from recording
  - [ ] Export replay to shareable format

- [ ] **Reconnection Handling**
  
  - [ ] Client reconnection after disconnect
  - [ ] Server maintains game state during disconnect
  - [ ] Timeout and forfeit logic

- [ ] **Statistics & Ranking**
  
  - [ ] Track player statistics (wins, losses, avg turns)
  - [ ] Rating system
  - [ ] Leaderboard

- [ ] 
- [ ] **Spectator Mode**
  
  - [ ] Allow observers to watch games
  - [ ] Limited visibility (no hidden information)

---

## Implementation Guidelines

### Code Quality Standards

1. **Function size**: Maximum 30 lines of actual code per function
2. **File size**: Maximum 500 lines per file (target 400)
3. **Comments**: Doxygen-style for all public functions
4. **Naming**: Consistent naming conventions (snake_case for C)
5. **Error handling**: Always check return values
6. **Memory management**: No leaks, free all allocated memory

### Testing Strategy

- **Test as you go**: Don't wait until the end
- **Unit test first**: Test individual functions in isolation
- **Integration test second**: Test modules working together
- **System test last**: Test complete workflows

### Performance Considerations

- **Profile before optimizing**: Don't guess, measure
- **Hot paths**: Combat calculation, action generation
- **Memory pools**: Consider for frequently allocated structures
- **Cache locality**: Keep related data together

### Version Control

- **Commit frequently**: Small, logical commits
- **Meaningful messages**: Describe what and why
- **Feature branches**: One feature per branch
- **Tag releases**: Tag major milestones

---

## Milestone Checklist

### Milestone 1: Core Game Engine

- [ ] All data structures defined
- [ ] Action system complete
- [ ] Combat resolution working
- [ ] Turn logic implemented
- [ ] Can simulate AI vs AI game to completion

### Milestone 2: Standalone Modes

- [ ] Random AI working
- [ ] Balanced AI working
- [ ] Heuristic AI working
- [ ] Automated simulation mode (`-sa`)
- [ ] TUI mode (`-st`) with human gameplay
- [ ] Interactive simulation mode (`-ss`)

### Milestone 3: Basic Multiplayer

- [ ] Protocol implemented
- [ ] Server working
- [ ] Human client working (`-ct`)
- [ ] AI client working (`-ai`)
- [ ] Can play networked game successfully

### Milestone 4: Polished Release

- [ ] All modes tested and working
- [ ] Documentation complete
- [ ] Test suite passing
- [ ] Performance acceptable
- [ ] Ready for user testing

---

## Quick Reference: Command-Line Usage

```bash
# Standalone Modes
oracle -sa              # Automated simulation (1000 games, AI vs AI)
oracle -ss              # Interactive simulation mode
oracle -st              # Text UI (human vs AI or human vs human)
oracle -sg              # GUI mode (future)

# Network Modes
oracle -sv -p 5555       # Start server on port 5555
oracle -ct -h localhost -p 5555  # Connect client (TUI)
oracle -cg -h localhost -p 5555  # Connect client (GUI, future)
oracle -ai AGENT -h localhost -p 5555  # Connect AI client using AGENT
```

---

## Dependencies & Tools

### Required Libraries

- **ncurses**: For TUI mode
- **pthread**: For threading
- Standard C library

### Optional Libraries (Future)

- **SDL2** : For GUI mode
- **OpenSSL**: For encrypted network communication

### Development Tools

- **GCC**: C compiler
- **Make**: Build automation
- **GDB**: Debugging
- **Valgrind**: Memory leak detection
- **Python 3**: For utility scripts
- **Git**: Version control

---

## Notes & Tips

### Debugging Strategy

1. **Compilation errors**: Fix immediately, don't accumulate
2. **Runtime errors**: Use GDB and printf debugging
3. **Memory errors**: Run with Valgrind regularly
4. **Logic errors**: Write unit tests to isolate

### Common Pitfalls to Avoid

- **Function too long**: Refactor when approaching 30 lines
- **File too long**: Split into multiple files
- **Global state**: Minimize global variables
- **Magic numbers**: Use named constants
- **Ignoring errors**: Always check return values
- **Memory leaks**: Free all malloc'ed memory

### Performance Optimization Order

1. **First**: Get it working correctly
2. **Second**: Profile to find bottlenecks
3. **Third**: Optimize hot paths only
4. **Fourth**: Test that optimizations didn't break anything

### When in Doubt

- **Simplify**: Start with simplest working solution
- **Test**: Write tests to verify correctness
- **Refactor**: Improve code structure once working
- **Document**: Explain non-obvious decisions

---

## Success Criteria

- [ ] - [ ] Win rates are reasonable (~50% for balanced agents) 

- [ ] Human can play vs AI through CLI and TUI
- [ ] InteractivesSimulation mode produces useful statistics
- [ ] UI is responsive and bug-free

- [ ] Advanced AIs outperform basic AIs
- [ ] Hybrid AI beats both parents (balanced and heuristic)
- [ ] Monte Carlo AI makes intelligent decisions 

- [ ] Two clients can connect and play complete game
- [ ] Server handles disconnects gracefully
- [ ] Network latency doesn't affect gameplay 

- [ ] All modes work reliably
- [ ] Code is clean and maintainable
- [ ] Documentation is complete
- [ ] Ready for public release