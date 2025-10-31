Looking at your current codebase and the Technical Design Document, here are the principal adjustments needed to mirror the proposed client/server workflow while maintaining code reusability:

## 1. **Separate Game Logic from Action Execution**

Currently, your `turn_logic.c` directly executes actions. You need to separate **decision-making** from **action application**:

```c
// Current approach (standalone):
void attack_phase(struct gamestate* gstate, StrategySet* strategies, GameContext* ctx)
{ 
  strategies->attack_strategy[attacker](gstate, ctx);  // Makes AND applies decision
  gstate->turn_phase = DEFENSE;
}

// Proposed approach (client/server compatible):
// In turn_logic.c - just orchestrates, doesn't execute
Action request_attack_action(const VisibleGameState* vgs, StrategySet* strategies, GameContext* ctx)
{
  // Strategy returns an Action, doesn't modify state
  return strategies->attack_strategy[vgs->my_player_id](vgs, ctx);
}

// In game_engine.c or action_processor.c
bool apply_action(GameState* gstate, const Action* action, GameContext* ctx)
{
  // Server-side validation and execution
  if (!action_is_valid(action, gstate)) return false;
  
  switch(action->type) {
    case ACTION_PLAY_CHAMPIONS:
      for(int i = 0; i < action->data.cards.num_cards; i++)
        play_champion(gstate, action->player_id, 
                     action->data.cards.card_ids[i], ctx);
      break;
    // ... other action types
  }
  return true;
}
```

## 2. **Refactor Strategy Functions to Return Actions**

Change strategy signatures from `void` to returning `Action`:

```c
// Old signature:
typedef void (*AttackStrategyFunc)(struct gamestate* gstate, GameContext* ctx);

// New signature:
typedef Action (*AttackStrategyFunc)(const VisibleGameState* vgs, GameContext* ctx);

// Example refactored random strategy:
Action random_attack_strategy(const VisibleGameState* vgs, GameContext* ctx)
{
  ActionList possible;
  get_list_of_possible_actions(vgs, &possible);
  
  if(possible.count == 0)
    return action_create_pass(vgs->my_player_id);
    
  uint16_t choice = RND_randn(possible.count, ctx);
  return possible.actions[choice];
}
```

## 3. **Create Action List Generator**

Implement `get_list_of_possible_actions()` as specified in the design doc:

```c
// In gamestate_logic.c
void get_list_of_possible_actions(const VisibleGameState* vgs, ActionList* list)
{
  list->count = 0;
  
  switch(vgs->phase) {
    case PHASE_ATTACK:
      get_attack_actions(vgs, list);
      break;
    case PHASE_DEFENSE:
      get_defense_actions(vgs, list);
      break;
    case PHASE_DISCARD:
      get_discard_actions(vgs, list);
      break;
    case PHASE_MULLIGAN:
      get_mulligan_actions(vgs, list);
      break;
  }
}

void get_attack_actions(const VisibleGameState* vgs, ActionList* list)
{
  // Generate all possible champion combinations (1-3 cards)
  uint8_t* hand = get_hand_array(vgs);
  uint8_t hand_size = vgs->my_state.hand_count;
  
  // Pass action always available
  list->actions[list->count++] = action_create_pass(vgs->my_player_id);
  
  // Single champion plays
  for(uint8_t i = 0; i < hand_size; i++) {
    if(is_champion_affordable(hand[i], vgs))
      list->actions[list->count++] = action_create_play_champions(
        vgs->my_player_id, &hand[i], 1);
  }
  
  // Two champion combinations
  for(uint8_t i = 0; i < hand_size-1; i++) {
    for(uint8_t j = i+1; j < hand_size; j++) {
      uint8_t cards[2] = {hand[i], hand[j]};
      if(are_champions_affordable(cards, 2, vgs))
        list->actions[list->count++] = action_create_play_champions(
          vgs->my_player_id, cards, 2);
    }
  }
  
  // Draw/recall cards
  // Cash exchange cards
  // etc...
}
```

## 4. **Restructure Game Loop for Action-Response Pattern**

Your current game loop needs to support the request-action-apply-broadcast pattern:

