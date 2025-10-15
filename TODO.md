# oracle TO DOs
1. check for memory leaks with valgrind

2. Check that following logic was captured in claudeAI generated code.
  resolveCombat(), which will swith to turn_phase = attachk as the last line of code
  
  check 'strategy' code : my notes were
  
  attack_phase():
    apply_attack_strat(curPlayer) = apply_attack_def_strat[curPlayer, Attack (enum)]
       which will switch to turn_phase = defense as the last line of code
  
  defense_phase():
    if combatZone[curPlayer] > 0
      apply_defense_strat[notCurPlayer] =  apply_attack_def_strat[notCurPlayer, Defense (enum)]
    endif
  
  AttStratA, etc are 4 arrays of function pointers to be executed sequentially to execute the Attack Strategy for Player A, player B, etc.
  void (*AttStratA[n]) (&gamestat, &gamestate, ...): n is the maximum number of steps in a strategy
  void (*AttStratB[n]) (&gamestat, &gamestate, ...)
  void (*DefStratA[n]) (&gamestat, &gamestate, ...)
  void (*DefStratB[n]) (&gamestat, &gamestate, ...)
  
  AttStratA[0] = function name, etc.; or initialize this way: AttStratA = {f1, f2, ...}
  
  ApplyAttDefStrat(PlayerID, AttackDefenseIndicator (from an enum))
   function that will find, based on PlayerID and A/D ID which of the 4 array of function pointers to use, and will then call them
  
  inside ApplyAttDefStrat, 
    define void (*Strat[n]) (&gamestat, &gamestate, ...);
  then based on PlayerID and A/D ID in switch case stmts, could then do:
    Strat = AttStratA;
  
  then loop over n indices of Strat and call functions:
    for each i from 0 to n - 1
      Strat[i] (&gamestat, &gamestate, ...);
 
3. check that combo bonuses are properly calculated

my pseudo code originally was

s1, s2, s3: species combat cards 1, 2, 3
c1, c2, c3: colours combat cards 1, 2, 3
o1, o2, o3: order combat cards 1, 2, 3

switch deck_drawing_approach

case RANDOM:

  if 2 cards in combat zone:
    if s2=s1 then +10
    else if o2=o1 then +7
    else if c2=c1 then +5

  if 3 cards in combat zone:
    if s2=s1 then
      if s3=s1 then +16
      else if o3=o1 then +14
      else if c3=c1 then +13
      else +10
    else if s3=s1 then
      if o3=o2 then +14
      else if c3=c2 then +13
      else +10
    else if o2=o1 then
      if o3=o1 then +11
      else if c3=c2 then +9
      else +7
    else if o3=o1 then
      if c2=c1 then +9
      else +7
    else if c2=c1 then
      if c3=c1 then +8
      else +5
    else if c3=c1 then = 5
    endif

case MONOCHROME:
  if 2 cards in combat zone:
    if o2=o1 then +7

  if 3 cards in combat zone:
    if o2=o1 then
      if o3=o1 then +12
      else +7
    endif

case CUSTOM:
  if 2 cards in combat zone:
    if s2=s1 then +7
    else if o2=o1 then +4
    
  if 3 cards in combat zone:
    if s2=s1 then
      if s3=s1 then +12
      else if o3=o1 then +9
      else +7
    else if s3=s1 then
      if o3=o2 then +9
      else +7
    else if o2=o1 then
      if o3=o1 then +6
      else +4
    else if o3=o1 then +4
    endif
    
4. Verify the cash card implementation
      Cash Card Implementation
      - `has_champion_in_hand()` - checks if champions are available
      - `select_champion_for_cash_exchange()` - selects lowest power champion
      - `play_cash_card()` - complete cash card logic
      - Properly integrated into card selection and play logic

5. Implement a ncurses based text user interface to allow two human players to interactively play against each other or a human to play against the 'AI': 
     start immediately in 'interactive' mode unless user provided the -sim command line option.
       for interactive present the table on the left and a console on the right: console will allow a 'simmode' command to switch to simulation mode.
       for simulation present a console on the right, output on the left and parameters at the bottom left, with ability to export results to txt files (name of which
         on the bottom left as well). console in sim mode may allow an 'intermode' command to switch back to interactive mode.

6. Build foundation to allow for smart AI playing agents:

   integrate cards visibility indicators inside the gamestate struct: eg ai agent should only be provided visible (or 'known to be somewhere') portion of gamestate    
      card visibility: for each card in hands and decks, need to have one value in game state 
      from player A perspective and one from player B perspective:
        visible
        hidden (could be anywhere, including in cards left completely out of play for this game)
        hidden but known to be somewhere in own deck
        hidden but known to be somewhere in opponent deck
        hidden but known to be in opponent's hand (effectively, this is the same as 'visible' from 
          the strategic value perspective)
        hidden but known to be in either opponent's hand or deck
   
7. Finalize foundation to allow for smart AI playing agents:

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

8. Build a first set of 'smart' AI playing agents
    
    enhance decision rules to mimic what a smart player would do, and find what could be more optimal 
       decision rules, probably using heuristics to keep things simple. See notes in balanced rules strategy source file: strat_balancedrules1.c and in heuristics strategy
       source file.
       
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
         
9. Build a 'somewhat smarter' AI playing agent

    combine the 'balanced' and 'heuristic' strategies into a 3rd hybrid strategy that is better than those 2
   
10. Build even stronger AI playing agents:
      - Monte Carlo Single Stage Analysis (strat_simplemc1): pruning in 4 episodes

      - Gradual progressive pruning
      
      - Ucb1
      
      - pucb1
      
      - mcts (uct)
      
      - info set MCTS (Info Set Monte Carlo Tree Search))
      
      - mcts with prior predictor
      
      - mcts with neural network based (eg dqn) prior predictor: presumably this would be a very very strong agent
  
11. implement a GUI version of the game: may need to figure out how to make sure the program does not 'freeze' PC when calculating AI strategy. e.g. may want button / menu
      to let GUI user stop the calc or terminate the program in a 'clean' way. One option is to span the calculation intensive task to a 'worker thread'.
       
12. implement the client / server approach:
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

13. **Add Documentation**
     - Doxygen comments
     - API documentation
     - Strategy guide

14. **Performance Optimization**
     - Profile hot paths
     - Optimize combo calculations
     - Memory pool for frequently allocated structures

