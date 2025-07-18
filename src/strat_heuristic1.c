/*
 * Heuristic approach to making decisions of which card(s) to play for both attacker and defender. '1 move look-ahead'
 * Calculate player's advantage (the heuristic) and make the play / move (among all the possible moves) that maximizes the advantage following the play / move.
 * 
 * Advantage = Energy Advantage * epsilon + Cards Advantage * gamma + Cash Advantage
 * Energy Advantage = own energy - opponent energy + 100,000 if opponent energy = 0
 * Cards Advantage = Nbr of Effective Cards in Own Deck - Nbr of Cards in Opponent Deck * Effective Adjustment (as we don't know the details of the opponent's hand)
 * Cash Advantage = Own cash - Opponent cash
 * 
 * Calibrate (optimization) epsilon and gamma by doing AI vs AI game sims with each AI using different values of epsilon and gamma and picking winning combination
 * 
 * Are there heuristics parameters epsilon and gamma that make the heuristic approach equivalent to the 'balanced approach': I think these might be epsilon = 1 and gamma = 0
 * 
 * Can the heuristic approach be a superset of the balanced approach? How? Should it be, or does it become too complex? Or is the 'balanced approach' already using 'optimal'
 * epsilon and gamma parameters?
 * 
 */
