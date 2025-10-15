/*
   Heuristic approach to making decisions of which card(s) to play for both attacker and defender. '1 move look-ahead'
   Calculate player's advantage (the heuristic) and make the play / move (among all the possible moves) that maximizes the advantage following the play / move.

   Advantage = Energy Advantage * epsilon + Cards Advantage * gamma + Cash Advantage
   Energy Advantage = own energy - opponent energy + 100,000 if opponent energy = 0
   Cards Advantage = Nbr of Effective Cards in Own Deck - Nbr of Cards in Opponent Deck * Effective Adjustment (as we don't know the details of the opponent's hand)
   Cash Advantage = Own cash - Opponent cash

   Calibrate (optimization) epsilon and gamma by doing AI vs AI game sims with each AI using different values of epsilon and gamma and picking winning combination

   Are there heuristics parameters epsilon and gamma that make the heuristic approach equivalent to the 'balanced approach': I think these might be epsilon = 1 and gamma = 0

   Can the heuristic approach be a superset of the balanced approach? How? Should it be, or does it become too complex? Or is the 'balanced approach' already using 'optimal'
   epsilon and gamma parameters?

   Consider reducing weights on number of cards and cash advantage (possibly reducing them to the minimum of 0) gradually as the opponent’s energy gets lower.
   It’s okay (ideal) to win the game with no remaining cards in hand and no remaining / unspent cash.

   in heuristic strat, add a hand total power advantage metric. add actual hand combo power (based on best combos that can be achieved with cards in hand).
   estimate a potential (probability weighted) combo hand power (2 rounds look forward only) and add to the total hand power: eg player has only one card in hand and
   it's an orange card, calCulate prob of drawing another orange next round and say that's 35%, add 0.35 * 2 cards of same colour combo / expected cost of the 2 cards
   to the total hand power

*/
