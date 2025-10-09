/* 
 * Monte Carlo Single Stage Analysis (strat_simplemc1):
      manually create 100 distinct 'attack' phase game states at various stages of the game.
        for each game state, use the MonteCarloSingleStageAnalysis strategy in the 'applyAttDefStrat(Att)' function, which will:
        make a list of all possible moves by player A (Nm, maximum of 93 moves): getAvailableMoves(). Create a structure that will be able to represent a 'move', see below.     
        perform 100 simulations with all of the possible candidate moves: for each simulation
          make a clone of the root gamestate and randomize in this clone the information not seen by player A at this stage: 
            clone_and_randomize_gamestate() (which can use the clone_gamestate() function)
          for each possible move
            make a clone of the randomized copy from last step above, to test this move
            assume that first move is made (apply it to the new clone) and then 
            randomly (use the strat_random strategy) make moves 2+ (among legal moves) for each player alternately until 
            player A wins (1 point), loses (0), or there is a draw (0.5 points). 
           
        discard worst candidate moves, keeping Nm ^ (¾) moves (max 30). 
        perform 200 more sim with the remaining candidate moves (same approach as above)
        
        then prune to Nm ^ (½) moves (max 10), 
        perform 400 more sim, 
        
        then prune to Nm  ^ (¼) best moves (max 4). 
        perform 800 more sims (cumulative total of 1500 sims) 
        
        display results for 4 best moves to the console. 
        return the best move
      
      using a separate tool, analyse 4 best moves for each of the 100 game states to develop rules / decision tree / heuristics that would have generated the same best moves.
       
      re-do the exercise, starting from 100 manually created distinct 'defense' phase game states

      modify the 'MC single stage' strategy to also be applicable to an arbitrary starting game state to be able to use it as an "Interactive Mode AI assistant"
        this is similar to the MCTS below, but the 'tree' only has one parent node with pointers to all of its children nodes (93 max), and anywhere between 100 and 1500
        simulations for each children node. By contrast, the ISMCTS will have a 'deeper' tree and will concentrate more quickly on exploring 'promising' children nodes.
 * 
 * in mcts, use a pucb or puct approach with a prior prob of success of each move estimated from the expected advantage heuristic

in single stage monte carlo search strat, consider running 7 sim for each possible move, then discard any move with 0 win, do another sim for all remaining 
* possible moves, eventually discarding any move with prob of winning (using confidence interval) that is much less than winning % of the best candidate. 
* use normal approx to binomial dist to build conf int. put conditions in place to stop search such as max num of sim, or only one candidate left. see progressive pruning approach.

 * 
 * */