```c
// Current monolithic approach in play_turn():
void play_turn(struct gamestats* gstats, struct gamestate* gstate,
               StrategySet* strategies, GameContext* ctx)
{
  begin_of_turn(gstate, ctx);
  attack_phase(gstate, strategies, ctx);  // Decision + execution combined
  // ...
}

// Proposed action-based approach:
typedef enum {
  GAME_PHASE_BEGIN_TURN,
  GAME_PHASE_ATTACK_REQUEST,
  GAME_PHASE_ATTACK_RESOLVE,
  GAME_PHASE_DEFENSE_REQUEST,
  GAME_PHASE_DEFENSE_RESOLVE,
  GAME_PHASE_COMBAT_RESOLVE,
  GAME_PHASE_END_TURN
} GamePhase;

// In game_engine.c
GamePhase advance_game_phase(GameState* gstate, GamePhase current_phase,
                             const Action* action, GameContext* ctx)
{
  switch(current_phase) {
    case GAME_PHASE_BEGIN_TURN:
      execute_begin_turn(gstate, ctx);  // Draw card, etc.
      return GAME_PHASE_ATTACK_REQUEST;
      
    case GAME_PHASE_ATTACK_REQUEST:
      // Wait for action from client/AI
      return GAME_PHASE_ATTACK_REQUEST;  // Stay in this phase
      
    case GAME_PHASE_ATTACK_RESOLVE:
      if(action && apply_action(gstate, action, ctx)) {
        if(has_combat(gstate))
          return GAME_PHASE_DEFENSE_REQUEST;
        else
          return GAME_PHASE_END_TURN;
      }
      break;
      
    // ... other phases
  }
}

// Standalone mode wrapper:
void play_turn_standalone(struct gamestate* gstate, StrategySet* strategies,
                         GameContext* ctx)
{
  GamePhase phase = GAME_PHASE_BEGIN_TURN;
  Action action;
  
  while(phase != GAME_PHASE_END_TURN && !gstate->someone_has_zero_energy) {
    phase = advance_game_phase(gstate, phase, &action, ctx);
    
    // If waiting for action, get it from strategy
    if(phase == GAME_PHASE_ATTACK_REQUEST) {
      VisibleGameState vgs;
      gamestate_get_visible(gstate, gstate->current_player, &vgs);
      action = strategies->attack_strategy[gstate->current_player](&vgs, ctx);
      phase = GAME_PHASE_ATTACK_RESOLVE;
    }
    else if(phase == GAME_PHASE_DEFENSE_REQUEST) {
      VisibleGameState vgs;
      gamestate_get_visible(gstate, 1 - gstate->current_player, &vgs);
      action = strategies->defense_strategy[1-gstate->current_player](&vgs, ctx);
      phase = GAME_PHASE_DEFENSE_RESOLVE;
    }
  }
  
  execute_end_turn(gstate, ctx);
}
```

## 5. **Create Display Hooks for UI Updates**

Add callback points where UI can display information:

```c
typedef struct {
  void (*on_card_drawn)(PlayerID player, uint8_t card_id, void* ui_ctx);
  void (*on_action_taken)(const Action* action, void* ui_ctx);
  void (*on_combat_resolved)(int16_t damage, void* ui_ctx);
  void (*on_game_state_changed)(const VisibleGameState* vgs, void* ui_ctx);
  void* ui_context;  // Pointer to UI-specific data
} UICallbacks;

// In begin_of_turn:
void execute_begin_turn(GameState* gstate, GameContext* ctx, UICallbacks* ui)
{
  gstate->turn++;
  
  if(!(gstate->turn == 1 && gstate->current_player == PLAYER_A)) {
    uint8_t drawn_card = draw_1_card(gstate, gstate->current_player, ctx);
    
    // Notify UI
    if(ui && ui->on_card_drawn)
      ui->on_card_drawn(gstate->current_player, drawn_card, ui->ui_context);
  }
}
```

## 6. **Modify `draw_1_card()` to Return Card ID**

Your current version doesn't return the card drawn:

