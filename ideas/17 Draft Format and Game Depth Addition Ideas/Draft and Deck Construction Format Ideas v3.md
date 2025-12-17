# Oracle Tactical Card Game: Draft and Deck Construction Formats

---

## Format a: Sealed Deck

**Complexity Level:** Very Low  
**Reference URL:** https://magic.wizards.com/en/formats/sealed-deck

### Oracle Adaptation

With one full deck of 120 cards: deal 60 cards randomly to each player, and player can build their deck which must contain 40 cards exactly. With two decks (total of 240 cards): shuffle the two decks together, then deal 90 cards to each player and players can build their deck which must contain 40 cards exactly. Shuffle and play.

---

## Format f: Order/Species Draft (Implement 1)

**Complexity Level:** Low  
**Reference URL:** [Goblin Artisans: Two-Player Draft Formats](http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html)

### Oracle Adaptation

See Order/Species Draft Format notes in separate markdown document. Set aside non-champion cards from one deck. Shuffle champions (102 cards), then cut the top n cards and retain the bottom 102-n. Sort 102-n cards into 5 piles (Order draft) / 15 piles (Species draft). Randomly select first player. First player chooses one pile, second player chooses 2, first player chooses 2 (keep going 2 and 2 until no more piles remain). Each player must then choose their 34 champions from their draft (they get to choose the ones they want from the n cards cut at the beginning if they have less than 34 champions in their draft, but only to get to 34). Add draw/recall and cash cards as per rules for 'monochrome' pre-constructed deck. Calibrate n so that there is only a 20% chance that a player does not have 34 champions in their original draft.

Pile selection by Order (5 piles) or Species (15 piles); buffer system to ensure fairness. Currently in calibration/testing phase.

---

## Format g: Grid Draft (Implement 2)

**Complexity Level:** Low  
**Reference URL:** https://mtg.fandom.com/wiki/Grid_Draft

### Oracle Adaptation

**Using one deck (120 cards):** Pull 3 cash cards, give one to each player and discard one. Shuffle remaining 117 cards. Randomly select first player. Arrange 9 first cards as a 3×3 grid. First player picks one row or column (3 cards), and then second player must pick in the same orientation as the first player (e.g., one of the 2 remaining columns if first player picked a column), which is 3 cards as well. Repeat, alternating who picks first. 13 rounds × 3 cards each = 39 cards drafted + 1 cash card = formed deck of 40 cards. Shuffle and play.

**Using 2 decks (240 cards):** Shuffle all 240 cards. Pick first player randomly. Arrange cards in 4×4=16. First player picks one row or column (4 cards). Second player picks a row or column (3 or 4 cards). Repeat 15 times, alternating who picks first. Players each form a deck of exactly 40 cards with their drafted cards. Shuffle and play.

Perfect information draft (no hidden cards). Emphasizes denial and reading opponent's strategy.

---

## Format b: Minidraft

**Complexity Level:** Medium  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Oracle Adaptation

Requires: Shuffle the full deck of 120 cards.

1. Keep only the bottom 90 cards from the shuffled deck
2. Divide the 90 cards into a 45-card stack for each player.
3. Simultaneously, each draw a pack of 5 cards from the top of your stack, take one, and pass the pack of 4 remaining cards to your opponent.
4. From the pack of 4 cards you are passed, take one and 'burn' the remaining 3 by discarding them face down into a common discard pile in the centre of the table.
5. Repeat steps 2-3 until the original stacks are empty (9 rounds).
6. Each player now has 18 cards (9 1st picks and 9 2nd picks).
7. Shuffle the 54 cards from the 'burn' pile and split them between the 2 players, 27 cards each
8. Repeat steps 2-3 until there is no longer at least 5 cards left for another round (5 rounds). Each player will have drafted an extra 10 cards from this second drafting phase.
9. Each player will now have a total deck of 28 cards, which they must shuffle before playing.
10. Play with a reduced initial energy of 75 (instead of the standard 99) and a reduced luna budget of 22 (instear of the standard 30) 

