```c
// New: game_server.h
typedef struct GameServer GameServer;

GameServer* server_create(uint16_t initial_cash);
void server_apply_action(GameServer* srv, PlayerID player, Action* action);
VisibleGameState server_get_view(GameServer* srv, PlayerID observer);
void server_destroy(GameServer* srv);

// New: game_client.h  
typedef struct GameClient GameClient;

GameClient* client_create(PlayerID player_id);
void client_receive_state(GameClient* cli, VisibleGameState* state);
Action client_get_action(GameClient* cli);  // AI or human input
void client_destroy(GameClient* cli);
```

### Action Structure

```c
// action.h
typedef enum {
    ACTION_PLAY_CHAMPIONS,
    ACTION_PLAY_DRAW_CARD,
    ACTION_PLAY_CASH_CARD,
    ACTION_PASS,
    ACTION_DEFEND,
    ACTION_DISCARD
} ActionType;

typedef struct {
    ActionType type;
    uint8_t num_cards;
    uint8_t card_indices[3];
    uint8_t draw_or_recall;
} Action;

// Clean interface
bool action_is_legal(const Action* action, const VisibleGameState* state);
void action_apply(Action* action, struct gamestate* gstate);
```

### Visibility System

Critical for AI and multiplayer:

```c
// visibility.h
typedef enum {
    CARD_VISIBLE,
    CARD_KNOWN_IN_OPPONENT_HAND,
    CARD_HIDDEN,
    CARD_KNOWN_IN_OWN_DECK,
    CARD_KNOWN_IN_OPPONENT_DECK
} CardVisibility;

typedef struct {
    struct gamestate base;
    CardVisibility visibility[2][FULL_DECK_SIZE];
} GameStateWithVisibility;
```
