#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stda_cli.h"
#include "game_types.h"
#include "mtwister.h"
#include "game_constants.h"
#include "strategy.h"
#include "strat_random.h"
#include "game_state.h"
#include "turn_logic.h"
#include "combat.h"
#include "game_state.h"

#define MAX_COMMAND_LEN 256

extern MTRand MTwister_rand_struct;

// command line interface interactive mode
int run_mode_stda_cli(config_t* cfg)
{ printf("Running in command line interface mode...\n");

  // pull here the mechanics from the automatedsim mode first, assuming at first that only one game will be played. reuse as many function of the current function calls as possible, such as
  // a call to play_game(), and make play_game() and other functions 'mode-aware / context sensitive'. When in interactive mode, use the strategy for the AI player, and ask the user (human player)
  // for decisions (actions) at various points, after prompting the user with enough information about the game state, and then provide the user with the results of combats, information about
  // decisons made by the AI player, etc.

  // Initialize random number generator: this is something that would be server side (mostly, unless an AI client needs a RNG, in which case it could have a separate one)
  MTwister_rand_struct = seedRand(M_TWISTER_SEED);

  // Initialize game statistics: this is something that would be server side once we split the code between the client and server side code. this will see the entry of only one game for the interactive mode
  struct gamestats gstats;
  memset(&gstats, 0, sizeof(struct gamestats));

  uint16_t initial_cash = INITIAL_CASH_DEFAULT;

  // Setup strategies for player B: this is something that would be client side
  // Assume for this first version of the CLI that the human player will be player A (the first player)
  StrategySet* strategies = create_strategy_set();
  set_player_strategy(strategies, PLAYER_A,
                      random_attack_strategy, random_defense_strategy);
  set_player_strategy(strategies, PLAYER_B,
                      random_attack_strategy, random_defense_strategy);

  // play the game
  gstats.simnum = 0;

  struct gamestate gstate;
  setup_game(initial_cash, &gstate);

  char input_buffer[MAX_COMMAND_LEN];
  printf("\n=== Game Start ===\n");
  // there will need to be code here to determine who is player A and who is player B if the game involves at least one interactive player (if the game is AI against AI, it may be a bit of a moot point given that we choose the strategies for each)
  // for the first version of the CLI, assume a game between an interactive player (assume they are player A), and an AI player (player B, whose strategy is selected above)

  // Apply mulligan for player B: when in interactive mode (CLI, TUI, GUI), this needs to be delegated to the user or AI to make a choice of what to mulligan (if anything)
  // apply_mulligan(&gstate); // this needs to be replaced by a question and answer to the user via the CLI (if the interactive user is player B). see apply_mulligan() proposed in oracle_cli.c proposed code in the ideas/cli folder

  gstate.turn = 0;

  // this loop needs to be executing the CLI to play an interactive game: see main() function in suggested oracle_cli.c code
  do
  { begin_of_turn(&gstate); // there is no need to get user input here

    if(gstate.current_player == PLAYER_A)  // could be replaced by 'if player to move is an interactive player, use the cli, and otherwise use the ai strategy set'
    { // use the cli to ask and receive from interactive player the attacker actions
      // add to this CLI a condition to break from the loop if the user enters the 'exit' or 'quit' command (instaed of providing info on cards to play)
      // TODO: complete the implementation in this set of brackets
      display_game_state();
      display_hand(current);

      printf("\n%s> ", current->name);

      if(fgets(input_buffer, sizeof(input_buffer), stdin) != NULL)
      { input_buffer[strcspn(input_buffer, "\n")] = 0;
        execute_command(input_buffer);
      }
      else
      { printf("Error reading input.\n");;
      }
    }
    else
    { attack_phase(&gstate, strategies);  // apply ai strategy for player B
    }


    if(gstate.combat_zone[gstate.current_player].size > 0)
    { if(gstate.current_player == PLAYER_A)   // if attacker (current player) is the interactive player, then defender is the ai in our simplified first implementation of the cli. could be replaced by 'if player to move is an interactive player, use the cli, and otherwise use the ai strategy set'
        defense_phase(&gstate, strategies);
      else // the defender is the interactive player (player A)
      { // use the cli to ask and receive from interactive player the defender actions
        // add to this CLI a condition to break from the loop if the user enters the 'exit' or 'quit' command (instead of providing info on defending champions)
        // TODO: complete the implementation in this set of brackets, using as inspiration the code in the corresponding section of the attack phase
      }

      resolve_combat(&gstate);
    }

    if(gstate.someone_has_zero_energy)
    { break; // Game ended
    }

    // end of turn actions
    collect_1_luna(&gstate);

    //discard_to_7_cards(&gstate); // this needs to be replaced by a question and answer to the attacker (current player), and is only applicable if there are more than 7 cards in their hand
    // add to this CLI a condition to break from the loop if the user enters the 'exit' or 'quit' command (instead of providing the card IDs to discard)

    change_current_player(&gstate);
  }
  while(gstate.turn < MAX_NUMBER_OF_TURNS && !gstate.someone_has_zero_energy);

  // game has ended
  if(!gstate.someone_has_zero_energy)
    gstate.game_state = DRAW;

  // add code here to display final outcome of the game that was just played (we likely simply want a nice presentation of the gamestate)
  // present gstate->game_state
  // present gstate->turn (as that's the turn number that corresponds to the end of the game)
  // present the round number that corresponds to the turn number
  // could present the energy left for the winning player, as well as the cash left for both players

  // Cleanup (counterpart to initialization strategies struct earlier)
  // Free heap memory
  DeckStk_emptyOut(&gstate.deck[PLAYER_A]);
  DeckStk_emptyOut(&gstate.deck[PLAYER_B]);
  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_A]);
  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_B]);
  HDCLL_emptyOut(&gstate.hand[PLAYER_A]);
  HDCLL_emptyOut(&gstate.hand[PLAYER_B]);
  HDCLL_emptyOut(&gstate.discard[PLAYER_A]);
  HDCLL_emptyOut(&gstate.discard[PLAYER_B]);

  free_strategy_set(strategies);

  return EXIT_SUCCESS;
} // run_mode_stda_cli
