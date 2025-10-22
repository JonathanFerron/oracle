# oracle TO DOs
1. Implement a ncurses based text user interface (TUI) to allow two human players to interactively play against each other or a human to play against the 'AI' (strategy sets): 
       for TUI mode: present the game table on the left and a console on the right: console will allow a 'simmode' command to switch to simulation mode.
       for interactive simulation mode: present a console on the right, output on the left and parameters at the bottom left, with ability to export results to txt files 
         (name of which on the bottom left as well). console in interactive sim mode may allow an 'tuimode' command to switch back to TUI mode.
      
       see https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/helloworld.html to decide how to structure the ncurses TUI code
       
   Additonal idea on TUI mode with sample (generic) code:
   #include <ncurses.h>
   #include <stdio.h>

    // Function for ncurses mode

    void run_ncurses_mode() {
        initscr();              // Initialize the screen
        cbreak();               // Disable line buffering
        noecho();               // Don't echo user input
        printw("Running in ncurses mode. Press any key to exit.");
        getch();                // Wait for user input
        endwin();               // End ncurses mode
    }      

2. Build foundation to allow for smart AI playing agents:

   integrate cards visibility indicators inside the gamestate struct: eg ai agent should only be provided visible (or 'known to be somewhere') portion of gamestate    
      card visibility: for each card in hands and decks, need to have one value in game state 
      from player A perspective and one from player B perspective:
        visible
        hidden but known to be in opponent's hand (effectively, this is the same as 'visible' from 
          the strategic value perspective)
        hidden (could be anywhere, including in fullDeck cards left completely out of play for this game (may not even be in any one's deck'))
        hidden but known to be somewhere in own deck
        hidden but known to be somewhere in opponent deck        
        hidden but known to be in opponent's hand or deck (but we don't know which, since of course the opponent's hand is hidden from the other player)
   
3. Finalize foundation to allow for smart AI playing agents:

    should make use of the 'action' structure: build (encode) it in decision portion of the code, and then perform an action by decoding the action struct
      at the end of the day, the strat code should be building the action struct (using visible gamestate info passed from main code), 
      and the main code should be executing it (applying it to the gamestate)

    clearly delineate and delegate (to a separate function) the code that creates the 'list of possible moves' when faced with a decision, and then call 
    the 'select_action' code using that and the (visible portion of) gamestate
    
    action structure, possibly implemented in an action.h and action.c set of files:

      action type enum: 
        PLAYEXCHANGECARD, PLAYDRAWRECALLCARD, PASS, PLAYCHAMPIONCARDS, 
        OFFERDRAW, FORFEIT, ACCEPTDRAW, REFUSEDRAW, DISCARDCARDS (for mulligan and discard to 7)

      championtoexchange, when playing exchange card

      numberofchampioncardsplayed = uint 0 to 3
      numberofcardsdiscarted

      cardstorecall from discard when playing draw card
      recallordrawcards enum: recallcards, drawcards

      championcardsplayed = array(3) of uint (index in full deck)

      cardsdiscarted = array(3) of uint (index in full deck)

4. Build a first set of 'smart' AI playing agents
    
    enhance decision rules to mimic what a smart player would do, and find what could be more optimal 
       decision rules, probably using heuristics to keep things simple. See notes in balanced rules strategy source file: strat_balancedrules1.c and in heuristics strategy
       source file. Don't forget that a call to a strategy function should also be done when mulliganing and discarting (to 7 cards).
       
      Need to put in place, in the module that calls the simulation code (from main()), an optimization mechanism that can be used to automate the fine tuning of AI agent
        heuristics / parameters with the goal of maximizing that agent's win rate, or eventually finding the parameters that will make a given agent's win rate equal to x%
        against a pre-determined 'benchmark' agent. This would allow a human user to select an AI agent with a given 'strength' to play against. 
       
      Work on implementing a correct 'power' for non-champion cards to allow better 'power heuristic'-based choices. Model multiple simulations with varying power from 2 to 15
      for the non-champion cards for player A, and keep the same card's power to a fixed value for player B. Which of the values between 2, 3, 4, ..., 15 yield the best
      win percentage for player A? Say that's 5.00. Use 5.00 as the new default 'power' value for the card, and do the simulation again, keeping the default value of 5.00
      for player B's decisions but iterating from 2 to 15 for player A to confirm that it now yields a better win percentage for player B for all of A's values except when
      A also uses the 'optimal' value 5.00. When the 'cash card' is implemented, use the same approach to calculate a 'power heuristic' for it: calibrate the heuristic
      parameter to maximize the chances of winning.
       
       To Add New Strategies:
         - Copy strat_random.c as template
         - Implement attack/defense functions
         - Register in main.c
         
