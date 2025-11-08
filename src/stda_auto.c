#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stda_auto.h"
#include "game_types.h"
#include "mtwister.h"
#include "game_constants.h"
#include "strategy.h"
#include "strat_random.h"
#include "game_state.h"
#include "turn_logic.h"
#include "card_actions.h"
#include "debug.h"

//extern MTRand MTwister_rand_struct;

// Standalone Auto mode code
int run_mode_stda_auto(config_t* cfg)
{ // Create game context
  GameContext* ctx = create_game_context(cfg->prng_seed, cfg);
  if(ctx == NULL)
  { fprintf(stderr, "Failed to create game context\n");
    return EXIT_FAILURE;
  }

  // Initialize game statistics: this is something that would be server side once we split the code between the client and server side code
  struct gamestats gstats;
  memset(&gstats, 0, sizeof(struct gamestats));

  // Simulation parameters: this is something that is specific to a 'simulation' mode (stda.sim or client.sim for interactive simulation or stda.auto for automated simulation)
  uint16_t numsim = (cfg->numsim > 0) ? oraclemin(cfg->numsim, MAX_NUMBER_OF_SIM) : MAX_NUMBER_OF_SIM;
  uint16_t initial_cash = INITIAL_CASH_DEFAULT;

  // Setup strategies for both players: this is something that would be client side
  StrategySet* strategies = create_strategy_set();
  set_player_strategy(strategies, PLAYER_A,
                      random_attack_strategy, random_defense_strategy);
  set_player_strategy(strategies, PLAYER_B,
                      random_attack_strategy, random_defense_strategy);

  // Run simulation: this is something that is specific to simulation mode (in this specific case, for the CLI only application, it's the automated simulation stda.auto)
  run_simulation(numsim, initial_cash, &gstats, strategies, ctx);
  present_results(&gstats);

  // Cleanup (counterpart to initialization strategies struct earlier)
  free_strategy_set(strategies);
  destroy_game_context(ctx);

  return EXIT_SUCCESS;
} // end of standalone auto mode code

void run_simulation(uint16_t numsim, uint16_t initial_cash,
                    struct gamestats* gstats, StrategySet* strategies, GameContext* ctx)
{ for(gstats->simnum = 0; gstats->simnum < numsim; gstats->simnum++)
  { DEBUG_PRINT("Begin game %.4u\n", gstats->simnum);
    play_stda_auto_game(initial_cash, gstats, strategies, ctx);
    DEBUG_PRINT("End game %.4u\n\n", gstats->simnum);
  }
}

void play_stda_auto_game(uint16_t initial_cash, struct gamestats* gstats,
                         StrategySet* strategies, GameContext* ctx)  // need to accept a *cfg here so as to use later on
{ struct gamestate gstate;
  setup_game(initial_cash, &gstate, ctx);

  // Apply mulligan for player B: when in interactive mode (CLI, TUI, GUI), this needs to be delegated to the user or AI to make a choice of what to mulligan (if anything)
  apply_mulligan(&gstate, ctx);

  DEBUG_PRINT("Game started with %d A, %d B cash; %d A, %d B energy\n",
              gstate.current_cash_balance[PLAYER_A],
              gstate.current_cash_balance[PLAYER_B],
              gstate.current_energy[PLAYER_A],
              gstate.current_energy[PLAYER_B]);


  gstate.turn = 0;

  do
  { play_turn(gstats, &gstate, strategies, ctx); // need to pass cfg pointer to provide game mode information
  }
  while(gstate.turn < MAX_NUMBER_OF_TURNS && !gstate.someone_has_zero_energy);

  if(!gstate.someone_has_zero_energy)
    gstate.game_state = DRAW;

  DEBUG_PRINT("Game ended at round %.4u, turn %.4u, winner is %s\n",
              (uint16_t)((gstate.turn-1) * 0.5)+1,
              gstate.turn,
              GAME_STATE_NAMES[gstate.game_state]);


  record_final_stats(gstats, &gstate); // need to pass cfg pointer to provide game mode information

  // Free heap memory
  DeckStk_emptyOut(&gstate.deck[PLAYER_A]);
  DeckStk_emptyOut(&gstate.deck[PLAYER_B]);
  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_A]);
  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_B]);
  HDCLL_emptyOut(&gstate.hand[PLAYER_A]);
  HDCLL_emptyOut(&gstate.hand[PLAYER_B]);
  HDCLL_emptyOut(&gstate.discard[PLAYER_A]);
  HDCLL_emptyOut(&gstate.discard[PLAYER_B]);
} // play_game

