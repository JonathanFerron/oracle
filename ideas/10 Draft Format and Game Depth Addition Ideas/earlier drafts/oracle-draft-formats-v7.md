# Oracle Tactical Card Game: Draft and Deck Construction Formats

**Format Order:** Sorted by recommended implementation priority

---

## TIER 1: ALREADY DESIGNED (Implement First)

---

## Solomon Draft

**Complexity Level:** Medium  
**Reference URL:** [Solomon Draft - MTG Wiki](https://mtg.fandom.com/wiki/Solomon_Draft)

### Oracle Adaptation

**Setup:**
- Full 120 cards deck, shuffled
- Randomly determine Player A and Player B
- Player A drafts first, Player B chooses who plays first in games

**Draft Process:**
- Player A flips over the top 8 cards from the deck
- Player A divides these 8 cards into two piles of any distribution:
  - Can be even (4-4) or uneven (7-1, 6-2, 5-3, 8-0)
- Player B chooses one pile and adds it to their drafted cards
- Player A gets the remaining pile
- Roles alternate: Player B splits next, Player A chooses
- Continue until all cards are drafted

**Deck Construction:**
- Build a 40-card decks
- If a player has drafted less than 40 cards (meaning the other player drafted more than 80), the player with less than 40 cards can select blindly from their opponent's draft as many cards as required to get to 40
- Shuffle and play using draft combo bonuses

**Strategic Depth:**
- Tests card evaluation skills
- Requires reading opponent's preferences
- Balancing powerful cards with mediocre ones in pile splits
- Memory element (tracking what's been drafted by both players)

**Why It Works:**
- Not just picking a single card, but analyzing pile values
- Must predict opponent's deck strategy
- Social and psychological elements in pile splitting

Players alternate revealing cards and using "I cut, you choose" psychology. Complete information and tactical pile-splitting decisions.

---

## Grid Draft

**Complexity Level:** Low  
**Reference URL:** https://mtg.fandom.com/wiki/Grid_Draft

### Oracle Adaptation

**Using one deck (120 cards):** Pull 3 cash cards, give one to each player and discard one. Shuffle remaining 117 cards. Randomly select first player. Arrange 9 first cards as a 3×3 grid. First player picks one row or column (3 cards), and then second player must pick in the same orientation as the first player (e.g., one of the 2 remaining columns if first player picked a column), which is 3 cards as well. Repeat, alternating who picks first. 13 rounds × 3 cards each = 39 cards drafted + 1 cash card = formed deck of 40 cards. Shuffle and play.

**Using 2 decks (240 cards):** Shuffle all 240 cards. Pick first player randomly. Arrange cards in 4×4=16. First player picks one row or column (4 cards). Second player picks a row or column (3 or 4 cards). Repeat 15 times, alternating who picks first. Players each form a deck of exactly 40 cards with their drafted cards. Shuffle and play.

Perfect information draft (no hidden cards). Emphasizes denial and reading opponent's strategy.

---

## Booster Draft

**Complexity Level:** Medium  
**Reference URL:** [MTG Booster Draft | Magic: The Gathering](https://magic.wizards.com/en/formats/booster-draft)

### Oracle Adaptation

**Setup:**
- Each player receives 45 cards after shuffling one champion deck (keep 90 of a toral of 102 champion cards)
- Split the 45 cards in three packs of 15 without looking

**Draft Process:**
- **Pack 1:** Open, pick 1 card, pass. Repeat until pack is empty
- **Pack 2:** Open, pick 1 card, pass. Repeat until pack is empty  
- **Pack 3:** Open, pick 1 card, pass. Repeat until pack is empty

**Deck Construction:**
- Form a 40-card deck from drafted pool, including a maximum of four Draw2 cards, three Draw3 cards and one Cash card.
- Remaining cards form sideboard (which could be used to re-form deck for games 2 and 3 of a 'best 2 of 3' series)

**Strategic Elements:**
- **Hidden information:** Don't know what opponent has
- **Signal reading:** What cards are passed reveals opponent's preferences
- **Wheel picks:** Predicting what comes back around
- **Hate drafting:** Taking cards to deny opponent
- **Color / Order / Species commitment:** Balancing early flexibility vs. late synergy
- **Pick orders:** Understanding relative card values
- **Gradual process:** Building functional deck while drafting

**Why It's Popular:**
- Most skill-testing Limited format
- Requires both drafting and deck-building skills
- Equal footing (everyone opens same amount of product)
- Synergy and archetype building
- Reading signals between drafters
- Works well for organized play and tournaments

**Time Commitment:**
- Draft: ~30-45 minutes
- Deck building: ~15 minutes
- Games: ~1-2 hours for full tournament round

Sequential pick-and-pass of cards with hidden information.

---

## TIER 2: EASY WINS (Add Next)

---

## Sealed Deck

**Complexity Level:** Very Low  
**Reference URL:** https://magic.wizards.com/en/formats/sealed-deck

### Oracle Adaptation

With one full deck of 120 cards: deal 60 cards randomly to each player, and player can build their deck which must contain 40 cards exactly.

With two decks (total of 240 cards): shuffle the two decks together, then deal 90 cards to each player and players can build their deck which must contain 40 cards exactly. 

Shuffle and play using either custom deck format or draft format combo bonuses.

---

## Winchester Draft

**Complexity Level:** Medium  
**Reference URL:** [Winchester Draft - MTG Wiki](https://mtg.fandom.com/wiki/Winchester_Draft)

### Oracle Adaptation

**Setup:**
- Shuffle full deck of 120 cards, and then set aside the top 20 cards, keeping a 100 cards pool
- Randomly determine first drafter
- Second drafter chooses who plays first in games

**Draft Process:**
- Create 4 face-up piles, each starting with 1 card from deck
- **On your turn:**
  - Choose one of the four piles
  - Take all cards in that pile
  - Add 1 new card from deck face-up to each pile (including the empty spot)
- Alternate turns until all cards are drafted
- Piles grow asymmetrically over time
- Players may end with different pool sizes (piles vary in size)
- If a player has less than 40 cards in their pool, they may blindly select as many cards as required to get a total of 40 from the buffer of 20 cards that were set aside at the beginning

**Deck Construction:**
- Build a 40-card deck using drafted cards
- Shuffle and play using draft combo bonuses

**Strategic Elements:**
- **Face-up information:** See all available cards
- **Quantity vs. Quality:** Small pile with bomb vs. large pile of filler
- **Pile growth tracking:** Predicting which piles will become valuable
- **Timing decisions:** Take now or wait for pile to grow?
- **Opponent prediction:** Will they take that pile before it grows more?
- **Hate drafting:** Deny opponent key cards by taking their piles
- **Color reading:** Can see opponent's strategy developing
- **Counter-drafting:** Draft to beat opponent's known deck

**Comparison to Other Formats:**
- **vs. Winston:** Face-up (no hidden piles), sees opponent's picks, faster
- **vs. Solomon:** No pile splitting decisions, but cards accumulate naturally
- **vs. Rochester:** Much faster, but with natural pile-growth mechanic

**Advantages:**
- Faster than Solomon Draft
- More skill-testing than Winston Draft
- Face-up reduces pure luck
- Pile growth creates interesting tension
- Can counter-draft opponent's strategy
- Good hybrid between speed and strategy

**Disadvantages:**
- Can have very large piles that slow decision-making
- Possible for uneven pool sizes
- Less mystery/surprise than Winston Draft

**Variations:**
- **3 piles:** Makes drafting more fine-grained (go with 99 or 102 card pool instead of 100)
- **5-6 piles:** More complexity and choices (go with 102 card pool for 6 piles approach)
- **2 cards per pile:** Faster pile growth (go with 96 or 104 card pool at the start)
- **Keep all 120 cards** More substantial decks, closer to custom deck power level and hence may warrant custom deck combo bonuses to balance

Pile selection with face-up growth: 4-pile system where players alternate choosing entire piles, cards added to each pile after every pick, visible strategy development, and natural quality-vs-quantity tension.

---

## TIER 3: SPECIALIZED FORMATS

---

## Order/Species Draft

**Complexity Level:** Low  
**Reference URL:** [Goblin Artisans: Two-Player Draft Formats](http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html)

### Oracle Adaptation

See Order/Species Draft Format notes in separate markdown document. Set aside non-champion cards from one deck. Shuffle champions (102 cards), then cut the top n cards and retain the bottom 102-n. Sort 102-n cards into 5 piles (Order draft) / 15 piles (Species draft). Randomly select first player. First player chooses one pile, second player chooses 2, first player chooses 2 (keep going 2 and 2 until no more piles remain). Each player must then choose their 34 champions from their draft (they get to choose the ones they want from the n cards cut at the beginning if they have less than 34 champions in their draft, but only to get to 34). Add draw/recall and cash cards as per rules for 'monochrome' pre-constructed deck. Calibrate n so that there is only a 20% chance that a player does not have 34 champions in their original draft.

Pile selection by Order (5 piles) or Species (15 piles); buffer system to ensure fairness. Currently in calibration/testing phase.

---

## Winston Draft

**Complexity Level:** Medium  
**Reference URL:** https://magic.wizards.com/en/articles/archive/winston-draft-2005-03-25

### Oracle Adaptation

- Shuffle 102 champion cards, without looking at them.
- Choose someone to draft first, then the opponent put the top three cards from the deck face down next to it as three new small piles of one card each.
- The first player looks at the first small pile. They may choose to draft that pile or not.
- If they draft it, they replace that pile with a new face-down card from the deck.
- If they don't draft it, they put it back, add a new card from the deck face down, and move on to the next pile.
- They look at that pile and decide to draft it or not, replacing it with a new card if they draft it, adding a new card to it and moving on if they don't.
- If they don't want to draft the third pile, they add a card to it, then draft a random card from the top of the deck.
- Continue until all 102 cards have been drafted. 
- Each player contructs a 40-cards deck, with a maximum of four draw2 cards, three draw3 cards, and 1 cash card.
- In the unlikely situation that one player has drafted less than 32 champions (meaning the other drafted more than 70, more than twice as many), the player with less than 32 champions can blindly select from the other player's champion deck as many cards as required to reach 32 champions 
- Shuffle and play using the draft combo bonuses.

---

## TIER 4: ADVANCED/VARIANT

---

## Rotisserie Draft

**Complexity Level:** Very High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Oracle Adaptation

**Setup:**
- ALL cards laid face-up, one pile at a time
- Shuffle all 120 cards from one deck, set aside top 24 cards and keep 96 card pool
- Form 6 piles of 16 cards each
- Complete information from the start

**Draft Process:**
- Choose first drafter randomly
- Second drafter reveals first pile and arrange in 4 columns x 4 rows
- Players alternate picking cards, one card at a time, until all 16 are picked
- Change the roles of each player and repeat for the second pile
- Do this same process for the remaining piles (each player will end up with 48 cards)
- Cards remain face-up and visible throughout

**Deck Construction:**
- Build a deck of 40 cards from the 48 drafted
- Shuffle and play with the draft combo bonuses

**Strategic Elements:**
- **Complete set knowledge:** See every available card from start
- **Maximum information:** Know exactly what's available and what's taken
- **Deep planning:** Can strategize entire draft arc from beginning
- **Hate drafting critical:** Precisely deny opponent's synergies
- **Archetype identification:** Plan your deck and counter opponents' simultaneously
- **Pick tracking:** Mental tracking of all picks is essential
- **Color signaling:** Opponent immediately sees your direction
- **Synergy blocking:** Can prevent opponents from assembling combos
- **Pivot decisions:** When to commit vs. stay flexible
- **Turn optimization:** Value of each pick position

**Advantages:**
- Deepest strategic draft format
- Perfect information enables advanced planning
- No luck in availability (everything is visible)
- Ultimate skill test
- Great for experienced drafters who know card pool well

**Disadvantages:**
- **Extremely slow:** Slowest draft format
- Requires complete card pool familiarity
- Prone to analysis paralysis
- Can feel overwhelming with too many options
- Requires serious time commitment (2+ hours for draft alone)
- Less "discovery" excitement than hidden-information formats

**Use Cases:**
- Expert-level drafters with time
- Groups who know card pool intimately
- When you want zero randomness
- Cube drafts where players know every card
- Competitive preparation (simulating matchups)

**Variations:**
- **Time limits per pick:** Prevents excessive analysis
- **Graduated time limits:** More time early, less later
- **Online/asynchronous:** Draft via Google Docs over days

---

## Peacemaker Draft

**Complexity Level:** Medium  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Oracle Adaptation

**Setup:**
- Shuffle entire deck of 120 cards, set aside top 22 cards and keep bottom 98 cards
- Create 6 face-up piles, starting with 1 card each

**Draft Process:**
- Randomly choose the first drafter
- On your turn:
  - Choose and take one of the six piles
  - Replace it with a new card from the deck face-up
  - Draft a second card and place the card face up on any of the six piles (your choice)
  - **Then:** Look at the top two cards of the deck
  - Place these two cards face-down on ANY of the six piles (your choice, each card can go on the same, or different, piles)
- Alternate turns until all cards drafted
- Piles grow with 50-50 mix of face-up and face-down cards
- If a player has less than 40 cards in their pool, they may blindly select as many cards as required to get a total of 40 from the buffer of 22 cards that were set aside at the beginning

**Deck Construction:**
- Build a 40-card deck using drafted cards
- Shuffle and play using draft combo bonuses

**Strategic Elements:**
- **Modified Winchester:** Adds hidden information layer
- **Strategic placement:** Choose which pile gets the face-down card
- **Bluffing potential:** Put strong card in pile you want to discourage opponent from taking
- **Memory element:** Track which piles have face-down cards
- **Information asymmetry:** You know the face-down card you placed, opponent doesn't
- **Deception:** Place weak card face-down on pile with strong face-up cards
- **Value assessment:** Weigh known face-up cards vs. unknown face-down cards
- **Risk/reward:** Take pile with unknown cards or wait for full information?

**Comparison to Winchester:**
- Adds face-down card placement for strategy
- Reduces pure luck of card revelation timing
- Player has agency over where new cards go
- Bluffing and deception layer

**Advantages:**
- Less luck-dependent than Winchester
- Player choice in card placement adds strategy
- Hidden information creates intrigue
- Can manipulate opponent's decisions
- Faster than Solomon (no pile-splitting time)

**Disadvantages:**
- More complex than Winchester
- Memory demands (tracking face-down cards)
- Longer than pure face-up formats
- Analysis paralysis from 6 piles + placement decision

**Why the Name:**
- Peacemaker implies negotiation/positioning
- Refers to strategic placement that "makes peace" with RNG

---

## FORMATS NOT RECOMMENDED FOR IMPLEMENTATION

---

## Minidraft

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
10. Play with a reduced initial energy of 75 (instead of the standard 99) and a reduced luna budget of 22 (instead of the standard 30) 

---

## Rochester Draft

**Complexity Level:** High  
**Reference URL:** [Rochester Draft - MTG Wiki](https://mtg.fandom.com/wiki/Rochester_Draft)

### Oracle Adaptation

**Setup:**
- 17 cards revealed at a time
- 6 rounds (of 17 cards) total for full draft

**Draft Process:**
- Shuffle all 102 champion cards and split in 6 piles of 17 cards without looking
- Randomly choose a first drafter
- Second drafter player takes a pile and lays all 17 cards face-up on table
- Players given ~20 seconds to review all cards
- Players alternate picking cards, one card at a time, until all 17 are picked
- Change the roles of each player and repeat for the second pile
- Do this same process for the remaining piles
- Cards remain face-up and visible throughout

**Deck Construction:**
- Build a 40-card deck using drafted cards plus a maximum of four draw2 cards, three draw3 cards, and one cash card
- Shuffle and play using custom deck combo bonuses

**Strategic Elements:**
- **Complete information:** See every card in every pack
- **Perfect knowledge:** Know exactly what opponents drafted
- **Hate drafting crucial:** Can precisely deny opponent's needs
- **Signal sending:** Make your colors obvious early
- **Matchup building:** Build specifically against known opponent decks
- **Pre-sideboarding:** Can maindeck narrow answers since you know opponent's cards
- **Memory intensive:** Track all picks across all packs

**Advantages:**
- Highest skill ceiling
- Complete transparency enables advanced strategy
- Great teaching format for newer drafters
- No luck in what you see

**Disadvantages:**
- **Very slow:** 4-8 times longer than Booster Draft
- Analysis paralysis can occur
- Requires familiarity with card pool for reasonable pace
- Can feel like "work" rather than "play" for casual groups

**Historical Context:**
- One of the original Magic draft formats
- Named after Rochester, NY where first showcased at convention (1993-94)
- Previously used in high-level competitive play
- Now mostly replaced by Booster Draft in tournaments due to time concerns

Complete set knowledge from start, sequential single-card picks, perfect information for matchup-based deck building.

---

## INFORMATIONAL/REFERENCE FORMATS (Not Adapted for Oracle)

---

## Poison Draft (Back Draft)

**Complexity Level:** Medium  
**Reference URL:** [Goblin Artisans: Two-Player Draft Formats](http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html)

### Key Mechanics

**Setup:**
- Standard draft setup (typically 3 packs per player)
- Any normal draft format structure (Booster, Winchester, etc.)

**Draft Process:**
- Draft normally BUT with inverted goal
- Try to draft the WORST possible deck
- Goal: Give opponent the most unplayable pool possible
- After draft completion: **Players swap decks**
- Build and play with the deck your opponent drafted for you

**Strategic Elements:**
- **Inverse card evaluation:** Identifying truly weak cards vs. deceptively playable ones
- **Hidden gems:** Cards that look bad but have secret value
- **Mana screw potential:** Draft bad mana curves, conflicting colors
- **Trap cards:** Cards that seem good but are actually terrible
- **Color hosing:** Force opponent into unplayable color combinations
- **Synergy prevention:** Ensure no cards work together well
- **Playable avoidance:** Can't just draft unplayables (opponent might make them work)
- **Double-think:** What looks bad to me might be usable to a skilled player
- **Meta-gaming:** Understanding opponent's skill level affects drafting

**Why It Works:**
- Tests card evaluation from opposite perspective
- Hilarious and memorable games
- Educational: Learn what makes cards actually bad
- Great for groups that draft same set repeatedly
- Challenges assumptions about card quality

**Deck Construction:**
- Receive opponent's drafted pool
- Build best possible deck from what you're given
- Minimum 40 cards typically
- Strategic challenge: Make silk purse from sow's ear

**Advantages:**
- Novel and entertaining experience
- Teaches card evaluation through opposition
- Great with playgroups that need variety
- Lower stakes, higher fun
- Works well as casual/party format

**Disadvantages:**
- Can lead to unfun games if one player way better at "bad drafting"
- Sometimes hard to tell what's truly unplayable
- May require re-drafting if both pools are genuinely terrible
- Can take skill out of gameplay (both decks are bad)

**Variations:**
- **Chaff Draft:** Draft worst cards, keep your own deck (see who can win with trash)
- **Chaos Poison:** Use random packs from different sets
- **Timed Poison:** Add time pressure to picks
- **Blind Poison:** Draft face-down packs

**Alternative Name:**
- "Back Draft" - you're drafting backwards/for opponent

### Oracle Adaptation

Oracle could implement draw-and-bid-each-turn during actual game: on draw step reveal card, players bid Luna funds, winner plays card for free (or saves), economic tension as payment goes to opponent, in-game drafting integrated with gameplay.

---

## Live Booster Draft

**Complexity Level:** High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

**Setup:**
- Each player needs booster packs (1 per player minimum for short games)
- Each player opens a pack and adds one of each basic land to it
- Shuffle packs WITHOUT looking at contents
- Randomly determine starting player

**Draft Process:**
- **Initial Draft:**
  - Pick 1 card from top of your pack
  - Pass pack left (or to opponent in 2-player)
  - Continue until you've drafted 7 cards total
  - These 7 cards form your STARTING HAND for the first game
- **During Gameplay:**
  - Each time you would draw a card: draft a card instead
  - To draft: pick 1 card, pass remainder
  - If you draw extra cards (e.g., from spell effect), take extra packs from right
  - When packs skip players due to extra draws, it's fine
  - When all packs are empty, open a single new pack

**Library Interactions:**
- If you would search/look at library: shuffle next pack, interact with that
- If you would scry: can ignore (or treat next pack as library)
- Library-ordering effects use next pack as the "library"

**Deck Construction:**
- Continuously evolving during game
- No minimum deck size (starts with 7-card "deck")
- Builds toward 30-40 cards by end of game
- Unlimited basic lands available

**Strategic Elements:**
- **First 7 picks = starting hand:** Different priorities than normal draft
- **Dual-purpose evaluation:** Is this good now AND good later?
- **Curve considerations:** Need playable early cards immediately
- **Mana fixing critical:** Must draft lands along with spells
- **Anticipating draws:** What will I need in 2-3 turns?
- **Reading opponent's strategy:** See their deck develop in real-time
- **Hate drafting during game:** Deny opponent cards they desperately need
- **Situational drafting:** Draft based on current board state
- **Resource management:** Balancing immediate needs vs. long-term power

**Land Drafting:**
- Must draft basic lands as regular picks
- Double-color spells require drafting 2+ sources of that color
- Dual lands become incredibly valuable
- Mana fixing is active gameplay, not automatic

**Novel Aspects:**
- Drafting happens during actual gameplay
- Your deck literally builds as you play
- Can respond to opponent's board state with draft choices
- Much more gameplay from fewer packs (1 pack = full game)
- "Drawing" a card means making a draft decision

**Advantages:**
- Incredibly novel experience
- Maximum gameplay from minimum product (1 pack per player works!)
- Exciting to see deck evolve during play
- Intense decision-making every draw step
- Can be replacement for Mini-Master (similar pack count, better gameplay)
- Works with any set/cube

**Disadvantages:**
- Very different from normal Magic
- Can be overwhelming (draft decisions mid-combat)
- Slower than normal games (draft decisions take time)
- Requires mental flexibility
- Some players find it exhausting

**Multi-Game Options:**
1. **Single game:** Play once with 1 pack per player
2. **Replay with land:** After Game 1, add unlimited basics, replay same pool
3. **Combine pools:** Play 2 drafts, then combine into 30-40 card deck for Game 3
4. **Continue drafting:** Keep combining pools for increasingly powerful deck

**Playtesting Notes:**
- 1 booster per player is enough for most games
- Having basic lands in draft packs works perfectly
- Drafted lands are crucial game decisions
- Pack-as-library interactions are intuitive enough
- Huge upgrade over Mini-Master format
- Can replay deck with added lands for Game 2
- Combining two draft pools into single deck works well

**Group Size Variants:**
- **2 players:** Duel format
- **3 players:** Three-way free-for-all or "Three-Headed In-Fighting"
- **4 players:** Two-Headed Giant or two duels
- **5+ players:** Multiplayer formats (Star, Conspiracy, Emperor)

**Extended Play:**
- With 5+ players: Each player does live draft vs. different opponent each round
- Round 1: Live draft → play
- Round 2: Live draft with new opponent → play
- Round 3: Combine pools → play 30-card decks
- Round 4: Live draft again → play
- Round 5: Combine all pools → play 40-card decks
- "Maximum 2-player play from six booster packs" - quote from testing

### Oracle Adaptation

Oracle could implement draft-champions-as-you-draw: first 7 picks form starting hand, each subsequent "draw" is a draft pick during game turn, build deck dynamically during gameplay, in-game drafting with situational decisions based on board state.

---d implement inverse drafting strategy: draft normally but aiming for worst possible cards, after completion players swap drafted pools, build and play with opponent's sabotaged deck, teaches card evaluation through opposition, fun casual variant.

---

## Vinci Draft (Small World Draft)

**Complexity Level:** Medium-High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

**Setup:**
- Shuffle all booster packs together
- Deal 5 cards face-up in a row next to the deck
- Each player starts with 5 tokens

**Draft Process:**
- Players take turns selecting a single card from the row
- **Cost structure:**
  - Position 1 (furthest): FREE (no tokens)
  - Position 2: Costs 1 token (place on Position 1)
  - Position 3: Costs 2 tokens (place on Positions 1 and 2)
  - Position 4: Costs 3 tokens (place on Positions 1, 2, and 3)
  - Position 5: Costs 4 tokens (place on Positions 1, 2, 3, and 4)
- **When taking a card:**
  - Collect any tokens already on that card
  - Slide remaining cards to fill gap
  - Reveal new card in rightmost (Position 5) slot

**Deck Construction:**
- Standard Limited deck building
- Minimum 40 cards typically

**Strategic Elements:**
- **Economic decisions:** When to spend vs. save tokens
- **Token economy:** Taking weak cards early to collect tokens for later bombs
- **Bidding timing:** Spend maximum now or wait for discount?
- **Value assessment:** Is this card worth 4 tokens or can I get it for 2 next turn?
- **Opportunity cost:** Pay premium for bomb or let it slide and collect tokens?
- **Token transfer:** Your tokens become opponent's if they take that card
- **Future planning:** Saving tokens for anticipated bombs
- **Risk evaluation:** Will opponent take this if I wait?

**Named After:**
- Small World board game (Days of Wonder) - more well-known
- Originally from Vinci board game (1999) where bidding mechanic debuted
- Bidding mechanic adapted to card drafting context

**Advantages:**
- Elegant token-based economy
- Deep strategic decisions without complex rules
- Interesting tension between spending and saving
- Can get powerful cards if willing to pay
- Creates natural balancing (spending tokens helps opponent)

**Disadvantages:**
- Time-consuming (evaluating economic decisions)
- Can feel "mathy" for some players
- Token management adds bookkeeping
- Sometimes you just can't afford the card you want

**Variations:**
- Start with different token amounts (3-7)
- More/fewer card positions (3-6)
- Different cost structures

### Oracle Adaptation

Oracle could implement Luna-based bidding on champion piles: reveal 5-6 face-up card positions, players use Luna tokens to bid on positions with escalating costs, taking cards also earns you any tokens on them, auction/economy mechanics integrated into drafting.

---

## Simultaneous Bid Draft

**Complexity Level:** Medium-High  
**Reference URL:** http://goblinartisans.blogspot.com/2016/04/two-player-draft-formats.html

### Key Mechanics

**Setup:**
- Shuffle 6 packs together (90 cards)
- Deal 1 face-up card to each of 5-6 piles
- Mark piles with numbers or dice (1-6)
- Each player has 3d6 to use as bidding tokens

**Draft Process:**
- **Each round:**
  - Each player secretly sets their 3d6 to any faces (1-6)
  - Both players reveal their dice simultaneously
  - For each pile: player who bid MORE dice wins that pile
  - Tied piles: No one wins (see Tiebreaker Variations)
  - Add new card to each pile that had no bids
  - Replace empty piles with 1 new card each
- Continue until all cards drafted

**Deck Construction:**
- Standard Limited deck building
- Minimum 40 cards typically

**Strategic Elements:**
- **Simultaneous hidden bidding:** No reaction advantage
- **Resource allocation:** Distribute 3 dice across 5-6 piles
- **Prediction game:** Guess opponent's priorities
- **Value assessment:** Which piles worth bidding multiple dice?
- **Opportunity cost:** Bid high on bomb or spread for multiple pickups?
- **Reading opponent:** Track their color preferences and patterns
- **Bluffing potential:** Bid unexpectedly to confuse opponent
- **Risk management:** Go all-in on one pile or diversify?
- **Round strategy:** Early rounds vs. late rounds have different values

**Outcome Possibilities:**
- One player gets 1 bomb, other gets 4+ filler cards
- Split 2-2 or 3-3 when bids spread evenly
- Complete mismatch if players prioritize different piles
- Sometimes players "trade" by bidding on different piles

**Advantages:**
- Elegant simultaneous bidding eliminates turn order advantage
- Using dice as bidding tokens is simple and clear
- Predicting opponent is core skill
- Fast decision-making (compared to sequential auction)
- Exciting reveals
- Engaging mind games

**Disadvantages:**
- Can be slow due to strategic thinking
- Dice setting/revealing can be fiddly
- Sometimes luck-driven (what if both want same pile?)
- Requires physical dice (or substitute)

**Tiebreaker Variations:**
1. **No one gets tied pile** - simplest, add card next round
2. **Highest unmatched die wins** - e.g., your K♥ beats my 10♣
3. **Tied piles Solomon drafted** - split pile, opponent chooses
4. **Coin flip** - random resolution

**Playing Cards Variant:**
- Each player uses 13 cards of a suit as bidding tokens
- Bid cards face-down (1-3 cards per pile)
- Ties broken by highest single card among bid cards
- More granular bidding (13 values vs. 6)

### Oracle Adaptation

Oracle could implement simultaneous secret Luna-bidding: reveal 5-6 champion piles, each player secretly distributes 3 Luna tokens across piles, highest bid on each pile wins it, mind-gaming and prediction core to strategy.

---

## Auction Magic (Auction Draft)

**Complexity Level:** High  
**Reference URL:** http://www.mtgsalvation.com/forums/the-game/casual-related-formats/homebrew-and-variant-formats/588039-auction-magic-a-new-format-as-discussed-on

### Key Mechanics

**Setup:**
- Shuffle booster packs into single deck (typically 3 per player)
- Each player starts with set amount of "funds" (typically 120 points)
- Use tokens, notepad, or play money to track funds

**Draft Process:**
- **This happens DURING GAMEPLAY, not before:**
- On your draw step: Reveal top card of draft deck
- Both players bid funds on that card
- Bidding continues until one player opts out
- Winner pays their bid to opponent
- Winner plays card immediately for FREE (or saves to play for free later)
- Continue until draft deck exhausted

**Gameplay Rules:**
- Normal Magic rules otherwise
- Cards won by bidding can be played without paying mana cost
- Paying opponent your bid creates economic tension
- Start with normal opening hands (drawn from draft deck)

**Strategic Elements:**
- **In-game drafting:** Draft and gameplay are simultaneous
- **Situational bidding:** Card value changes based on board state
- **Economic warfare:** Giving opponent funds helps them get future cards
- **Tempo decisions:** When to bid high for critical blocker/removal
- **Value shifts:** Mana costs don't matter (everything free), so evaluation changes
- **Threat assessment:** Bid more when behind, less when ahead?
- **Resource management:** Spend early or save for bombs?
- **Meta-reads:** Opponent low on funds = you can get cards cheap
- **Bluffing:** Bid high on mediocre card to drain opponent's funds
- **Card timing:** Good card now vs. saving funds for better card later

**Economic Dynamics:**
- Paying opponent 1 more than them = they can guarantee next bomb
- If they spend everything on Blightsteel Colossus, you get cheap removal
- Bidding wars escalate quickly
- Late-game funds become more valuable (fewer opportunities)

**Why It's Revolutionary:**
- Completely changes Magic's economy
- No mana screw/flood issues
- Games are highly interactive
- Draft happens in context of actual game state
- Every draw step is a bidding decision

**Advantages:**
- Extremely novel format
- High interaction between players
- No mana issues (cards played for free)
- Strategic depth in bidding
- Games are never dull
- Shortcutting mana economy creates unique gameplay

**Disadvantages:**
- Not really "drafting" in traditional sense
- Requires trust and bookkeeping (tracking funds)
- Can be swingy if one player gets early bomb
- Very different from normal Magic
- Some players find it too chaotic
- Games can be longer due to bidding

**Invented By:**
- Reuben Covington and Dan Felder
- Discussed on Remaking Magic podcast

**Similar Games:**
- Epic Card Game - plays similarly minus bidding
- Fast and smashy combat
- Scratches Limited Magic itch with zero setup
- Not Magic, but worth checking out

### Oracle Adaptation

Oracle coul