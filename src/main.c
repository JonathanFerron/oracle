/* License: GNU GPL 3.0, see gnu-gpl-v3.0.md file in this project */

/* 
To Do:
  1. Implement 'discard to 7 cards' rule. 
     Test first player advantage again.
     Implement mulligan and test if that resolves any first player advantage.
  
  2. Make small 'improvement' to 'random' strategy to make it play a champion card in the defense phase only x% of the time. x% should be based on information available at the
       decision time on hand size, cash left and opponent energy.
  
  3. Bundle some functionalities into 'begin_of_turn()' and 'end_of_turn()' helper functions, and move turn++ from the playgame() function to the playturn() function.   
   
  4. Further delegate work done in the playturn() function to helper functions to reduce number of lines of codes to about 30    
  
  5. Implement a ncurses based text user interface to allow two human players to interactively play against each other or a human to play against the 'AI': first screen should ask what mode
       the program should run in: simulation vs interactive, and then for interactive present the table on the left and a console on the right, for simulation present a console
       on the right, output on the left and parameters at the bottom left, with ability to export results to txt files.

  6. enhance decision rules to mimic what a smart player would do, and find what could be more optimal 
       decision rules, probably using heuristics to keep things simple. See notes in balanced rules strategy source file: strat_balancedrules1.c

  7. Monte Carlo Single Stage Analysis:
      manually create 100 distinct 'attack' phase game states at various stages of the game.
        for each game state, use the MonteCarloSingleStageAnalysis strategy in the 'applyAttackStragegy' function, which will:
        make a list of all possible moves by player A (Nm, maximum of 93 moves): getAvailableMoves(). need to create a structure that will be able to represent a 'move'      
        perform 100 simulations with all of the possible candidate moves: for each simulation
          make a clone of the root gamestate and randomize in this clone the information not seen by player A at this stage: clone_and_randomize_gamestate() (which can use the clone_gamestate() function)
          for each possible move
            make a clone of the randomized copy from last step above, to test this move
            assume that first move is made (apply it to the new clone) and then 
            randomly make moves 2+ (among legal moves) for each player alternately until 
            player A wins (1 point), loses (0), or there is a draw (0.5 points). 
           
        discard worst candidates, keeping Nm ^ (¾) moves (max 30). 
        perform 200 more sim with the remaining candidate moves (same approach as above)
        
        then prune to Nm ^ (½) moves (max 10), 
        perform 400 more sim, 
        
        then prune to Nm  ^ (¼) best moves (max 4). 
        perform 800 more sims (cumulative total of 1500 sims) 
        
        display results for 4 best moves to the console. 
        return the best move
      using a separate tool, analyse 4 best moves for each of the 100 game states to develop rules / decision tree / heuristics that would have generated the same best moves.

  8. Info Set Monte Carlo Tree Search:
        define this strategy as a call to the ISMCTS() function, from the applyAttDefStrat() function
        make a clone of the root gamestate as provided by the call from applyAttDefStrat(): clone_gamestate()
        apply ISMCTS to the clone gamestate, and return the best move: 
          - clone gamestate will provide the 'playerToMove' data field
          - implement a new Clone_And_Randomize_gamestate(observer) function : Create a copy of the state, then determinize the hidden information from observer's point of view.
          - implement a new GetResult(player) function: If the state is terminal, return 1 if the given player has won, 0 if not, 0.5 for a draw. If the state is not 
              terminal, the result is undefined. This should be trivial given the gamestate structure.
          - make use of the 'getAvailableMoves()' function:  Get a list of the legal moves from this state, or return an empty list if the state is terminal.
          - implement a new DoMove(move) function: Apply the given move to the state, and update playerToMove to specify whose turn is next and turn_phase to specify what the
              new phase is (attack or defense). Controlable by the AI with one call to DoMove() for the attacker followed by another call to DoMove() for the defender (if there is combat), etc.

*/

// includes
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"  // this includes the function prototypes
#include "mtwister.h"
#include "deckstack.h"
#include "hdcll.h"
#include "rnd.h"

// defines (constant scalars)
#define M_TWISTER_SEED 1337 // seed for pseudo-random number generator

// pre-compiler constants
#define MAX_NUMBER_OF_TURNS 500
#define MAX_NUMBER_OF_SIM 1000

#define FULL_DECK_SIZE 117 // update to 120 once 3 cash cards have been added

// macros
#define max(a,b) \
       ({ typeof (a) _a = (a); \
           typeof (b) _b = (b); \
         _a > _b ? _a : _b; })
         
#define min(a,b) \
       ({ typeof (a) _a = (a); \
           typeof (b) _b = (b); \
         _a < _b ? _a : _b; })

// global constants (constant arrays and struct)

// Constants (game description):
enum PlayerID {
  PLAYER_A = 0,
  PLAYER_B = 1
};

const char *const PLAYER_NAMES[] = {"PLAYER A", "PLAYER B"};

enum GameState {
  PLAYER_A_WINS=0, 
  PLAYER_B_WINS=1,
  DRAW=2,
  ACTIVE=3  
};

const char *const GAME_STATE_NAMES[] = {"PLAYER A WINS", "PLAYER B WINS", "DRAW", "ACTIVE"};

enum TurnPhase {
  ATTACK,
  DEFENSE
};

const char *const TURN_PHASE_NAMES[] = {"ATTACK", "DEFENSE"};

enum CardType {
  CHAMPION_CARD,
  DRAW_CARD,
  CASH_CARD
};

const char *const CARD_TYPE_SHORT_NAMES[] = {"CHAM", "DRAW", "CASH"};

enum ChampionColor {
    COLOR_RED,     
    COLOR_INDIGO,  
    COLOR_ORANGE,
    COLOR_NOT_APPLICABLE
};

const char *const CHAMPION_COLOR_NAMES[] = {"RED", "INDIGO", "ORANGE", "NA"};

enum ChampionSpecies {
    SPECIES_HUMAN,
    SPECIES_ELF,
    SPECIES_DWARF,
    SPECIES_ORC,
    SPECIES_GOBLIN,
    SPECIES_DRAGON,
    SPECIES_HOBBIT,
    SPECIES_CENTAUR,
    SPECIES_MINOTAUR,
    SPECIES_AVEN,
    SPECIES_CYCLOPS,
    SPECIES_FAUN,
    SPECIES_FAIRY,
    SPECIES_KOATL,
    SPECIES_LYCAN,
    SPECIES_NOT_APPLICABLE
} ;

