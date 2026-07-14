# Oracle: Champions of Arcadia - Complete Architecture Design

**Version:** 2025.10  
**Target Platforms:** MSYS2 (Windows), Arch Linux, iOS (future), Android (future)  
**Language:** C23 with GCC compiler  
**Development Environment:** Geany IDE, Python tools

---

## Table of Contents

1. [Overview](#overview)
2. [Core Architecture Principles](#core-architecture-principles)
3. [Action System](#action-system)
4. [Game State & Visibility](#game-state--visibility)
5. [Network Protocol Design](#network-protocol-design)
6. [Client Architecture](#client-architecture)
7. [Server Architecture](#server-architecture)
8. [AI Integration](#ai-integration)
9. [Security & Validation](#security--validation)
10. [Implementation Roadmap](#implementation-roadmap)
11. [File Organization](#file-organization)

---

## Overview

### Design Philosophy

Oracle's architecture follows these core principles:

1. **Separation of concerns**: Game logic, network, UI, and AI are independent modules
2. **Server authority**: Server is the single source of truth for all game state
3. **Information hiding**: Clients only see what their player should see
4. **Modular design**: Functions ≤30 lines, files ≤500 lines
5. **Platform agnostic core**: Game logic works standalone or networked

### Architectural Layers

```
┌─────────────────────────────────────────────────────────┐
│                    User Interface Layer                  │
│  (CLI, TUI/ncurses, GUI/SDL3) - Platform Specific      │
└─────────────────────────────────────────────────────────┘
                          ↕
┌─────────────────────────────────────────────────────────┐
│                   Network Layer (Optional)               │
│        Client ←→ Protocol ←→ Server                     │
└─────────────────────────────────────────────────────────┘
                          ↕
┌─────────────────────────────────────────────────────────┐
│                   Game Logic Layer                       │
│  (Core engine: game_state, turn_logic, combat, etc.)   │
└─────────────────────────────────────────────────────────┘
                          ↕
┌─────────────────────────────────────────────────────────┐
│                   Strategy/AI Layer                      │
│  (random, balanced, heuristic, MCTS, etc.)             │
└─────────────────────────────────────────────────────────┘
                          ↕
┌─────────────────────────────────────────────────────────┐
│                  Data Structures & Utilities             │
│  (deckstack, hdcll, rnd, mtwister, combo_bonus)        │
└─────────────────────────────────────────────────────────┘
```

---

## Core Architecture Principles

### 1. Authoritative Server Model

**Definition:** The server maintains the complete, authoritative game state. Clients are "thin" - they display information and send input, but never make authoritative decisions.

```c
// Server maintains FULL state
typedef struct {
    PlayerState players[2];        // Complete info for both
    uint8_t deck_order[2][40];     // Exact card positions
    uint8_t discard[2][40];        // All discard piles
    struct HDCLList combat_zone[2]; // Combat cards
    GameContext ctx;               // RNG state
    uint16_t turn;
    GameStateEnum game_state;
    // ... full state
} ServerGameState;

// Client sees FILTERED state
typedef struct {
    uint8_t my_player_id;
    
    // My complete info
    PlayerState my_state;          // My hand, deck count, etc.
    
    // Opponent's PUBLIC info only
    uint8_t opp_energy;
    uint8_t opp_lunas;
    uint8_t opp_hand_count;        // Count, NOT cards
    uint8_t opp_deck_count;
    
    // Shared public info
    struct card combat_zone_attacker[3];
    struct card combat_zone_defender[3];
    uint8_t turn_number;
    TurnPhase phase;
} VisibleGameState;
```

### 2. Information Visibility Model

**Three-tier system:**

| Tier | Visibility | Examples |
|------|-----------|----------|
| **Public** | Both players | Energy, lunas, combat cards, discard piles, deck sizes, turn number |
| **Private** | Owner only | Own hand (specific cards), own deck order (unknown to owner) |
| **Hidden** | Server only | Opponent's hand, deck order, RNG seed |

### 3. Standalone vs Network Modes

The architecture supports both modes through abstraction:

```c
// Standalone mode: Direct function calls
void standalone_attack_phase(gamestate* gs, StrategySet* strat) {
    strat->attack_strategy[gs->current_player](gs, &gs->ctx);
    // Directly applies changes to gs
}

// Network mode: Action encoding
void network_attack_phase(ClientState* client) {
    Action action = ui_get_player_action(client);
    send_action_to_server(client, &action);
    // Wait for server response
    VisibleGameState new_state = receive_state_update(client);
    update_client_display(client, &new_state);
}
```

---

## Action System

### Action Types

```c
// action.h
typedef enum {
    ACTION_DRAW_CARD,              // Begin of turn
    ACTION_PLAY_CHAMPIONS,         // 1-3 champions for attack/defense
    ACTION_PLAY_DRAW_RECALL,       // Draw cards or recall champions
    ACTION_PLAY_EXCHANGE,          // Exchange champion for 5 lunas
    ACTION_PASS_TURN,              // Skip action
    ACTION_DEFEND,                 // Explicit defense action
    ACTION_DECLINE_DEFENSE,        // Pass on defense
    ACTION_DISCARD_TO_SEVEN,       // End of turn discard
    // Special/meta actions
    ACTION_QUIT,
    ACTION_REQUEST_GAMESTATE,
    ACTION_INVALID
} ActionType;
```

### Action Structure

```c
// Card play data
typedef struct {
    uint8_t card_ids[3];      // Up to 3 cards
    uint8_t num_cards;
    bool recall_option;       // For draw/recall cards
    uint8_t recall_ids[2];    // Champion IDs to recall (max 2)
    uint8_t num_recall;
} CardPlayData;

// Main action structure
typedef struct {
    ActionType type;
    uint8_t player_id;
    uint16_t sequence_num;    // For ordering/replay prevention
    union {
        CardPlayData cards;
        uint8_t simple_value;
    } data;
} Action;
```

### Action Validation

**Server-side validation (NEVER trust client):**

```c
bool validate_action(const Action* action, const ServerGameState* gs) {
    // 1. Is it this player's turn?
    if (action->player_id != gs->current_player) return false;
    
    // 2. Is it the right phase?
    if (!is_valid_phase_for_action(gs->phase, action->type)) 
        return false;
    
    // 3. Type-specific validation
    switch (action->type) {
        case ACTION_PLAY_CHAMPIONS:
            return validate_play_champions(action, gs);
        case ACTION_PLAY_DRAW_RECALL:
            return validate_draw_recall(action, gs);
        // ... etc
    }
    
    return false;
}

bool validate_play_champions(const Action* action, 
                             const ServerGameState* gs) {
    uint8_t pid = action->player_id;
    
    // Check card count (1-3)
    if (action->data.cards.num_cards < 1 || 
        action->data.cards.num_cards > 3) return false;
    
    // Check cards exist in hand
    for (int i = 0; i < action->data.cards.num_cards; i++) {
        uint8_t card_idx = action->data.cards.card_ids[i];
        if (!card_in_hand(&gs->players[pid].hand, card_idx))
            return false;
    }
    
    // Check affordability
    int total_cost = calculate_total_cost(action->data.cards);
    if (total_cost > gs->players[pid].lunas) return false;
    
    return true;
}
```

### Action Generation

**For AI/UI to get valid moves:**

```c
// action_generator.h
typedef struct {
    Action actions[256];
    uint16_t count;
} ActionList;

void get_list_of_possible_actions(const VisibleGameState* vgs, 
                                  ActionList* list) {
    list->count = 0;
    
    switch (vgs->phase) {
        case PHASE_ATTACK:
            get_attack_actions(vgs, list);
            get_draw_recall_actions(vgs, list);
            get_exchange_actions(vgs, list);
            add_pass_action(list);
            break;
            
        case PHASE_DEFENSE:
            if (vgs->combat_active) {
                get_defense_actions(vgs, list);
                add_decline_defense_action(list);
            }
            break;
            
        case PHASE_DISCARD:
            get_discard_actions(vgs, list);
            break;
    }
}

void get_attack_actions(const VisibleGameState* vgs, 
                       ActionList* list) {
    uint8_t pid = vgs->my_player_id;
    uint8_t hand_size = vgs->my_state.hand_count;
    
    // Generate all combinations of 1-3 champions
    for (uint8_t n = 1; n <= 3 && n <= hand_size; n++) {
        generate_champion_combinations(vgs, list, n);
    }
}

void generate_champion_combinations(const VisibleGameState* vgs,
                                   ActionList* list, uint8_t n) {
    // Recursive combination generator
    uint8_t indices[3];
    generate_combinations_recursive(vgs, list, indices, 0, n, 0);
}
```

---

## Game State & Visibility

### Server Game State

```c
// Complete authoritative state
typedef struct {
    // Player states (full info)
    PlayerState players[2];
    
    // Hidden information
    struct deck_stack deck[2];          // Exact order
    struct HDCLList hand[2];            // All cards
    struct HDCLList discard[2];         // All discards
    struct HDCLList combat_zone[2];     // Combat cards
    
    // Game flow
    PlayerID current_player;
    TurnPhase turn_phase;
    uint16_t turn;
    bool first_turn_first_player;       // Special rule
    GameStateEnum game_state;
    
    // RNG (server-controlled)
    GameContext ctx;
    
    // Configuration
    DeckType deck_type;
} ServerGameState;

typedef struct {
    uint8_t energy;
    uint8_t lunas;
    struct HDCLList hand;
    struct deck_stack deck;
    struct HDCLList discard;
} PlayerState;
```

### Visible Game State (Client)

```c
typedef struct {
    uint8_t my_player_id;
    uint8_t active_player_id;
    TurnPhase phase;
    
    // My full information
    PlayerState my_state;
    
    // Opponent's visible information
    uint8_t opp_energy;
    uint8_t opp_lunas;
    uint8_t opp_hand_count;        // Just count
    uint8_t opp_deck_count;
    uint8_t opp_discard_count;
    
    // Current combat state (public)
    struct card attack_cards[3];
    uint8_t attack_count;
    struct card defense_cards[3];
    uint8_t defense_count;
    
    // Metadata
    uint16_t turn_number;
    uint8_t last_damage;
    bool game_over;
    uint8_t winner_id;
} VisibleGameState;
```

### State Filtering

```c
void gamestate_get_visible(const ServerGameState* server_gs,
                           uint8_t for_player,
                           VisibleGameState* visible) {
    visible->my_player_id = for_player;
    visible->active_player_id = server_gs->current_player;
    visible->phase = server_gs->turn_phase;
    
    // Copy my full state
    visible->my_state = server_gs->players[for_player];
    
    // Copy opponent's PUBLIC state only
    uint8_t opponent = 1 - for_player;
    visible->opp_energy = server_gs->players[opponent].energy;
    visible->opp_lunas = server_gs->players[opponent].lunas;
    visible->opp_hand_count = server_gs->players[opponent].hand.size;
    visible->opp_deck_count = server_gs->players[opponent].deck.top + 1;
    visible->opp_discard_count = 
        server_gs->players[opponent].discard.size;
    
    // Copy combat zone (public)
    copy_combat_zone_cards(server_gs, visible);
    
    // Metadata
    visible->turn_number = server_gs->turn;
    visible->game_over = server_gs->someone_has_zero_energy;
    if (visible->game_over) {
        visible->winner_id = 
            (server_gs->game_state == PLAYER_A_WINS) ? 0 : 1;
    }
}
```

---

## Network Protocol Design

### Message Types

```c
typedef enum {
    // Connection (0-9)
    MSG_CONNECT_REQUEST = 0,
    MSG_CONNECT_RESPONSE = 1,
    MSG_DISCONNECT = 2,
    MSG_HEARTBEAT = 3,
    
    // Game setup (10-19)
    MSG_GAME_START = 10,
    MSG_MULLIGAN_REQUEST = 11,
    MSG_MULLIGAN_RESPONSE = 12,
    
    // Actions (20-39)
    MSG_ACTION_PLAY_CHAMPIONS = 20,
    MSG_ACTION_PLAY_DRAW = 21,
    MSG_ACTION_PLAY_EXCHANGE = 22,
    MSG_ACTION_PASS = 23,
    MSG_ACTION_DISCARD = 24,
    
    // State updates (40-59)
    MSG_STATE_FULL = 40,
    MSG_STATE_DELTA = 41,
    MSG_STATE_CHECKSUM = 42,
    
    // Events (60-79)
    MSG_EVENT_CARD_DRAWN = 60,
    MSG_EVENT_COMBAT_START = 61,
    MSG_EVENT_COMBAT_RESULT = 62,
    MSG_EVENT_TURN_END = 63,
    MSG_EVENT_GAME_OVER = 64,
    
    // Errors (80-89)
    MSG_ERROR_INVALID_ACTION = 80,
    MSG_ERROR_NOT_YOUR_TURN = 81,
    MSG_ERROR_INSUFFICIENT_FUNDS = 82,
    
    // Meta (90-99)
    MSG_CHAT = 90,
    MSG_REQUEST_FULL_STATE = 91,
    MSG_OPPONENT_DISCONNECTED = 92
} MessageType;
```

### Message Format

**Wire format (binary protocol):**

```
┌──────────┬──────────┬────────────┬──────────┐
│ Header   │ Auth     │ Payload    │ Checksum │
│ (8 bytes)│(16 bytes)│ (variable) │(4 bytes) │
└──────────┴──────────┴────────────┴──────────┘

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
                 void* payload, uint16_t length,
                 ClientConnection* client) {
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
    
    // Send in order: header, payload, checksum
    send_all(socket, &header, sizeof(header));
    if (length > 0) send_all(socket, payload, length);
    send_all(socket, &checksum, sizeof(checksum));
}
```

### Combat Resolution Protocol

**Example sequence:**

```
Client A (Attacker)          Server               Client B (Defender)
      │                         │                         │
      │  ACTION_PLAY_CHAMPIONS  │                         │
      ├────────────────────────>│                         │
      │                         │ Validate                │
      │                         │ Apply to state          │
      │                         │                         │
      │      STATE_DELTA        │                         │
      │<────────────────────────┤                         │
      │                         │                         │
      │                         │    EVENT_COMBAT_START   │
      │                         ├────────────────────────>│
      │                         │   (attacker: 3 cards)   │
      │                         │                         │
      │                         │  Wait for defender      │
      │                         │<────────────────────────┤
      │                         │  ACTION_PLAY_CHAMPIONS  │
      │                         │   (or ACTION_PASS)      │
      │                         │                         │
      │                         │ Roll dice (server RNG)  │
      │                         │ Calculate damage        │
      │                         │ Apply to state          │
      │                         │                         │
      │  EVENT_COMBAT_RESULT    │                         │
      │<────────────────────────┤                         │
      │  (dice:[4,8,6])         │  EVENT_COMBAT_RESULT    │
      │  (damage: 12)           ├────────────────────────>│
      │                         │  (dice:[4,8,6])         │
      │                         │  (damage: 12)           │
```

---

## Client Architecture

### Client Responsibilities

**What clients DO:**
- Display game state visually
- Collect user input
- Send actions to server
- Maintain local copy of visible state
- Render UI/animations

**What clients DON'T DO:**
- Validate actions (server does this)
- Calculate combat results
- Generate random numbers
- Determine legal moves (server provides list)

### Client State Management

```c
typedef struct {
    // Network
    int socket_fd;
    uint8_t my_player_id;
    uint64_t auth_token;
    bool connected;
    
    // Game state (as known to this client)
    VisibleGameState visible_state;
    
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

### Client Network Loop

```c
// Two threads: Main (UI) + Network

// Network thread - receives messages from server
void* client_network_thread(void* arg) {
    ClientState* client = (ClientState*)arg;
    
    while (client->connected) {
        uint8_t buffer[4096];
        int received = recv_with_timeout(client->socket_fd,
                                        buffer, sizeof(buffer),
                                        1000); // 1 sec timeout
        
        if (received > 0) {
            pthread_mutex_lock(&client->state_mutex);
            client_handle_message(client, buffer);
            pthread_mutex_unlock(&client->state_mutex);
        } else if (received == 0) {
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
    initialize_client(&client, "server.address", 5555);
    
    // Start network thread
    pthread_t net_thread;
    pthread_create(&net_thread, NULL, 
                  client_network_thread, &client);
    
    // Main UI loop
    while (client.connected && !quit_requested) {
        handle_user_input(&client);
        
        pthread_mutex_lock(&client.state_mutex);
        render_game_state(&client.visible_state);
        pthread_mutex_unlock(&client.state_mutex);
        
        usleep(16666); // ~60 FPS
    }
    
    pthread_join(net_thread, NULL);
    return 0;
}
```

### Client UI Modes

```c
// Different UI implementations can share same client core

// CLI mode
int run_mode_client_cli(config_t* cfg) {
    ClientState client;
    connect_to_server(&client, cfg);
    
    while (game_active) {
        display_game_status_text(&client.visible_state);
        char command[256];
        fgets(command, sizeof(command), stdin);
        
        Action action = parse_cli_command(command, &client);
        send_action_to_server(&client, &action);
        
        wait_for_server_response(&client);
    }
    
    disconnect_client(&client);
    return 0;
}

// TUI mode (ncurses)
int run_mode_client_tui(config_t* cfg) {
    ClientState client;
    TUIState tui;
    
    init_ncurses(&tui);
    connect_to_server(&client, cfg);
    
    while (game_active) {
        draw_tui_game_board(&tui, &client.visible_state);
        int ch = getch();
        
        Action action = handle_tui_input(&tui, ch, &client);
        if (action.type != ACTION_INVALID) {
            send_action_to_server(&client, &action);
            wait_for_server_response(&client);
        }
    }
    
    cleanup_ncurses(&tui);
    disconnect_client(&client);
    return 0;
}

// GUI mode (SDL3) - future
int run_mode_client_gui(config_t* cfg) {
    ClientState client;
    SDL_Window* window;
    SDL_Renderer* renderer;
    
    init_sdl(&window, &renderer);
    connect_to_server(&client, cfg);
    
    SDL_Event event;
    while (game_active) {
        while (SDL_PollEvent(&event)) {
            Action action = handle_sdl_event(&event, &client);
            if (action.type != ACTION_INVALID) {
                send_action_to_server(&client, &action);
            }
        }
        
        render_sdl_game_state(renderer, &client.visible_state);
        SDL_RenderPresent(renderer);
    }
    
    cleanup_sdl(window, renderer);
    disconnect_client(&client);
    return 0;
}
```

---

## Server Architecture

### Server Responsibilities

**What servers DO:**
- Maintain authoritative game state
- Validate all client actions
- Execute game logic
- Generate all random numbers
- Synchronize clients
- Manage matchmaking (future)
- Log games for analysis

### Server State Management

```c
typedef struct {
    uint32_t game_id;
    ServerGameState game;             // Full state
    ClientConnection clients[2];
    TurnPhase phase;
    
    // Synchronization
    pthread_mutex_t mutex;
    time_t last_activity;
    
    // RNG (unique per game)
    GameContext ctx;
} GameSession;

typedef struct {
    GameSession* sessions[MAX_CONCURRENT_GAMES];
    int num_sessions;
    
    // Matchmaking queue
    ClientConnection* waiting_players[MAX_QUEUE];
    int queue_size;
    
    // Server socket
    int listen_socket;
    
    // Thread pool
    pthread_t worker_threads[WORKER_THREAD_COUNT];
} GameServer;
```

### Server Main Loop

```c
void server_main_loop(GameServer* server) {
    fd_set read_fds;
    struct timeval timeout;
    
    while (server->running) {
        FD_ZERO(&read_fds);
        FD_SET(server->listen_socket, &read_fds);
        
        int max_fd = server->listen_socket;
        
        // Add all client sockets
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
        
        int activity = select(max_fd + 1, &read_fds, 
                             NULL, NULL, &timeout);
        
        // New connection?
        if (FD_ISSET(server->listen_socket, &read_fds)) {
            handle_new_connection(server);
        }
        
        // Messages from clients?
        for (int i = 0; i < server->num_sessions; i++) {
            GameSession* session = server->sessions[i];
            
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

### Action Processing Pipeline

```c
void handle_client_message(GameSession* session, uint8_t player_id) {
    ClientConnection* client = &session->clients[player_id];
    
    // 1. Receive message
    uint8_t buffer[4096];
    int received = recv(client->socket_fd, buffer, 
                       sizeof(buffer), 0);
    if (received <= 0) {
        handle_client_disconnect(session, player_id);
        return;
    }
    
    // 2. Parse message
    MessageHeader* header = (MessageHeader*)buffer;
    void* payload = buffer + sizeof(MessageHeader);
    
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
        // ... etc
    }
    
    pthread_mutex_unlock(&session->mutex);
}

void handle_play_champions(GameSession* session,
                          uint8_t player_id,
                          void* payload) {
    PlayChampionsMessage* msg = (PlayChampionsMessage*)payload;
    
    // 1. Build action from message
    Action action;
    action.type = ACTION_PLAY_CHAMPIONS;
    action.player_id = player_id;
    action.data.cards.num_cards = msg->num_cards;
    memcpy(action.data.cards.card_ids, msg->card_ids,
          sizeof(uint8_t) * msg->num_cards);
    
    // 2. Validate
    if (!validate_action(&action, &session->game)) {
        send_error(&session->clients[player_id],
                  MSG_ERROR_INVALID_ACTION);
        send_full_state(session, player_id);
        return;
    }
    
    // 3. Apply to game state
    apply_action(&session->game, &action, &session->ctx);
    
    // 4. Broadcast to both clients
    broadcast_action_notification(session, &action);
    
    // 5. Send updated states
    send_delta_update(session, player_id);
    send_delta_update(session, 1 - player_id);
    
    // 6. Check if combat should occur
    if (session->game.combat_zone[player_id].size > 0) {
        session->phase = PHASE_DEFENSE;
        notify_combat_start(session);
    } else {
        session->phase = PHASE_TURN_END;
    }
}
```

---

## AI Integration

### AI Client Architecture

AI agents can connect as clients just like human players:

```c
// AI agent runs as a client
int run_mode_client_ai(config_t* cfg) {
    ClientState client;
    connect_to_server(&client, cfg);
    
    // Load AI strategy from config
    StrategySet* strategies = load_ai_strategy(cfg->ai_agent);
    
    while (game_active) {
        // Wait for our turn
        wait_for_turn_notification(&client);
        
        // Get list of valid moves from server
        ActionList moves;
        request_valid_moves(&client, &moves);
        
        // AI decides which move to make
        Action action = ai_choose_action(strategies,
                                        &client.visible_state,
                                        &moves);
        
        // Send action to server
        send_action_to_server(&client, &action);
        
        // Wait for result
        wait_for_action_result(&client);
    }
    
    disconnect_client(&client);
    free_strategy_set(strategies);
    return 0;
}
```

### Strategy Interface

All AI strategies implement the same interface:

```c
// strategy.h
typedef void (*AttackStrategyFunc)(gamestate* gs, GameContext* ctx);
typedef void (*DefenseStrategyFunc)(gamestate* gs, GameContext* ctx);

typedef struct {
    AttackStrategyFunc attack_strategy[2];
    DefenseStrategyFunc defense_strategy[2];
} StrategySet;

// Each strategy implements these functions
void random_attack_strategy(gamestate* gs, GameContext* ctx);
void random_defense_strategy(gamestate* gs, GameContext* ctx);

void balanced_attack_strategy(gamestate* gs, GameContext* ctx);
void balanced_defense_strategy(gamestate* gs, GameContext* ctx);

void heuristic_attack_strategy(gamestate* gs, GameContext* ctx);
void heuristic_defense_strategy(gamestate* gs, GameContext* ctx);

// Future: MCTS, neural network, etc.
void mcts_attack_strategy(gamestate* gs, GameContext* ctx);
void mcts_defense_strategy(gamestate* gs, GameContext* ctx);
```

### AI Strategy Flow

```c
// Common pattern for all AI strategies
void example_attack_strategy(gamestate* gs, GameContext* ctx) {
    PlayerID attacker = gs->current_player;
    
    // 1. Get list of possible actions
    ActionList possible_moves;
    get_list_of_possible_actions(&gs->visible, &possible_moves);
    
    if (possible_moves.count == 0) return; // No valid moves
    
    // 2. Evaluate each possible action (strategy-specific)
    double best_score = -INFINITY;
    int best_move_idx = 0;
    
    for (int i = 0; i < possible_moves.count; i++) {
        double score = evaluate_move(&possible_moves.actions[i],
                                     gs, ctx);
        if (score > best_score) {
            best_score = score;
            best_move_idx = i;
        }
    }
    
    // 3. Apply the best action
    apply_action(gs, &possible_moves.actions[best_move_idx], ctx);
}

// Strategy-specific evaluation function
double evaluate_move(const Action* action, gamestate* gs, 
                    GameContext* ctx) {
    // Random strategy: random score
    return genRand(&ctx->rng);
    
    // Balanced strategy: rule-based heuristics
    // return evaluate_balanced_heuristic(action, gs);
    
    // MCTS strategy: monte carlo simulation
    // return run_mcts_simulations(action, gs, ctx);
}
```

### Server-Side AI

The server can also run AI opponents directly:

```c
// Server decides which strategy to use
void server_game_tick(GameSession* session) {
    if (session->phase != PHASE_WAITING_FOR_INPUT) return;
    
    PlayerID current = session->game.current_player;
    ClientConnection* client = &session->clients[current];
    
    // If this is an AI client, server generates action
    if (client->is_ai) {
        ActionList moves;
        generate_valid_moves(&session->game, &moves);
        
        // Let AI decide
        Action action = ai_choose_action(client->ai_strategy,
                                        &session->game,
                                        &moves);
        
        // Process like any other action
        handle_action(session, &action);
    }
    // Otherwise wait for human client to send action
}
```

---

## Security & Validation

### Server-Side RNG (CRITICAL)

**ALL randomness MUST be generated on the server:**

```c
// ❌ WRONG - Client-side RNG
void client_roll_dice() {
    int dice = rand() % 20 + 1;  // Client generates
    send_to_server(dice);         // NEVER DO THIS
}

// ✅ CORRECT - Server-side RNG
void server_process_combat(GameSession* session) {
    // Server generates ALL dice rolls
    uint8_t attacker_dice[3];
    uint8_t defender_dice[3];
    
    for (int i = 0; i < attack_count; i++) {
        uint8_t die_type = attack_cards[i].defense_dice;
        attacker_dice[i] = RND_dn(die_type, &session->ctx);
    }
    
    for (int i = 0; i < defend_count; i++) {
        uint8_t die_type = defense_cards[i].defense_dice;
        defender_dice[i] = RND_dn(die_type, &session->ctx);
    }
    
    // Calculate results
    int attack_total = calculate_attack_total(attack_cards,
                                             attacker_dice);
    int defense_total = calculate_defense_total(defense_cards,
                                               defender_dice);
    
    // Send results to BOTH clients
    send_combat_result(session, attacker_dice, defender_dice,
                      attack_total, defense_total);
}
```

### Action Authentication

Every action includes authentication to prevent impersonation:

```c
typedef struct {
    uint8_t msg_type;
    uint8_t player_id;
    uint64_t auth_token;      // Prevents impersonation
    uint16_t sequence_num;    // Prevents replay
    uint64_t timestamp;       // Prevents old actions
    // ... action data
} AuthenticatedAction;

bool verify_action_auth(GameSession* session, 
                       AuthenticatedAction* action) {
    ClientConnection* client = &session->clients[action->player_id];
    
    // 1. Check token
    if (action->auth_token != client->auth_token) {
        log_security_violation("Invalid auth token");
        return false;
    }
    
    // 2. Check sequence (prevent replay)
    if (action->sequence_num != client->next_expected_sequence) {
        log_security_violation("Out of sequence");
        return false;
    }
    
    // 3. Check timestamp (must be recent)
    uint64_t now = time(NULL);
    if (now - action->timestamp > 60) {
        log_security_violation("Stale action");
        return false;
    }
    
    client->next_expected_sequence++;
    return true;
}
```

### Input Validation Checklist

Server MUST validate:

- [ ] Is it this player's turn?
- [ ] Is it the correct phase?
- [ ] Do the cards exist in player's hand?
- [ ] Can player afford the action?
- [ ] Is the action type valid for this phase?
- [ ] Are card indices within valid range?
- [ ] Is the action count valid (1-3 champions)?
- [ ] For recall: are cards in discard pile?
- [ ] For exchange: does player have champions?

**Never trust client data without validation.**

---

## Implementation Roadmap

### Phase 1: Foundation (Weeks 1-2)

**Goal:** Basic client/server communication

**Deliverables:**
1. Socket wrapper functions (`socket_utils.c/h`)
2. Message header structure and serialization
3. Simple echo server (client sends, server echoes)
4. Client connection and authentication

**Test:** Two clients can connect and exchange messages

### Phase 2: Game State Transfer (Weeks 3-4)

**Goal:** Server maintains state, sends filtered views to clients

**Deliverables:**
1. `ServerGameState` structure
2. `VisibleGameState` structure
3. State filtering function: `gamestate_get_visible()`
4. Full state message implementation
5. State serialization/deserialization

**Test:** 
- Server creates game with pre-defined state
- Both clients receive full state
- Verify each client sees correct filtered information

### Phase 3: Action Processing (Weeks 5-7)

**Goal:** Clients send actions, server validates and applies

**Deliverables:**
1. Action enumeration and structures (`action.h`)
2. Action serialization
3. Server validation framework
4. Action application (integrate with `card_actions.c`)
5. Delta update messages

**Implementation priority:**
1. `ACTION_PLAY_CHAMPIONS` (most common)
2. `ACTION_PASS`
3. `ACTION_PLAY_DRAW_RECALL`
4. `ACTION_PLAY_EXCHANGE`

**Test:**
- Client A plays champions → Server validates → Both see update
- Client tries invalid action → Server rejects with error
- Client tries action on wrong turn → Server rejects

### Phase 4: Full Game Loop (Weeks 8-10)

**Goal:** Complete turn-based game flow

**Deliverables:**
1. Turn phase management on server
2. Combat resolution over network
3. Server-side dice rolling
4. Game end detection and notification
5. Reconnection handling

**Integration:**
- Reuse existing `turn_logic.c` functions
- Reuse existing `combat.c` functions
- Server controls flow, clients react

**Test:**
- Play complete game: setup → turns → combat → end
- Test disconnect/reconnect mid-game
- Verify game state consistency throughout

### Phase 5: Client UI (Weeks 11-13)

**Goal:** Polished client interfaces

**CLI Deliverables:**
- Command parsing (already implemented)
- Game status display
- Action feedback

**TUI Deliverables (ncurses):**
- Game board rendering
- Card selection interface
- Action buttons
- Message console
- Status bar

**GUI Deliverables (SDL3 - future):**
- Card rendering
- Drag-and-drop
- Animations
- Sound effects

**Test all three client types against same server**

### Phase 6: AI Integration (Weeks 14-16)

**Goal:** AI agents can play as clients

**Deliverables:**
1. AI client wrapper
2. Strategy selection from config
3. Action generation from visible state
4. Multiple AI strategies implemented:
   - Random (already done)
   - Balanced rules
   - Heuristic
   - Simple Monte Carlo (future)
   - MCTS (future)

**Test:**
- AI vs AI games
- Human vs AI games
- AI win rate analysis

### Phase 7: Testing & Deployment (Weeks 17-18)

**Testing:**
1. **Unit tests:** Message serialization, action validation
2. **Integration tests:** Full game simulation
3. **Stress tests:** Multiple concurrent games
4. **Network tests:** Latency, packet loss
5. **Security tests:** Invalid actions, replay attacks

**Deployment:**
- Server setup on dedicated machine
- Firewall configuration
- Logging and monitoring
- Backup/restore procedures
- Admin documentation

---

## File Organization

### Current Structure (Standalone)

```
oracle/
├── src/
│   ├── main.c
│   ├── cmdline.c/h
│   ├── version.h
│   ├── game_types.h
│   ├── game_constants.c/h
│   ├── game_context.c/h
│   ├── game_state.c/h
│   ├── turn_logic.c/h
│   ├── card_actions.c/h
│   ├── combat.c/h
│   ├── combo_bonus.c/h
│   ├── strategy.c/h
│   ├── strat_random.c/h
│   ├── strat_balancedrules1.c (stub)
│   ├── strat_heuristic1.c (stub)
│   ├── strat_simplemc1.c (stub)
│   ├── strat_ismcts1.c (stub)
│   ├── stda_auto.c/h
│   ├── stda_cli.c/h
│   ├── deckstack.c/h
│   ├── hdcll.c/h
│   ├── rnd.c/h
│   ├── mtwister.c/h
│   └── debug.h
├── makefile
├── README.md
└── TODO.md
```

### Future Structure (With Network)

```
oracle/
├── src/
│   ├── core/              # Platform-agnostic game logic
│   │   ├── game_types.h
│   │   ├── game_constants.c/h
│   │   ├── game_context.c/h
│   │   ├── game_state.c/h
│   │   ├── turn_logic.c/h
│   │   ├── card_actions.c/h
│   │   ├── combat.c/h
│   │   ├── combo_bonus.c/h
│   │   ├── action.c/h         # NEW: Action system
│   │   ├── action_generator.c/h  # NEW: Move generation
│   │   └── validation.c/h     # NEW: Action validation
│   │
│   ├── network/           # Client/server networking
│   │   ├── protocol.c/h       # Message format
│   │   ├── socket_utils.c/h   # Socket wrappers
│   │   ├── serialization.c/h  # State serialization
│   │   └── crypto.c/h         # Authentication/checksums
│   │
│   ├── server/            # Server-specific code
│   │   ├── server_main.c
│   │   ├── server_state.c/h
│   │   ├── session_manager.c/h
│   │   ├── matchmaking.c/h    # Future
│   │   └── persistence.c/h    # Game saving
│   │
│   ├── client/            # Client-specific code
│   │   ├── client_main.c
│   │   ├── client_state.c/h
│   │   ├── client_network.c/h
│   │   └── client_ui.c/h
│   │
│   ├── ai/                # AI strategies
│   │   ├── strategy.c/h       # Strategy interface
│   │   ├── strat_random.c/h
│   │   ├── strat_balanced.c/h
│   │   ├── strat_heuristic.c/h
│   │   ├── strat_simplemc.c/h
│   │   └── strat_mcts.c/h
│   │
│   ├── ui/                # UI implementations
│   │   ├── cli/
│   │   │   └── cli_interface.c/h
│   │   ├── tui/
│   │   │   ├── tui_interface.c/h
│   │   │   └── tui_render.c/h
│   │   └── gui/           # Future SDL3
│   │       ├── gui_interface.c/h
│   │       ├── card_renderer.c/h
│   │       └── texture_cache.c/h
│   │
│   ├── standalone/        # Standalone mode runners
│   │   ├── stda_auto.c/h
│   │   ├── stda_cli.c/h
│   │   └── stda_sim.c/h   # Future: interactive simulation
│   │
│   ├── util/              # Utilities
│   │   ├── deckstack.c/h
│   │   ├── hdcll.c/h
│   │   ├── rnd.c/h
│   │   ├── mtwister.c/h
│   │   ├── debug.h
│   │   └── logger.c/h
│   │
│   └── main/              # Entry points
│       ├── main.c             # Mode dispatcher
│       ├── cmdline.c/h
│       └── version.h
│
├── tests/                 # Unit and integration tests
│   ├── test_combo_bonus.c
│   ├── test_action.c
│   ├── test_protocol.c
│   └── test_validation.c
│
├── tools/                 # Python utilities
│   ├── generate_cards.py
│   ├── test_protocol.py
│   └── analyze_games.py
│
├── assets/                # Game assets (future GUI)
│   ├── cards/
│   ├── icons/
│   └── fonts/
│
├── docs/                  # Documentation
│   ├── ARCHITECTURE.md    # This document
│   ├── PROTOCOL.md        # Network protocol spec
│   ├── DESIGN_NOTES.md
│   └── API.md
│
├── Makefile
├── README.md
└── TODO.md
```

### Module Dependencies

```
Dependency Graph:

main.c
├─ cmdline.c/h (mode selection)
├─ version.h
└─ Mode-specific entry points:
    ├─ stda_auto (standalone automated)
    ├─ stda_cli (standalone CLI)
    ├─ server_main (server mode)
    └─ client_main (client modes)

Core Game Logic (shared by all modes):
game_state.c
├─ game_types.h
├─ game_context.c/h
├─ turn_logic.c/h
│   ├─ card_actions.c/h
│   ├─ combat.c/h
│   │   └─ combo_bonus.c/h
│   └─ strategy.c/h
├─ deckstack.c/h
├─ hdcll.c/h
├─ rnd.c/h
│   └─ mtwister.c/h
└─ debug.h

Network Layer (client/server modes only):
client_main.c / server_main.c
├─ protocol.c/h
├─ socket_utils.c/h
├─ serialization.c/h
├─ action.c/h
│   └─ action_generator.c/h
└─ validation.c/h

UI Layer (pluggable):
├─ cli_interface.c/h
├─ tui_interface.c/h (ncurses)
└─ gui_interface.c/h (SDL3, future)

AI Layer:
strategy.c/h (interface)
├─ strat_random.c/h
├─ strat_balanced.c/h
├─ strat_heuristic.c/h
└─ strat_mcts.c/h (future)
```

---

## Key Design Decisions Summary

### Why Client/Server?

| Alternative | Why Not? |
|------------|----------|
| **Peer-to-peer** | Trust issues - either player could cheat with hidden information |
| **Standalone only** | No remote multiplayer |
| **Cloud gaming** | Overkill for turn-based, high bandwidth |

### Why TCP over UDP?

| Reason | Impact |
|--------|--------|
| Turn-based | Latency of 50-100ms is imperceptible |
| Critical data | Cannot afford lost actions |
| Simple code | TCP handles reliability automatically |
| Easy debugging | Ordered, reliable messages |

**When UDP would be better:** Real-time action games, voice chat

### Why Binary Protocol?

| Factor | Binary | JSON |
|--------|--------|------|
| **Speed** | Fast (no parsing) | Slower (parse overhead) |
| **Size** | Compact (~50 bytes/action) | Larger (~200 bytes/action) |
| **Debug** | Harder to read | Human-readable |
| **Version** | Manual handling | Self-documenting |

**Recommendation:** Binary for production, JSON option for debugging

### Why Authoritative Server?

**Prevents cheating:**
- Client can't see opponent's hand
- Client can't manipulate dice rolls
- Client can't play invalid actions
- Client can't modify energy/lunas

**Ensures consistency:**
- Both players see same game state
- No synchronization issues
- One source of truth

---

## Code Style Guidelines

### Function Length

**Target:** ≤30 lines of actual code (excluding comments/whitespace)

**When function exceeds limit:**
```c
// ❌ Too long (45 lines)
void process_combat(gamestate* gs, GameContext* ctx) {
    // Calculate attack
    int attack = 0;
    for (int i = 0; i < attack_count; i++) {
        attack += cards[i].base;
        attack += roll_dice(cards[i].die);
    }
    // Calculate defense
    int defense = 0;
    for (int i = 0; i < defend_count; i++) {
        defense += roll_dice(cards[i].die);
    }
    // Calculate combos
    int attack_bonus = calc_combo(attack_cards);
    int defense_bonus = calc_combo(defense_cards);
    // Apply bonuses
    attack += attack_bonus;
    defense += defense_bonus;
    // Calculate damage
    int damage = max(attack - defense, 0);
    // Apply damage
    gs->energy[defender] -= damage;
    // Check win
    if (gs->energy[defender] <= 0) {
        gs->game_state = (attacker == 0) ? PLAYER_A_WINS : PLAYER_B_WINS;
    }
    // Move cards
    move_cards_to_discard(gs, attacker);
    move_cards_to_discard(gs, defender);
}

// ✅ Refactored (3 functions, each ≤30 lines)
void process_combat(gamestate* gs, GameContext* ctx) {
    int attack = calculate_total_attack(gs, ctx);
    int defense = calculate_total_defense(gs, ctx);
    apply_combat_damage(gs, attack, defense);
}

int calculate_total_attack(gamestate* gs, GameContext* ctx) {
    int total = sum_base_attack(gs->combat_zone[gs->current_player]);
    total += roll_attack_dice(gs->combat_zone[gs->current_player], ctx);
    total += calc_combo(gs->combat_zone[gs->current_player]);
    return total;
}

void apply_combat_damage(gamestate* gs, int attack, int defense) {
    int damage = max(attack - defense, 0);
    PlayerID defender = 1 - gs->current_player;
    gs->energy[defender] -= damage;
    check_win_condition(gs);
    clear_combat_zones(gs);
}
```

### File Length

**Target:** ≤500 lines of code (excluding comments)

**When file exceeds limit:**
- Split by functionality (e.g., `combat.c` → `combat_calc.c` + `combat_apply.c`)
- Move utility functions to separate file
- Extract data structures to dedicated file

### Naming Conventions

```c
// Types: PascalCase
typedef struct {
    uint8_t player_id;
} PlayerState;

// Functions: snake_case with module prefix
void game_state_init(gamestate* gs);
int combat_calculate_total(card* cards, int count);
bool action_validate(const Action* action);

// Constants: SCREAMING_SNAKE_CASE
#define MAX_HAND_SIZE 20
#define INITIAL_ENERGY 99

// Enums: SCREAMING_SNAKE_CASE values
typedef enum {
    PLAYER_A,
    PLAYER_B
} PlayerID;

// Variables: snake_case
uint8_t current_player;
int total_damage;
bool game_over;
```

---

## Platform-Specific Considerations

### MSYS2 (Windows)

**Build:**
```bash
# Install dependencies
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-ncurses  # For TUI

# Compile
make
```

**Network:**
- Use Winsock2 (`-lws2_32`)
- Initialize with `WSAStartup()`
- Different error codes than POSIX

**UTF-8 Console:**
```c
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
```

### Arch Linux

**Build:**
```bash
# Install dependencies
sudo pacman -S gcc ncurses

# Compile
make
```

**Network:**
- Standard POSIX sockets
- No special initialization needed

### Cross-Platform Abstraction

```c
// socket_utils.h
#ifdef _WIN32
    #include <winsock2.h>
    #define SOCKET_ERROR_CODE WSAGetLastError()
    typedef SOCKET socket_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #define SOCKET_ERROR_CODE errno
    typedef int socket_t;
#endif

socket_t socket_create(void);
int socket_connect(socket_t sock, const char* host, uint16_t port);
int socket_send(socket_t sock, const void* data, size_t len);
int socket_recv(socket_t sock, void* buffer, size_t len);
void socket_close(socket_t sock);
```

---

## Testing Strategy

### Unit Tests

**Test individual components:**

```c
// test_action.c
void test_action_serialization() {
    Action action = {
        .type = ACTION_PLAY_CHAMPIONS,
        .player_id = 0,
        .data.cards = {
            .num_cards = 2,
            .card_ids = {5, 12}
        }
    };
    
    uint8_t buffer[256];
    size_t length;
    action_serialize(&action, buffer, &length);
    
    Action deserialized;
    assert(action_deserialize(&deserialized, buffer, length));
    assert(deserialized.type == action.type);
    assert(deserialized.data.cards.num_cards == 2);
    assert(deserialized.data.cards.card_ids[0] == 5);
}

void test_action_validation() {
    ServerGameState gs = create_test_game();
    Action action = create_test_action();
    
    // Valid action
    assert(validate_action(&action, &gs));
    
    // Invalid: wrong player
    action.player_id = 1 - gs.current_player;
    assert(!validate_action(&action, &gs));
    
    // Invalid: insufficient funds
    gs.players[0].lunas = 0;
    assert(!validate_action(&action, &gs));
}
```

### Integration Tests

**Test full game flow:**

```c
// test_full_game.c
void test_complete_game() {
    GameServer server;
    server_init(&server, 5555);
    
    ClientState client1, client2;
    client_connect(&client1, "localhost", 5555);
    client_connect(&client2, "localhost", 5555);
    
    // Play until game ends
    while (!client1.visible_state.game_over) {
        if (is_my_turn(&client1)) {
            Action action = generate_test_action(&client1);
            send_action(&client1, &action);
            wait_for_response(&client1);
        }
        if (is_my_turn(&client2)) {
            Action action = generate_test_action(&client2);
            send_action(&client2, &action);
            wait_for_response(&client2);
        }
    }
    
    assert(client1.visible_state.game_over);
    assert(client2.visible_state.game_over);
    assert(client1.visible_state.winner_id == 
           client2.visible_state.winner_id);
}
```

### Network Tests

**Test with simulated network conditions:**

```bash
# Add latency (100ms)
sudo tc qdisc add dev lo root netem delay 100ms

# Add packet loss (5%)
sudo tc qdisc add dev lo root netem loss 5%

# Run tests
make test

# Remove rules
sudo tc qdisc del dev lo root
```

---

## Conclusion

This architecture provides:

1. **Flexibility:** Supports standalone, client/server, and AI modes
2. **Security:** Server authority prevents cheating
3. **Modularity:** Clear separation of concerns
4. **Scalability:** Can add new features without refactoring
5. **Maintainability:** Functions ≤30 lines, files ≤500 lines
6. **Testability:** Unit and integration tests for all components

The design leverages your existing game logic while adding network capability through clean abstraction layers. The standalone mode can continue to work while network mode is developed in parallel.

**Next steps:** Begin with Phase 1 (Foundation) of the implementation roadmap.