---

## Format c: Winston Draft

**Complexity Level:** Medium  
**Reference URL:** https://magic.wizards.com/en/articles/archive/winston-draft-2005-03-25

### Oracle Adaptation

• Shuffle 102 champion cards, without looking at them.
• Choose someone to draft first, then the opponent put the top three cards from the deck face down next to it as three new small piles of one card each.
• The first player looks at the first small pile. They may choose to draft that pile or not.
• If they draft it, they replace that pile with a new face-down card from the deck.
• If they don't draft it, they put it back, add a new card from the deck face down, and move on to the next pile.
• They look at that pile and decide to draft it or not, replacing it with a new card if they draft it, adding a new card to it and moving on if they don’t.
• If they don’t want to draft the third pile, they add a card to it, then draft a random card from the top of the deck.
• Continue until all 102 cards have been drafted. 
• Each player contructs a 40-cards deck, with a maximum of four draw2 cards, three draw3 cards, and 1 cash card.
• In the unlikely situation that one player has drafted less than 32 champions (meaning the other drafted more than 70, more than twice as many), the player with less than 32 champions can blindly select from the other player's champion deck as many cards as required to reach 32 champions 
• Shuffle and play using the typical draft combo bonuses.

---

## Format h: Solomon Draft

**Complexity Level:** Medium  
**Reference URL:** [Solomon Draft - MTG Wiki](https://mtg.fandom.com/wiki/Solomon_Draft)

### Key Mechanics

Each player needs 3 packs shuffled together (90 cards pool); players alternate flipping 8 cards and dividing into two piles of any distribution; opponent chooses pile, divider gets remainder; final round has 10 cards. Divide and choose strategy tests both card evaluation and reading opponent. Psychological element of pile splitting. Can make uneven splits (7-1, 6-2, 5-3, 4-4, 8-0).

### Oracle Adaptation

Oracle's Solomon 7×7 somewhat similar to this; psychology of "I cut, you choose"; proven 2-player design.

---

## Format e: Booster Draft

**Complexity Level:** Medium  
**Reference URL:** [MTG Booster Draft | Magic: The Gathering](https://magic.wizards.com/en/formats/booster-draft)

### Key Mechanics

Pick 1 card from pack, pass remainder; 3 packs per player; build 40-card minimum deck with unlimited basic lands. Standard draft has 8 players passing packs in snake pattern (left-right-left). Hidden information creates tension. Most skill-testing limited format. Requires reading signals from passed cards.

### Oracle Adaptation

Oracle's Draft 1-2-3 format somewhat similar to this. Could be called "Draft 6×15".

---

## Format j: Rochester Draft

**Complexity Level:** High  
**Reference URL:** [Rochester Draft - MTG Wiki](https://mtg.fandom.com/wiki/Rochester_Draft)

### Key Mechanics

Lay out cards face-up and players take turns picking individual cards in snake draft order. Open all packs without looking, remove basic lands, shuffle together (84-90 cards); decide minimum deck size (30 or 40); players alternate drafting single cards in snake pattern. Complete information throughout. Emphasizes hate-drafting and reading opponent strategy. Very slow but maximum strategic depth for 2 players.

### Oracle Adaptation

Oracle: sequential snake draft of entire 120-card pool face-up; complete transparency.

---

## Format i: Winchester Draft

**Complexity Level:** Medium  
**Reference URL:** [Daily MTG – MTG News, Announcements, and Podcasts | Magic: The Gathering](https://magic.wizards.com/en/articles/archive/latest-developments/winchester-draft-2011-12-30)

### Key Mechanics

Shuffle 6 packs together (90 cards); create 4 face-up single-card piles; first player takes entire pile; add 1 card to each pile including empty spot; alternate until cards exhausted. Hybrid between Winston (pile growth) and Rochester (face-up cards); faster than Winston with more skill-testing decisions. Piles grow over time creating tension.

### Oracle Adaptation

pile selection with face-up growth.

---

## Format d: Peacemaker

**Complexity Level:** Medium  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

Modified Winchester with 6 piles; when taking pile, replace with new card then look at deck top and add it face-down to any of the six piles. Reduced card revelation decreases luck. Player chooses where to place new card creates bluffing layer. Face-down placement adds memory element. More strategic control than standard Winchester.

### Oracle Adaptation

Oracle: 6-pile draft with strategic card placement.

---

## Format k: Rotisserie Draft

**Complexity Level:** Very High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

All cards laid face-up in order for snake-style selection where players alternate picking single cards in rotating order until pool exhausted. Complete set knowledge from start. Extremely slow but deepest strategic draft. Requires tracking all picks and predicting opponent needs. Best for experienced drafters with time.

### Oracle Adaptation

Oracle: face-up snake draft of all 120 cards; complete set knowledge.

---

## Format m: Back Draft

**Complexity Level:** Medium  
**Reference URL:** [Goblin Artisans: Two-Player Draft Formats](http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html)

### Key Mechanics

Involves drafting the worst deck to give your opponent while they do the same. Inverse strategy where you want to saddle opponent with unplayable cards. After draft, players swap decks and play with what opponent built. Requires deep card evaluation to identify truly weak cards versus hidden gems.

### Oracle Adaptation

Could be called "Poison Draft"; inverse drafting strategy teaches card evaluation through opposition; fun casual variant.

---

## Format l: Vinci/Small World Draft

**Complexity Level:** Medium-High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

Shuffle packs and deal 5 cards face-up in row; each player starts with 5 tokens; furthest card is free, second costs 1 (put token on first card), third costs 2 (tokens on first and second), etc.; taking card also gains any tokens on it; slide cards and refill rightmost slot. Bidding mechanic using tokens as currency. Economic decisions on when to spend versus save. Taking weak cards early to collect tokens for later bombs. Tokens transfer between players.

### Oracle Adaptation

Oracle: Luna-based bidding on champion piles; auction/bidding mechanics.

---

## Format n: Simultaneous Bid

**Complexity Level:** Medium-High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

Shuffle 6 packs; deal face-up card to each of 5-6 piles marked 1-6; each player secretly sets 3d6 to chosen faces and reveals simultaneously; highest bid on each pile claims it; add new card to piles with no bids; replace empty piles and repeat. Simultaneous hidden bidding eliminates reaction advantage. Using dice as bidding tokens is elegant. Predicting opponent bids is core skill. Sometimes one player gets bomb, other gets multiple filler cards.

### Oracle Adaptation

Oracle: bid 3 Luna tokens across 5-6 champion piles; simultaneous secret bidding.

---

## Format o: Auction Magic

**Complexity Level:** High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

Shuffle packs into single deck; during game draw phase, reveal card and both players bid tokens; winner plays card immediately for free (or saves for later free play); pay tokens to opponent. Draft happens during actual gameplay, not before. Shortcutting mana economy changes card values drastically. Situational bidding based on board state (e.g., bidding high for blocker when behind). Paying opponent creates economic tension.

### Oracle Adaptation

Oracle: draw and bid each turn during actual game; in-game drafting.

---

## Format p: Live Booster Draft

**Complexity Level:** High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

Each player opens pack and adds one of each basic land; draft one card and pass left; first 7 picks form starting hand; each time you would draw during game, draft from next pack instead; open new single pack when all exhausted. Drafting happens during actual gameplay. First 7 picks create starting hand. Drawing cards means drafting new cards. Novel integration of draft and play phases. Much more gameplay from fewer packs.

### Oracle Adaptation

Could shorten the name to "Live Draft"; draft champions as you draw during game turns; in-game drafting.