// Card structure: add info necessary to hold details of a 'cash' card, and consider storing statically the efficiency information for champion cards
struct card {
  // applicable to both types of cards
  enum CardType card_type;  
  uint8_t cost;
  
  // applicable to champion cards
  uint8_t champion_id;  // champion ID (1 to 102) : default to 0 for non-champion cards
  uint8_t defense_dice;
  uint8_t attack_base;        
  enum ChampionColor color;
  enum ChampionSpecies species;
  
  // applicable to draw cards
  uint8_t draw_num; // how many cards to draw from the deck (default to 0 for champion cards)
  uint8_t choose_num; // how many cards to choose from the discard
  
  // expected attack
  // expected defense
  // attack efficiency: use an arbitrary 'cost' number for cards with 0 cost, such as 0.1
  // defense efficiency
  // power
  
  // specs for 'exchange champion for lunas' cards
};

// consider storing this statically as string in the 'fullDeck' array
void print_card_details(struct card c)
{
	// CHAM 0Luna d4+0
	// CHAM 0Luna d6+0
	// DRAW 1Luna Draw2 Choose1
  //c.card_type;
  //c.cost;
  //c.defense_dice;
  //c.attack_base;
  
  //c.draw_num;
  //c.choose_num;
}

// define 117 cards here: add 3 'cash' cards at the end
// deck including all of the 117 cards, e.ge fullDeck[0] will give the first card struct
struct card fullDeck[FULL_DECK_SIZE] = {
  {CHAMPION_CARD, 0, 1, 4, 0, COLOR_ORANGE, SPECIES_HUMAN, 0, 0},  // fullDeck[0]
  {CHAMPION_CARD, 0, 2, 6, 0, COLOR_ORANGE, SPECIES_HOBBIT, 0, 0},
  {CHAMPION_CARD, 0, 3, 4, 1, COLOR_ORANGE, SPECIES_ORC, 0, 0},
  {CHAMPION_CARD, 1, 4, 8, 0, COLOR_ORANGE, SPECIES_HUMAN, 0, 0},
  {CHAMPION_CARD, 1, 5, 6, 1, COLOR_ORANGE, SPECIES_DRAGON, 0, 0},
  {CHAMPION_CARD, 1, 6, 4, 2, COLOR_ORANGE, SPECIES_AVEN, 0, 0},
  {CHAMPION_CARD, 1, 7, 8, 1, COLOR_ORANGE, SPECIES_HOBBIT, 0, 0},
  {CHAMPION_CARD, 1, 8, 6, 2, COLOR_ORANGE, SPECIES_HOBBIT, 0, 0},
  {CHAMPION_CARD, 1, 9, 4, 3, COLOR_ORANGE, SPECIES_ORC, 0, 0},
  {CHAMPION_CARD, 1, 10, 12, 0, COLOR_ORANGE, SPECIES_DRAGON, 0, 0},
  {CHAMPION_CARD, 1, 11, 4, 4, COLOR_ORANGE, SPECIES_AVEN, 0, 0},
  {CHAMPION_CARD, 1, 12, 8, 2, COLOR_ORANGE, SPECIES_HUMAN, 0, 0},
  {CHAMPION_CARD, 1, 13, 6, 3, COLOR_ORANGE, SPECIES_HUMAN, 0, 0},
  {CHAMPION_CARD, 1, 14, 6, 4, COLOR_ORANGE, SPECIES_ORC, 0, 0},
  {CHAMPION_CARD, 1, 15, 12, 1, COLOR_ORANGE, SPECIES_DRAGON, 0, 0},
  {CHAMPION_CARD, 1, 16, 4, 5, COLOR_ORANGE, SPECIES_AVEN, 0, 0},
  {CHAMPION_CARD, 1, 17, 8, 3, COLOR_ORANGE, SPECIES_DRAGON, 0, 0},  // fullDeck[16]
  {CHAMPION_CARD, 2, 18, 8, 4, COLOR_ORANGE, SPECIES_ORC, 0, 0},
  {CHAMPION_CARD, 2, 19, 6, 5, COLOR_ORANGE, SPECIES_HOBBIT, 0, 0},
  {CHAMPION_CARD, 2, 20, 12, 2, COLOR_ORANGE, SPECIES_DRAGON, 0, 0},
  {CHAMPION_CARD, 2, 21, 4, 6, COLOR_ORANGE, SPECIES_ORC, 0, 0},
  {CHAMPION_CARD, 2, 22, 8, 5, COLOR_ORANGE, SPECIES_HOBBIT, 0, 0},
  {CHAMPION_CARD, 2, 23, 6, 6, COLOR_ORANGE, SPECIES_HUMAN, 0, 0},
  {CHAMPION_CARD, 2, 24, 12, 3, COLOR_ORANGE, SPECIES_AVEN, 0, 0},
  {CHAMPION_CARD, 2, 25, 20, 0, COLOR_ORANGE, SPECIES_DRAGON, 0, 0},
  {CHAMPION_CARD, 2, 26, 12, 4, COLOR_ORANGE, SPECIES_ORC, 0, 0},
  {CHAMPION_CARD, 2, 27, 8, 6, COLOR_ORANGE, SPECIES_HOBBIT, 0, 0},
  {CHAMPION_CARD, 3, 28, 20, 1, COLOR_ORANGE, SPECIES_HUMAN, 0, 0},
  {CHAMPION_CARD, 3, 29, 12, 5, COLOR_ORANGE, SPECIES_DRAGON, 0, 0},
  {CHAMPION_CARD, 3, 30, 20, 2, COLOR_ORANGE, SPECIES_AVEN, 0, 0},
  {CHAMPION_CARD, 3, 31, 12, 6, COLOR_ORANGE, SPECIES_ORC, 0, 0},
  {CHAMPION_CARD, 3, 32, 20, 3, COLOR_ORANGE, SPECIES_HOBBIT, 0, 0},
  {CHAMPION_CARD, 3, 33, 20, 4, COLOR_ORANGE, SPECIES_HUMAN, 0, 0},
  {CHAMPION_CARD, 3, 34, 20, 5, COLOR_ORANGE, SPECIES_AVEN, 0, 0},    // fullDeck[33]
  {CHAMPION_CARD, 0, 35, 4, 0, COLOR_RED, SPECIES_ELF, 0, 0},
  {CHAMPION_CARD, 0, 36, 6, 0, COLOR_RED, SPECIES_FAUN, 0, 0},
  {CHAMPION_CARD, 0, 37, 4, 1, COLOR_RED, SPECIES_GOBLIN, 0, 0},
  {CHAMPION_CARD, 1, 38, 8, 0, COLOR_RED, SPECIES_ELF, 0, 0},
  {CHAMPION_CARD, 1, 39, 6, 1, COLOR_RED, SPECIES_CYCLOPS, 0, 0},
  {CHAMPION_CARD, 1, 40, 4, 2, COLOR_RED, SPECIES_KOATL, 0, 0},
  {CHAMPION_CARD, 1, 41, 8, 1, COLOR_RED, SPECIES_FAUN, 0, 0},
  {CHAMPION_CARD, 1, 42, 6, 2, COLOR_RED, SPECIES_FAUN, 0, 0},
  {CHAMPION_CARD, 1, 43, 4, 3, COLOR_RED, SPECIES_GOBLIN, 0, 0},
  {CHAMPION_CARD, 1, 44, 12, 0, COLOR_RED, SPECIES_CYCLOPS, 0, 0},
  {CHAMPION_CARD, 1, 45, 4, 4, COLOR_RED, SPECIES_KOATL, 0, 0},
  {CHAMPION_CARD, 1, 46, 8, 2, COLOR_RED, SPECIES_ELF, 0, 0},
  {CHAMPION_CARD, 1, 47, 6, 3, COLOR_RED, SPECIES_ELF, 0, 0},
  {CHAMPION_CARD, 1, 48, 6, 4, COLOR_RED, SPECIES_GOBLIN, 0, 0},
  {CHAMPION_CARD, 1, 49, 12, 1, COLOR_RED, SPECIES_CYCLOPS, 0, 0},
  {CHAMPION_CARD, 1, 50, 4, 5, COLOR_RED, SPECIES_KOATL, 0, 0},
  {CHAMPION_CARD, 1, 51, 8, 3, COLOR_RED, SPECIES_CYCLOPS, 0, 0},
  {CHAMPION_CARD, 2, 52, 8, 4, COLOR_RED, SPECIES_GOBLIN, 0, 0},
  {CHAMPION_CARD, 2, 53, 6, 5, COLOR_RED, SPECIES_FAUN, 0, 0},
  {CHAMPION_CARD, 2, 54, 12, 2, COLOR_RED, SPECIES_CYCLOPS, 0, 0},
  {CHAMPION_CARD, 2, 55, 4, 6, COLOR_RED, SPECIES_GOBLIN, 0, 0},
  {CHAMPION_CARD, 2, 56, 8, 5, COLOR_RED, SPECIES_FAUN, 0, 0},
  {CHAMPION_CARD, 2, 57, 6, 6, COLOR_RED, SPECIES_ELF, 0, 0},
  {CHAMPION_CARD, 2, 58, 12, 3, COLOR_RED, SPECIES_KOATL, 0, 0},
  {CHAMPION_CARD, 2, 59, 20, 0, COLOR_RED, SPECIES_CYCLOPS, 0, 0},
  {CHAMPION_CARD, 2, 60, 12, 4, COLOR_RED, SPECIES_GOBLIN, 0, 0},
  {CHAMPION_CARD, 2, 61, 8, 6, COLOR_RED, SPECIES_FAUN, 0, 0},
  {CHAMPION_CARD, 3, 62, 20, 1, COLOR_RED, SPECIES_ELF, 0, 0},
  {CHAMPION_CARD, 3, 63, 12, 5, COLOR_RED, SPECIES_CYCLOPS, 0, 0},
  {CHAMPION_CARD, 3, 64, 20, 2, COLOR_RED, SPECIES_KOATL, 0, 0},
  {CHAMPION_CARD, 3, 65, 12, 6, COLOR_RED, SPECIES_GOBLIN, 0, 0},
  {CHAMPION_CARD, 3, 66, 20, 3, COLOR_RED, SPECIES_FAUN, 0, 0},
  {CHAMPION_CARD, 3, 67, 20, 4, COLOR_RED, SPECIES_ELF, 0, 0},
  {CHAMPION_CARD, 3, 68, 20, 5, COLOR_RED, SPECIES_KOATL, 0, 0},    // fullDeck[67]
  {CHAMPION_CARD, 0, 69, 4, 0, COLOR_INDIGO, SPECIES_DWARF, 0, 0},
  {CHAMPION_CARD, 0, 70, 6, 0, COLOR_INDIGO, SPECIES_CENTAUR, 0, 0},
  {CHAMPION_CARD, 0, 71, 4, 1, COLOR_INDIGO, SPECIES_MINOTAUR, 0, 0},
  {CHAMPION_CARD, 1, 72, 8, 0, COLOR_INDIGO, SPECIES_DWARF, 0, 0},
  {CHAMPION_CARD, 1, 73, 6, 1, COLOR_INDIGO, SPECIES_FAIRY, 0, 0},
  {CHAMPION_CARD, 1, 74, 4, 2, COLOR_INDIGO, SPECIES_LYCAN, 0, 0},
  {CHAMPION_CARD, 1, 75, 8, 1, COLOR_INDIGO, SPECIES_CENTAUR, 0, 0},
  {CHAMPION_CARD, 1, 76, 6, 2, COLOR_INDIGO, SPECIES_CENTAUR, 0, 0},
  {CHAMPION_CARD, 1, 77, 4, 3, COLOR_INDIGO, SPECIES_MINOTAUR, 0, 0},
  {CHAMPION_CARD, 1, 78, 12, 0, COLOR_INDIGO, SPECIES_FAIRY, 0, 0},
  {CHAMPION_CARD, 1, 79, 4, 4, COLOR_INDIGO, SPECIES_LYCAN, 0, 0},
  {CHAMPION_CARD, 1, 80, 8, 2, COLOR_INDIGO, SPECIES_DWARF, 0, 0},
  {CHAMPION_CARD, 1, 81, 6, 3, COLOR_INDIGO, SPECIES_DWARF, 0, 0},
  {CHAMPION_CARD, 1, 82, 6, 4, COLOR_INDIGO, SPECIES_MINOTAUR, 0, 0},
  {CHAMPION_CARD, 1, 83, 12, 1, COLOR_INDIGO, SPECIES_FAIRY, 0, 0},
  {CHAMPION_CARD, 1, 84, 4, 5, COLOR_INDIGO, SPECIES_LYCAN, 0, 0},
  {CHAMPION_CARD, 1, 85, 8, 3, COLOR_INDIGO, SPECIES_FAIRY, 0, 0},
  {CHAMPION_CARD, 2, 86, 8, 4, COLOR_INDIGO, SPECIES_MINOTAUR, 0, 0},
  {CHAMPION_CARD, 2, 87, 6, 5, COLOR_INDIGO, SPECIES_CENTAUR, 0, 0},
  {CHAMPION_CARD, 2, 88, 12, 2, COLOR_INDIGO, SPECIES_FAIRY, 0, 0},
  {CHAMPION_CARD, 2, 89, 4, 6, COLOR_INDIGO, SPECIES_MINOTAUR, 0, 0},
  {CHAMPION_CARD, 2, 90, 8, 5, COLOR_INDIGO, SPECIES_CENTAUR, 0, 0},
  {CHAMPION_CARD, 2, 91, 6, 6, COLOR_INDIGO, SPECIES_DWARF, 0, 0},
  {CHAMPION_CARD, 2, 92, 12, 3, COLOR_INDIGO, SPECIES_LYCAN, 0, 0},
  {CHAMPION_CARD, 2, 93, 20, 0, COLOR_INDIGO, SPECIES_FAIRY, 0, 0},
  {CHAMPION_CARD, 2, 94, 12, 4, COLOR_INDIGO, SPECIES_MINOTAUR, 0, 0},
  {CHAMPION_CARD, 2, 95, 8, 6, COLOR_INDIGO, SPECIES_CENTAUR, 0, 0},
  {CHAMPION_CARD, 3, 96, 20, 1, COLOR_INDIGO, SPECIES_DWARF, 0, 0},
  {CHAMPION_CARD, 3, 97, 12, 5, COLOR_INDIGO, SPECIES_FAIRY, 0, 0},
  {CHAMPION_CARD, 3, 98, 20, 2, COLOR_INDIGO, SPECIES_LYCAN, 0, 0},
  {CHAMPION_CARD, 3, 99, 12, 6, COLOR_INDIGO, SPECIES_MINOTAUR, 0, 0},
  {CHAMPION_CARD, 3, 100, 20, 3, COLOR_INDIGO, SPECIES_CENTAUR, 0, 0},
  {CHAMPION_CARD, 3, 101, 20, 4, COLOR_INDIGO, SPECIES_DWARF, 0, 0},
  {CHAMPION_CARD, 3, 102, 20, 5, COLOR_INDIGO, SPECIES_LYCAN, 0, 0},    // fullDeck[101]

  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1}, // fullDeck[102]
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  {DRAW_CARD, 1, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 2, 1},
  
  {DRAW_CARD, 2, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 3, 2},  // fullDeck[111]
  {DRAW_CARD, 2, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 3, 2},
  {DRAW_CARD, 2, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 3, 2},
  {DRAW_CARD, 2, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 3, 2},
  {DRAW_CARD, 2, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 3, 2},
  {DRAW_CARD, 2, 0, 0, 0, COLOR_NOT_APPLICABLE, SPECIES_NOT_APPLICABLE, 3, 2}  // fullDeck[116]
}; 


