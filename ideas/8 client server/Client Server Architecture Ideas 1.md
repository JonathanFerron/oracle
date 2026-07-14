

## Core Architecture Design

### 1. Action System (`action.h` / `action.c`)

```c
// action.h
#ifndef ACTION_H
#define ACTION_H

#include <stdint.h>
#include <stdbool.h>

// Action types
typedef enum {
    ACTION_DRAW_CARD,
    ACTION_PLAY_CHAMPIONS,      // 1-3 champions for attack/defense
    ACTION_PLAY_DRAW_RECALL,    // Draw cards or recall champions
    ACTION_PLAY_EXCHANGE,       // Exchange champion for 5 lunas
    ACTION_PASS_TURN,
    ACTION_DEFEND,
    ACTION_DECLINE_DEFENSE,
    // Special commands
    ACTION_QUIT,
    ACTION_REQUEST_GAMESTATE,
    ACTION_INVALID
} ActionType;

// Action payload for playing cards
typedef struct {
    uint8_t card_ids[3];  // Up to 3 cards
    uint8_t num_cards;
    bool recall_option;   // For draw/recall cards: true=recall
    uint8_t recall_ids[2]; // Champion IDs to recall (max 2)
    uint8_t num_recall;
} CardPlayData;

// Main action structure
typedef struct {
    ActionType type;
    uint8_t player_id;
    union {
        CardPlayData cards;
        uint8_t simple_value;  // For simple actions
    } data;
} Action;

// Action validation and utilities
bool action_is_valid(const Action* action, const void* gamestate);
Action action_create_draw(uint8_t player_id);
Action action_create_play_champions(uint8_t player_id, const uint8_t* card_ids, 
                                     uint8_t num_cards);
Action action_create_pass(uint8_t player_id);
void action_serialize(const Action* action, uint8_t* buffer, size_t* length);
bool action_deserialize(Action* action, const uint8_t* buffer, size_t length);

#endif // ACTION_H
```

### 2. Game State and Visibility (`gamestate.h`)

```c
// gamestate.h
#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_HAND_SIZE 20
#define MAX_DECK_SIZE 40
#define MAX_DISCARD_SIZE 40
#define INITIAL_ENERGY 99
#define INITIAL_LUNAS 30

// Card structure
typedef struct {
    uint8_t id;           // Unique card identifier
    uint8_t champion_id;  // 0-101 for champions, 102-119 for special cards
    uint8_t cost;         // Luna cost
    uint8_t attack;       // Attack value (0 for non-champions)
    uint8_t defense;      // Defense value (0 for non-champions)
    uint8_t color;        // 0=Indigo, 1=Orange, 2=Rouge
    uint8_t species;      // 0-4 (Centaur, Dwarf, Fairy, Minotaur, Lycan, etc.)
    uint8_t rank;         // 0-4 (A-E)
} Card;

// Player state
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

// Visible game state (what each player can see)
typedef struct {
    uint8_t my_player_id;
    uint8_t active_player_id;
    uint8_t phase;  // Setup, Attack, Defense, etc.

    // My full information
    PlayerState my_state;

    // Opponent's visible information
    uint8_t opp_energy;
    uint8_t opp_lunas;
    uint8_t opp_hand_count;
    uint8_t opp_deck_count;
    uint8_t opp_discard_count;

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

// Full game state (server-side only)
typedef struct {
    PlayerState players[2];
    uint8_t active_player;
    uint8_t phase;
    uint8_t turn_number;
    bool first_turn_first_player;  // Special rule: no draw on first turn

    // Combat state
    Card attack_cards[3];
    uint8_t attack_count;
    Card defense_cards[3];
    uint8_t defense_count;

    // Dice results
    uint8_t attack_dice[3];
    uint8_t defense_dice[3];

    // Game configuration
    uint8_t deck_type;  // 0=random, 1=monochrome, 2=custom

    bool game_over;
    uint8_t winner_id;
} GameState;

// Visibility functions
void gamestate_get_visible(const GameState* gs, uint8_t player_id, 
                           VisibleGameState* visible);
void gamestate_serialize_visible(const VisibleGameState* vgs, uint8_t* buffer, 
                                 size_t* length);
bool gamestate_deserialize_visible(VisibleGameState* vgs, const uint8_t* buffer, 
                                   size_t length);

#endif // GAMESTATE_H
```

### 3. Action Generation (`action_generator.h`)

```c
// action_generator.h
#ifndef ACTION_GENERATOR_H
#define ACTION_GENERATOR_H

#include "action.h"
#include "gamestate.h"

#define MAX_POSSIBLE_ACTIONS 256

// List of possible actions
typedef struct {
    Action actions[MAX_POSSIBLE_ACTIONS];
    uint16_t count;
} ActionList;

// Central function available to all AI agents
void get_list_of_possible_actions(const VisibleGameState* vgs, ActionList* list);

// Helper functions for specific phases
void get_attack_actions(const VisibleGameState* vgs, ActionList* list);
void get_defense_actions(const VisibleGameState* vgs, ActionList* list);
void get_draw_recall_actions(const VisibleGameState* vgs, ActionList* list);
void get_discard_actions(const VisibleGameState* vgs, ActionList* list);

// Action filtering
bool can_afford_cards(const PlayerState* ps, const uint8_t* card_ids, 
                      uint8_t num_cards);
uint8_t calculate_combo_bonus(const Card* cards, uint8_t num_cards, 
                               uint8_t deck_type);

#endif // ACTION_GENERATOR_H
```

### 4. Server Architecture (`server.h`)

```c
// server.h
#ifndef SERVER_H
#define SERVER_H

#include "gamestate.h"
#include "action.h"
#include <stdbool.h>

typedef enum {
    MSG_ACTION,
    MSG_GAMESTATE,
    MSG_GAME_EVENT,
    MSG_ERROR,
    MSG_QUIT
} MessageType;

typedef struct {
    MessageType type;
    uint8_t player_id;
    uint16_t length;
    uint8_t data[512];  // Payload
} Message;

// Server state
typedef struct {
    GameState game;
    int client_fds[2];
    bool clients_connected[2];
    bool game_started;
} Server;

// Server functions
bool server_init(Server* srv, uint16_t port);
void server_shutdown(Server* srv);
bool server_accept_client(Server* srv);
bool server_receive_message(Server* srv, uint8_t player_id, Message* msg);
void server_broadcast_action(Server* srv, const Action* action);
void server_send_visible_state(Server* srv, uint8_t player_id);
bool server_apply_action(Server* srv, const Action* action);
void server_process_combat(Server* srv);
void server_roll_dice(uint8_t* dice, uint8_t count);

#endif // SERVER_H
```

### 5. Client Architecture (`client.h`)

```c
// client.h
#ifndef CLIENT_H
#define CLIENT_H

#include "gamestate.h"
#include "action.h"
#include <stdbool.h>

typedef struct {
    int socket_fd;
    uint8_t my_player_id;
    VisibleGameState visible_state;
    bool connected;
} Client;

// Client functions
bool client_connect(Client* cli, const char* host, uint16_t port);
void client_disconnect(Client* cli);
bool client_send_action(Client* cli, const Action* action);
bool client_receive_update(Client* cli);
void client_request_gamestate(Client* cli);

#endif // CLIENT_H
```

### 6. UI Interface (`ui.h`)

```c
// ui.h
#ifndef UI_H
#define UI_H

#include "gamestate.h"
#include "action.h"
#include <stdbool.h>

typedef enum {
    UI_MODE_STANDALONE,  // Local game with AI
    UI_MODE_CLIENT       // Client/server mode
} UIMode;

typedef struct {
    UIMode mode;
    VisibleGameState visible_state;
    void* mode_data;  // Points to Client* or local game data
} UI;

// UI functions
void ui_init(UI* ui, UIMode mode);
void ui_shutdown(UI* ui);
void ui_display_state(const UI* ui);
Action ui_get_player_action(const UI* ui);
void ui_display_action(const Action* action);
void ui_display_combat_result(uint8_t damage, uint8_t attacker_total, 
                               uint8_t defender_total);

#endif // UI_H
```

### 7. File Organization

```
oracle/
├── src/
│   ├── core/
│   │   ├── action.c/h           (~150 lines)
│   │   ├── gamestate.c/h        (~300 lines)
│   │   ├── action_generator.c/h (~400 lines)
│   │   ├── card_data.c/h        (~200 lines - card definitions)
│   │   └── combat.c/h           (~200 lines - combat logic)
│   ├── network/
│   │   ├── server.c/h           (~350 lines)
│   │   ├── client.c/h           (~250 lines)
│   │   └── protocol.c/h         (~150 lines - serialization)
│   ├── ai/
│   │   ├── ai_interface.h       (~50 lines)
│   │   ├── ai_random.c/h        (~150 lines)
│   │   ├── ai_greedy.c/h        (~200 lines)
│   │   └── ai_mcts.c/h          (~400 lines - future)
│   ├── ui/
│   │   ├── ui.c/h               (~300 lines)
│   │   ├── ui_terminal.c/h      (~250 lines)
│   │   └── ui_display.c/h       (~200 lines - rendering helpers)
│   └── main/
│       ├── main_server.c        (~100 lines)
│       ├── main_client.c        (~100 lines)
│       └── main_standalone.c    (~150 lines)
├── scripts/
│   ├── generate_cards.py        (Card data generation)
│   └── test_protocol.py         (Protocol testing)
├── Makefile
└── README.md
```

### 8. Key Design Principles

**Action Flow:**

1. **Standalone mode**: UI → Action → Local GameState → UI
2. **Client/Server mode**: Client UI → Action → Server → GameState → Broadcast → All Clients

**Information Hiding:**

- Clients only receive `VisibleGameState`
- Server maintains full `GameState`
- `get_list_of_possible_actions()` works with visible state only

**Serialization:**

- All structs have serialize/deserialize functions
- Binary protocol for efficiency
- Length-prefixed messages

**AI Integration:**

- All AI agents use same interface: `Action ai_choose_action(const VisibleGameState* vgs)`
- All AIs call `get_list_of_possible_actions()` for legal moves
- Easy to add new AI implementations



