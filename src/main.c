// main.c
// Oracle: The Champions of Arcadia - Main entry point

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_types.h"
#include "game_constants.h"
#include "game_state.h"
#include "strategy.h"
#include "strat_random.h"
#include "mtwister.h"

#define M_TWISTER_SEED 1337  // we may consider moving this define to game_constants.h to have all of those in the same place

// Global variables
bool debug_enabled = false;  // we may consider moving this line to game_constants.h to have all of them in the same place, and in fact we could likely declare it as const
extern MTRand MTwister_rand_struct;

int main()
{ 
  // Note: command line argument parsing could be based on 'if-else if ladder with strcmp', esp. if the command line arguments are strings longer than 1 char
  
  run_chosen_mode();
  
  return EXIT_SUCCESS;
} // main

void run_chosen_mode()
{
  // use parsed command line arguments to decide which mode to launch.
  runmode_standalone_automatedsim(); // of course this will be based on command line arguments that will specify which mode to launch
}

// Standalone Auto mode code
void runmode_standalone_automatedsim()
{    // Initialize random number generator: this is something that would be server side (mostly, unless an AI client needs a RNG, in which case it could have a separate one)
  MTwister_rand_struct = seedRand(M_TWISTER_SEED);

  // Initialize game statistics: this is something that would be server side once we split the code between the client and server side code
  struct gamestats gstats;
  memset(&gstats, 0, sizeof(struct gamestats));

  // Simulation parameters: this is something that is specific to a 'simulation' mode (stda.sim or client.sim for interactive simulation or stda.auto for automated simulation)
  uint16_t numsim = debug_enabled ? DEBUG_NUMBER_OF_SIM : MAX_NUMBER_OF_SIM;
  uint16_t initial_cash = 30;

  // Setup strategies for both players: this is something that would be client side
  StrategySet* strategies = create_strategy_set();
  set_player_strategy(strategies, PLAYER_A,
                      random_attack_strategy, random_defense_strategy);
  set_player_strategy(strategies, PLAYER_B,
                      random_attack_strategy, random_defense_strategy);

  // Run simulation: this is something that is specific to simulation mode (in this specific case, for the CLI only application, it's the automated simulation stda.auto)
  run_simulation(numsim, initial_cash, &gstats, strategies);  
  present_results(&gstats);

  // Cleanup (counterpart to initialization strategies struct earlier)
  free_strategy_set(strategies);
} // end of standalone auto mode code
