// turn_logic.c
// Turn flow and phase management implementation
#include <stdio.h>

#include "turn_logic.h"
#include "card_actions.h"
#include "combat.h"
#include "game_context.h"
#include "debug.h"
#include "game_state.h"

void play_turn(struct gamestats* gstats, struct gamestate* gstate,
               StrategySet* player_strategies, GameContext* ctx) // need to accept cfg pointer to provide (at least) game mode information (eg auto vs interactive modes)
{ begin_of_turn(gstate, ctx);

  attack_phase(gstate, player_strategies, ctx);  // need to pass cfg pointer to provide game mode information

  if(gstate->combat_zone[gstate->current_player].size > 0)
  { defense_phase(gstate, player_strategies, ctx);  // need to pass cfg pointer to provide game mode information
    resolve_combat(gstate, ctx);  // need to pass cfg pointer to provide game mode information
  }

  if(gstate->someone_has_zero_energy)
  { return; // Game ended
  }

  end_of_turn(gstate, ctx); // need to pass cfg pointer to provide game mode information
} // play_turn

void begin_of_turn(struct gamestate* gstate, GameContext* ctx)
{ gstate->turn++;
  gstate->turn_phase = ATTACK;  // TODO: may eventually need to look into whethere or not this line of code should be moved to become the very last instruction in the end_of_turn() function. this is likely to matter only when come the time to implement the MCTS AI engines, due to the nature of the strategy sets that may be required to allow the MCTS to recursively traverse the tree
  gstate->player_to_move = gstate->current_player; // TODO: look into whether the change discussed above for the turn_phase should extend to the player_to_move as well

  // Draw 1 card (except first player on turn 1)
  if(!(gstate->turn == 1 && gstate->current_player == PLAYER_A))
    draw_1_card(gstate, gstate->current_player, ctx);

  DEBUG_PRINT(" Begin round %.4u, turn %.4u\n",
           (uint16_t)((gstate->turn-1) * 0.5)+1, gstate->turn);
  
} // begin_of_turn

void attack_phase(struct gamestate* gstate, StrategySet* strategies, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;

  // Call strategy function to make attack decision
  strategies->attack_strategy[attacker](gstate, ctx);

  gstate->turn_phase = DEFENSE;  // TODO: may eventually need to look into whether or not this line of code should be moved inside the attack_strategy function (as the last line of code), when we implement the Monte-Carlo Tree Search AI engines (because of their tree recursive traversal search structure) also see comment at the beginning of the begin_of_turn() function about the timing of when we switch the turn phase back to attack
  gstate->player_to_move = 1 - gstate->current_player; // TODO: look into whether the change discussed above for the turn_phase should extend to the player_to_move as well
}

void defense_phase(struct gamestate* gstate, StrategySet* strategies, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  // Only defend if there's combat
  if(gstate->combat_zone[gstate->current_player].size > 0)  // this check is likely not necessary since it is already done prior to calling the defense_phase function
    strategies->defense_strategy[defender](gstate, ctx);
}

void end_of_turn(struct gamestate* gstate, GameContext* ctx) // need to accept cfg pointer to provide game mode information
{ collect_1_luna(gstate);
  discard_to_7_cards(gstate, ctx); // need to pass cfg pointer to provide game mode information
  change_current_player(gstate);

  DEBUG_PRINT(" End round %.4u, turn %.4u\n",
           (uint16_t)((gstate->turn-2) * 0.5)+1, gstate->turn-1);
  DEBUG_PRINT(" Turn ended: %d A, %d B cash; %d A, %d B energy\n",
           gstate->current_cash_balance[PLAYER_A],
           gstate->current_cash_balance[PLAYER_B],
           gstate->current_energy[PLAYER_A],
           gstate->current_energy[PLAYER_B]);
} // end_of_turn