# Client/Server Architecture for Oracle: The Champions of Arcadia

## A Comprehensive Design Guide for Turn-Based Card Games

---

## Table of Contents

1. [Introduction & Theoretical Foundation](#1-introduction--theoretical-foundation)
2. [Core Architecture Principles](#2-core-architecture-principles)
3. [Network Communication Patterns](#3-network-communication-patterns)
4. [Game State Management](#4-game-state-management)
5. [Security & Anti-Cheat Measures](#5-security--anti-cheat-measures)
6. [Synchronization & Consistency](#6-synchronization--consistency)
7. [Protocol Design](#7-protocol-design)
8. [Client Architecture](#8-client-architecture)
9. [Server Architecture](#9-server-architecture)
10. [Implementation Roadmap](#10-implementation-roadmap)

---

## 1. Introduction & Theoretical Foundation

### 1.1 The Client/Server Paradigm

The client/server architecture represents a fundamental computing model where responsibilities are distributed between service providers (servers) and service requesters (clients). This separation offers several critical advantages for multiplayer games:

**Theoretical Foundation:**

- **Separation of Concerns**: Different components handle different responsibilities
- **Information Hiding**: Clients don't need to know implementation details
- **Centralized Authority**: Server acts as the single source of truth
- **Scalability**: Multiple clients can connect to one server

**Why Client/Server for Oracle?**

1. **Trust Model**: In a turn-based card game, you cannot trust clients with hidden information (opponent's hand, deck order)
2. **Game Integrity**: Server prevents cheating by validating all actions
3. **Consistency**: Server ensures both players see the same game state
4. **Future Expansion**: Supports AI opponents, matchmaking, rankings, and tournaments

### 1.2 Alternative Architectures (And Why We Don't Use Them)

**Peer-to-Peer (P2P):**

- **Pro**: No server needed, direct connection
- **Con**: Trust issues - either player could cheat by modifying their client
- **Verdict**: Unsuitable for Oracle due to hidden information

**Standalone:**

- **Pro**: No network complexity
- **Con**: Only local play or "hot seat" multiplayer
- **Verdict**: Good for single-player vs AI, insufficient for remote play

**Cloud Gaming (Server-side rendering):**

- **Pro**: No client-side computation needed
- **Con**: High latency, bandwidth intensive
- **Verdict**: Overkill for a turn-based game

### 1.3 Oracle-Specific Requirements

Your game has unique characteristics that inform the architecture:

1. **Turn-Based**: No real-time requirements, latency tolerance is high
2. **Hidden Information**: Cards in hand, deck order must be secret
3. **Random Elements**: Dice rolls, deck shuffling must be server-authoritative
4. **Complex Rules**: Combo bonuses, combat resolution requires validation
5. **Game Length**: ~30 rounds, ~60 turns, manageable state size

---

## 2. Core Architecture Principles

### 2.1 The Authoritative Server Model

**Definition**: The server is the single source of truth for all game state. Clients are "thin" - they display information and send input, but never make authoritative decisions.

**Implementation for Oracle:**

```
┌─────────────────────────────────────────────────────────┐
│                    AUTHORITATIVE SERVER                  │
│                                                          │
│  ┌────────────────────────────────────────────────┐    │
│  │         Full Game State (Hidden)               │    │
│  │  • Both players' hands (all cards)             │    │
│  │  • Both decks (order known)                    │    │
│  │  • Energy, cash, discard piles                 │    │
│  │  • Combat zones                                │    │
│  │  • RNG state (for dice, shuffling)             │    │
│  └────────────────────────────────────────────────┘    │
│                                                          │
│  ┌────────────────┐        ┌────────────────┐          │
│  │ Game Logic     │        │ Validation     │          │
│  │ • Turn flow    │        │ • Legal moves  │          │
│  │ • Combat       │        │ • Action costs │          │
│  │ • Combos       │        │ • Win/loss     │          │
│  └────────────────┘        └────────────────┘          │
└─────────────────────────────────────────────────────────┘
          │                              │
          │ Visible State                │ Visible State
          │ (filtered)                   │ (filtered)
          ▼                              ▼
    ┌──────────┐                   ┌──────────┐
    │ Client A │                   │ Client B │
    │          │                   │          │
    │ • My hand│                   │ • My hand│
    │ • Opp #  │                   │ • Opp #  │
    │ • Public │                   │ • Public │
    └──────────┘                   └──────────┘
```

**Key Principle**: Clients never "decide" what happens - they only:

1. Display the visible game state
2. Collect player input
3. Send action requests to server
4. Receive and display server responses

### 2.2 Information Visibility Model

Oracle requires careful control of what each player can see. Use a **three-tier visibility system**:

**Tier 1 - Public Information** (visible to both players):

- Both players' energy
- Both players' cash (lunas)
- Combat zones (cards being played)
- Discard piles (both players)
- Deck sizes (not contents)
- Current turn/phase
- Previous actions taken

**Tier 2 - Private Information** (visible only to owner):

- Own hand (specific cards)
- Draw order in own deck (unknown even to owner)

**Tier 3 - Hidden Information** (visible only to server):

- Opponent's hand
- Exact cards in opponent's deck
- Order of cards in any deck
- RNG seed state

**Data Structure Example:**

```c
// Server-side: Full game state
typedef struct {
    PlayerState players[2];      // Complete info for both
    uint8_t deck_order[2][40];   // Exact card order
    MTRand rng;                  // RNG state
    // ... full state
} ServerGameState;

// Client-side: Visible game state
typedef struct {
    uint8_t my_player_id;

    // My complete info
    PlayerState my_state;

    // Opponent's public info only
    uint8_t opp_energy;
    uint8_t opp_lunas;
    uint8_t opp_hand_count;      // Count, not cards
    uint8_t opp_deck_count;

    // Public shared info
    Card combat_zone_attacker[3];
    Card combat_zone_defender[3];
    // ...
} ClientGameState;
```

### 2.3 Request/Response vs. Event-Driven

For turn-based games, **request/response** is more appropriate than event-driven:

**Request/Response Model:**

```
Client: "I want to play cards 1, 3, 5 as attacker"
  ↓
Server: Validates request
  ↓
Server: Applies action to game state
  ↓
Server: "Action accepted. Here's the new visible state for you"
  ↓
Server: "Action notification for opponent: Player A played 3 cards"
  ↓
Both clients: Update display
```

**Why This Works for Oracle:**

- Turns are discrete
- No time pressure (unlike real-time games)
- Clear action boundaries
- Easy to validate before committing

### 2.4 Stateful vs. Stateless

**Oracle's server should be STATEFUL:**

**Stateful Server:**

- Maintains full game state in memory
- Each client connection is tracked
- Game continues across multiple messages

**Why stateful is correct:**

- Game state is too large to send with every message
- Game spans many turns (30+ rounds)
- Need to track whose turn it is
- RNG state must persist

**Trade-off**: Stateful servers are harder to scale, but for Oracle:

- You only need 1 server per game (2 players)
- Games are relatively short (~20 minutes)
- Memory footprint is small (~few KB per game)

---

## 3. Network Communication Patterns

### 3.1 Transport Layer: TCP vs. UDP

**Recommendation: TCP for Oracle**

**TCP (Transmission Control Protocol):**

- ✅ Reliable delivery (packets arrive or you know they failed)
- ✅ Ordered delivery (messages arrive in order sent)
- ✅ Connection-oriented (persistent connection)
- ❌ Slightly higher latency (but negligible for turn-based)

**UDP (User Datagram Protocol):**

- ✅ Lower latency
- ❌ No reliability guarantee
- ❌ No ordering guarantee
- ❌ You have to implement reliability yourself

**Why TCP wins for Oracle:**

1. **Turn-based**: Latency of 50-100ms is imperceptible
2. **Critical information**: Cannot afford lost actions
3. **Simpler code**: TCP handles reliability for you
4. **Debugging**: Easier to debug ordered, reliable messages

**When UDP would be better:**

- Real-time action games (FPS, racing)
- High-frequency updates (60+ per second)
- Voice chat

### 3.2 Message Serialization

You need to convert C structures to bytes for network transmission. Options:

**Option 1: Binary Protocol (Recommended for Oracle)**

```c
// Example: Action message
typedef struct {
    uint8_t msg_type;           // 1 byte: ACTION_PLAY_CHAMPIONS
    uint8_t player_id;          // 1 byte: PLAYER_A or PLAYER_B
    uint8_t num_cards;          // 1 byte: 1-3
    uint8_t card_ids[3];        // 3 bytes: indices in hand
    uint8_t reserved[4];        // 4 bytes: padding/future use
} __attribute__((packed)) ActionPlayChampions;

// Serialization: just memcpy to buffer
uint8_t buffer[10];
memcpy(buffer, &action, sizeof(action));
send(socket, buffer, sizeof(action), 0);
```

**Pros:**

- Fast: no parsing needed
- Compact: minimal bandwidth
- Simple: direct memory copy

**Cons:**

- Not human-readable (hard to debug)
- Endianness issues (big-endian vs little-endian)
- Version compatibility harder

**Option 2: Text Protocol (JSON, CSV)**

```json
{
  "type": "ACTION_PLAY_CHAMPIONS",
  "player": "A",
  "cards": [1, 3, 5]
}
```

**Pros:**

- Human-readable (great for debugging)
- Language-agnostic (easy to add web client)
- Self-documenting

**Cons:**

- Slower: parsing overhead
- Larger: more bandwidth
- More complex: need JSON library

**Recommendation for Oracle:**

- **Development/Testing**: Use JSON for easy debugging
- **Production**: Switch to binary for performance
- **Or**: Use binary with debug logging to text file

### 3.3 Connection Management

**Persistent Connection Model:**

```
Client connects → Server authenticates → Game loop → Client disconnects
    ↓                    ↓                    ↓              ↓
   TCP socket       Assign player ID      Exchange msgs    Close socket
```

**Design Pattern: One socket per player per game**

```c
typedef struct {
    int socket_fd;              // TCP socket
    uint8_t player_id;          // PLAYER_A or PLAYER_B
    bool connected;
    time_t last_activity;       // For timeout detection
    char player_name[64];
} ClientConnection;

typedef struct {
    GameState game;
    ClientConnection clients[2];
    bool game_started;
    uint32_t game_id;
} GameSession;
```

**Handling Disconnections:**

1. **Graceful disconnect**: Client sends "QUIT" message
2. **Timeout**: No message for 60 seconds → assume disconnect
3. **Error**: Socket error → disconnect

**Reconnection Strategy:**

```c
typedef struct {
    uint32_t game_id;
    uint8_t player_id;
    uint64_t auth_token;    // For reconnection authentication
} ReconnectRequest;
```

Allow reconnection within 5 minutes:

- Client sends game_id + auth_token
- Server verifies and resumes game
- Send full game state to reconnected client

---

## 4. Game State Management

### 4.1 State Synchronization

**Problem**: How do you keep both clients showing the same game state?

**Solution: Authoritative Server + Delta Updates**

**Full State Update** (sent on connect/reconnect):

```c
typedef struct {
    uint8_t msg_type;              // MSG_FULL_GAMESTATE
    uint32_t game_id;
    uint16_t turn_number;
    ClientGameState state;         // Entire visible state
} FullStateMessage;
```

**Delta Update** (sent after each action):

```c
typedef struct {
    uint8_t msg_type;              // MSG_DELTA_UPDATE
    uint16_t sequence_num;         // For ordering

    // What changed
    uint8_t player_affected;
    int8_t energy_delta;
    int8_t cash_delta;

    // Cards moved
    uint8_t cards_added_to_hand[3];
    uint8_t num_added;
    uint8_t cards_removed_from_hand[3];
    uint8_t num_removed;

    // ...
} DeltaUpdate;
```

**When to use each:**

- **Full state**: Connection, reconnection, or after validation error
- **Delta**: After every successful action

**Sequence Numbers**: Prevent out-of-order processing

```c
// Client side
static uint16_t expected_sequence = 0;

void handle_delta_update(DeltaUpdate *msg) {
    if (msg->sequence_num != expected_sequence) {
        // Out of order - request full state resync
        request_full_state_resync();
        return;
    }

    apply_delta_to_local_state(msg);
    expected_sequence++;
}
```

### 4.2 Server State Machine

The server progresses through clear states:

```
WAITING_FOR_PLAYERS
    ↓ (both connected)
GAME_SETUP
    ↓ (deck dealt, mulligan done)
PLAYER_A_TURN_DRAW
    ↓ (card drawn or skipped if turn 1)
PLAYER_A_TURN_ACTION
    ↓ (champion cards played or pass)
PLAYER_B_TURN_DEFENSE (if combat)
    ↓ (defense cards played or pass)
COMBAT_RESOLUTION (if combat occurred)
    ↓ (damage applied)
CHECK_WIN_CONDITION
    ↓ (if someone at 0 energy → GAME_OVER)
    ↓ (otherwise)
TURN_END_COLLECT_LUNA
    ↓
TURN_END_DISCARD
    ↓
SWITCH_PLAYER
    ↓ (back to PLAYER_X_TURN_DRAW)
GAME_OVER
```

**State Machine Implementation:**

```c
typedef enum {
    STATE_WAITING,
    STATE_SETUP,
    STATE_TURN_DRAW,
    STATE_TURN_ACTION,
    STATE_TURN_DEFENSE,
    STATE_COMBAT,
    STATE_CHECK_WIN,
    STATE_TURN_END,
    STATE_GAME_OVER
} ServerGamePhase;

void server_game_tick(GameSession *session) {
    switch (session->phase) {
        case STATE_TURN_ACTION:
            // Wait for action from current player
            if (action_received) {
                validate_and_apply_action();
                if (combat_triggered) {
                    session->phase = STATE_TURN_DEFENSE;
                } else {
                    session->phase = STATE_TURN_END;
                }
            }
            break;

        case STATE_COMBAT:
            resolve_combat(&session->game);
            session->phase = STATE_CHECK_WIN;
            break;

        // ...
    }
}
```

### 4.3 Action Validation

**Critical Security Principle**: Never trust the client.

**Server must validate EVERY action:**

```c
bool validate_play_champions_action(GameState *gs, Action *action) {
    // 1. Is it this player's turn?
    if (action->player_id != gs->current_player) {
        return false;
    }

    // 2. Is it the right phase?
    if (gs->phase != PHASE_ATTACK && gs->phase != PHASE_DEFENSE) {
        return false;
    }

    // 3. Are we playing 1-3 cards?
    if (action->num_cards < 1 || action->num_cards > 3) {
        return false;
    }

    // 4. Do the cards exist in player's hand?
    for (int i = 0; i < action->num_cards; i++) {
        if (!card_in_hand(gs, action->player_id, action->card_ids[i])) {
            return false;
        }
    }

    // 5. Are they champion cards?
    for (int i = 0; i < action->num_cards; i++) {
        uint8_t card_idx = action->card_ids[i];
        if (fullDeck[card_idx].card_type != CHAMPION_CARD) {
            return false;
        }
    }

    // 6. Can player afford them?
    int total_cost = 0;
    for (int i = 0; i < action->num_cards; i++) {
        uint8_t card_idx = action->card_ids[i];
        total_cost += fullDeck[card_idx].cost;
    }
    if (total_cost > gs->players[action->player_id].lunas) {
        return false;
    }

    return true;  // All checks passed
}
```

**On validation failure:**

```c
if (!validate_action(gs, action)) {
    send_error_message(client, "Invalid action");
    send_full_state_resync(client);  // Resync client
    return;
}

// Apply action only after validation
apply_action(gs, action);
broadcast_action_notification(session, action);
```

---

## 5. Security & Anti-Cheat Measures

### 5.1 Threat Model

**Threats for Oracle:**

1. **Information Disclosure**: Client tries to see opponent's hand
2. **Action Manipulation**: Client tries to play cards they don't have
3. **Resource Cheating**: Client tries to have infinite lunas/energy
4. **Timing Attacks**: Client disconnects to avoid losing
5. **Replay Attacks**: Client resends old actions

**NOT threats** (because of authoritative server):

- Client modifies their own energy (server doesn't trust client's reported energy)
- Client forces opponent to discard (server controls all state changes)

### 5.2 Server-Side RNG

**Critical Security Rule**: ALL randomness must be generated on the server.

```c
// ❌ WRONG - Client-side RNG
void client_attack() {
    int dice_roll = rand() % 20 + 1;  // Client generates
    send_attack(dice_roll);            // Sends to server
}

// ✅ CORRECT - Server-side RNG
void server_process_attack(Action *action) {
    validate_action(action);

    // Server generates dice rolls
    uint8_t dice_results[3];
    for (int i = 0; i < num_attackers; i++) {
        dice_results[i] = RND_dn(defender->dice, &server_ctx);
    }

    // Server calculates damage
    int damage = calculate_damage(attackers, dice_results);

    // Server applies damage
    apply_damage(defender, damage);

    // Notify clients of results (including dice rolls)
    notify_combat_result(session, dice_results, damage);
}
```

**Shuffle on Server:**

```c
void server_shuffle_deck(GameSession *session, uint8_t player_id) {
    uint8_t *deck = session->game.deck[player_id];
    uint8_t size = session->game.deck_size[player_id];

    // Server uses its RNG
    RND_partial_shuffle(deck, size, size, &session->server_ctx);

    // Clients are NOT told the order
    // Only notify: "Your deck has been shuffled"
    send_notification(session->clients[player_id], 
                     MSG_DECK_SHUFFLED, NULL, 0);
}
```

### 5.3 Action Authentication

Every action should include authentication:

```c
typedef struct {
    uint8_t msg_type;
    uint8_t player_id;
    uint64_t auth_token;       // Prevents impersonation
    uint16_t sequence_num;     // Prevents replay
    uint64_t timestamp;        // Prevents old actions

    // ... action data
} AuthenticatedAction;

bool verify_action_auth(GameSession *session, AuthenticatedAction *action) {
    ClientConnection *client = &session->clients[action->player_id];

    // 1. Check token matches
    if (action->auth_token != client->auth_token) {
        log_security_violation("Invalid auth token");
        return false;
    }

    // 2. Check sequence number is next expected
    if (action->sequence_num != client->next_expected_sequence) {
        log_security_violation("Out of sequence action");
        return false;
    }

    // 3. Check timestamp is recent (within 60 seconds)
    uint64_t now = time(NULL);
    if (now - action->timestamp > 60) {
        log_security_violation("Stale action");
        return false;
    }

    return true;
}
```

### 5.4 Disconnect Handling

**Graceful Disconnects:**

```c
void handle_client_disconnect(GameSession *session, uint8_t player_id) {
    ClientConnection *client = &session->clients[player_id];

    if (session->phase == STATE_GAME_OVER) {
        // Game already over, just close
        close_socket(client->socket_fd);
        return;
    }

    // Save disconnect time
    client->disconnect_time = time(NULL);
    client->connected = false;

    // Notify opponent
    send_notification(session->clients[1 - player_id],
                     MSG_OPPONENT_DISCONNECTED, NULL, 0);

    // Start timer for reconnection window
    session->reconnect_deadline = time(NULL) + 300;  // 5 minutes

    // If they don't reconnect, opponent wins by forfeit
}

void check_reconnect_deadline(GameSession *session) {
    if (time(NULL) > session->reconnect_deadline) {
        // Time's up - award win to connected player
        uint8_t winner = session->clients[0].connected ? 0 : 1;
        session->game.game_state = winner ? PLAYER_B_WINS : PLAYER_A_WINS;
        session->phase = STATE_GAME_OVER;

        send_notification(session->clients[winner],
                         MSG_WIN_BY_FORFEIT, NULL, 0);
    }
}
```

---

## 6. Synchronization & Consistency

### 6.1 Optimistic vs. Pessimistic Updates

**Pessimistic (Server-Authoritative):** Wait for server confirmation

```
Client: "I want to play card 1"
Client: [Shows "Waiting for server..." indicator]
    ↓
Server: Validates, processes action
    ↓
Server: "Action accepted"
    ↓
Client: Updates display
```

**Optimistic (Prediction):** Update immediately, rollback if server rejects

```
Client: "I want to play card 1"
Client: [Immediately shows card being played]
    ↓
Server: Validates
    ↓
Server: "Action rejected - not enough lunas"
    ↓
Client: Rolls back display, shows error
```

**Recommendation for Oracle: PESSIMISTIC**

**Why:**

- Turn-based: no need for instant feedback
- Complex validation: hard to predict server decision
- Rollback is jarring: confusing for users
- Server latency is acceptable: 100-200ms is fine

**User Experience:**

```
┌──────────────────────────────────────────┐
│  [Card 1] [Card 2] [Card 3]              │
│                                           │
│  [Play Selected Cards]  ← Player clicks  │
│                                           │
│  Sending action to server...             │
│  ████████░░░░ [Cancel]                   │
└──────────────────────────────────────────┘
    ↓ (100-200ms later)
┌──────────────────────────────────────────┐
│  Cards played! Combat begins.            │
│  [Your Attack: 15] [Roll Dice]           │
└──────────────────────────────────────────┘
```

### 6.2 Handling Concurrent Actions

**Problem**: What if both players send actions at the exact same time?

**Solution for Oracle**: This shouldn't happen due to turn-based nature.

**Enforcement:**

```c
void server_process_action(GameSession *session, Action *action) {
    // Only accept actions from current player
    if (action->player_id != session->game.current_player) {
        send_error(session->clients[action->player_id],
                  "Not your turn");
        return;
    }

    // Only accept actions in valid phase
    if (!is_valid_phase_for_action(session->phase, action->type)) {
        send_error(session->clients[action->player_id],
                  "Invalid action for current phase");
        return;
    }

    // Process action atomically
    pthread_mutex_lock(&session->mutex);
    validate_and_apply_action(session, action);
    pthread_mutex_unlock(&session->mutex);
}
```

### 6.3 Client State Reconciliation

**Problem**: Client and server state drift (network issues, bugs)

**Detection:**

```c
// Server sends periodic state checksums
typedef struct {
    uint8_t msg_type;        // MSG_STATE_CHECKSUM
    uint32_t checksum;       // Hash of game state
    uint16_t turn_number;
} StateChecksum;

// Client computes its own checksum
uint32_t client_compute_checksum(ClientGameState *state) {
    // Simple hash of critical values
    return hash(state->my_energy) ^
           hash(state->opp_energy) ^
           hash(state->my_lunas) ^
           hash(state->opp_lunas) ^
           hash(state->turn_number);
}

void client_verify_checksum(StateChecksum *msg) {
    uint32_t local = client_compute_checksum(&local_state);

    if (local != msg->checksum) {
        // Mismatch - request full state
        send_request(MSG_REQUEST_FULL_STATE);
    }
}
```

---

## 7. Protocol Design

### 7.1 Message Types

**Oracle Protocol Message Catalog:**

```c
typedef enum {
    // Connection messages (0-9)
    MSG_CONNECT_REQUEST = 0,
    MSG_CONNECT_RESPONSE = 1,
    MSG_DISCONNECT = 2,
    MSG_HEARTBEAT = 3,

    // Game setup messages (10-19)
    MSG_GAME_START = 10,
    MSG_MULLIGAN_REQUEST = 11,
    MSG_MULLIGAN_RESPONSE = 12,

    // Action messages (20-39)
    MSG_ACTION_PLAY_CHAMPIONS = 20,
    MSG_ACTION_PLAY_DRAW = 21,
    MSG_ACTION_PLAY_CASH = 22,
    MSG_ACTION_PASS = 23,
    MSG_ACTION_DISCARD = 24,

    // State update messages (40-59)
    MSG_STATE_FULL = 40,
    MSG_STATE_DELTA = 41,
    MSG_STATE_CHECKSUM = 42,

    // Event notification messages (60-79)
    MSG_EVENT_CARD_DRAWN = 60,
    MSG_EVENT_COMBAT_START = 61,
    MSG_EVENT_COMBAT_RESULT = 62,
    MSG_EVENT_TURN_END = 63,
    MSG_EVENT_GAME_OVER = 64,

    // Error messages (80-89)
    MSG_ERROR_INVALID_ACTION = 80,
    MSG_ERROR_NOT_YOUR_TURN = 81,
    MSG_ERROR_INSUFFICIENT_FUNDS = 82,

    // Meta messages (90-99)
    MSG_CHAT = 90,
    MSG_REQUEST_FULL_STATE = 91,
    MSG_OPPONENT_DISCONNECTED = 92
} MessageType;
```

### 7.2 Message Format

**Wire Format (Binary Protocol):**

```
┌────────────┬────────────┬────────────┬──────────────────┐
│  Header    │  Auth      │  Payload   │  Checksum        │
│  (8 bytes) │  (16 bytes)│  (variable)│  (4 bytes)       │
└────────────┴────────────┴────────────┴──────────────────┘

Header:
  uint8_t  msg_type
  uint8_t  protocol_version
  uint16_t payload_length
  uint32_t sequence_num

Auth:
  uint64_t auth_token
  uint64_t timestamp

Payload:
  (message-specific data)

Checksum:
  uint32_t crc32
```

**Implementation:**

```c
typedef struct {
    uint8_t msg_type;
    uint8_t protocol_version;
    uint16_t payload_length;
    uint32_t sequence_num;
    uint64_t auth_token;
    uint64_t timestamp;
} __attribute__((packed)) MessageHeader;

void send_message(int socket, MessageType type, 
                  void *payload, uint16_t length,
                  ClientConnection *client) {
    MessageHeader header;
    header.msg_type = type;
    header.protocol_version = 1;
    header.payload_length = length;
    header.sequence_num = client->next_sequence++;
    header.auth_token = client->auth_token;
    header.timestamp = time(NULL);

    // Compute checksum
    uint32_t checksum = compute_crc32(&header, sizeof(header));
    checksum = update_crc32(checksum, payload, length);

    // Send header
    send(socket, &header, sizeof(header), 0);

    // Send payload
    if (length > 0) {
        send(socket, payload, length, 0);
    }

    // Send checksum
    send(socket, &checksum, sizeof(checksum), 0);
}
```

### 7.3 Example: Combat Resolution Protocol

**Sequence Diagram:**

```
Client A (Attacker)          Server               Client B (Defender)
      │                         │                         │
      │  ACTION_PLAY_CHAMPIONS  │                         │
      ├────────────────────────>│                         │
      │                         │  Validate               │
      │                         │  Apply to state         │
      │                         │                         │
      │      STATE_DELTA        │                         │
      │<────────────────────────┤                         │
      │                         │                         │
      │                         │    EVENT_COMBAT_START   │
      │                         ├────────────────────────>│
      │                         │    (attacker: 3 cards)  │
      │                         │                         │
      │                         │  Wait for defender      │
      │                         │
```

```
      │                         │<────────────────────────┤
      │                         │  ACTION_PLAY_CHAMPIONS  │
      │                         │  (or ACTION_PASS)       │
      │                         │                         │
      │                         │  Roll dice (server RNG) │
      │                         │  Calculate damage       │
      │                         │  Apply to state         │
      │                         │                         │
      │  EVENT_COMBAT_RESULT    │                         │
      │<────────────────────────┤                         │
      │  (dice:[4,8,6])         │  EVENT_COMBAT_RESULT    │
      │  (damage: 12)           ├────────────────────────>│
      │                         │  (dice:[4,8,6])         │
      │                         │  (damage: 12)           │
      │                         │                         │
      │      STATE_DELTA        │                         │
      │<────────────────────────┤                         │
      │  (your_energy: -12)     │      STATE_DELTA        │
      │                         ├────────────────────────>│
      │                         │  (opp_energy: -12)      │
```

**Message Structures:**

```c
// Attacker's action
typedef struct {
    MessageHeader header;  // msg_type = ACTION_PLAY_CHAMPIONS
    uint8_t num_cards;     // 1-3
    uint8_t card_ids[3];   // Indices in hand
} PlayChampionsMessage;

// Server notifies defender of combat
typedef struct {
    MessageHeader header;  // msg_type = EVENT_COMBAT_START
    uint8_t num_attackers;
    Card attacker_cards[3]; // Full card details
    uint16_t total_expected_attack;  // Without dice
} CombatStartEvent;

// Server sends combat results to both
typedef struct {
    MessageHeader header;  // msg_type = EVENT_COMBAT_RESULT
    uint8_t attacker_dice[3];
    uint8_t defender_dice[3];
    uint16_t total_attack;
    uint16_t total_defense;
    uint16_t damage_dealt;
    uint8_t combo_bonus_attacker;
    uint8_t combo_bonus_defender;
} CombatResultEvent;
```

---

## 8. Client Architecture

### 8.1 Client Responsibilities

The client should be as "thin" as possible:

**Primary Responsibilities:**

1. **Display**: Render game state visually
2. **Input**: Collect user actions
3. **Communication**: Send actions to server, receive updates
4. **Local State**: Maintain visible game state (as told by server)

**NOT Responsible For:**

- Game logic validation
- Determining legal moves (server provides list)
- Calculating combat results
- Generating random numbers

### 8.2 Client State Management

```c
typedef struct {
    // Network
    int socket_fd;
    uint8_t my_player_id;
    uint64_t auth_token;
    bool connected;

    // Game state (as known to this client)
    ClientGameState visible_state;

    // UI state
    bool waiting_for_server;
    char status_message[256];
    uint8_t selected_cards[3];
    uint8_t num_selected;

    // Sequence tracking
    uint16_t next_sequence_to_send;
    uint16_t last_sequence_received;

} ClientState;
```

**State Update Pattern:**

```c
void client_handle_message(ClientState *client, uint8_t *buffer) {
    MessageHeader *header = (MessageHeader*)buffer;
    void *payload = buffer + sizeof(MessageHeader);

    switch (header->msg_type) {
        case MSG_STATE_FULL:
            memcpy(&client->visible_state, payload, 
                   sizeof(ClientGameState));
            client->waiting_for_server = false;
            redraw_ui();
            break;

        case MSG_STATE_DELTA:
            apply_delta_update(&client->visible_state, 
                              (DeltaUpdate*)payload);
            redraw_ui();
            break;

        case MSG_EVENT_COMBAT_RESULT:
            display_combat_animation((CombatResultEvent*)payload);
            client->waiting_for_server = false;
            break;

        case MSG_ERROR_INVALID_ACTION:
            show_error_popup("Invalid action");
            client->waiting_for_server = false;
            request_full_state_resync(client);
            break;
    }
}
```

### 8.3 Client Network Loop

**Two Threads: Main (UI) + Network**

```c
// Network thread - receives messages from server
void *client_network_thread(void *arg) {
    ClientState *client = (ClientState*)arg;

    while (client->connected) {
        // Blocking receive with timeout
        uint8_t buffer[4096];
        int received = recv_with_timeout(client->socket_fd, 
                                        buffer, sizeof(buffer), 
                                        1000);  // 1 second timeout

        if (received > 0) {
            pthread_mutex_lock(&client->state_mutex);
            client_handle_message(client, buffer);
            pthread_mutex_unlock(&client->state_mutex);
        } else if (received == 0) {
            // Connection closed
            client->connected = false;
            break;
        }

        // Send heartbeat every 30 seconds
        if (time(NULL) - client->last_heartbeat > 30) {
            send_heartbeat(client);
            client->last_heartbeat = time(NULL);
        }
    }

    return NULL;
}

// Main thread - UI and user input
int main() {
    ClientState client;
    initialize_client(&client, "server.example.com", 5555);

    // Start network thread
    pthread_t net_thread;
    pthread_create(&net_thread, NULL, client_network_thread, &client);

    // Main UI loop
    while (client.connected && !quit_requested) {
        handle_user_input(&client);

        pthread_mutex_lock(&client.state_mutex);
        render_game_state(&client.visible_state);
        pthread_mutex_unlock(&client.state_mutex);

        usleep(16666);  // ~60 FPS
    }

    pthread_join(net_thread, NULL);
    return 0;
}
```

### 8.4 Client UI Patterns

**Action Workflow:**

```c
void handle_play_button_clicked(ClientState *client) {
    if (client->waiting_for_server) {
        show_message("Please wait for server response");
        return;
    }

    if (client->num_selected == 0) {
        show_message("Select at least one card");
        return;
    }

    // Build action message
    PlayChampionsMessage msg;
    msg.header.msg_type = MSG_ACTION_PLAY_CHAMPIONS;
    msg.num_cards = client->num_selected;
    memcpy(msg.card_ids, client->selected_cards, 
           sizeof(uint8_t) * client->num_selected);

    // Send to server
    send_message(client, &msg, sizeof(msg));

    // Update UI state
    client->waiting_for_server = true;
    strcpy(client->status_message, "Sending action to server...");

    // Clear selection
    client->num_selected = 0;
}
```

**Display Patterns:**

```c
void render_game_state(ClientGameState *state) {
    // Opponent info (top of screen)
    draw_player_info(state->opp_energy, state->opp_lunas,
                    state->opp_hand_count, state->opp_deck_count);

    // Combat zone (middle)
    if (state->combat_active) {
        draw_combat_zone(state->combat_attacker_cards,
                        state->combat_defender_cards);
    }

    // My hand (bottom)
    draw_hand(state->my_state.hand, state->my_state.hand_count);

    // Status message
    draw_status_bar(state->current_phase, 
                   state->whose_turn,
                   state->turn_number);
}
```

---

## 9. Server Architecture

### 9.1 Server Responsibilities

The server is the authoritative game engine:

**Primary Responsibilities:**

1. **State Management**: Maintain full game state for all games
2. **Validation**: Verify all client actions are legal
3. **Game Logic**: Execute all game rules
4. **Synchronization**: Keep clients in sync
5. **Random Generation**: All dice rolls, shuffling
6. **Persistence**: Save/load games (optional)

### 9.2 Server State Management

```c
typedef struct {
    // Game sessions (one per active game)
    GameSession *sessions[MAX_CONCURRENT_GAMES];
    int num_sessions;

    // Matchmaking queue
    ClientConnection *waiting_players[MAX_QUEUE];
    int queue_size;

    // Server socket
    int listen_socket;

    // Thread pool for handling clients
    pthread_t worker_threads[WORKER_THREAD_COUNT];

} GameServer;

typedef struct {
    uint32_t game_id;
    ServerGameState game;      // Full authoritative state
    ClientConnection clients[2];
    ServerGamePhase phase;

    // Synchronization
    pthread_mutex_t mutex;
    time_t last_activity;

    // RNG (unique per game)
    GameContext context;

} GameSession;
```

### 9.3 Server Main Loop

**Event-Driven with Select/Poll:**

```c
void server_main_loop(GameServer *server) {
    fd_set read_fds;
    struct timeval timeout;

    while (server->running) {
        FD_ZERO(&read_fds);
        FD_SET(server->listen_socket, &read_fds);

        int max_fd = server->listen_socket;

        // Add all client sockets to select
        for (int i = 0; i < server->num_sessions; i++) {
            for (int j = 0; j < 2; j++) {
                int fd = server->sessions[i]->clients[j].socket_fd;
                if (fd > 0) {
                    FD_SET(fd, &read_fds);
                    if (fd > max_fd) max_fd = fd;
                }
            }
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select error");
            continue;
        }

        // New connection?
        if (FD_ISSET(server->listen_socket, &read_fds)) {
            handle_new_connection(server);
        }

        // Messages from existing clients?
        for (int i = 0; i < server->num_sessions; i++) {
            GameSession *session = server->sessions[i];

            for (int j = 0; j < 2; j++) {
                int fd = session->clients[j].socket_fd;

                if (fd > 0 && FD_ISSET(fd, &read_fds)) {
                    handle_client_message(session, j);
                }
            }
        }

        // Game tick for each session
        for (int i = 0; i < server->num_sessions; i++) {
            server_game_tick(server->sessions[i]);
        }

        // Cleanup finished games
        cleanup_finished_sessions(server);
    }
}
```

### 9.4 Action Processing Pipeline

```c
void handle_client_message(GameSession *session, uint8_t player_id) {
    ClientConnection *client = &session->clients[player_id];

    // 1. Receive message
    uint8_t buffer[4096];
    int received = recv(client->socket_fd, buffer, sizeof(buffer), 0);

    if (received <= 0) {
        handle_client_disconnect(session, player_id);
        return;
    }

    // 2. Parse message
    MessageHeader *header = (MessageHeader*)buffer;
    void *payload = buffer + sizeof(MessageHeader);

    // 3. Verify authentication
    if (!verify_message_auth(client, header)) {
        send_error(client, MSG_ERROR_INVALID_AUTH);
        return;
    }

    // 4. Update last activity
    client->last_activity = time(NULL);

    // 5. Route to handler
    pthread_mutex_lock(&session->mutex);

    switch (header->msg_type) {
        case MSG_ACTION_PLAY_CHAMPIONS:
            handle_play_champions(session, player_id, payload);
            break;

        case MSG_ACTION_PLAY_DRAW:
            handle_play_draw(session, player_id, payload);
            break;

        case MSG_ACTION_PASS:
            handle_pass(session, player_id);
            break;

        case MSG_DISCONNECT:
            handle_client_disconnect(session, player_id);
            break;

        case MSG_REQUEST_FULL_STATE:
            send_full_state(session, player_id);
            break;

        default:
            send_error(client, MSG_ERROR_UNKNOWN_MESSAGE);
    }

    pthread_mutex_unlock(&session->mutex);
}

void handle_play_champions(GameSession *session, 
                          uint8_t player_id, 
                          void *payload) {
    PlayChampionsMessage *msg = (PlayChampionsMessage*)payload;

    // 1. Build action struct from message
    Action action;
    action.type = ACTION_PLAY_CHAMPIONS;
    action.player_id = player_id;
    action.data.cards.num_cards = msg->num_cards;
    memcpy(action.data.cards.card_ids, msg->card_ids, 
           sizeof(uint8_t) * msg->num_cards);

    // 2. Validate action
    if (!validate_action(&session->game, &action)) {
        send_error(&session->clients[player_id], 
                  MSG_ERROR_INVALID_ACTION);
        send_full_state(session, player_id);  // Resync
        return;
    }

    // 3. Apply action to game state
    apply_action(&session->game, &action, &session->context);

    // 4. Broadcast action to both clients
    broadcast_action_notification(session, &action);

    // 5. Send updated states
    send_delta_update(session, player_id);
    send_delta_update(session, 1 - player_id);

    // 6. Check if combat should occur
    if (session->game.combat_zone[player_id].size > 0) {
        session->phase = STATE_TURN_DEFENSE;
        notify_combat_start(session);
    } else {
        session->phase = STATE_TURN_END;
    }
}
```

### 9.5 Matchmaking

**Simple Queue-Based Matchmaking:**

```c
void handle_new_connection(GameServer *server) {
    int client_socket = accept(server->listen_socket, NULL, NULL);

    if (client_socket < 0) {
        return;
    }

    // Create client connection
    ClientConnection *client = malloc(sizeof(ClientConnection));
    client->socket_fd = client_socket;
    client->connected = true;
    client->auth_token = generate_auth_token();

    // Receive connect message
    ConnectRequest req;
    recv(client_socket, &req, sizeof(req), 0);

    strncpy(client->player_name, req.player_name, 
            sizeof(client->player_name));

    // Send connect response
    ConnectResponse resp;
    resp.auth_token = client->auth_token;
    resp.player_id = 0;  // Will be assigned when matched
    send(client_socket, &resp, sizeof(resp), 0);

    // Add to matchmaking queue
    server->waiting_players[server->queue_size++] = client;

    send_notification(client, MSG_WAITING_FOR_OPPONENT, NULL, 0);

    // Try to match
    if (server->queue_size >= 2) {
        create_game_session(server,
                           server->waiting_players[0],
                           server->waiting_players[1]);

        // Remove from queue
        server->queue_size -= 2;
    }
}

void create_game_session(GameServer *server,
                        ClientConnection *p1,
                        ClientConnection *p2) {
    GameSession *session = malloc(sizeof(GameSession));

    // Assign game ID
    session->game_id = server->next_game_id++;

    // Assign players
    session->clients[0] = *p1;
    session->clients[0].player_id = PLAYER_A;
    session->clients[1] = *p2;
    session->clients[1].player_id = PLAYER_B;

    // Initialize game
    session->context = create_game_context(
        session->game_id,  // Use game ID as seed
        NULL
    );

    setup_game(INITIAL_CASH_DEFAULT, 
              &session->game, 
              &session->context);

    session->phase = STATE_SETUP;

    // Initialize mutex
    pthread_mutex_init(&session->mutex, NULL);

    // Add to server
    server->sessions[server->num_sessions++] = session;

    // Notify players game is starting
    send_game_start_notification(session);
    send_full_state(session, 0);
    send_full_state(session, 1);
}
```

### 9.6 Game Persistence (Optional)

**Saving Game State:**

```c
void save_game_session(GameSession *session, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    // Save version number
    uint32_t version = 1;
    fwrite(&version, sizeof(version), 1, f);

    // Save game ID and timestamp
    fwrite(&session->game_id, sizeof(session->game_id), 1, f);
    time_t now = time(NULL);
    fwrite(&now, sizeof(now), 1, f);

    // Save full game state
    fwrite(&session->game, sizeof(ServerGameState), 1, f);

    // Save RNG state
    fwrite(&session->context.rng, sizeof(MTRand), 1, f);

    // Save player names
    for (int i = 0; i < 2; i++) {
        fwrite(session->clients[i].player_name, 64, 1, f);
    }

    fclose(f);
}

bool load_game_session(GameSession *session, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;

    // Check version
    uint32_t version;
    fread(&version, sizeof(version), 1, f);
    if (version != 1) {
        fclose(f);
        return false;
    }

    // Load game ID and timestamp
    fread(&session->game_id, sizeof(session->game_id), 1, f);
    time_t saved_time;
    fread(&saved_time, sizeof(saved_time), 1, f);

    // Load game state
    fread(&session->game, sizeof(ServerGameState), 1, f);

    // Load RNG state
    fread(&session->context.rng, sizeof(MTRand), 1, f);

    // Load player names
    for (int i = 0; i < 2; i++) {
        fread(session->clients[i].player_name, 64, 1, f);
    }

    fclose(f);
    return true;
}
```

---

## 10. Implementation Roadmap

### 10.1 Phase 1: Foundation (Weeks 1-2)

**Goal**: Basic client/server communication

**Deliverables:**

1. Socket wrapper functions (`socket_utils.c/h`)
2. Message header structure and serialization
3. Simple echo server (client sends message, server echoes back)
4. Client connection and authentication

**Code Structure:**

```
src/
├── network/
│   ├── socket_utils.c/h       # Socket creation, send/recv
│   ├── message.c/h            # Message serialization
│   └── protocol.c/h           # Protocol definitions
├── server/
│   └── server_main.c          # Basic server loop
└── client/
    └── client_main.c          # Basic client connection
```

**Testing:**

```bash
# Terminal 1
./bin/server -p 5555

# Terminal 2
./bin/client -h localhost -p 5555 -m "Hello server"
```

### 10.2 Phase 2: Game State Transfer (Weeks 3-4)

**Goal**: Server maintains game state, sends to clients

**Deliverables:**

1. `ServerGameState` structure
2. `ClientGameState` structure (visible subset)
3. State filtering (server → client visible state)
4. Full state message implementation

**Key Functions:**

```c
void server_create_visible_state(ServerGameState *full,
                                 ClientGameState *visible,
                                 uint8_t for_player);

void client_apply_full_state(ClientState *client,
                             ClientGameState *state);
```

**Testing:**

- Server creates game with pre-defined state
- Both clients connect and receive full state
- Verify each client sees correct information
  - Player A sees their hand, not B's
  - Player B sees their hand, not A's
  - Both see public information

### 10.3 Phase 3: Action Processing (Weeks 5-7)

**Goal**: Clients send actions, server validates and applies

**Deliverables:**

1. Action enumeration and structures
2. Action serialization
3. Server validation framework
4. Action application (integrate with existing `card_actions.c`)
5. Delta update messages

**Implementation Priority:**

1. `ACTION_PLAY_CHAMPIONS` (most common)
2. `ACTION_PASS`
3. `ACTION_PLAY_DRAW`
4. `ACTION_PLAY_CASH`

**Testing:**

- Client A plays champions → Server validates → Both clients see update
- Client A tries to play cards they don't have → Server rejects
- Client A tries to play on opponent's turn → Server rejects

### 10.4 Phase 4: Full Game Loop (Weeks 8-10)

**Goal**: Complete turn-based game flow

**Deliverables:**

1. Turn phase management on server
2. Combat resolution over network
3. Dice rolling on server
4. Game end detection and notification
5. Reconnection handling

**Integration:**

- Reuse existing `turn_logic.c` functions
- Reuse existing `combat.c` functions
- Server controls flow, clients react

**Testing:**

- Play complete game: setup → turns → combat → end
- Test disconnect/reconnect mid-game
- Verify game state consistency

### 10.5 Phase 5: Client UI (Weeks 11-13)

**Goal**: Polished client interface

**Deliverables:**

1. Choose UI framework (SDL3, ncurses, or Qt)
2. Game board rendering
3. Card selection interface
4. Action buttons with server communication
5. Waiting indicators
6. Error displays

**Features:**

- Show own hand with card details
- Show opponent's hand count (not cards)
- Display combat zone
- Energy/luna bars
- Status messages ("Waiting for opponent...", "Your turn!")

### 10.6 Phase 6: Advanced Features (Weeks 14-16)

**Goal**: Production-ready features

**Deliverables:**

1. **Security:**
   
   - TLS/SSL for encrypted communication
   - Rate limiting (prevent spam)
   - Input sanitization

2. **Robustness:**
   
   - Automatic reconnection
   - Heartbeat/keepalive
   - Timeout handling
   - Error recovery

3. **Features:**
   
   - Chat system
   - Game history/replay
   - Statistics tracking
   - Ranking system integration

4. **Performance:**
   
   - Connection pooling
   - Message batching
   - Bandwidth optimization

### 10.7 Phase 7: Testing & Deployment (Weeks 17-18)

**Testing:**

1. **Unit Tests:**
   
   - Message serialization/deserialization
   - Action validation
   - State filtering

2. **Integration Tests:**
   
   - Full game simulation (automated clients)
   - Stress testing (many simultaneous games)
   - Network condition simulation (latency, packet loss)

3. **User Acceptance Testing:**
   
   - Real players test the system
   - Gather feedback on UX
   - Fix bugs

**Deployment:**

1. Set up dedicated server machine
2. Configure firewall rules (port 5555)
3. Set up logging and monitoring
4. Create deployment scripts
5. Write server administration documentation

---

## 11. Advanced Topics

### 11.1 AI Integration

**Server-Side AI:**

The server can run AI opponents:

```c
// Server decides which strategy to use
if (session->clients[player_id].is_ai) {
    // Let AI decide action
    Action action = ai_choose_action(&session->game, 
                                     player_id,
                                     &session->context);

    // Process like any other action
    apply_action(&session->game, &action, &session->context);
    broadcast_action_notification(session, &action);
}
```

**Client-Side AI (for offline play):**

Clients can have local AI:

```c
// Local standalone mode
if (game_mode == MODE_STDA) {
    // Use existing strategy functions
    random_attack_strategy(&gstate, &ctx);
}

// Client/server mode
else if (game_mode == MODE_CLIENT && opponent_is_ai) {
    // AI is on server, just wait for updates
    display_waiting_message("Opponent (AI) is thinking...");
}
```

### 11.2 Spectator Mode

Allow third parties to watch games:

```c
typedef struct {
    int socket_fd;
    uint32_t watching_game_id;
    time_t connected_at;
} Spectator;

void send_spectator_update(GameSession *session, Spectator *spec) {
    // Spectators see public information only
    ClientGameState visible;

    // Don't reveal either player's hand
    visible.my_state.hand_count = session->game.players[0].hand_count;
    visible.opp_hand_count = session->game.players[1].hand_count;

    // But show everything else
    visible.my_state.energy = session->game.players[0].energy;
    visible.opp_energy = session->game.players[1].energy;
    // ... etc

    send_full_state_to_socket(spec->socket_fd, &visible);
}
```

### 11.3 Tournament Mode

Server manages multi-game tournaments:

```c
typedef struct {
    uint32_t tournament_id;
    uint32_t *player_ids;
    int num_players;

    TournamentFormat format;  // SINGLE_ELIM, DOUBLE_ELIM, SWISS

    GameSession **matches;
    int num_matches;

    Standings standings;
} Tournament;

void tournament_create_next_round(Tournament *t) {
    // Based on current standings, pair players
    for (int i = 0; i < t->num_players / 2; i++) {
        uint32_t p1 = determine_pairing(t, i);
        uint32_t p2 = determine_pairing(t, t->num_players - 1 - i);

        create_match(t, p1, p2);
    }
}
```

### 11.4 Web Client

Add web browser support using WebSockets:

```javascript
// JavaScript client
const socket = new WebSocket('ws://server.example.com:5555');

socket.onopen = function() {
    // Send connection request
    const connectMsg = {
        type: 'CONNECT_REQUEST',
        player_name: 'WebPlayer',
        version: 1
    };
    socket.send(JSON.stringify(connectMsg));
};

socket.onmessage = function(event) {
    const msg = JSON.parse(event.data);

    switch (msg.type) {
        case 'STATE_FULL':
            updateGameDisplay(msg.state);
            break;
        case 'EVENT_COMBAT_RESULT':
            showCombatAnimation(msg);
            break;
    }
};

function playCards(cardIndices) {
    const action = {
        type: 'ACTION_PLAY_CHAMPIONS',
        cards: cardIndices
    };
    socket.send(JSON.stringify(action));
}
```

**Server Adaptation:**

```c
// WebSocket handler (using libwebsockets or similar)
void handle_websocket_message(struct lws *wsi, void *data, size_t len) {
    // Parse JSON message
    cJSON *json = cJSON_Parse(data);
    const char *type = cJSON_GetObjectItem(json, "type")->valuestring;

    // Convert to binary protocol internally
    if (strcmp(type, "ACTION_PLAY_CHAMPIONS") == 0) {
        Action action;
        action.type = ACTION_PLAY_CHAMPIONS;
        // ... parse from JSON

        handle_action(session, &action);
    }

    cJSON_Delete(json);
}
```

---

## 12. Conclusion

### 12.1 Key Takeaways

**Architecture Principles for Turn-Based Card Games:**

1. **Authoritative Server**: Server is always the source of truth
2. **Information Hiding**: Clients only see what they should see
3. **Validation-First**: Never trust client input
4. **Server-Side RNG**: All randomness generated on server
5. **Clear State Machine**: Well-defined phases and transitions
6. **Pessimistic Updates**: Wait for server confirmation
7. **Graceful Degradation**: Handle disconnects and errors
8. **Message-Based**: Clear protocol with typed messages

**Oracle-Specific Design Choices:**

- **TCP** for reliability (turn-based allows slight latency)
- **Binary protocol** for efficiency (or JSON for debugging)
- **Persistent connections** (one socket per player per game)
- **Request/response** model (fits turn-based nature)
- **State filtering** (carefully control visibility)
- **Delta updates** (efficiency) with **full state resync** (recovery)

### 12.2 Why This Architecture?

**Scalability:**

- Each game session is independent
- Can run multiple servers (one per region)
- Lightweight per-game footprint (<1MB RAM)

**Security:**

- Server validation prevents cheating
- Hidden information truly hidden
- Deterministic RNG with server-side seeding

**Maintainability:**

- Clear separation of concerns
- Existing game logic (`turn_logic.c`, `combat.c`) mostly reusable
- Protocol changes don't require client updates (versioning)

**Extensibility:**

- Easy to add AI opponents (server-side)
- Easy to add spectators (additional socket type)
- Easy to add web clients (WebSocket adapter)
- Easy to add replay system (log messages)

### 12.3 Common Pitfalls to Avoid

1. **Trusting the client**: Always validate on server
2. **Sending hidden info**: Be careful with state filtering
3. **Client-side RNG**: Never let clients generate random numbers
4. **Blocking sends**: Use non-blocking I/O or timeouts
5. **Missing error handling**: Network is unreliable, plan for failures
6. **No reconnection**: Players

```
will disconnect - handle gracefully
7. **Tight coupling**: Keep network code separate from game logic
8. **No versioning**: Protocol will change - version your messages
9. **Poor logging**: You'll need logs to debug network issues
10. **No testing**: Test with bad network conditions (latency, drops)

### 12.4 Alternative Approaches and Trade-offs

**Alternative 1: Hybrid P2P with Server Validation**
```

Players communicate directly (P2P) for speed
↓
Server receives copy of all actions
↓
Server validates and can override
↓
If client cheats, server disconnects them

```
**Pros:**
- Lower latency (direct connection)
- Less server bandwidth

**Cons:**
- More complex
- Still need to prevent information leaking in P2P
- NAT traversal issues

**Verdict**: Not worth complexity for turn-based game

---

**Alternative 2: Serverless (Blockchain/Smart Contract)**
```

Actions are transactions on blockchain
↓
Consensus mechanism validates
↓
Immutable game history

```
**Pros:**
- Truly decentralized
- Provably fair
- No server costs

**Cons:**
- High latency (block time)
- Transaction fees
- Complex development
- Hidden information is very hard

**Verdict**: Interesting for research, impractical for Oracle

---

**Alternative 3: Cloud Gaming (Server Renders Everything)**
```

All game logic + rendering on server
↓
Stream video to clients
↓
Clients send input only

```
**Pros:**
- No client-side code needed
- Perfect security

**Cons:**
- High bandwidth
- High server costs
- Latency sensitive

**Verdict**: Overkill for a card game

---

### 12.5 Performance Considerations

**Expected Performance Characteristics:**

**Bandwidth per Player:**
- Setup phase: ~5KB (full initial state)
- Per action: ~100 bytes (action + delta update)
- Per turn: ~500 bytes average
- Full game: ~30KB total

**Latency Requirements:**
- Action response time: <200ms acceptable, <100ms good
- Heartbeat interval: 30 seconds
- Timeout threshold: 60 seconds no activity

**Server Resources (per game):**
- Memory: ~50KB game state + 10KB buffers = 60KB
- CPU: Minimal (mostly waiting for I/O)
- Network: ~1KB/s average bandwidth

**Scaling:**
```

Single modest server (4 cores, 8GB RAM) can handle:

- 1000 concurrent games
- 2000 connected players
- ~1MB/s network bandwidth

```
**Bottlenecks:**
1. **Network I/O**: Use async I/O (select/epoll)
2. **Lock contention**: Keep critical sections small
3. **State serialization**: Cache common messages

**Optimization Strategies:**

```c
// Message caching for repeated sends
typedef struct {
    uint8_t msg_type;
    uint8_t cached_data[512];
    uint16_t cached_length;
    bool cache_valid;
} CachedMessage;

void send_state_delta_cached(GameSession *session, uint8_t player) {
    if (!session->delta_cache[player].cache_valid) {
        // Build delta message
        build_delta_message(&session->delta_cache[player], 
                          &session->game, player);
        session->delta_cache[player].cache_valid = true;
    }

    // Send cached version
    send(session->clients[player].socket_fd,
         session->delta_cache[player].cached_data,
         session->delta_cache[player].cached_length, 0);
}

// Invalidate cache when state changes
void apply_action(GameState *gs, Action *action) {
    // ... apply action ...

    // Invalidate delta caches
    session->delta_cache[0].cache_valid = false;
    session->delta_cache[1].cache_valid = false;
}
```

---

## 13. Implementation Examples

### 13.1 Complete Minimal Example

**Minimal Server (250 lines):**

```c
// minimal_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 5555
#define MAX_CLIENTS 2

typedef struct {
    int socket_fd;
    uint8_t player_id;
    bool connected;
} Client;

typedef struct {
    Client clients[MAX_CLIENTS];
    int num_clients;
    uint16_t turn;
    uint8_t current_player;
} GameServer;

void broadcast_message(GameServer *server, const char *msg) {
    for (int i = 0; i < server->num_clients; i++) {
        if (server->clients[i].connected) {
            send(server->clients[i].socket_fd, msg, strlen(msg), 0);
        }
    }
}

int main() {
    GameServer server = {0};

    // Create socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    // Allow reuse of address
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    // Listen
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    // Accept clients
    while (server.num_clients < MAX_CLIENTS) {
        int client_fd = accept(listen_fd, NULL, NULL);

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        server.clients[server.num_clients].socket_fd = client_fd;
        server.clients[server.num_clients].player_id = server.num_clients;
        server.clients[server.num_clients].connected = true;

        printf("Player %d connected\n", server.num_clients);

        char welcome[64];
        snprintf(welcome, sizeof(welcome), 
                "Welcome Player %d!\n", server.num_clients);
        send(client_fd, welcome, strlen(welcome), 0);

        server.num_clients++;
    }

    // Game started
    broadcast_message(&server, "Game starting!\n");

    // Simple game loop
    server.current_player = 0;
    server.turn = 1;

    while (1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Turn %d - Player %d's turn\n",
                server.turn, server.current_player);
        broadcast_message(&server, msg);

        // Receive action from current player
        char buffer[256];
        int received = recv(
            server.clients[server.current_player].socket_fd,
            buffer, sizeof(buffer), 0);

        if (received <= 0) {
            printf("Player %d disconnected\n", server.current_player);
            break;
        }

        buffer[received] = '\0';
        printf("Player %d action: %s", server.current_player, buffer);

        // Echo action to both players
        snprintf(msg, sizeof(msg), "Player %d played: %s",
                server.current_player, buffer);
        broadcast_message(&server, msg);

        // Next turn
        server.current_player = 1 - server.current_player;
        server.turn++;

        if (server.turn > 10) {
            broadcast_message(&server, "Game over!\n");
            break;
        }
    }

    // Cleanup
    for (int i = 0; i < server.num_clients; i++) {
        close(server.clients[i].socket_fd);
    }
    close(listen_fd);

    return 0;
}
```

**Minimal Client (150 lines):**

```c
// minimal_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 5555

typedef struct {
    int socket_fd;
    bool connected;
} Client;

void *receive_thread(void *arg) {
    Client *client = (Client*)arg;
    char buffer[256];

    while (client->connected) {
        int received = recv(client->socket_fd, buffer, 
                          sizeof(buffer) - 1, 0);

        if (received <= 0) {
            printf("Disconnected from server\n");
            client->connected = false;
            break;
        }

        buffer[received] = '\0';
        printf("Server: %s", buffer);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    Client client = {0};

    // Create socket
    client.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client.socket_fd < 0) {
        perror("socket");
        return 1;
    }

    // Connect to server
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &addr.sin_addr);

    if (connect(client.socket_fd, (struct sockaddr*)&addr, 
                sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to server\n");
    client.connected = true;

    // Start receive thread
    pthread_t thread;
    pthread_create(&thread, NULL, receive_thread, &client);

    // Send actions
    char input[256];
    while (client.connected) {
        printf("Enter action: ");
        if (!fgets(input, sizeof(input), stdin)) break;

        if (strncmp(input, "quit", 4) == 0) break;

        send(client.socket_fd, input, strlen(input), 0);
    }

    // Cleanup
    client.connected = false;
    pthread_join(thread, NULL);
    close(client.socket_fd);

    return 0;
}
```

**Compile and Test:**

```bash
# Compile
gcc minimal_server.c -o server -pthread
gcc minimal_client.c -o client -pthread

# Terminal 1: Start server
./server

# Terminal 2: Connect client 1
./client 127.0.0.1

# Terminal 3: Connect client 2
./client 127.0.0.1

# Now clients can send messages, server echoes to both
```

### 13.2 Message Serialization Example

```c
// message_utils.c
#include <stdint.h>
#include <string.h>

// Serialize action to buffer
size_t serialize_action(Action *action, uint8_t *buffer) {
    size_t offset = 0;

    // Write type
    buffer[offset++] = action->type;
    buffer[offset++] = action->player_id;

    switch (action->type) {
        case ACTION_PLAY_CHAMPIONS:
            buffer[offset++] = action->data.cards.num_cards;
            for (int i = 0; i < action->data.cards.num_cards; i++) {
                buffer[offset++] = action->data.cards.card_ids[i];
            }
            break;

        case ACTION_PASS:
            // No additional data
            break;

        // ... other types
    }

    return offset;
}

// Deserialize buffer to action
bool deserialize_action(uint8_t *buffer, size_t length, Action *action) {
    if (length < 2) return false;

    size_t offset = 0;

    action->type = buffer[offset++];
    action->player_id = buffer[offset++];

    switch (action->type) {
        case ACTION_PLAY_CHAMPIONS:
            if (offset >= length) return false;
            action->data.cards.num_cards = buffer[offset++];

            if (action->data.cards.num_cards > 3) return false;
            if (offset + action->data.cards.num_cards > length) return false;

            for (int i = 0; i < action->data.cards.num_cards; i++) {
                action->data.cards.card_ids[i] = buffer[offset++];
            }
            break;

        case ACTION_PASS:
            // No additional data
            break;

        default:
            return false;
    }

    return true;
}
```

---

## 14. Debugging and Troubleshooting

### 14.1 Logging Strategy

**Structured Logging:**

```c
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

void log_message(LogLevel level, const char *component, 
                const char *format, ...) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), 
            "%Y-%m-%d %H:%M:%S", tm_info);

    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};

    printf("[%s] [%s] [%s] ", timestamp, level_str[level], component);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

// Usage
log_message(LOG_INFO, "SERVER", 
           "Player %d connected from %s", player_id, ip_addr);
log_message(LOG_ERROR, "VALIDATION", 
           "Invalid action from player %d: insufficient funds", player_id);
log_message(LOG_DEBUG, "NETWORK", 
           "Sent %d bytes to socket %d", bytes_sent, socket_fd);
```

**What to Log:**

**Server:**

- Client connections/disconnections
- Actions received and validation results
- Game state changes (turn start/end, combat)
- Errors and warnings
- Performance metrics (messages/second)

**Client:**

- Connection attempts
- Messages sent and received
- State updates applied
- User interactions
- Errors

### 14.2 Network Debugging Tools

**Packet Capture:**

```bash
# Capture traffic on port 5555
sudo tcpdump -i any -s 0 -w oracle_capture.pcap port 5555

# View captured packets
tcpdump -r oracle_capture.pcap -X
```

**Wireshark Analysis:**

- Display filter: `tcp.port == 5555`
- Follow TCP stream to see full conversation
- Check for retransmissions (red packets)

**Network Simulation:**

```bash
# Add artificial latency (100ms)
sudo tc qdisc add dev lo root netem delay 100ms

# Add packet loss (5%)
sudo tc qdisc add dev lo root netem loss 5%

# Remove rules
sudo tc qdisc del dev lo root
```

### 14.3 Common Issues and Solutions

**Issue 1: Client and server state diverge**

*Symptoms:*

- Client shows different energy than server
- Cards appear/disappear
- Actions fail validation

*Diagnosis:*

```c
// Add state checksum to every message
typedef struct {
    // ... normal message fields ...
    uint32_t state_checksum;
} StateMessage;

uint32_t compute_state_checksum(GameState *gs) {
    uint32_t sum = 0;
    sum += gs->players[0].energy * 1000;
    sum += gs->players[1].energy * 1001;
    sum += gs->players[0].lunas * 1002;
    sum += gs->players[1].lunas * 1003;
    sum += gs->turn * 1004;
    return sum;
}

// Client verifies
void verify_state_checksum(ClientState *client, uint32_t server_checksum) {
    uint32_t local = compute_state_checksum(&client->visible_state);
    if (local != server_checksum) {
        log_message(LOG_ERROR, "CLIENT", 
                   "State mismatch! Local: %u, Server: %u", 
                   local, server_checksum);
        request_full_state_resync(client);
    }
}
```

*Solution:*

- Add state checksums to every update
- Request full resync on mismatch
- Log differences to find bug

---

**Issue 2: Messages arrive out of order**

*Symptoms:*

- Client receives turn N+1 before turn N completes
- Actions applied twice or skipped

*Diagnosis:*

```c
// Log message sequence numbers
log_message(LOG_DEBUG, "NETWORK", 
           "Received seq %u, expected %u", 
           msg->sequence_num, expected_sequence);
```

*Solution:*

- Use TCP (already ordered)
- Check sequence numbers
- Buffer out-of-order messages

```c
typedef struct {
    Message *buffered[16];
    uint16_t buffer_count;
    uint16_t next_expected;
} MessageBuffer;

void handle_message_with_ordering(MessageBuffer *buf, Message *msg) {
    if (msg->sequence == buf->next_expected) {
        // In order - process immediately
        process_message(msg);
        buf->next_expected++;

        // Check if buffered messages can now be processed
        process_buffered_messages(buf);
    } else if (msg->sequence > buf->next_expected) {
        // Future message - buffer it
        buf->buffered[buf->buffer_count++] = msg;
    } else {
        // Old message - ignore (duplicate)
        log_message(LOG_WARNING, "NETWORK", 
                   "Ignoring old message seq %u", msg->sequence);
    }
}
```

---

**Issue 3: Client hangs waiting for server**

*Symptoms:*

- UI freezes
- "Waiting for server..." never completes
- No error message

*Diagnosis:*

```c
// Add timeout to receives
struct timeval timeout;
timeout.tv_sec = 5;
timeout.tv_usec = 0;
setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
          &timeout, sizeof(timeout));

int received = recv(socket_fd, buffer, size, 0);
if (received < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
    log_message(LOG_WARNING, "CLIENT", 
               "Server response timeout");
    show_error_message("Server not responding");
}
```

*Solution:*

- Use timeouts on all blocking operations
- Implement heartbeat system
- Allow user to cancel action

---

## 15. Security Checklist

Before deploying, verify:

**Server Security:**

- [ ] All client actions validated on server
- [ ] Hidden information never sent to clients
- [ ] RNG seed is server-controlled
- [ ] Authentication tokens used and verified
- [ ] Sequence numbers prevent replay attacks
- [ ] Rate limiting prevents spam
- [ ] Input sanitization (buffer sizes, ranges)
- [ ] Timeout disconnects inactive clients
- [ ] Logs security violations

**Network Security:**

- [ ] Use TLS/SSL for production (encrypts traffic)
- [ ] Firewall rules limit access to game port
- [ ] DDoS mitigation (rate limiting, connection limits)
- [ ] Regular security audits

**Data Security:**

- [ ] Player passwords hashed (if login system)
- [ ] Sensitive data (emails) stored securely
- [ ] Game logs don't expose sensitive info
- [ ] Backups encrypted

---

## 16. Final Recommendations

### 16.1 Start Simple, Iterate

**Phase 1**: Get basic communication working (weeks 1-2)

- Echo server
- Simple messages
- Don't worry about security yet

**Phase 2**: Add game state (weeks 3-4)

- Full state transfer
- State filtering
- Still no security

**Phase 3**: Complete game loop (weeks 5-10)

- All actions
- Full turn flow
- Combat

**Phase 4**: Polish and secure (weeks 11+)

- Add authentication
- Add encryption
- Add UI polish
- Stress testing

### 16.2 Testing Philosophy

**Test at Every Layer:**

1. **Unit Tests**: Message serialization, validation
2. **Integration Tests**: Full game simulation
3. **Network Tests**: Latency, packet loss
4. **Load Tests**: Many concurrent games
5. **User Tests**: Real players

**Automated Test Client:**

```c
// Simulates player behavior for testing
void run_test_client(const char *server_ip) {
    Client client;
    connect_to_server(&client, server_ip, PORT);

    while (game_not_over) {
        // Wait for your turn
        wait_for_turn_notification(&client);

        // Make random valid move
        Action action = choose_random_legal_action(&client.state);
        send_action(&client, &action);

        // Wait for result
        wait_for_action_result(&client);
    }

    disconnect(&client);
}

// Run 100 test games
for (int i = 0; i < 100; i++) {
    run_test_client("localhost");
}
```

### 16.3 Documentation Deliverables

**For Developers:**

- Protocol specification document
- Message format reference
- Server API documentation
- Client integration guide

**For Users:**

- Connection instructions
- Troubleshooting guide
- Firewall configuration

**For Operations:**

- Server deployment guide
- Monitoring setup
- Backup procedures
- Incident response plan

---

## 17. Conclusion

This document has provided a comprehensive foundation for implementing a client/server architecture for Oracle: The Champions of Arcadia. The key principles—authoritative server, information hiding, validation-first, and clear protocol design—will serve you well throughout development.

**Remember:**

- Start simple, add complexity gradually
- Test constantly, at every layer
- Security is not optional, but can be added incrementally
- The turn-based nature of Oracle makes many problems easier
- Your existing game logic (`turn_logic.c`, `combat.c`) will integrate smoothly

**Next Steps:**

1. Review Phase 1 of the implementation roadmap
2. Set up your development environment (two machines or VMs)
3. Implement the minimal echo server/client
4. Gradually add Oracle-specific functionality
5. Test with real players early and often

The journey from standalone to networked multiplayer is significant, but with careful design and iterative development, you'll create a robust, secure, and enjoyable online experience for Oracle players.

Good luck with your implementation! 🎮🃏