// externs
extern MTRand MTwister_rand_struct;

// typedefs


/* Game State Struct (output from each simulation) */
struct gamestate 
{
  enum PlayerID current_player;  // ID of current player (A (first player) or B)
  
  uint16_t current_cash_balance[2];  // current cash balance for each player  
  
  uint8_t current_energy[2];
  bool someone_has_zero_energy;  // will be set by various methods
  
  struct deck_stack deck[2];  // use a 'stack' of indices (0 to 116) to the fullDeck
  
  struct HDCLList hand[2];
  
  struct HDCLList discard[2];
  
  struct HDCLList combat_zone[2];
   
  uint16_t turn;
  
  enum GameState game_state;  // active, playerAwins, playerBwins, draw
  
  enum TurnPhase turn_phase; // ATTACK, DEFENSE
  
  enum PlayerID player_to_move; // PLAYER_A, PLAYER_B (this will generally be current_player in attack phase, and the not-current player in defense phase)
};

// game / sim stats
struct gamestats {  
  uint16_t cumul_player_wins[2]; // number of wins for each player
  uint16_t cumul_number_of_draws;  // to track number  of games that do not end before the maximum number of turns
  uint16_t game_end_turn_number[MAX_NUMBER_OF_SIM]; // keeps track of 'turn' at which game ended for each simulation
  uint16_t cash_progression[MAX_NUMBER_OF_SIM][MAX_NUMBER_OF_TURNS][2]; // cash on hand by each player at each turn of the game, for each sim
  uint16_t simnum; // simulation counter
};

