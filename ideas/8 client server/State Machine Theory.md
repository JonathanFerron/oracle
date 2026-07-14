# State Machine Theory and Application to Oracle: The Champions of Arcadia

## Table of Contents

1. Theoretical Foundations of State Machines
2. Formal State Machine Models
3. State Machine Design Patterns
4. Application to Turn-Based Card Games
5. Implementation Considerations for Oracle

---

## 1. Theoretical Foundations of State Machines

### 1.1 Definition and Core Concepts

A **finite state machine (FSM)**, also known as a **finite automaton**, is a mathematical model of computation consisting of:

- **Q**: A finite set of states
- **Σ**: A finite input alphabet (set of events/inputs)
- **δ**: A transition function δ: Q × Σ → Q
- **q₀**: An initial state (q₀ ∈ Q)
- **F**: A set of final/accepting states (F ⊆ Q)

The machine exists in exactly one state at any given time, called the **current state**. It transitions between states in response to external inputs according to the transition function δ.

### 1.2 Types of Finite State Machines

**Deterministic Finite Automaton (DFA)**:

- For each state and input symbol, there is exactly one transition
- δ: Q × Σ → Q (function, not relation)
- No ambiguity in next state determination

**Nondeterministic Finite Automaton (NFA)**:

- Multiple possible transitions for a given state-input pair
- δ: Q × Σ → 2^Q (maps to power set of Q)
- May include ε-transitions (state changes without input)

**Moore Machine**:

- Output depends only on current state
- Output function: λ: Q → Ω (where Ω is output alphabet)
- Outputs are associated with states

**Mealy Machine**:

- Output depends on current state and input
- Output function: λ: Q × Σ → Ω
- Outputs are associated with transitions

For game logic, we typically use **Moore machines** because game phases naturally produce outputs (allowed actions, display information) based solely on the current state, independent of how we arrived there.

### 1.3 Extended State Machines

Classical FSMs have limitations: they cannot remember arbitrary amounts of information. **Extended State Machines (ESMs)** augment FSMs with:

- **Extended state variables**: Data that exists alongside the FSM state
- **Guard conditions**: Boolean predicates on extended state that enable/disable transitions
- **Actions**: Operations that modify extended state during transitions

Formally, an ESM is a tuple (Q, Σ, Γ, δ, λ, q₀, γ₀) where:

- Γ is the extended state space
- δ: Q × Σ × Γ → Q × Γ (transitions may depend on and modify extended state)
- λ: Q × Σ × Γ → Ω (outputs may depend on extended state)
- γ₀ is the initial extended state

This is essential for games: while the *phase* might be "ATTACK_PHASE", the extended state includes which player is attacking, current card selections, energy values, etc.

### 1.4 Hierarchical State Machines

**Hierarchical State Machines (HSMs)** or **Statecharts** (introduced by David Harel, 1987) add structure to complex FSMs through:

**Composite states**: States that contain sub-state machines

- Example: COMBAT_STATE contains {ATTACK_SELECTION, DEFENSE_SELECTION, RESOLUTION}

**Orthogonal regions**: Concurrent sub-machines within a state

- Example: Tracking both players' actions simultaneously

**History states**: Pseudo-states that remember the last active sub-state

- Example: Returning to the exact phase before a pause

Formally, a statechart adds:

- **Hierarchy relation**: parent(s) ⊆ Q × Q indicating state containment
- **Concurrency relation**: concurrent(s) → 2^Q indicating orthogonal regions
- **Entry/exit actions**: Operations executed when entering/leaving states

HSMs provide **behavioral inheritance**: sub-states inherit transitions from parent states unless explicitly overridden.

---

## 2. Formal State Machine Models

### 2.1 State Transition Systems

A **state transition system (STS)** is a more general model than FSM:

STS = (S, →, S₀)

Where:

- S is a set of states (may be infinite)
- → ⊆ S × S is a transition relation
- S₀ ⊆ S is a set of initial states