void apply_mulligan(struct gamestate* gstate, GameContext* ctx)
{ uint8_t max_nbr_cards_to_mulligan = 2;

  // Count cards to mulligan
  struct LLNode* current = gstate->hand[PLAYER_B].head;
  uint8_t nbr_cards_to_mulligan = 0;

  for(uint8_t i = 0; (i < gstate->hand[PLAYER_B].size) &&
      (nbr_cards_to_mulligan < max_nbr_cards_to_mulligan); i++)
  { if(fullDeck[current->data].power < AVERAGE_POWER_FOR_MULLIGAN)
      nbr_cards_to_mulligan++;
    current = current->next;
  }

  DEBUG_PRINT("Number of cards to mulligan: %u\n", nbr_cards_to_mulligan);

  // Discard lowest power cards
  float minpower;
  uint8_t card_with_lowest_power;
  uint8_t nbr_cards_left_to_mulligan = nbr_cards_to_mulligan;

  while(nbr_cards_left_to_mulligan > 0)
  { minpower = 100.0;
    card_with_lowest_power = 0;
    current = gstate->hand[PLAYER_B].head;

    for(uint8_t i = 0; i < gstate->hand[PLAYER_B].size; i++)
    { if(fullDeck[current->data].power < minpower)
      { minpower = fullDeck[current->data].power;
        card_with_lowest_power = current->data;
      }
      current = current->next;
    }

    HDCLL_removeNodeByValue(&gstate->hand[PLAYER_B], card_with_lowest_power);
    HDCLL_insertNodeAtBeginning(&gstate->discard[PLAYER_B], card_with_lowest_power);
    nbr_cards_left_to_mulligan--;
  }

  // Draw replacement cards
  for(uint8_t i = 0; i < nbr_cards_to_mulligan; i++)
    draw_1_card(gstate, PLAYER_B, ctx);
} // apply_mulligan

void record_final_stats(struct gamestats* gstats, struct gamestate* gstate) // need to accept cfg pointer to see game mode information
{ // the following code works well in automated simulation mode. when building the 'interactive' modes (CLI, TUI, GUI), consider directly exporting the stats to a game stats database (actual file) instead
  switch(gstate->game_state)
  { case PLAYER_A_WINS:
      ++gstats->cumul_player_wins[PLAYER_A];
      break;
    case PLAYER_B_WINS:
      ++gstats->cumul_player_wins[PLAYER_B];
      break;
    case DRAW:
      ++gstats->cumul_number_of_draws;
      break;
    case ACTIVE:
      break;
  }

  gstats->game_end_turn_number[gstats->simnum] = gstate->turn;
} // record_final_stats

/* Histogram preparation:
    Total bins: The NUM_BINS includes special underflow and overflow bins, resulting in a total of NUM_BINS + 2 bins in the histogram array.
    Underflow bin (Index 0): Any value in the raw data that is less than MIN_VALUE will be counted in this special bin.
    Overflow bin (Index NUM_BINS + 1): Any value in the raw data that is greater than or equal to the maximum valid value (MIN_VALUE + NUM_BINS * BIN_WIDTH) will be counted in this bin.
    Standard bins (Index 1 to NUM_BINS): The original range of bins now starts at index 1 and ends at index NUM_BINS.
    Calculations: The logic for assigning a data point to a bin checks for the underflow and overflow conditions before calculating the index for the standard bins.
      The size of the histogram_array should reflect that.
    Printout changes: The output section is updated to properly label and display the results for the special underflow and overflow bins.
*/

