// turn_logic.h
// Turn flow and phase management

#ifndef TURN_LOGIC_H
#define TURN_LOGIC_H

#include "game_types.h"
#include "strategy.h"

// Main turn function
void play_turn(struct gamestats* gstats, struct gamestate* gstate,
               StrategySet* player_strategies, GameContext* ctx);

// Turn phase functions
void begin_of_turn(struct gamestate* gstate, GameContext* ctx);
void end_of_turn(struct gamestate* gstate, GameContext* ctx);
void attack_phase(struct gamestate* gstate, StrategySet* strategies, GameContext* ctx);
void defense_phase(struct gamestate* gstate, StrategySet* strategies, GameContext* ctx);

#endif // TURN_LOGIC_H