// global variables (keep to a minimum)
bool debug_enabled = false;

// function definitions

int main() 
{
  MTwister_rand_struct = seedRand(M_TWISTER_SEED);
  
  struct gamestats gstats;
    gstats.cumul_player_wins[PLAYER_A] = 0;
    gstats.cumul_player_wins[PLAYER_B] = 0;
    gstats.cumul_number_of_draws = 0;
    memset(gstats.game_end_turn_number, 0, sizeof(gstats.game_end_turn_number));
  
  // decide on number of simulations (games) to run
  uint16_t numsim;
  numsim = MAX_NUMBER_OF_SIM;
  //numsim = 1;  
  
  // initialize initial cash for a simulation run
  uint16_t initial_cash;
  initial_cash = 24;
  
  ORACLE_0a_simulation(numsim, initial_cash, &gstats);
  ORACLE_9_present_results(&gstats);
  
  return EXIT_SUCCESS;
} // main

void ORACLE_0a_simulation(uint16_t numsim, uint16_t initial_cash, struct gamestats* gstats) 
{
  for (gstats->simnum = 0; gstats->simnum < numsim; gstats->simnum++)
  {
    if (debug_enabled) { printf("Begin game %.4u\n", gstats->simnum); }
    ORACLE_0b_play_game(initial_cash, gstats);
    if (debug_enabled) { printf("End game   %.4u\n\n", gstats->simnum); }
  }  // for each simulation

} // simulation