// Hardcoded histogram parameters
#define NUM_BINS 27
#define BIN_WIDTH 4
#define MIN_VALUE 20

// Define indices for the special bins
#define UNDERFLOW_BIN 0
#define OVERFLOW_BIN (NUM_BINS + 1)
#define TOTAL_BINS (NUM_BINS + 2)

// Function to create a histogram array from unsigned int data
void createHistogram(uint16_t data[], uint16_t data_size, uint16_t histogram_array[])
{ // Initialize the histogram array with zeros
  for(uint16_t i = 0; i < TOTAL_BINS; i++)
    histogram_array[i] = 0;

  // Define the upper boundary of the standard bins
  uint16_t max_valid_value = MIN_VALUE + (NUM_BINS * BIN_WIDTH);

  // Process each data point
  for(uint16_t i = 0; i < data_size; i++)
  { uint16_t value = data[i];

    if(value < MIN_VALUE)
    { // Assign to the underflow bin
      histogram_array[UNDERFLOW_BIN]++;
    }
    else if(value >= max_valid_value)
    { // Assign to the overflow bin
      histogram_array[OVERFLOW_BIN]++;
    }
    else
    { // Calculate the bin index for standard bins
      uint8_t bin_index = (value - MIN_VALUE) / BIN_WIDTH;
      // The standard bins start at index 1
      histogram_array[bin_index + 1]++;
    }
  }
} // createHistogram

void present_results(struct gamestats* gstats)
{ printf("Number of wins for player A: %u\n", gstats->cumul_player_wins[PLAYER_A]);
  printf("Number of wins for player B: %u\n", gstats->cumul_player_wins[PLAYER_B]);
  printf("Number of draws: %u\n", gstats->cumul_number_of_draws);

  uint16_t minNbrTurn = MAX_NUMBER_OF_TURNS;
  uint16_t maxNbrTurn = 0;
  uint32_t totalNbrTurn = 0;

  for(uint16_t s = 0; s < gstats->simnum; s++)
  { minNbrTurn = oraclemin(minNbrTurn, gstats->game_end_turn_number[s]);
    maxNbrTurn = oraclemax(maxNbrTurn, gstats->game_end_turn_number[s]);
    totalNbrTurn += gstats->game_end_turn_number[s];
  }

  printf("\nAverage = %.1f, Minimum = %u, Maximum = %d number of turns per game\n",
         (float)totalNbrTurn / (float)gstats->simnum, minNbrTurn, maxNbrTurn);

  // compute and display histogram
  { // Array to store the histogram counts. Size is total number of bins.
    uint16_t histogram[TOTAL_BINS];

    // Create the histogram
    createHistogram(gstats->game_end_turn_number, gstats->simnum, histogram);

    // Print the histogram results
    printf("\nHistogram with %d bins, each with a width of %d, starting from %u:\n", NUM_BINS, BIN_WIDTH, MIN_VALUE);
    printf("Bin (<%3u): %d\n", MIN_VALUE, histogram[UNDERFLOW_BIN]);

    for(uint8_t i = 0; i < NUM_BINS; i++)
    { uint16_t bin_start = MIN_VALUE + (i * BIN_WIDTH);
      uint16_t bin_end = bin_start + BIN_WIDTH - 1;
      printf("Bin [%3u - %3u]: %d\n", bin_start, bin_end, histogram[i + 1]);
    }

    printf("Bin (>=%3u): %d\n", MIN_VALUE + (NUM_BINS * BIN_WIDTH), histogram[OVERFLOW_BIN]);
  } // histogram section

} // present_results