```c
// Current:
void draw_1_card(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{
  // ... existing code ...
  uint8_t cardindex = DeckStk_pop(&gstate->deck[player]);
  HDCLL_insertNodeAtBeginning(&gstate->hand[player], cardindex);
}

// Modified:
uint8_t draw_1_card(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{
  if(DeckStk_isEmpty(&gstate->deck[player])) {
    shuffle_discard_and_form_deck(&gstate->discard[player], 
                                  &gstate->deck[player], ctx);
  }
  
  uint8_t cardindex = DeckStk_pop(&gstate->deck[player]);
  HDCLL_insertNodeAtBeginning(&gstate->hand[player], cardindex);
  
  return cardindex;  // Return for UI display
}
```

## 7. **Implement VisibleGameState Conversion**

```c
// In visibility.c
void gamestate_get_visible(const GameState* gs, uint8_t player_id, 
                           VisibleGameState* visible)
{
  visible->my_player_id = player_id;
  visible->active_player_id = gs->current_player;
  visible->phase = gs->turn_phase;
  visible->turn_number = gs->turn;
  
  // Copy own complete state
  visible->my_state = gs->players[player_id];
  
  // Copy opponent's visible info only
  uint8_t opp = 1 - player_id;
  visible->opp_energy = gs->players[opp].energy;
  visible->opp_lunas = gs->players[opp].lunas;
  visible->opp_hand_count = gs->players[opp].hand_count;
  visible->opp_deck_count = gs->players[opp].deck_count;
  
  // Copy opponent's discard (fully visible)
  memcpy(visible->opp_discard, gs->players[opp].discard, 
         gs->players[opp].discard_count);
  visible->opp_discard_count = gs->players[opp].discard_count;
  
  // Copy combat zone
  visible->attack_count = gs->attack_count;
  memcpy(visible->attack_cards, gs->attack_cards, 
         gs->attack_count * sizeof(Card));
  // ... etc
}
```

## 8. **File Organization Suggestion**

Create these new files to maintain clean separation:

```
src/core/
  action.c/h              - Action creation and validation
  action_generator.c/h    - get_list_of_possible_actions()
  action_processor.c/h    - apply_action() - server-side execution
  game_engine.c/h         - Phase management, game loop orchestration
  visibility.c/h          - VisibleGameState conversions
  
src/ui/
  ui_callbacks.h          - UICallbacks structure definition
  ui_stda_cli.c          - Standalone CLI with callbacks
```

## Key Benefits of This Approach

1. **Same engine works for all modes**: Standalone, server, and client use the same `apply_action()` function
2. **UI displays at natural points**: Callbacks fire when information becomes available
3. **Strategies are mode-agnostic**: They only see `VisibleGameState` and return `Action` objects
4. **Easy to add networking**: Server maintains `GameState`, clients work with `VisibleGameState`
5. **Testable**: Can unit test action generation, validation, and application separately

You're absolutely right! Let me clarify the distinction:

## Action Flow by Component Type

### **Server (Authoritative)**
```c
// Server receives action from client, validates, and applies
bool server_process_action(Server* srv, uint8_t player_id, const Action* action)
{
  // Validate action against current game state
  if(!action_is_valid(action, &srv->game_state))
    return false;
  
  // Apply to authoritative game state
  apply_action(&srv->game_state, action, srv->ctx);
  
  // Broadcast result to both clients
  server_broadcast_game_state(srv);
  return true;
}
```

### **Human Client (Interactive)**
```c
// Human player NEVER calls apply_action()
// They only:
// 1. View VisibleGameState (via UI)
// 2. Select action through UI
// 3. Send action to server
// 4. Receive updated VisibleGameState

int run_mode_client_tui(config_t* cfg)
{
  Client cli;
  client_connect(&cli, cfg->server_host, cfg->server_port);
  
  VisibleGameState vgs;
  while(!vgs.game_over) {
    // Receive current state from server
    client_receive_gamestate(&cli, &vgs);
    
    // Display state to human
    display_gamestate_tui(&vgs);
    
    if(vgs.active_player_id == vgs.my_player_id) {
      // Get action from human via UI
      Action action = get_human_action_tui(&vgs);
      
      // Send to server (doesn't apply locally!)
      client_send_action(&cli, &action);
    }
    
    // Wait for server's response with updated state
  }
}
```