void ORACLE_0b_play_game(uint16_t initial_cash, struct gamestats* gstats)
{
  // always assume first player is player A
  struct gamestate gstate;
  ORACLE_1_setup_game(initial_cash, &gstate);
  if (debug_enabled) { printf("Game started");}
  if (debug_enabled) { printf("Game started with %d A, %d B cash balances; %d A, %d B energy; %d A, %d B hand size\n", 
                              gstate.current_cash_balance[PLAYER_A],
                              gstate.current_cash_balance[PLAYER_B],
                              gstate.current_energy[PLAYER_A],
                              gstate.current_energy[PLAYER_B],
                              gstate.hand[PLAYER_A].size,
                              gstate.hand[PLAYER_B].size);}  
  if (debug_enabled) { printf(" Hand A "); }
  if (debug_enabled) { HDCLL_printLinkedList(&gstate.hand[PLAYER_A]); }
  if (debug_enabled) { printf(" Hand B "); }
  if (debug_enabled) { HDCLL_printLinkedList(&gstate.hand[PLAYER_B]); }
  if (debug_enabled) { printf("\n\n"); }
  
  gstate.turn = 0;
  do
  {
    gstate.turn++;  // consider moving this step inside 'play_turn()' function
    if (debug_enabled) { printf(" Begin round %.4u, turn %.4u\n", (uint16_t) ((gstate.turn-1) * 0.5)+1, gstate.turn); }
    ORACLE_4a_play_turn(gstats, &gstate);
    if (debug_enabled) { printf(" End   round %.4u, turn %.4u\n", (uint16_t) ((gstate.turn-1) * 0.5)+1, gstate.turn); }
    if (debug_enabled) { printf(" Turn ended with %d A, %d B cash balances; %d A, %d B energy; %d A, %d B hand size; %d A, %d B deck size; %d A, %d B discard size\n", 
                                gstate.current_cash_balance[PLAYER_A],
                                gstate.current_cash_balance[PLAYER_B],
                                gstate.current_energy[PLAYER_A],
                                gstate.current_energy[PLAYER_B],
                                gstate.hand[PLAYER_A].size,
                                gstate.hand[PLAYER_B].size,
                                gstate.deck[PLAYER_A].top+1,
                                gstate.deck[PLAYER_B].top+1,
                                gstate.discard[PLAYER_A].size,
                                gstate.discard[PLAYER_B].size); }
    if (debug_enabled) { printf(" Hand A "); }
    if (debug_enabled) { HDCLL_printLinkedList(&gstate.hand[PLAYER_A]); }
    if (debug_enabled) { printf(" Hand B "); }
    if (debug_enabled) { HDCLL_printLinkedList(&gstate.hand[PLAYER_B]); }
    if (debug_enabled) { printf("\n\n"); }
        
  } while (gstate.turn < MAX_NUMBER_OF_TURNS && !gstate.someone_has_zero_energy); // for each turn
  
  if (!gstate.someone_has_zero_energy) gstate.game_state = DRAW;
   
  if (debug_enabled) { printf("Game ended at round %.4u, turn %.4u, winner is %u\n", (uint16_t)((gstate.turn-1) * 0.5)+1, gstate.turn, gstate.game_state); }
  
  // compile final game stats (increment cumulative player wins and 
  // save game end turn number) in &gstats
  ORACLE_8_record_final_stats(gstats, &gstate);
  
  // free any heap memory related to the game that just finished
  // empty decks, combat zones, discards, hands
  DeckStk_emptyOut(&gstate.deck[PLAYER_A]);
  DeckStk_emptyOut(&gstate.deck[PLAYER_B]);
  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_A]);
  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_B]);
  HDCLL_emptyOut(&gstate.hand[PLAYER_A]);
  HDCLL_emptyOut(&gstate.hand[PLAYER_B]);
  HDCLL_emptyOut(&gstate.discard[PLAYER_A]);
  HDCLL_emptyOut(&gstate.discard[PLAYER_B]);  
} // play_game

void ORACLE_1_setup_game(uint16_t initial_cash, struct gamestate* gstate)
{
  // initialize gstate to default game start values
  gstate->current_player = PLAYER_A;

  gstate->current_cash_balance[PLAYER_A] = initial_cash;
  gstate->current_cash_balance[PLAYER_B] = initial_cash;
  
  gstate->current_energy[PLAYER_A] = 99;
  gstate->current_energy[PLAYER_B] = 99;
  gstate->someone_has_zero_energy = false;
  gstate->game_state = ACTIVE;
    
  // initialize decks to an empty stack
  gstate->deck[PLAYER_A].top = -1; 
  gstate->deck[PLAYER_B].top = -1;
  
  // draw deckA and deckB
  // randomly sample 39 * 2 = 78 card indices from the full deck
  uint8_t rndCardIndex[FULL_DECK_SIZE];
  uint8_t i;
  for (i = 0; i < FULL_DECK_SIZE; i++)
  {
    rndCardIndex[i] = i;
  }
  
  RND_partial_shuffle(rndCardIndex, FULL_DECK_SIZE, 2*MAX_DECK_STACK_SIZE);

  // push to deck_A and deck_B alternately the 78 card indices
  i = 0;
  while (i < 2*MAX_DECK_STACK_SIZE)
  {
    DeckStk_push(&gstate->deck[PLAYER_A], rndCardIndex[i]);
    i++;
    DeckStk_push(&gstate->deck[PLAYER_B], rndCardIndex[i]);
    i++;
  }
  
  if (debug_enabled) { printf("Player A deck: ");}
  if (debug_enabled) { DeckStk_print(&gstate->deck[PLAYER_A]);}
  if (debug_enabled) { printf("\nDeck A size: %u", gstate->deck[PLAYER_A].top+1);}
  if (debug_enabled) { printf("\nPlayer B deck: ");}
  if (debug_enabled) { DeckStk_print(&gstate->deck[PLAYER_B]);}
  if (debug_enabled) { printf("\nDeck B size: %u", gstate->deck[PLAYER_B].top+1);}  
  if (debug_enabled) { printf("\n");}

  // initialize hands, combat zones and discards to all zeros
  HDCLL_initialize(&gstate->hand[PLAYER_A]); 
  HDCLL_initialize(&gstate->hand[PLAYER_B]);

  HDCLL_initialize(&gstate->discard[PLAYER_A]);
  HDCLL_initialize(&gstate->discard[PLAYER_B]);

  HDCLL_initialize(&gstate->combat_zone[PLAYER_A]);
  HDCLL_initialize(&gstate->combat_zone[PLAYER_B]);
  
  // draw 5 cards hand for each player
  // for each player, pop 5 card indices from the deck and place them in their hand  
  uint8_t cardindex;
  for (i = 0; i < 5; i++)
  {
    cardindex = DeckStk_pop(&gstate->deck[PLAYER_A]);
    HDCLL_insertNodeAtBeginning(&gstate->hand[PLAYER_A], cardindex);
    cardindex = DeckStk_pop(&gstate->deck[PLAYER_B]);
    HDCLL_insertNodeAtBeginning(&gstate->hand[PLAYER_B], cardindex);    
  }  
   
  ORACLE_3_apply_mulligan();

} // setup_game