5. Build a 'somewhat smarter' AI playing agent

    combine the 'balanced' and 'heuristic' strategies into a 3rd hybrid strategy that is better than those 2
   
6. Build even stronger AI playing agents:
      - Monte Carlo Single Stage Analysis (strat_simplemc1): pruning in 4 episodes

      - Gradual progressive pruning
      
      - Ucb1
      
      - pucb1
      
      - mcts (uct)
      
      - info set MCTS (Info Set Monte Carlo Tree Search))
      
      - mcts with prior predictor
      
      - mcts with neural network based (eg dqn) prior predictor: presumably this would be a very very strong agent
  
7. implement a GUI version of the game: may need to figure out how to make sure the program does not 'freeze' PC when calculating AI strategy. e.g. may want button / menu
      to let GUI user stop the calc or terminate the program in a 'clean' way. One option is to span the calculation intensive task to a 'worker thread'.
       
8. implement the client / server approach:
     - server handles game mechanics (including throwing the dices, coming up with the list of possible moves, etc.) and is only one knowing the full game state
     - server will provide clients also with opponent's last play and visible game state (which varies for each player because of the hand visibility to only one player) 
         for drawing the screen
     - client can be human or AI
     - AI client takes a visible game state, implements a playing strategy to make decisions, and returns an action
     - human client takes a visible game state, present game state on screen, take action (input) from user, and then return an action to the server
     - server applies the action chosen by the client to the gamestate
  
      how the different modes can be chosen when the game is launched via the command line:
      usage: oracle -m mode
        mode options: 
          (st)andalone: (a)utomated (use 'sta' as the mode), s(imulation) (use 'sts'), t(ext user interface) ('stt'), g(raphical user interface) ('stg')
                            in automated style, use a default of 1000 simulations and default console output that only takes 25 rows max
                            interactive play code should call tui or gui draw functions depending on context
          (se)rver: will automatically use text CLI
          (cl)ient (for human players): s(imulation) ('cls'), t(ext user interface) ('clt'), g(raphical user interface) ('clg')
          (ai) (for ai players): will automatically use text CLI
    
    - additional notes on client / server approach for consideration / integration in the proposed approach:
      AI client to be called by server: could be on the same physical machine as the server, or a different one
      source code files could be split between client (mostly user I/O) (cl_xyz.c), server (game logic and state) (sr_abc.c) and shared constants and structures (sh_xyz.c)
        ai client implementation could be in ai_xyz.c source files (likely mostly using the strategy.c files)
        simulation in standalone mode could be in sim_xyz.c source files
      
      client and server communicate via socket text messages when on different machines or simple function calls (with the 'text commands') when running in standalone mode
      when in 'network' mode, text messages and function calls travel this way:
          client -> client command to text translation -> socket port (client side) -> net -> socket (server side) -> server text to command interpreter -> server
            in 'network' mode, both sockets could also be on the same PC on 2 different ports, attached to 2 independent programs
      
      when in 'standalone' mode, message could travel this way. may want to eventually consider further reducing 'overhead' created by the 'encode, then decode' of the command by using
        another more efficient / fast way to get the client and server to communicate (e.g. directly passing an 'action' struct when the client wants to inform the server of the
        action it wants to take?):
          client -> client command to text translation -> server text to command interpreter -> server
     
     server's message back often to be the 'visible game state' (from the perspective of a given client / player) or the 'result of last dice roll'
     
     text commands: 4 letters (mandatory) + 6 digits (optional) (e.g. "play 01,04,06" or "cham 01,04,06" to indicate we want to play card IDs 1, 4 and 6 (decide if IDs should represent
     position in player's hand array, or the actual card's index in the fullDeck array)

9. **Add Documentation**
     - Doxygen comments
     - API documentation
     - Strategy guide

10. **Performance Optimization**
     - Profile hot paths
     - Optimize combo calculations
     - Memory pool for frequently allocated structures

