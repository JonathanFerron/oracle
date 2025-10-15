/*
   Balanced rules strategy

   Effective hand size = actual hand size + 1 * NbrOfPige2CardsInHand + 2 * NbrOfPige3CardsInHand, if player has enough cash to play the 'pige' cards
   Effective actual cash = actual cash - total cost of pige cards in hand

   A: Attacker:
   A1. Get available moves
   A2. Play 0 cost cards D4+1 and D6+1 if in hand (up to 3 max)
   A3. if enough cash,
       - If hand has less than 7 cards, play pige 2 cards if held. If hand has less than 6 cards, play pige 3 cards if held.
           Unless we're in late stage of game and 1 or 2 attack rounds would likely reduce opponent energy to 0 (if attacks can be afforded)
   A4. if enough cash, Play 1 or 2 best attacker cards. decide whether to play 0, 1 or 2 attackers using the following constraints:

   Target effective hand size after attack play of
     5 when opponent has 99 energy
     4 when opponent has 81 energy
     3                   63
     2                   45
     1                   27
     0                   8

   Enemy Energy = 91 / 5 * TargetEffectiveNbrCardsLeft + 8 => Target Effective Nbr Cards left = (Enemy Energy - 8) * 5/91
   NbrOfCards to play = round (Efective cards in hand - target effective nbr cards left)

   Play that many cards as long as actual cash - cost of those cards >= target cash

   Target effective cash left after attack play (before collecting 1 luna at end of turn)
     19 when opponent has 99 energy
     15                   80
     11                   61
     7                    42
     4                    27
     0                    8

   Enemy Energy = 91/19 * effective cash + 8 => Target Effective Cash Left = (Enemy Energy - 8) / (91 / 19)

   D: Defender
   D1: get available moves
   D2: Play 0 to 2 best defender cards if enough cash, using following guideline:

   Use similar target hand size left and cash left as for attack phase to decide number of defenders

   Prioritize D4+0 card with 0 cost if in hand. Otherwise, prioritize '+0' cards as other cards have higher efficiency when used for attack.
   Prioritize cards with best defense efficiency. Pick the cards with lowest attack efficiency if there is a tie between 2 defense cards with same defense efficiency.

   Make sure E[Total Def] <= E[Total Attack] - Beta * StdDev [Total Attack], may want to try Beta = 1.0
     This is so as not to waste defence cards and cash, which happens when Total Defence > Total Attack

   SD[A1+A2+A3] = sqrt(Var[A1+A2+A3])
   Var[A1+A2+A3] = V[A1] + V[A2] + V[A3] as the attack dices are independent = V[DiceA1] + V[DiceA2] + V[DiceA3]
   V[Dn] = (n * n - 1) / 12 where n is the number of faces of the dice
   Could calc V[Ai] for all 102 champion and store in the fullDeck[] array

   Thought for further development: divide cash and cards budget for a round between attack and defense phases based on 'offensive' vs 'defensive' parameter


   General Notes:
   Struct to contain decision rules / strategy to be used for each player (separately) whenever decisions can be made.
   Decisions:
   - decide what to play as attacker
      - attack: how many and which champion
      - pige / rappelle: which card
      - pass
   - decide what to play as defender
      - how many champions (0-3) and which
   - for 2nd player (player B), decide what to do when presented with option to mulligan:
       how many to mulligan and which.
         available info: own hand, overall content of full deck, energy = 99, cash = initcash for both a and b
   - as attacker at end of turn, decide which cards to discard at the 'discard to 7 cards' step

   available info to make decisions as active player:
   - own hand details
   - own discard and other player's discard details
   - own and other player energy and cash
   - overall content of own deck if it has been formed from shuffling discard (but ordering unknown)
   - overall content of opponent deck + hand if it has been formed from shuffling discard (but no info on ordering nor which are in hand vs deck)
   - overall content of full deck (e.g. there can't bemore than 9 'pige 2 / rappelle 1' cards in both ownership)

   Available info to make decision as defender:
   - all info above + details of 1 to 3 champion cards that have been played by attacker



  // Enhanced AI with situational awareness
  int choose_smart_card(Player* player, MTRand* rng, int prefer_attack, int available_cash)
  {
    if (available_cash <= 0) return -1;

    int best_card = -1;
    double best_score = -1.0;

    for (int i = 0; i < player->hand_size; i++) {
        if (player->hand[i].cost <= available_cash) {
            double attack_eff = calculate_attack_efficiency(&player->hand[i]);
            double defense_eff = calculate_defense_efficiency(&player->hand[i]);
            double power = calculate_power_rating(&player->hand[i]);

            // Weight the score based on situation
            double score;
            if (prefer_attack) {
                score = attack_eff * 0.6 + defense_eff * 0.2 + power * 0.2;
            } else {
                score = attack_eff * 0.2 + defense_eff * 0.6 + power * 0.2;
            }

            // Add small random factor for variety
            score += genRand(rng) * 0.1;

            if (score > best_score) {
                best_score = score;
                best_card = i;
            }
        }
    }

    return best_card;
  }

*/