void ORACLE_3_apply_mulligan() 
{
  // for each card in Player B's hand
    // compile stats about 'power' of each card in hand of current player: make use of helper function for that
    // store 'power' and 'efficiency' directly as constants in 'fullDeck' and just look them up here for each card in the hand  
    // track cards positions in hand for the 'lowest' and 'second lowest' power cards: 
    // e.g. initialize 'lowest power' and 'second lowest power' to 1,000,000 and lowest_power_card_pos and scnd_lowest_power_card_pos to -1
    // as we loop through the hand, if a card has lower power than 'lowest power' card, replace 'lowest power' and 'lowest_power_card_pos' with the new card and 'push' the
    // info for the previously lowest power card to 'second lowest'
    // otherwise, if card has higher or equal power to current 'lowest power' card, but lower than 'second lowest power' card, then put the information for that card in 'second lowest' slot
  
  // decide based on 'power' of cards in the hand how many cards to mulligan (within the allowed range)
  
  // while target number of cards to mulligan is not attained
    // discard card with lowest 'power' that is still in hand: make use of helper function here as well: add card to discard, remove card from hand
    
  // draw from Player B's deck as many cards as have been mulliganed (make sure all cards have been mulliganed before drawing one single card) 
	
} // ORACLE_3_apply_mulligan