Unlike FSMs, transitions are not labeled with inputs. This abstraction is useful for reasoning about system behavior without committing to specific implementation details.

**Labeled Transition System (LTS)** adds labels:

LTS = (S, Λ, →, S₀)

Where:

- Λ is a set of labels (actions, events)
- → ⊆ S × Λ × S is a labeled transition relation

We write s →^a s' to mean (s, a, s') ∈ →.

### 2.2 Pushdown Automata

When state machines need to remember an unbounded sequence of choices (like function call stacks), we use **Pushdown Automata (PDA)**:

PDA = (Q, Σ, Γ, δ, q₀, Z₀, F)

Where:

- Γ is a stack alphabet
- δ: Q × (Σ ∪ {ε}) × Γ → 2^(Q × Γ*) is the transition function
- Z₀ ∈ Γ is the initial stack symbol

PDAs are more powerful than FSMs but less powerful than Turing machines. They recognize **context-free languages**.

For game development, PDAs are useful when:

- Implementing nested menu systems
- Managing hierarchical game modes (main game → combat → spell resolution → back)
- Undo/redo functionality (stack of previous states)

### 2.3 State Machine Composition

**Parallel composition** combines multiple state machines running concurrently:

M₁ ∥ M₂ = (Q₁ × Q₂, Σ₁ ∪ Σ₂, δ, (q₁₀, q₂₀), F₁ × F₂)

Where:

- States are pairs (q₁, q₂)
- Both machines must agree on shared events
- Useful for multiplayer games where each player has their own state

**Sequential composition** chains machines:

M₁ · M₂: When M₁ reaches a final state, control transfers to M₂

**Choice composition** (M₁ + M₂): Initially, nondeterministically choose between M₁ or M₂

These operators form a **process algebra** for describing complex interactive systems compositionally.

### 2.4 Temporal Properties and Model Checking

State machines enable **formal verification** of system properties using temporal logic:

**Linear Temporal Logic (LTL)** operators:

- □φ (always φ): φ holds in all future states
- ◇φ (eventually φ): φ holds in some future state
- φ U ψ (φ until ψ): φ holds until ψ becomes true
- ○φ (next φ): φ holds in the next state

**Examples for Oracle**:

- **Safety**: □(energy ≥ 0) — "Energy never goes negative"
- **Liveness**: □(attack_phase → ◇defense_phase) — "Every attack eventually leads to defense"
- **Fairness**: □◇(turn = PLAYER_A) ∧ □◇(turn = PLAYER_B) — "Both players get infinitely many turns"

**Model checking** algorithms (like BDD-based or SAT-based methods) can automatically verify these properties on finite-state abstractions of the game.

---

## 3. State Machine Design Patterns

### 3.1 The State Pattern (Gang of Four)

The **State pattern** is an object-oriented implementation of FSMs. It defines:

**Context**: Maintains a reference to the current state object

**State Interface**: Declares methods for handling requests

**Concrete States**: Implement behavior specific to each state

```c
// Conceptual structure (not literal C code)
typedef struct State {
    void (*handle_input)(struct State* self, Event e);
    void (*enter)(struct State* self);
    void (*exit)(struct State* self);
} State;

typedef struct Context {
    State* current_state;
    void* game_data;  // Extended state
} Context;

void context_handle_event(Context* ctx, Event e) {
    ctx->current_state->handle_input(ctx->current_state, e);
}
```

**Advantages**:

- States are encapsulated in separate objects/modules
- Adding new states doesn't require modifying existing code
- State-specific behavior is localized

**Disadvantages**:

- Can lead to many small objects/files
- State transitions require careful management of object lifecycles

### 3.2 Table-Driven State Machines

A **transition table** explicitly encodes the transition function:

```
Current State | Input Event    | Next State     | Action
-------------|----------------|----------------|------------------
IDLE         | START_GAME     | SETUP          | initialize_game()
SETUP        | MULLIGAN_DONE  | TURN_START     | first_player_draw()
TURN_START   | DRAW_COMPLETE  | ACTION_PHASE   | enable_actions()
ACTION_PHASE | PLAY_CHAMPIONS | DEFENSE_PHASE  | notify_defender()
```

Implementation uses 2D arrays or hash tables:

```c
typedef enum { IDLE, SETUP, TURN_START, ... } State;
typedef enum { START_GAME, MULLIGAN_DONE, ... } Event;

typedef struct {
    State next_state;
    void (*action)(GameState*);
} Transition;

Transition transition_table[NUM_STATES][NUM_EVENTS];
```

**Advantages**:

- Very compact representation
- Easy to visualize and verify completeness
- Can be generated from formal specifications

**Disadvantages**:

- Sparse tables waste memory
- Complex actions require separate function calls
- Guard conditions require additional logic

### 3.3 Hierarchical State Pattern

Implements HSMs using **delegation** and **inheritance**:

**Abstract base state** provides default transitions **Derived states** override specific transitions **Composite states** delegate to sub-state machines

```c
typedef struct State State;

struct State {
    State* parent;  // NULL for top-level states
    State* (*handle_event)(State* self, Event e, GameState* gs);
    void (*on_enter)(State* self, GameState* gs);
    void (*on_exit)(State* self, GameState* gs);
};

State* state_handle_event(State* s, Event e, GameState* gs) {
    State* next = s->handle_event(s, e, gs);
    if (next == s) {
        // Event not handled, try parent
        if (s->parent) {
            return state_handle_event(s->parent, e, gs);
        }
    }
    return next;
}
```

This implements **behavioral inheritance**: sub-states handle specific events, deferring to parents for common behaviors.

### 3.4 Event Queue Pattern

Separates **event generation** from **event processing**:

```c
typedef struct {
    Event events[MAX_QUEUE_SIZE];
    int head, tail, count;
} EventQueue;

void game_loop(GameState* gs, EventQueue* eq) {
    while (gs->running) {
        // Generate events from input
        poll_inputs(eq);

        // Process events
        while (eq->count > 0) {
            Event e = dequeue(eq);
            State* next = gs->current_state->handle_event(
                gs->current_state, e, gs);
            transition_to(gs, next);
        }

        // Render current state
        render(gs);
    }
}
```

**Advantages**:

- Decouples input from game logic
- Enables event recording/replay
- Allows asynchronous event injection (network messages)

**Disadvantages**:

- Events may arrive out of order
- Queue overflow must be handled
- Priority events need special treatment

---

## 4. Application to Turn-Based Card Games

### 4.1 Game Phase State Machine

Turn-based card games naturally decompose into phases. For Oracle:

**Top-level states**:

```
GAME_START → SETUP → TURN_LOOP → GAME_END
```

**TURN_LOOP refinement** (hierarchical):

```
TURN_START
  ├─ DRAW_PHASE (unless first player, first turn)
  └─ ACTION_PHASE
      ├─ PLAY_CHAMPIONS → DEFENSE_OPPORTUNITY → COMBAT_RESOLUTION
      ├─ PLAY_DRAW_RECALL → EXECUTE_DRAW_RECALL
      ├─ PLAY_EXCHANGE → EXECUTE_EXCHANGE
      └─ PASS → TURN_END
TURN_END
  ├─ COLLECT_LUNA
  ├─ DISCARD_TO_SEVEN
  └─ CHECK_WIN_CONDITION → (back to TURN_START or GAME_END)
```

**Formal definition**:

Let Q = {GAME_START, SETUP, DRAW, ACTION, DEFENSE, COMBAT, TURN_END, GAME_END}

Let Σ = {start, setup_complete, card_drawn, play_action, defend_action, pass, luna_collected, discarded, win_detected}

Key transitions (δ):