### **AI Client (Non-MCTS)**
```c
// Simple AI (random, heuristic) also doesn't apply_action()
// It just selects from available moves

Action ai_simple_select_action(const VisibleGameState* vgs, GameContext* ctx)
{
  ActionList possible;
  get_list_of_possible_actions(vgs, &possible);
  
  // Evaluate actions using heuristic
  int best_idx = evaluate_actions_heuristic(&possible, vgs);
  
  return possible.actions[best_idx];
  // Never applies anything - just returns choice
}
```

### **AI Client with MCTS (Tree Search)**
```c
// MCTS AI DOES use apply_action() for simulations
Action ai_mcts_select_action(const VisibleGameState* vgs, GameContext* ctx)
{
  // Clone and determinize hidden information
  GameState simulated_state;
  clone_and_randomize_gamestate(&simulated_state, vgs, ctx);
  
  MCTSNode* root = mcts_create_root(&simulated_state);
  
  for(int i = 0; i < NUM_SIMULATIONS; i++) {
    // Copy state for this simulation
    GameState sim_copy = simulated_state;
    
    MCTSNode* leaf = mcts_select_and_expand(root, &sim_copy);
    
    // ROLLOUT: Apply multiple actions in simulation
    while(!is_terminal(&sim_copy)) {
      ActionList possible;
      get_list_of_possible_actions_from_gamestate(&sim_copy, &possible);
      
      Action random_action = possible.actions[rand() % possible.count];
      
      // AI applies action to SIMULATED state only
      apply_action(&sim_copy, &random_action, ctx);
    }
    
    float result = get_result(&sim_copy, vgs->my_player_id);
    mcts_backpropagate(leaf, result);
  }
  
  return mcts_get_best_action(root);
}
```

### **Standalone Mode**
```c
// Standalone DOES apply actions because there's no server
// It's acting as both client and server

void play_turn_standalone(struct gamestate* gstate, StrategySet* strategies,
                         GameContext* ctx, UICallbacks* ui)
{
  // Get action from strategy (AI or human)
  VisibleGameState vgs;
  gamestate_get_visible(gstate, gstate->current_player, &vgs);
  
  Action action;
  if(is_human_player(gstate->current_player)) {
    action = get_human_action_tui(&vgs, ui);
  } else {
    action = strategies->attack_strategy[gstate->current_player](&vgs, ctx);
  }
  
  // Standalone applies directly (no server to validate)
  apply_action(gstate, &action, ctx);
  
  // UI callback to show result
  if(ui && ui->on_action_taken)
    ui->on_action_taken(&action, ui->ui_context);
}
```

## Summary of Who Uses `apply_action()`

| Component | Uses apply_action()? | Why? |
|-----------|---------------------|------|
| **Server** | ✅ YES | Authoritative - applies validated actions |
| **Human Client** | ❌ NO | Just sends actions, receives state updates |
| **Simple AI Client** | ❌ NO | Just selects action from list |
| **MCTS AI Client** | ✅ YES | Needs to simulate future game states |
| **Standalone Mode** | ✅ YES | Acts as its own server |

## Updated Strategy Function Patterns

```c
// Pattern 1: Simple strategy (human or simple AI)
// - Works with VisibleGameState only
// - Returns Action without modifying anything
Action simple_strategy(const VisibleGameState* vgs, GameContext* ctx)
{
  ActionList possible;
  get_list_of_possible_actions(vgs, &possible);
  return choose_best_action(&possible, vgs);
}

// Pattern 2: Tree-search strategy (MCTS)
// - Clones and simulates GameState internally
// - Uses apply_action() on simulated states only
// - Still returns Action without modifying real state
Action mcts_strategy(const VisibleGameState* vgs, GameContext* ctx)
{
  GameState simulated;
  clone_and_randomize_gamestate(&simulated, vgs, ctx);
  
  // Internal simulation loop
  for(int sim = 0; sim < num_sims; sim++) {
    GameState sim_copy = simulated;
    simulate_game(&sim_copy, ctx);  // Uses apply_action internally
  }
  
  return best_action_from_tree;
}
```

You're correct - the key distinction is:
- **Declarative clients** (human, simple AI): Just choose actions
- **Simulation clients** (MCTS): Apply actions to **local copies** for lookahead
- **Server**: Applies actions to the **authoritative state**

