/* 
   store entire tree and cloned gamestates on the heap (malloc / free)
   
   use an array of pointers to children nodes as the tree node approach. initialize the array size to the total number of possible moves (also a dynamically created array 
   stored in the parent node alongside the array of children nodes pointers) (or all of the moves we want to consider, based on heuristic) and fill in the move information 
   as soon as it is created
   
   store a pointer to parent node in the child node
   store stats required to make uct decisions in parent node as well
   store gamestate in child node
   create child node only upon its first visit as its related move and stats are at the parent level
 * 
 * information for DoMove() function:      
Play Game:
  setup game
  turn = 0
  do
    turn++
    draw 1 card if not first player and first turn
    
    set turn phase = attack
    playerToMove = currentplayer
    
    decidemove(attacker): this is where we could call the AI in the real game
      getlistofavailablemoves(attacker) (called by decidemove())
    applymove(attacker) (put champion cards in combat area)
    if combat
      
      set turn phase = defense
      playerToMove = notcurrentplayer
      
      decidemove(defender): this is where we could call the AI in the real game 
        getlistofavailablemoves(defender) : called by decidemove()
      applymove(defender) (put champion cards in combat area)
      resolve combat, apply damage
      move cards from combat areas to discard      
    endif
    
    if defenderenergy = 0, 
      set end of game flag and indicate winning player in gamestate
    else
      collect1luna
      discardto7cards
      chgcurrentplayer
    end if
    
  while not end of game
  
Virtual game played by AI:
DoMove(move) function behavior:
  if turnphase == attack:
    applymove(attacker) (put champion cards in combat area)
    if combat
      set turn phase = defense
      playerToMove = notcurrentplayer
    else
      collect1luna
      discardto7cards
      chgcurrentplayer
      turn++
      draw1card for newCurrentPlayer
      set turn phase = attack (redundant here)
      playerToMove = theNewCurrentPlayer
    endif
  else (turnphase == defense)  // here, we know that we are in combat
    applymove(defender): put champion cards in combat area
    resolve combat, apply damage
    move cards from combat areas to discard
    if defenderenergy = 0
      set end of game flag and indicate winning player in gamestate
    else
      collect1luna (for currentplayer (attacker), not the playerToMove (defender))
      discardto7cards
      chgcurrentplayer
      turn++
      draw1card for newCurrentPlayer
      set turn phase = attack
      playerToMove = theNewCurrentPlayer (redundant here)
    endif
  end if
  
content of 'move' struct:
  movetype = one of 3 enum values, DONOTHING, COMBAT, DRAWCARD
  to be used when there is an attack or defense (combat):
    num_attacking_champions: 0 to 3
    champion1 ID (card ID in full deck)
    champion2 ID
    champion3 ID
  to be used when a draw card is played:
    drawcardID
 * 
 * 
 * 
 * 
 * 
 * 
 * MCTS Leaf Roll out:
 * Assuming move from leaf node that's just been developped has been applied.
 * rollOut(&gamestate):
 * 
 * switch (turnPhase)
 * case attack
 *   decideMove(attacker): random, balanced or heuristic (nont stochastic method). can use a ROLLOUTPHASE = TRUE flago to know a fall back 'strat' should be used (and not a nestest MCTS)
 *     this will call 'getlistofavailablemoves'
 *   applymove(attacker)
 *   if combat
 *     decidemove(def)
 *     applymove(def)
 *     resolvecombat
 *     movecards from combat to discard
 *   endif

 * case defense
 *   decidemove(defender)
 *     calls getlistofavailmoves(def)
 *     use random or a non-stochastic approach
 *   applymove(def)
 *   resolve combat
 *   move cards from combat to discard
 * end switch/case
 * 
 * if defenergy = 0
 *   set end of game flag and indicate winning player
 * else
 *   collect1luna
 *   discardto7cards
 *   chgcurrentplayer
 * end if
 * 
 * while not endofgame
 *   turn++
 *   draw1card
 *   decidemove(attacker)
 *   ...
 * wend
 * 
 * end of rollOut() function
 * */