// TO DO: delegate work done in this function to helper functions to reduce number of lines of codes to about 30
void ORACLE_4a_play_turn(struct gamestats* gstats, struct gamestate* gstate)
{
  enum PlayerID attacker = gstate->current_player;
  enum PlayerID defender = 1 - gstate->current_player;
  
  // 4a: draw 1 card
  if (!(gstate->turn == 1 && attacker == PLAYER_A)) ORACLE_4b_draw1card(gstate, attacker);
  
  // consider including in one 'helper' function called 'begin_of_turn()' the following 4 actions:
  // turn++ (to be transfered from 'play game')
  // draw1card
  // set turnphase = attack
  // set playertomove = currentplayer 
  
  
  // decide and process attacker and defender actions
  // simple non-optimal strategy: apply a simple decision rules which consists of picking one random card in the attacker's hand when hand is non-empty
  
  if (gstate->hand[attacker].size > 0) // if attacker's hand is not empty
  {
    // pick one card randomly from attacker's hand and play it. If that's a champion card, this means putting it in the combat zone.
    // need to make a list first of only the cards that the attacker has enough cash to play, and randomly pick one of those (not the entire hand)
    uint8_t attackerAffordableCardIndices[gstate->hand[attacker].size];
    uint8_t numberOfAffordableAttackerCardsInHand = 0;
    
    // convert gstate->hand[attacker] to an array here for more efficient searching in the loop below
    uint8_t* attacker_hand = HDCLL_toArray(&gstate->hand[attacker]);
    
    uint8_t possible_attacker_card_index = 0;
    for (uint8_t i = 0; i < gstate->hand[attacker].size; i++)
    {          
      possible_attacker_card_index = attacker_hand[i];
      if (fullDeck[possible_attacker_card_index].cost <= gstate->current_cash_balance[attacker])
      {
        attackerAffordableCardIndices[numberOfAffordableAttackerCardsInHand] = possible_attacker_card_index;
        numberOfAffordableAttackerCardsInHand++;
      }
    } // for
    free(attacker_hand);
    
    uint8_t attackerCardIndexToPlay = 0;
    uint8_t chosenCardIndex;
    if (numberOfAffordableAttackerCardsInHand > 0)
    {
      chosenCardIndex = RND_randn(numberOfAffordableAttackerCardsInHand);
      attackerCardIndexToPlay = attackerAffordableCardIndices[chosenCardIndex];
      
      if (fullDeck[attackerCardIndexToPlay].card_type == CHAMPION_CARD)
      {
        HDCLL_insertNodeAtBeginning(&gstate->combat_zone[attacker], attackerCardIndexToPlay);  // putting the card in the combat zone if the chosen card is a champion card
        if (debug_enabled) { printf(" Played champion card index %u from attacker's hand\n", attackerCardIndexToPlay); }
      }      
      
      HDCLL_removeNodeByValue(&gstate->hand[attacker], attackerCardIndexToPlay);
      
      // need to deduct from attacker's cash the cost of playing the card
      gstate->current_cash_balance[attacker] -= fullDeck[attackerCardIndexToPlay].cost;
    }
    	  
    // if there is a combat, as a temporary strategy, defender plays one champion randomly from their hand, if their hand is non-empty
    if (gstate->combat_zone[attacker].size > 0) // if there is a combat
    {      
      if (gstate->hand[defender].size > 0) // if defender hand is not empty
      {
        // check list of defender's champion cards and if there are some 1 or more champion cards,
        // pick one champion randomly from defender's hand and play it.
        // build array with list of champion cards' indices:
        uint8_t defenderAffordableChampionCardIndices[gstate->hand[defender].size];
        uint8_t numberOfAffordableDefenderChampionsInHand = 0;       
        
        // convert gstate->hand[defender] to an array here for more efficient searching in the loop below
        uint8_t* defender_hand = HDCLL_toArray(&gstate->hand[defender]);
        
        uint8_t possible_champion_card_index = 0;
        for (uint8_t i = 0; i < gstate->hand[defender].size; i++)
        {          
          possible_champion_card_index = defender_hand[i];
          if ((fullDeck[possible_champion_card_index].card_type == CHAMPION_CARD) 
              && (fullDeck[possible_champion_card_index].cost <= gstate->current_cash_balance[defender])) 
          {
            defenderAffordableChampionCardIndices[numberOfAffordableDefenderChampionsInHand] = possible_champion_card_index;
            numberOfAffordableDefenderChampionsInHand++;
          }
        } // for
        free(defender_hand);
        
        if (numberOfAffordableDefenderChampionsInHand > 0)
        {
          chosenCardIndex = RND_randn(numberOfAffordableDefenderChampionsInHand);
          uint8_t defenderCardIndexToPlay = defenderAffordableChampionCardIndices[chosenCardIndex];
          HDCLL_insertNodeAtBeginning(&gstate->combat_zone[defender], defenderCardIndexToPlay);
          HDCLL_removeNodeByValue(&gstate->hand[defender], defenderCardIndexToPlay);
          if (debug_enabled) { printf(" Played champion card index %u from defender's hand\n", defenderCardIndexToPlay); }
          
          // need to deduct from defender's cash the cost of playing the card
          gstate->current_cash_balance[defender] -= fullDeck[defenderCardIndexToPlay].cost;
        } // if there are champion cards in the defender deck
        
      } // if defender hand is not empty
      
      // resolve combat
      // calculate total attack
      int16_t total_attack = 0;
      uint8_t card_index;
      
      struct LLNode* combat_node = gstate->combat_zone[attacker].head;
      for (uint8_t i = 0; i < gstate->combat_zone[attacker].size; i++)
      {        
        card_index = combat_node->data;
        total_attack += fullDeck[card_index].attack_base + RND_dn(fullDeck[card_index].defense_dice);
        combat_node = combat_node->next; 
        if (debug_enabled) { printf(" Champion attacker card index %u played. D%u+%u, cost %u, cummulative total attack %u\n", card_index, 
          fullDeck[card_index].defense_dice, fullDeck[card_index].attack_base, fullDeck[card_index].cost, total_attack); }
      }    
      
      // calculate total defense
      int16_t total_defense = 0;      
      combat_node = gstate->combat_zone[defender].head;
      for (uint8_t i = 0; i < gstate->combat_zone[defender].size; i++)
      {        
        card_index = combat_node->data;
        total_defense += RND_dn(fullDeck[card_index].defense_dice);
        combat_node = combat_node->next;
        if (debug_enabled) { printf(" Champion defender card index %u played. D%u+%u, cost %u, cummulative total defense %u\n", card_index, 
          fullDeck[card_index].defense_dice, fullDeck[card_index].attack_base, fullDeck[card_index].cost, total_defense); }  
      }    
      
      // apply damage
      int16_t total_damage = max(total_attack - total_defense, 0);
      if (debug_enabled) { printf(" Defender energy prior to taking damage of %u", gstate->current_energy[defender]); }
      gstate->current_energy[defender] -= min((uint8_t)total_damage, gstate->current_energy[defender]);
      if (debug_enabled) { printf(" less damage of %u = energy after taking damage of %u\n", total_damage, gstate->current_energy[defender]); }
      
      
      // end of combat resolution
      
      // move all champion cards from combat areas to discards
      // attacker combat area
      for (uint8_t i = 0; i < gstate->combat_zone[attacker].size; i++)
      {
        card_index = HDCLL_removeNodeFromBeginning(&gstate->combat_zone[attacker]);
        HDCLL_insertNodeAtBeginning(&gstate->discard[attacker], card_index); 
      }      
      
      // defender combat area
      for (uint8_t i = 0; i < gstate->combat_zone[defender].size; i++)
      {
        card_index = HDCLL_removeNodeFromBeginning(&gstate->combat_zone[defender]);
        HDCLL_insertNodeAtBeginning(&gstate->discard[defender], card_index); 
      }
      
    } // if there is a combat
    else // there is no combat
    {
      if (fullDeck[attackerCardIndexToPlay].card_type == DRAW_CARD)
      {
        // process the draw card
        uint8_t n = fullDeck[attackerCardIndexToPlay].draw_num; // number of cards to draw from deck
        if (debug_enabled) { printf(" Playing draw card %u from attacker's hand\n", attackerCardIndexToPlay); }
        for (uint8_t i = 0; i < n; i++)
        {
          ORACLE_4b_draw1card(gstate, attacker);
        }
        
        // Using the draw card is already paid for in the above code        
        // Taking the played draw card out of the hand has already been done in the above code
        
        // Move the played draw card to the discard:
        HDCLL_insertNodeAtBeginning(&gstate->discard[attacker], attackerCardIndexToPlay);  // putting the draw card in the discard
        
      }
      else if (fullDeck[attackerCardIndexToPlay].card_type == CASH_CARD)
      {
        // implement here the actions to take when a Cash Card is played
        
        
        // Move the played cash card to the discard:
        HDCLL_insertNodeAtBeginning(&gstate->discard[attacker], attackerCardIndexToPlay);  // putting the draw card in the discard        
      }
    } // if there is no combat
    
  } // if attacker's hand is not empty
  
  // update gstats cash_progression
  gstats->cash_progression[gstats->simnum][gstate->turn-1][attacker] = gstate->current_cash_balance[attacker];
  gstats->cash_progression[gstats->simnum][gstate->turn-1][defender] = gstate->current_cash_balance[defender];

  if (gstate->current_energy[defender] == 0) // end of game
  {
    // set flag someone_has_zero_energy and change game state to indicate the winning player
    gstate->someone_has_zero_energy = true;
    gstate->game_state = (attacker == PLAYER_A) ? PLAYER_A_WINS : PLAYER_B_WINS;
  }
  else  // not end of game
  {
    // consider bundling these 3 function calls into a call to one helper function, named 'end_of_turn()', that calls them. this will allow re-use of the helper function outside of this situation.
    ORACLE_6_collect1luna(gstate);
    ORACLE_7a_discard_to_7_cards(gstate);
    ORACLE_7b_chg_current_player(gstate);
  } 

} // play_turn

