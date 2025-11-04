# Oracle: Technical Design Document

**Les Champions d'Arcadie / The Arcadian Champions of Light**

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Core Data Structures](#core-data-structures)
3. [Action System](#action-system)
4. [Game State & Visibility](#game-state--visibility)
5. [AI Agent Interface](#ai-agent-interface)
6. [Client/Server Architecture](#clientserver-architecture)
7. [User Interface Modes](#user-interface-modes)
8. [Game Mechanics](#game-mechanics)
9. [File Organization](#file-organization)

---

## 1. Architecture Overview

### Design Principles

- **Maximum 30 lines per function** (excluding comments, documentation, whitespace)
- **Maximum 500 lines per source file** (ideally ≤400 lines)
- **Clear separation of concerns** between game logic, network, UI, and AI
- **Information hiding**: Clients only see visible portions of game state
- **Authoritative server**: Server maintains full game state and validates all actions

### Supported Platforms

- **Windows**: MSYS2 with GCC
- **Linux**: Arch Linux with GCC
- **Editor**: Geany on both platforms
- **Build Tool**: Make
- **Scripting**: Python for utilities

### Execution Modes

```
oracle [OPTIONS]

General options:
    --help        : show command line options
    --verbose     : enable verbose output
    --version     : show version information
    --numsim N    : Set number of simulations to N
    --input FILE  : use FILE as input configuration
    --output FILE : output to FILE instead of stdout

Game Modes selection: combine a role and a UI in this fashion:
    --stda.auto, --stda.cli, --stda.tui, --server, --client.sim, --client.cli, --client.gui, --ai AGENT

Available Roles:
    stda        : standalone, will act as both the server and the client and allow an AI agent to be used as a player 
    server      : server role
    client      : client role
    ai AGENT    : AI agent client role

Available UI:
    auto        : No UI. The program will run in automated mode based on command line arguments only. Available only with the stda role.
    cli         : Command Line Interface. Available with all 4 roles.
    tui         : Text User Interface (ncurses). Available with the stda and client roles.
    sim         : Interactive simulation mode (ncurses). Available with the stda and client roles.
    gui         : Graphical User Interface. Availabe with the stda and client roles.
    
    
```

---

## 2. Core Data Structures

### 2.1 Card Structure

```c
typedef struct {
    uint8_t id;           // Unique card identifier within game instance
    uint8_t champion_id;  // 0-101 for champions, 102-119 for special cards
    uint8_t cost;         // Luna cost
    uint8_t attack;       // Attack value (0 for non-champions)
    uint8_t defense;      // Defense value (0 for non-champions)
    uint8_t color;        // 0=Indigo, 1=Orange, 2=Rouge
    uint8_t species;      // 0-4 (varies by color)
    uint8_t rank;         // 0-4 (A-E)
} Card;
```

### 2.2 Card Visibility System

Each card in the game has visibility indicators from each player's perspective:

```c
typedef enum {
    CARD_VIS_VISIBLE,              // Card is face-up and visible
    CARD_VIS_IN_OPP_HAND,          // Hidden but known to be in opponent's hand
    CARD_VIS_HIDDEN_UNKNOWN,       // Could be anywhere (including out of play)
    CARD_VIS_IN_OWN_DECK,          // Hidden but known to be in own deck
    CARD_VIS_IN_OPP_DECK,          // Hidden but known to be in opponent's deck
    CARD_VIS_IN_OPP_HAND_OR_DECK   // In opponent's possession, location unknown
} CardVisibility;

typedef struct {
    CardVisibility vis_player_a;   // Visibility from Player A's perspective
    CardVisibility vis_player_b;   // Visibility from Player B's perspective
} CardVisibilityState;
```

**Usage in GameState:**

```c
typedef struct {
    // ... other game state fields ...
    CardVisibilityState card_visibility[120];  // One per card in full deck
} GameState;
```

### 2.3 Player State

```c
#define MAX_HAND_SIZE 20
#define MAX_DECK_SIZE 40
#define MAX_DISCARD_SIZE 40

typedef struct {
    uint8_t energy;
    uint8_t lunas;
    uint8_t hand[MAX_HAND_SIZE];
    uint8_t hand_count;
    uint8_t deck[MAX_DECK_SIZE];
    uint8_t deck_count;
    uint8_t discard[MAX_DISCARD_SIZE];
    uint8_t discard_count;
} PlayerState;
```

### 2.4 Full Game State (Server-Side)

```c
typedef struct {
    PlayerState players[2];
    uint8_t active_player;
    uint8_t phase;
    uint8_t turn_number;
    bool first_turn_first_player;

    // Combat state
    Card attack_cards[3];
    uint8_t attack_count;
    Card defense_cards[3];
    uint8_t defense_count;

    // Dice results
    uint8_t attack_dice[3];
    uint8_t defense_dice[3];

    // Configuration
    uint8_t deck_type;  // 0=random, 1=monochrome, 2=custom

    // Card visibility tracking
    CardVisibilityState card_visibility[120];

    bool game_over;
    uint8_t winner_id;
} GameState;
```

### 2.5 Visible Game State (Client-Side)

```c
typedef struct {
    uint8_t my_player_id;
    uint8_t active_player_id;
    uint8_t phase;

    // My complete information
    PlayerState my_state;

    // Opponent's visible information
    uint8_t opp_energy;
    uint8_t opp_lunas;
    uint8_t opp_hand_count;
    uint8_t opp_deck_count;
    uint8_t opp_discard_count;
    Card opp_discard[MAX_DISCARD_SIZE];  // Discard is visible

    // Current combat state
    Card attack_cards[3];
    uint8_t attack_count;
    Card defense_cards[3];
    uint8_t defense_count;

    // Game metadata
    uint8_t turn_number;
    uint8_t last_damage;
    bool game_over;
    uint8_t winner_id;
} VisibleGameState;
```

**Key Function:**

```c
void gamestate_get_visible(const GameState* gs, uint8_t player_id, 
                           VisibleGameState* visible);
```

---

## 3. Action System

### 3.1 Action Types

```c
// possibly in action.h/c
typedef enum {
    ACTION_DRAW_CARD,
    ACTION_PLAY_CHAMPIONS,          // 1-3 champions for attack/defense
    ACTION_PLAY_DRAW_RECALL,        // Draw cards or recall champions
    ACTION_PLAY_EXCHANGE,           // Exchange champion for 5 lunas
    ACTION_PASS_TURN,
    ACTION_DEFEND,
    ACTION_DECLINE_DEFENSE,
    ACTION_DISCARD_CARDS,           // For mulligan and discard to 7
    ACTION_OFFER_DRAW,
    ACTION_ACCEPT_DRAW,
    ACTION_REFUSE_DRAW,
    ACTION_FORFEIT,
    ACTION_QUIT,
    ACTION_REQUEST_GAMESTATE,
    ACTION_INVALID
} ActionType;
```

### 3.2 Action Structure

```c
typedef struct {
    uint8_t card_ids[3];       // Up to 3 cards (champions played, champion exchanged, cards discated)
    uint8_t num_cards;         // champion played, cards discarted
    bool recall_option;        // For draw/recall: change this to an enum instead using RECALLCCARDS and DRAWCARDS
    uint8_t recall_ids[2];     // Champion IDs to recall (max 2)
    uint8_t num_recall;
} CardPlayData;

typedef struct {
    ActionType type;
    uint8_t player_id;
    union {
        CardPlayData cards;
        uint8_t simple_value;
    } data;
} Action;
```

### 3.3 Action Creation & Validation

```c
// Action constructors
Action action_create_draw(uint8_t player_id);
Action action_create_play_champions(uint8_t player_id, 
                                     const uint8_t* card_ids, 
                                     uint8_t num_cards);
Action action_create_pass(uint8_t player_id);
Action action_create_discard(uint8_t player_id,
                             const uint8_t* card_ids,
                             uint8_t num_cards);

// Validation
bool action_is_valid(const Action* action, const VisibleGameState* vgs);

// Serialization for network transport
void action_serialize(const Action* action, uint8_t* buffer, size_t* length);
bool action_deserialize(Action* action, const uint8_t* buffer, size_t length);
```

### 3.4 Action Generation (Central Function)

**Located in: `gamestate_logic.c`**

```c
#define MAX_POSSIBLE_ACTIONS 256

typedef struct {
    Action actions[MAX_POSSIBLE_ACTIONS];
    uint16_t count;
} ActionList;

// Primary function available to all AI agents and UI
void get_list_of_possible_actions(const VisibleGameState* vgs, 
                                   ActionList* list);

// Phase-specific helpers (internal)
void get_attack_actions(const VisibleGameState* vgs, ActionList* list);
void get_defense_actions(const VisibleGameState* vgs, ActionList* list);
void get_draw_recall_actions(const VisibleGameState* vgs, ActionList* list);
void get_discard_actions(const VisibleGameState* vgs, ActionList* list);

// Utility functions
bool can_afford_cards(const PlayerState* ps, const uint8_t* card_ids, 
                      uint8_t num_cards);
uint8_t calculate_combo_bonus(const Card* cards, uint8_t num_cards, 
                               uint8_t deck_type);
```

**Design Notes:**

- AI agents call `get_list_of_possible_actions()` to get legal moves
- AI agent's `select_action()` function chooses from the list
- Decision logic is separated from move generation
- Works entirely with `VisibleGameState` (no hidden information leaks)

---

## 4. Game State & Visibility

### 4.1 Visibility Rules

**Public Information (visible to both players):**

- Both players' energy and lunas
- Combat zones (cards being played)
- Both discard piles (complete contents)
- Deck sizes (not contents)
- Current turn/phase
- Previous actions taken

**Private Information (visible only to owner):**

- Own hand (specific cards)
- Draw order in own deck (unknown even to owner in server mode)

**Hidden Information (visible only to server):**

- Opponent's hand
- Exact cards in opponent's deck
- Order of cards in any deck
- RNG seed state

### 4.2 Visibility Functions

```c
// Convert full state to player-specific visible state
void gamestate_get_visible(const GameState* gs, uint8_t player_id, 
                           VisibleGameState* visible);

// Update visibility after card movements
void update_card_visibility_draw(GameState* gs, uint8_t player_id, 
                                 uint8_t card_id);
void update_card_visibility_play(GameState* gs, uint8_t card_id);
void update_card_visibility_discard(GameState* gs, uint8_t card_id);

// Serialization
void gamestate_serialize_visible(const VisibleGameState* vgs, 
                                 uint8_t* buffer, size_t* length);
bool gamestate_deserialize_visible(VisibleGameState* vgs, 
                                   const uint8_t* buffer, size_t length);
```

---

## 5. AI Agent Interface

### 5.1 Standard AI Interface

All AI agents must implement:

```c
// In ai_interface.h
typedef struct AIAgent AIAgent;

typedef Action (*AISelectActionFunc)(const VisibleGameState* vgs, 
                                     void* agent_data);

struct AIAgent {
    const char* name;
    AISelectActionFunc select_action;
    void* data;  // Agent-specific data
};

// Standard workflow for all agents:
// 1. Receive VisibleGameState
// 2. Call get_list_of_possible_actions() to get legal moves
// 3. Apply agent-specific logic to select best action
// 4. Return Action struct
```

### 5.2 Example Implementation

```c
// In ai_random.c
Action ai_random_select_action(const VisibleGameState* vgs, void* data) {
    ActionList possible_actions;
    get_list_of_possible_actions(vgs, &possible_actions);

    if (possible_actions.count == 0) {
        return action_create_pass(vgs->my_player_id);
    }

    // Select random action
    uint16_t index = rand() % possible_actions.count;
    return possible_actions.actions[index];
}

AIAgent ai_random = {
    .name = "Random",
    .select_action = ai_random_select_action,
    .data = NULL
};
```

### 5.3 AI Decision Points

AI agents must make decisions at:

1. **Mulligan phase**: Which cards to discard (max 2 for second player)
2. **Attack phase**: Play champions, draw/recall card, exchange card, or pass
3. **Defense phase**: Play defenders or decline
4. **Discard phase**: Which cards to discard (if hand > 7)

---

## 6. Client/Server Architecture

### 6.1 Communication Model

#### Standalone Mode (Single Process)

```
┌─────────────────────────────────────────────┐
│  Oracle Process                              │
│  ┌─────────┐    ┌──────────┐   ┌─────────┐ │
│  │   UI    │───→│ Gamestate│←──│ AI Agent│ │
│  │         │←───│  Logic   │───→│         │ │
│  └─────────┘    └──────────┘   └─────────┘ │
└─────────────────────────────────────────────┘
```

#### Client/Server Mode

```
┌──────────────┐                  ┌──────────────┐
│   Client A   │                  │   Client B   │
│ ┌──────────┐ │                  │ ┌──────────┐ │
│ │ UI/AI    │ │                  │ │ UI/AI    │ │
│ │ (Action) │ │                  │ │ (Action) │ │
│ └────┬─────┘ │                  │ └────┬─────┘ │
│      │       │                  │      │       │
│   [Socket]   │                  │   [Socket]   │
└──────┼───────┘                  └──────┼───────┘
       │                                 │
       │          ┌──────────────┐       │
       └─────────→│    Server    │←──────┘
                  │ ┌──────────┐ │
                  │ │ GameState│ │
                  │ │  Logic   │ │
                  │ │ (Full)   │ │
                  │ └──────────┘ │
                  │  - Validates │
                  │  - Applies   │
                  │  - Broadcasts│
                  └──────────────┘
```

### 6.2 Message Protocol

#### Message Types

```c
typedef enum {
    MSG_ACTION,           // Client → Server: Action to perform
    MSG_GAMESTATE,        // Server → Client: Visible game state
    MSG_GAME_EVENT,       // Server → Client: Event notification
    MSG_COMBAT_RESULT,    // Server → Clients: Dice rolls & damage
    MSG_ERROR,            // Server → Client: Error message
    MSG_QUIT,             // Client → Server: Disconnect
    MSG_REQUEST_STATE     // Client → Server: Request state resync
} MessageType;

typedef struct {
    MessageType type;
    uint8_t player_id;
    uint16_t length;
    uint8_t data[512];
} Message;
```

#### Text Protocol (Development/Debugging)

```
Commands (4 letters + optional parameters):
  cham 01,04,06  - Play champion cards at hand indices 1, 4, 6
  draw           - Play draw/recall card (draw option)
  recl 03,05     - Play draw/recall card (recall champions 3, 5)
  exch 02        - Play exchange card (exchange champion 2)
  pass           - Pass turn
  defe 01,02     - Defend with cards at hand indices 1, 2
  decl           - Decline defense
  disc 00,03     - Discard cards at hand indices 0, 3
  quit           - Disconnect
  stat           - Request full game state
```

### 6.3 Server Workflow

1. **Accept Connections**: Wait for 2 clients to connect

2. **Initialize Game**: Set up decks, deal hands, handle mulligan

3. **Game Loop**:
   
   ```
   While not game_over:
     a. Send visible state to both clients
     b. Wait for action from active player
     c. Validate action
     d. Apply action to game state
     e. If combat: roll dice, calculate damage, apply
     f. Broadcast action result to both clients
     g. Check win condition
     h. Advance turn/phase
   ```

4. **Game End**: Notify clients of winner, close connections

### 6.4 Key Server Functions

```c
// Server management
bool server_init(Server* srv, uint16_t port);
void server_shutdown(Server* srv);
bool server_accept_client(Server* srv);

// Message handling
bool server_receive_message(Server* srv, uint8_t player_id, Message* msg);
void server_send_message(Server* srv, uint8_t player_id, const Message* msg);
void server_broadcast_action(Server* srv, const Action* action);

// Game logic
bool server_apply_action(Server* srv, const Action* action);
void server_process_combat(Server* srv);
void server_roll_dice(uint8_t* dice, uint8_t count);

// State synchronization
void server_send_visible_state(Server* srv, uint8_t player_id);
```

### 6.5 Client Workflow

1. **Connect**: Establish connection to server

2. **Receive Initial State**: Get starting visible game state

3. **Game Loop**:
   
   ```
   While not game_over:
     a. Display visible game state
     b. If my turn:
        - Get action from player (human UI or AI agent)
        - Send action to server
     c. Wait for updates from server
     d. Update local visible state
     e. Display combat results if any
   ```

4. **Disconnect**: Close connection gracefully

### 6.6 Key Client Functions

```c
// Connection
bool client_connect(Client* cli, const char* host, uint16_t port);
void client_disconnect(Client* cli);

// Communication
bool client_send_action(Client* cli, const Action* action);
bool client_receive_update(Client* cli);
void client_request_gamestate(Client* cli);

// Action generation (human or AI)
Action client_get_action_from_ui(Client* cli);
Action client_get_action_from_ai(Client* cli, AIAgent* agent);
```

Old notes on Client / Server approach that need to be integrated above:

    AI client to be called by server: could be on the same physical machine as the server, or a different one
    source code files could be split between client (mostly user I/O) (cl_xyz.c), server (game logic and state) (sr_abc.c) and shared constants and structures (sh_xyz.c)
      ai client implementation could be in ai_xyz.c source files (likely mostly using the strategy.c files)
      simulation in standalone mode could be in sim_xyz.c source files
    
    client and server communicate via socket text messages when on different machines or simple function calls (with the 'text commands') when running in standalone mode
    when in 'network' mode, text messages and function calls travel this way:
        client -> client command to text translation -> socket port (client side) -> net -> socket (server side) -> server text to command interpreter -> server
          in 'network' mode, both sockets could also be on the same PC on 2 different ports, attached to 2 independent programs
    
    when in 'standalone' mode, message could travel this way. may want to eventually consider further reducing 'overhead' created by the 'encode, then decode' of the command by using
      another more efficient / fast way to get the client and server to communicate (e.g. directly passing an 'action' struct when the client wants to inform the server of the
      action it wants to take?):
        client -> client command to text translation -> server text to command interpreter -> server  
    
    text commands: 4 letters (mandatory) + 6 digits (optional) (e.g. "play 01,04,06" or "cham 01,04,06" to indicate we want to play card IDs 1, 4 and 6 (decide if IDs should represent
     position in player's hand array, or the actual card's index in the fullDeck array)
    

## 7. User Interface Modes

### 7.1 Text User Interface (TUI) - ncurses

#### Layout Design

```
┌────────────────────────────────────┬─────────────────────────┐
│                                    │  Console                │
│  Game Table                        │  >                      │
│  ┌──────────────────────────┐     │  > help                 │
│  │ Opponent                  │     │  Available commands:    │
│  │ Energy: 85  Lunas: 12    │     │  - help                 │
│  │ Hand: 5  Deck: 28        │     │  - simmode              │
│  │                           │     │  - quit                 │
│  ├──────────────────────────┤     │  >                      │
│  │ Combat Zone               │     │                         │
│  │ Attack: [Dragon][Elf]    │     │                         │
│  │ Defense: [Orc]           │     │                         │
│  ├──────────────────────────┤     │                         │
│  │ You                       │     │                         │
│  │ Energy: 92  Lunas: 15    │     │                         │
│  │ Hand: [1][2][3][4][5][6] │     │                         │
│  │ Deck: 30                  │     │                         │
│  └──────────────────────────┘     │                         │
└────────────────────────────────────┴─────────────────────────┘
```

#### Implementation Structure

```c
// In ui_tui.c
typedef struct {
    WINDOW* game_win;
    WINDOW* console_win;
    VisibleGameState* vgs;
} TUI;

void tui_init(TUI* tui);
void tui_shutdown(TUI* tui);
void tui_display_state(TUI* tui);
Action tui_get_player_action(TUI* tui);
void tui_display_message(TUI* tui, const char* msg);

// Mode switching
void tui_switch_to_sim_mode(TUI* tui);
```

### 7.2 Simulation Mode (Interactive)

#### Layout Design

```
┌────────────────────────────────────┬─────────────────────────┐
│  Simulation Results                │  Console                │
│                                    │  > run 1000             │
│  Games: 1000                       │  Running 1000 games...  │
│  Strategy A: Random                │  Progress: ████░░ 80%   │
│  Strategy B: Balanced              │  >                      │
│                                    │  > export results.txt   │
│  Win Rate A: 47.3%                 │  Results exported       │
│  Win Rate B: 52.7%                 │  > tuimode              │
│                                    │  Switching to TUI...    │
│  Avg Game Length: 28.4 turns       │  >                      │
│                                    │                         │
├────────────────────────────────────┤  Parameters:            │
│ Parameters:                        │  Games: 1000            │
│ ┌────────────────────────────────┐ │  Output: results.txt    │
│ │ Games: [1000]                  │ │                         │
│ │ Strategy A: [Random    ▼]      │ │                         │
│ │ Strategy B: [Balanced  ▼]      │ │                         │
│ │ Output File: [results.txt]     │ │                         │
│ │ [Run Simulation]               │ │                         │
│ └────────────────────────────────┘ │                         │
└────────────────────────────────────┴─────────────────────────┘
```

### 7.3 GUI (Future)

**Technology Option tentatively selected:**

- SDL3 (cross-platform, C-friendly)

**Key Features:**

- Card art display
- Drag-and-drop card playing
- Chat window for networked play

---

## 8. Game Mechanics

### 8.1 Combo Bonus Calculation

**Pseudo-code:**

```
s1, s2, s3: species of cards 1, 2, 3
c1, c2, c3: colors of cards 1, 2, 3
o1, o2, o3: orders (ranks) of cards 1, 2, 3

CASE deck_drawing_approach:

RANDOM:
  IF 2 cards:
    IF s2==s1 THEN +10
    ELSE IF o2==o1 THEN +7
    ELSE IF c2==c1 THEN +5

  IF 3 cards:
    IF s2==s1 THEN
      IF s3==s1 THEN +16
      ELSE IF o3==o1 THEN +14
      ELSE IF c3==c1 THEN +13
      ELSE +10
    ELSE IF s3==s1 THEN
      IF o3==o2 THEN +14
      ELSE IF c3==c2 THEN +13
      ELSE +10
    ELSE IF o2==o1 THEN
      IF o3==o1 THEN +11
      ELSE IF c3==c2 THEN +9
      ELSE +7
    ELSE IF o3==o1 THEN
      IF c2==c1 THEN +9
      ELSE +7
    ELSE IF c2==c1 THEN
      IF c3==c1 THEN +8
      ELSE +5
    ELSE IF c3==c1 THEN +5

MONOCHROME:
  IF 2 cards:
    IF o2==o1 THEN +7

  IF 3 cards:
    IF o2==o1 THEN
      IF o3==o1 THEN +12
      ELSE +7

CUSTOM:
  IF 2 cards:
    IF s2==s1 THEN +7
    ELSE IF o2==o1 THEN +4

  IF 3 cards:
    IF s2==s1 THEN
      IF s3==s1 THEN +12
      ELSE IF o3==o1 THEN +9
      ELSE +7
    ELSE IF s3==s1 THEN
      IF o3==o2 THEN +9
      ELSE +7
    ELSE IF o2==o1 THEN
      IF o3==o1 THEN +6
      ELSE +4
    ELSE IF o3==o1 THEN +4
```

### 8.2 Non-Champion Card Power Heuristic

**Calibration Approach:**

1. Run simulations with Player A using power values 2-15 for draw/recall cards
2. Player B uses fixed default value (e.g., 5)
3. Find optimal value that maximizes A's win rate
4. Verify by running reverse test (B varies, A uses optimal)
5. Confirm optimal value yields ~50% win rate when both use it
6. Repeat for exchange cards

**Current Working Values:**

- Draw 2 / Recall 1: TBD (requires calibration)
- Draw 3 / Recall 2: TBD (requires calibration)
- Exchange Champion: TBD (requires calibration)

---

## 9. File Organization

```
oracle/
├── src/
│   ├── core/
│   │   ├── action.c/h              (~150 lines)
│   │   ├── gamestate.c/h           (~300 lines)
│   │   ├── gamestate_logic.c/h     (~400 lines - includes get_list_of_possible_actions)
│   │   ├── card_data.c/h           (~200 lines)
│   │   ├── combat.c/h              (~200 lines)
│   │   └── visibility.c/h          (~250 lines - card visibility system)
│   ├── network/
│   │   ├── server.c/h              (~350 lines)
│   │   ├── client.c/h              (~250 lines)
│   │   └── protocol.c/h            (~150 lines)
│   ├── ai/
│   │   ├── ai_interface.h          (~50 lines)
│   │   ├── ai_random.c/h           (~150 lines)
│   │   ├── ai_balanced.c/h         (~250 lines)
│   │   ├── ai_heuristic.c/h        (~300 lines)
│   │   ├── ai_hybrid.c/h           (~350 lines)
│   │   └── ai_mcts.c/h             (~500 lines - future)
│   ├── ui/
│   │   ├── ui_interface.h          (~50 lines)
│   │   ├── ui_tui.c/h              (~400 lines)
│   │   ├── ui_sim.c/h              (~350 lines)
│   │   └── ui_gui.c/h              (~500 lines - future)
│   ├── sim/
│   │   ├── simulation.c/h          (~300 lines)
│   │   └── calibration.c/h         (~250 lines)
│   └── main/
│       ├── main.c                  (~150 lines)
│       ├── main_server.c           (~100 lines)
│       ├── main_client.c           (~100 lines)
│       └── main_standalone.c       (~150 lines)
├── scripts/
│   ├── generate_cards.py
│   ├── test_protocol.py
│   └── calibrate_heuristics.py
├── doc/
│   ├── DESIGN.md                   (this file)
│   ├── TODO.md
│   ├── API.md
│   └── PROTOCOL.md
├── Makefile
└── README.md
```



## 10. Key Implementation Guidelines

1. **Visibility**: Always work with `VisibleGameState` in client/AI code
2. **Action Flow**: UI/AI creates Action → Server validates → Server applies → Server broadcasts
3. **Testing**: Each module should be testable in isolation
4. **Documentation**: Doxygen-style comments for all public functions

---

## 11. Future Enhancements

### Phase 1 Enhancements (Post-Core Implementation)

- Reconnection handling
- Game state saving/loading
- Replay system
- Statistics tracking

### Phase 2 Enhancements

- Advanced AI (MCTS variants, neural networks)
- Mobile clients (iOS/Android)
- Spectator mode
- Ranked matchmaking

### Performance Optimizations

- Profile hot paths in combo calculation
- Memory pool for frequently allocated structures
- Message batching for network efficiency