- δ(SETUP, setup_complete) = DRAW (if not first player's first turn)
- δ(DRAW, card_drawn) = ACTION
- δ(ACTION, play_champions) = DEFENSE
- δ(DEFENSE, defend_action) = COMBAT
- δ(COMBAT, resolved) = TURN_END
- δ(TURN_END, win_detected) = GAME_END
- δ(TURN_END, continue) = DRAW (switch active player)

### 4.2 Extended State Variables

The phase alone is insufficient; we need extended state:

```c
typedef struct {
    // FSM state
    GamePhase phase;

    // Extended state
    PlayerState players[2];
    uint8_t active_player;
    uint8_t turn_number;

    // Combat context
    Card attack_cards[3];
    uint8_t attack_count;
    Card defense_cards[3];
    uint8_t defense_count;

    // Special flags
    bool first_turn_first_player;
    bool awaiting_defense;
} GameState;
```

**Guard conditions** on transitions:

```c
bool can_transition_to_combat(GameState* gs) {
    return gs->phase == ACTION_PHASE 
        && gs->attack_count > 0
        && !gs->awaiting_defense;
}

bool can_transition_to_turn_end(GameState* gs) {
    return gs->phase == COMBAT_RESOLUTION
        || (gs->phase == ACTION_PHASE && action_was_pass);
}
```

### 4.3 Player Turn State Machine

Each player conceptually has their own state machine:

```
WAITING → MY_TURN_DRAW → MY_TURN_ACTION → WAITING
```

In **parallel composition** with opponent:

```
Player A: MY_TURN_ACTION || Player B: WAITING
  ↓ (A plays attack)
Player A: WAITING || Player B: MY_TURN_DEFENSE
  ↓ (B plays defense)
Both: OBSERVING_COMBAT
  ↓ (combat resolves)
Player A: WAITING || Player B: MY_TURN_END
  ↓ (turn ends)
Player A: MY_TURN_DRAW || Player B: WAITING
```

This demonstrates **interleaving semantics**: players' actions alternate, but combat resolution is a shared synchronization point.

### 4.4 Client-Server State Synchronization

In networked play, client and server state machines must remain synchronized:

**Server state machine** (authoritative):

```
WAITING_FOR_PLAYERS → GAME_ACTIVE → GAME_COMPLETE
```

Within GAME_ACTIVE, it runs the full game phase machine.

**Client state machine** (reactive):

```
CONNECTING → WAITING_FOR_STATE → DISPLAYING_STATE 
  ↔ WAITING_FOR_INPUT → SENDING_ACTION → WAITING_FOR_RESPONSE
```

**State synchronization protocol**:

1. **Full state transfer**: Server sends complete visible state
   
   - On connection
   - After validation errors
   - Periodically (heartbeat)

2. **Delta updates**: Server sends incremental changes
   
   - After each action
   - More efficient than full state

3. **Acknowledgments**: Client confirms receipt
   
   - Prevents state drift
   - Enables retransmission on packet loss

**Consistency model**: Server state is **linearizable**

- All client operations appear to occur atomically
- Operations have a total order consistent with real-time order
- Clients eventually converge to server's state

---

## 5. Implementation Considerations for Oracle

### 5.1 State Representation in C

**Enum-based states** (simple, efficient):

```c
typedef enum {
    PHASE_GAME_START,
    PHASE_SETUP,
    PHASE_MULLIGAN,
    PHASE_TURN_DRAW,
    PHASE_TURN_ACTION,
    PHASE_TURN_DEFENSE,
    PHASE_COMBAT_RESOLUTION,
    PHASE_TURN_END_COLLECT,
    PHASE_TURN_END_DISCARD,
    PHASE_GAME_OVER
} GamePhase;
```

**Transition function** (switch-based):

```c
GamePhase phase_transition(GamePhase current, GameEvent event,
                          GameState* gs) {
    switch (current) {
        case PHASE_SETUP:
            if (event == EVENT_SETUP_COMPLETE) {
                return PHASE_MULLIGAN;
            }
            break;

        case PHASE_MULLIGAN:
            if (event == EVENT_MULLIGAN_DONE) {
                if (gs->first_turn_first_player) {
                    gs->first_turn_first_player = false;
                    return PHASE_TURN_ACTION;  // Skip draw
                }
                return PHASE_TURN_DRAW;
            }
            break;

        case PHASE_TURN_ACTION:
            if (event == EVENT_PLAY_CHAMPIONS) {
                return PHASE_TURN_DEFENSE;
            } else if (event == EVENT_PASS || event == EVENT_PLAY_OTHER) {
                return PHASE_TURN_END_COLLECT;
            }
            break;

        // ... other cases
    }
    return current;  // No transition
}
```

**Advantages of enum-based approach**:

- Fast (integer comparison)
- Type-safe (compiler checks)
- Easy to serialize (single byte)
- Debugger-friendly (shows state name)

### 5.2 Action Validation as State Guards

Actions are only legal in specific states:

```c
bool action_is_valid(Action* action, GameState* gs) {
    // Global constraints
    if (action->player_id != gs->active_player) {
        return false;  // Not your turn
    }

    // Phase-specific constraints
    switch (gs->phase) {
        case PHASE_TURN_ACTION:
            return action->type == ACTION_PLAY_CHAMPIONS
                || action->type == ACTION_PLAY_DRAW_RECALL
                || action->type == ACTION_PLAY_EXCHANGE
                || action->type == ACTION_PASS;

        case PHASE_TURN_DEFENSE:
            return action->type == ACTION_PLAY_CHAMPIONS
                || action->type == ACTION_DECLINE_DEFENSE;

        case PHASE_TURN_END_DISCARD:
            return action->type == ACTION_DISCARD;

        default:
            return false;
    }
}
```

This implements **guard conditions** from ESM theory.

### 5.3 Avoiding State Explosion

Complex games can have exponentially many states. Mitigation strategies:

**1. State factorization**: Separate orthogonal concerns

```c
// Instead of one mega-state:
// PLAYER_A_TURN_DRAW_WITH_COMBAT_PENDING_AND_MULLIGAN_AVAILABLE...

// Factor into independent variables:
typedef struct {
    GamePhase phase;
    uint8_t active_player;
    bool combat_pending;
    bool mulligan_available;
} GameState;
```

**2. Hierarchical decomposition**: Group related states

```c
bool is_turn_phase(GamePhase p) {
    return p >= PHASE_TURN_DRAW && p <= PHASE_TURN_END_DISCARD;
}

bool is_combat_phase(GamePhase p) {
    return p == PHASE_TURN_DEFENSE || p == PHASE_COMBAT_RESOLUTION;
}
```

**3. Abstraction**: Ignore irrelevant details

For AI search, abstract away:

- Exact card order in deck (only count matters)
- Specific luna coins held (only total matters)
- Historical information (previous turns)

This creates an **abstract state space** much smaller than the full space.

### 5.4 State Persistence and Serialization

For save/load and network transmission:

```c
typedef struct {
    uint8_t version;      // Protocol version
    GamePhase phase;      // 1 byte
    uint8_t active_player;
    uint8_t turn_number;
    uint8_t reserved[5];  // Future expansion
    PlayerState players[2];  // Fixed size
    // ... combat state ...
} __attribute__((packed)) SerializedGameState;

void serialize_gamestate(GameState* gs, uint8_t* buffer) {
    SerializedGameState* sgs = (SerializedGameState*)buffer;
    sgs->version = PROTOCOL_VERSION;
    sgs->phase = gs->phase;
    sgs->active_player = gs->active_player;
    // ... copy other fields ...
}
```

**Versioning** enables protocol evolution:

```c
bool deserialize_gamestate(uint8_t* buffer, GameState* gs) {
    SerializedGameState* sgs = (SerializedGameState*)buffer;

    if (sgs->version == 1) {
        // Old format - convert
        convert_v1_to_current(sgs, gs);
    } else if (sgs->version == PROTOCOL_VERSION) {
        // Current format - direct copy
        memcpy(gs, sgs, sizeof(SerializedGameState));
    } else {
        return false;  // Unsupported version
    }
    return true;
}
```

### 5.5 Debugging State Machines

**State logging**:

```c
void log_phase_transition(GamePhase old, GamePhase new, 
                          GameEvent event) {
    printf("[PHASE] %s --[%s]--> %s\n",
           phase_name(old), event_name(event), phase_name(new));
}
```

**State invariant checking**:

```c
void assert_gamestate_valid(GameState* gs) {
    // Energy never negative
    assert(gs->players[0].energy >= 0);
    assert(gs->players[1].energy >= 0);

    // Hand size within bounds
    assert(gs->players[0].hand_count <= MAX_HAND_SIZE);

    // Phase-specific invariants
    if (gs->phase == PHASE_COMBAT_RESOLUTION) {
        assert(gs->attack_count > 0);
    }

    // Total cards conserved
    int total = gs->players[0].hand_count + gs->players[0].deck_count
              + gs->players[0].discard_count;
    assert(total == 40);  // Initial deck size
}
```

**Transition coverage testing**:

```c
// Ensure all states are reachable
void test_state_coverage() {
    bool visited[NUM_PHASES] = {false};
    GameState gs;

    // Simulate full game
    initialize_game(&gs);
    while (gs.phase != PHASE_GAME_OVER) {
        visited[gs.phase] = true;
        simulate_one_action(&gs);
    }

    // Check coverage
    for (int i = 0; i < NUM_PHASES; i++) {
        if (!visited[i]) {
            printf("WARNING: State %s never reached\n", 
                   phase_name(i));
        }
    }
}
```

### 5.6 Performance Considerations

**Hot path optimization**: Most common transitions should be fast

```c
// Inline frequent transitions
static inline GamePhase try_fast_transition(GamePhase p, 
                                            GameEvent e) {
    // Handle 80% of transitions with simple checks
    if (p == PHASE_TURN_ACTION && e == EVENT_PLAY_CHAMPIONS) {
        return PHASE_TURN_DEFENSE;
    }
    if (p == PHASE_COMBAT_RESOLUTION && e == EVENT_COMBAT_DONE) {
        return PHASE_TURN_END_COLLECT;
    }
    return p;  // Fall back to full transition function
}
```

**State machine compiler**: Generate C code from formal specifications

```python
# transitions.spec
state SETUP:
    on EVENT_SETUP_COMPLETE -> MULLIGAN

state MULLIGAN:
    on EVENT_MULLIGAN_DONE -> TURN_DRAW if not first_turn
    on EVENT_MULLIGAN_DONE -> TURN_ACTION if first_turn

# Generate optimized C switch statement
```

**Branch prediction hints**:

```c
if (__builtin_expect(gs->phase == PHASE_TURN_ACTION, 1)) {
    // Hot path - action phase is most common
    handle_action_phase(gs);
}
```

---

## Conclusion

State machines provide a rigorous theoretical foundation for modeling game logic. For Oracle: The Champions of Arcadia:

**Key takeaways**:

1. Use **Moore machine** semantics (outputs depend only on state)
2. Implement as **Extended State Machine** (FSM + game data)
3. Represent states as **enums** for efficiency
4. Encode transitions as **switch statements** with guards
5. **Separate** orthogonal concerns (player state, phase, combat)
6. Use **hierarchical grouping** to manage complexity
7. Apply **formal verification** principles (invariant checking)

This approach yields code that is:

- **Verifiable**: Correct by construction
- **Maintainable**: States and transitions are explicit
- **Debuggable**: State history can be logged and replayed
- **Efficient**: Enum-based implementation is fast
- **Extensible**: New states/transitions are localized changes

The state machine is the **backbone** of the game engine, providing a clear contract between game logic, AI agents, and network protocol.