// consider adding the expected value of attack and defense, as well as the efficiency of attack and defense and total 'power' as new values in the 'FullDeck' as they will be static
// Calculate expected value for attack or defense
double ORACLE_5_calculate_expected_value(uint8_t base, uint8_t dice_type) {
    double dice_expected = (dice_type + 1) / 2.0;
    return base + dice_expected;
}

// Calculate attack/defense efficiency ratios: this needs to be adapted as it will
// not work with the zero cost cards
double ORACLE_5_calculate_attack_efficiency(struct card* card) {
    double expected_attack = ORACLE_5_calculate_expected_value(card->attack_base, card->defense_dice);
    return expected_attack / (card->cost+0.1);
}

double ORACLE_5_calculate_defense_efficiency(struct card* card) {
    double expected_defense = ORACLE_5_calculate_expected_value(0, card->defense_dice);
    return expected_defense / (card->cost+0.1);
}

double ORACLE_5_calculate_card_power(struct card* card) {
  return (ORACLE_5_calculate_attack_efficiency(card) 
        + ORACLE_5_calculate_defense_efficiency(card)) * 0.5;
}

void ORACLE_4b_draw1card(struct gamestate* gstate, enum PlayerID player)
{  
  if (DeckStk_isEmpty(&gstate->deck[player]))
  {
    // call 'shuffle_discard_and_form_deck() when deck is empty
    ORACLE_4c_shuffle_discard_and_form_deck(&gstate->discard[player], &gstate->deck[player]);
    if (debug_enabled) { printf(" Reshuffled deck for player %u\n", player); }
  }
  
  uint8_t cardindex;
  cardindex = DeckStk_pop(&gstate->deck[player]);
  HDCLL_insertNodeAtBeginning(&gstate->hand[player], cardindex);     
  if (debug_enabled) { printf(" Drew card index %u from player's %u deck\n", cardindex, player); }
} // draw1card

void ORACLE_4c_shuffle_discard_and_form_deck(struct HDCLList* discard, struct deck_stack * deck)
{
  // extract card indices from the discard
  uint8_t* A = HDCLL_toArray(discard);
  uint8_t n = discard->size;
  
  if (debug_enabled) { printf(" Discard prior to reforming deck: "); }
  if (debug_enabled) { HDCLL_printLinkedList(discard); }
  if (debug_enabled) { printf("\n"); }
  
  // shuffle the card indices
  RND_partial_shuffle(A, n, n);

  // push to deck the shuffled card indices
  for (uint8_t i = 0; i < n; i++)
  {
    DeckStk_push(deck, A[i]);    
  }
  
  // free heap memory used by A array, assigned inside 'toArray' function  
  free(A);
  
  if (debug_enabled) { printf(" Deck just after being reformed from discard (after shufling): "); }
  if (debug_enabled) { DeckStk_print(deck);}
  if (debug_enabled) { printf("\n"); }
  
  // empty the discard
   for (uint8_t i = 0; i < n; i++)
  {
    HDCLL_removeNodeFromBeginning(discard);
  }
} // ORACLE_4c_shuffle_discard_and_form_deck

void ORACLE_6_collect1luna(struct gamestate* gstate)
{
  gstate->current_cash_balance[gstate->current_player]++;  
} // collect1luna

void ORACLE_7a_discard_to_7_cards(struct gamestate* gstate)
{
  // count number of cards in hand of current player and exit the function if the hand has 7 or less cards
  
  // for each card in hand
    // compile stats about 'power' of each card in hand of current player: make use of helper function for that as will be able to re-use
    // store 'power' and 'efficiency' directly as constants in 'fullDeck' and just look them up here for each card in the hand    
  
  // while current player's hand size is larger than 7 cards loop
    // discard card with lowest 'power' that is still in hand: make use of helper function here as well as will be able to re-use for mulligan
}

void ORACLE_7b_chg_current_player(struct gamestate* gstate)
{
  gstate->current_player = 1 - gstate->current_player;
}

void ORACLE_8_record_final_stats(struct gamestats* gstats, struct gamestate* gstate)
{
  switch (gstate->game_state) 
  {
    case PLAYER_A_WINS:
      ++gstats->cumul_player_wins[PLAYER_A];
      break;
    case PLAYER_B_WINS:
      ++gstats->cumul_player_wins[PLAYER_B];
      break;
    case DRAW:
      ++gstats->cumul_number_of_draws;
      break;
    case ACTIVE:  // do nothing as should never be reached
      break;
  }
  
  gstats->game_end_turn_number[gstats->simnum] = gstate->turn;
}

void ORACLE_9_present_results(struct gamestats* gstats)
{
  // present the gamestats information in a way that will be usable for statistical analysis
  printf("Number of wins for player A: %u\n", gstats->cumul_player_wins[PLAYER_A]);
  printf("Number of wins for player B: %u\n", gstats->cumul_player_wins[PLAYER_B]);
  printf("Number of draws: %u\n", gstats->cumul_number_of_draws);
  
  printf("Number of turns for each game: ");
  uint16_t minNbrTurn = MAX_NUMBER_OF_TURNS;
  uint16_t maxNbrTurn = 0;
  uint16_t totalNbrTurn = 0;
  for (uint16_t s = 0; s < gstats->simnum; s++)
  {
    printf("%u, ", gstats->game_end_turn_number[s]);
    minNbrTurn = min(minNbrTurn, gstats->game_end_turn_number[s]);
    maxNbrTurn = max(maxNbrTurn, gstats->game_end_turn_number[s]);
    totalNbrTurn += gstats->game_end_turn_number[s];
  }
  printf("\nAverage = %.1f, Minimum = %u, Maximum = %d number of turns per game\n", (float)totalNbrTurn / (float)gstats->simnum, minNbrTurn, maxNbrTurn);
  
  printf("Cash Progression");
  for (uint16_t s = 0; s < gstats->simnum; s++)
  {
    printf("\nA: ");
    for (uint16_t t = 0; t < gstats->game_end_turn_number[s]; t++)
    {
      printf("%u, ", gstats->cash_progression[s][t][PLAYER_A]);
    }
    
    printf("\nB: ");
    for (uint16_t t = 0; t < gstats->game_end_turn_number[s]; t++)
    {
      printf("%u, ", gstats->cash_progression[s][t][PLAYER_B]);
    }    
    printf("\n");
  } // for each sim
  
} // present results
