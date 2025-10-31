// strat_hbt.h
// Heuristic-Balanced-Tactical Hybrid Strategy
// Combines systematic move evaluation with principled resource management
// and adaptive tactical awareness

#ifndef STRAT_HBT_H
#define STRAT_HBT_H

#include "game_types.h"
#include "game_context.h"

// Game phase assessment (from Tactical)
typedef enum {
    PHASE_EARLY,    // Energy > 75
    PHASE_MID,      // Energy 40-75  
    PHASE_LATE,     // Energy < 40
    PHASE_CRITICAL  // Energy < 15
} GamePhase;

// Move representation (from Heuristic)
typedef enum {
    MOVE_PASS,
    MOVE_CHAMPIONS,
    MOVE_DRAW_CARD,
    MOVE_CASH_CARD
} MoveType;

typedef struct {
    MoveType type;
    uint8_t num_champions;      // For MOVE_CHAMPIONS
    uint8_t champion_ids[3];    // Card indices from hand
    uint8_t special_card_id;    // For MOVE_DRAW_CARD or MOVE_CASH_CARD
    float expected_advantage;   // Calculated heuristic value
    int total_cost;             // Total luna cost
} Move;

// Strategic state assessment (from Tactical + Balanced)
typedef struct {
    GamePhase my_phase;
    GamePhase opp_phase;
    
    // Resource tracking
    float my_hand_power;
    float opp_estimated_power;
    int my_effective_cards;
    int opp_estimated_cards;
    
    // Balanced rules targets
    float target_cash_reserve;
    float target_effective_cards;
    int cash_available_to_spend;
    int cards_available_to_play;
    
    // Tactical assessment
    float aggression_factor;    // 0.0 (defensive) to 1.0 (aggressive)
    
    // Dynamic advantage weights (tuned based on game state)
    float epsilon_energy;       // Weight for energy advantage
    float gamma_cards;          // Weight for card advantage  
    float delta_cash;           // Weight for cash advantage
} StrategicState;

// Main strategy entry points
void hbt_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void hbt_defense_strategy(struct gamestate* gstate, GameContext* ctx);

// Core evaluation functions
StrategicState evaluate_hbt_state(struct gamestate* gstate, PlayerID player);
float calculate_hbt_advantage(struct gamestate* gstate, PlayerID player, 
                               StrategicState* state);

// Move generation and evaluation
int generate_viable_attack_moves(struct gamestate* gstate, PlayerID player,
                                 StrategicState* state, Move* moves);
Move select_best_move(struct gamestate* gstate, PlayerID player,
                     Move* moves, int num_moves, StrategicState* state,
                     GameContext* ctx);

// Helper functions
GamePhase get_game_phase(uint8_t energy);
float calculate_hand_power(struct HDCLList* hand);
float estimate_opponent_power(struct gamestate* gstate, PlayerID opponent);
int calculate_effective_cards(struct gamestate* gstate, PlayerID player);
float calculate_aggression_factor(StrategicState* state, struct gamestate* gstate,
                                  PlayerID player);

#endif // STRAT_HBT_H
