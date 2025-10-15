// turn_logic.c
// Turn flow and phase management implementation

#include "turn_logic.h"
#include "card_actions.h"
#include "combat.h"
#include <stdio.h>

extern bool debug_enabled;

void play_turn(struct gamestats* gstats, struct gamestate* gstate,
               StrategySet* player_strategies)
{ begin_of_turn(gstate);

  attack_phase(gstate, player_strategies);

  if(gstate->combat_zone[gstate->current_player].size > 0)
  { defense_phase(gstate, player_strategies);
    resolve_combat(gstate);
  }

  if(gstate->someone_has_zero_energy)
  { return; // Game ended
  }

  end_of_turn(gstate);
}

void begin_of_turn(struct gamestate* gstate)
{ gstate->turn++;
  gstate->turn_phase = ATTACK;
  gstate->player_to_move = gstate->current_player;

  // Draw 1 card (except first player on turn 1)
  if(!(gstate->turn == 1 && gstate->current_player == PLAYER_A))
    draw_1_card(gstate, gstate->current_player);

  if(debug_enabled)
  { printf(" Begin round %.4u, turn %.4u\n",
           (uint16_t)((gstate->turn-1) * 0.5)+1, gstate->turn);
  }
}

void end_of_turn(struct gamestate* gstate)
{ collect_1_luna(gstate);
  discard_to_7_cards(gstate);
  change_current_player(gstate);

  if(debug_enabled)
  { printf(" End round %.4u, turn %.4u\n",
           (uint16_t)((gstate->turn-2) * 0.5)+1, gstate->turn-1);
    printf(" Turn ended: %d A, %d B cash; %d A, %d B energy\n",
           gstate->current_cash_balance[PLAYER_A],
           gstate->current_cash_balance[PLAYER_B],
           gstate->current_energy[PLAYER_A],
           gstate->current_energy[PLAYER_B]);
  }
}

void attack_phase(struct gamestate* gstate, StrategySet* strategies)
{ PlayerID attacker = gstate->current_player;

  // Call strategy function to make attack decision
  strategies->attack_strategy[attacker](gstate);

  gstate->turn_phase = DEFENSE;
  gstate->player_to_move = 1 - gstate->current_player;
}

void defense_phase(struct gamestate* gstate, StrategySet* strategies)
{ PlayerID defender = 1 - gstate->current_player;

  // Only defend if there's combat
  if(gstate->combat_zone[gstate->current_player].size > 0)
    strategies->defense_strategy[defender](gstate);
}
