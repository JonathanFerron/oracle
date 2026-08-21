# Oracle: Les Champions d'Arcadie
## Game Depth Additions - Design Document

**Date:** October 29, 2025  
**Version:** 1.0  
**Purpose:** Document approved rule additions to add strategic depth while maintaining fast, action-oriented gameplay

---

## Overview

Three new mechanics have been approved to add depth to Oracle while keeping the game easy to learn and fast-paced:

1. **Champion Abilities** (Berserker & Vampire)
2. **Momentum System** (Streak rewards)
3. **Order Powers** (Once-per-game ultimates)

---

## 1. Champion Abilities

### Design Philosophy
Add simple triggered abilities only to the least efficient champion cards (power ≤ 3.25) to make them more attractive and add tactical decision-making.

### Power Calculation
Power = (Attack Efficiency + Defense Efficiency) / 2 / Cost

Where efficiency values are the expected values considering dice rolls and attack bases.

### Abilities Selected

#### Berserker
- **Icon:** d6 symbol (⚅) in light gray
- **Trigger:** When attacking alone (single champion attack)
- **Effect:** Roll dice twice and take the higher result
- **Cards:** 9 total (all with power ≤ 3.25)
- **Average Power:** 3.167

#### Vampire
- **Icon:** Fang mark in light gray
- **Trigger:** When your attack deals damage
- **Effect:** Heal 2 Energy
- **Cards:** 9 total (all with power ≤ 3.25)
- **Average Power:** 3.057

### Card Distribution

#### Berserker Cards (9 total):

**Cost 2 Champions (6 cards):**
- Card 18 (Orange): d8+4, power 3.25
- Card 52 (Red): d8+4, power 3.25
- Card 86 (Indigo): d8+4, power 3.25
- Card 19 (Orange): d6+5, power 3.00
- Card 53 (Red): d6+5, power 3.00
- Card 87 (Indigo): d6+5, power 3.00

**Cost 3 Champions (3 cards):**
- Card 29 (Orange): d12+5, power 3.00
- Card 63 (Red): d12+5, power 3.00
- Card 97 (Indigo): d12+5, power 3.00

#### Vampire Cards (9 total):

**Cost 2 Champions (6 cards):**
- Card 21 (Orange): d4+6, power 2.75
- Card 55 (Red): d4+6, power 2.75
- Card 89 (Indigo): d4+6, power 2.75
- Card 23 (Orange): d6+6, power 3.25
- Card 57 (Red): d6+6, power 3.25
- Card 91 (Indigo): d6+6, power 3.25

**Cost 3 Champions (3 cards):**
- Card 31 (Orange): d12+6, power 3.17
- Card 65 (Red): d12+6, power 3.17
- Card 99 (Indigo): d12+6, power 3.17

### Visual Implementation
- Small icons (5-8mm diameter) in **light gray**
- Positioned **near the species icon** on the card
- Discreet but recognizable at a glance
- Does not interfere with existing card artwork

### Balance Notes
- Each color receives exactly 3 Berserker and 3 Vampire cards
- Perfect balance across Red, Indigo, and Orange
- Average power difference: 0.110 (acceptable variance)
- Only 18 of 102 champions have abilities (17.6%)

---

## 2. Momentum System

### Design Philosophy
Reward consecutive successful actions to encourage aggressive, action-oriented play and create comeback mechanics.

### Mechanics

#### Gaining Momentum
- **Rate:** Maximum 1 token per turn
- **Conditions to gain 1 token:**
  - Your attack deals 10+ damage, OR
  - You defend and take 0 damage

#### Losing Momentum
- **Lose ALL tokens immediately when:**
  - Your attack deals 0 damage, OR
  - You take 15+ damage in one hit

#### Token Limits
- **Minimum:** 0 tokens
- **Maximum:** 3 tokens

### Token Benefits (Passive, Always Active)

Tokens are NOT spent - they provide ongoing benefits:

| Tokens | Benefit |
|--------|---------|
| 1 | Draw 2 cards instead of 1 during draw phase |
| 2 | +3 bonus to your next attack OR defense |
| 3 | Your next champion played costs 1 less luna (minimum 0) |

**Note:** With multiple tokens, you gain ALL corresponding benefits simultaneously.

### Physical Implementation
- Use small plastic or glass pebbles
- One color per player for easy tracking
- Place in visible play area

### Strategic Impact
- Creates "hot streak" gameplay
- Rewards skillful play
- Encourages aggressive attacks
- Provides comeback mechanics when losing tokens
- Adds swing potential without complexity

---

## 3. Order Powers

### Design Philosophy
Give each of the 5 Orders a powerful once-per-game ability to create memorable "ultimate" moments and make Order matching strategically important beyond combo bonuses.

### Usage Rules
- **Frequency:** Once per Order per game
- **Activation:** Discard a champion of that Order from your hand
- **Availability:** Can use all 5 powers in one game if you have champions from all Orders

### The Five Order Powers

#### Order A - Dawn Light (Lumière d'Aurore)
- **Name:** Resurrection
- **Effect:** Recall any champion from your discard pile to your hand for free
- **Species:** Human, Elf, Dwarf

#### Order B - Verdant Light (Lumière Verdoyante)
- **Name:** Growth
- **Effect:** All your champions in combat get +1 Attack Base this combat
- **Species:** Hobbit, Faun, Centaur

#### Order C - Ember Light (Lumière de Braise)
- **Name:** Fury
- **Effect:** Reroll all your dice this combat
- **Species:** Orc, Goblin, Minotaur

#### Order D - Eternal Light (Lumière Éternelle)
- **Name:** Timeless
- **Effect:** Draw 1 card and gain 2 lunas
- **Species:** Dragon, Cyclops, Fairy

#### Order E - Moonlight (Lumière Lunaire)
- **Name:** Eclipse
- **Effect:** Opponent discards 1 random card
- **Species:** Aven, Koatl, Lycan

### Balance Adjustments
Original proposals for Orders D and E were deemed too powerful:
- **Order D (original):** Draw 3 cards and gain 3 lunas → **REDUCED** to 1 card and 2 lunas
- **Order E (original):** Opponent discards 2 random cards → **REDUCED** to 1 random card

### Physical Implementation
- Create **half-sized cards** (or quarter-sized) for tracking
- One mini-card per Order (5 total per player)
- **Two-sided design:**
  - Front: "AVAILABLE" - Order symbol/name visible
  - Back: "USED" - flip when power is activated
- Keep in visible play area

### Strategic Impact
- Adds meta-game layer for experienced players
- Creates memorable "big play" moments
- Makes Order diversity valuable in deck building
- Works with all deck types (Random, Monochrome, Custom)
- Simple to track with physical tokens

---

## Implementation Priority

### Recommended Order of Implementation:

1. **Champion Abilities** (Highest Priority)
   - Integrates naturally with existing combat system
   - Requires card design updates only
   - Minimal rule complexity
   - Immediate tactical depth

2. **Momentum System** (Medium Priority)
   - Encourages desired gameplay (action-oriented)
   - Optional for new players
   - Easy to implement with tokens
   - Can be ignored until players are comfortable

3. **Order Powers** (Lower Priority)
   - Best for experienced players
   - Adds strategic depth for veterans
   - Requires tracking cards
   - Meta-game layer

---

## Code Implementation Notes

### Data Structure Changes Required

#### Champion Abilities
Add to `card` structure in `game_types.h`:
```c
typedef enum {
    ABILITY_NONE,
    ABILITY_BERSERKER,
    ABILITY_VAMPIRE
} ChampionAbility;

// Add to struct card:
ChampionAbility ability;
```

Update `fullDeck` array in `game_constants.c` to assign abilities to the 18 specified cards.

#### Momentum System
Add to player/game state:
```c
// Per player
int momentum_tokens; // 0-3

// Game state tracking
// Track damage dealt/taken per turn
```

#### Order Powers
Add to player state:
```c
// Per player
bool order_powers_used[5]; // One per Order (A-E)
```

---

## Testing Considerations

### Balance Testing Priorities
1. Verify Berserker doesn't make solo attacks too powerful
2. Verify Vampire healing (2 Energy) doesn't extend games excessively
3. Test Momentum token gain/loss rates for typical gameplay
4. Ensure Order Powers feel impactful but not game-breaking

### Edge Cases to Test
- Berserker with d20 dice (very high variance)
- Vampire healing when defender is at low Energy
- Multiple Momentum tokens accumulated
- Order Power timing interactions with combat
- Using Order Powers in combination with abilities

---

## Design Constraints Maintained

✓ Game remains easy to learn  
✓ Fast gameplay maintained (~20 minutes)  
✓ High action/turn ratio preserved  
✓ Minimal rule overhead  
✓ Visual clarity on cards  
✓ Balanced across all three colors  
✓ Works with all deck types (Random, Monochrome, Custom)

---

## Future Considerations

### Potential Expansions (Not Approved)
- Rally ability (duplicates combo system - rejected)
- Shield Wall ability (duplicates combo system - rejected)
- First Strike ability (duplicates combo system - rejected)
- Additional champion abilities for future expansions
- Momentum token effects beyond 3 tokens
- Additional Order Powers for alternate game modes

---

## Visual Design Guidelines

### Champion Ability Icons
- **Size:** 5-8mm diameter
- **Color:** Light gray (non-intrusive)
- **Position:** Near species icon on card
- **Style:** Simple, recognizable silhouettes
  - Berserker: d6 die face (⚅)
  - Vampire: Fang mark (V-shape or paired dots)

### Momentum Tokens
- Small plastic or glass pebbles
- Distinct color per player
- Size: 10-15mm diameter
- Placed in visible play area near player

### Order Power Cards
- Half-sized or quarter-sized cards
- Clear "AVAILABLE" / "USED" indication
- Order symbol prominent
- Power name and effect text
- Durable card stock

---

## Rules Text Additions

### Champion Abilities Rules Text

**Berserker:**
> When this champion attacks alone (as the only champion in your attack), roll its Defense Dice twice and use the higher result.

**Vampire:**
> When your attack that includes this champion deals damage to your opponent, heal 2 Energy.

### Momentum System Rules Text

**Gaining Momentum:**
> At the end of your turn, if your attack dealt 10 or more damage, OR if you defended and took 0 damage, gain 1 momentum token (maximum 3 tokens).

**Losing Momentum:**
> If your attack deals 0 damage, OR if you take 15 or more damage in a single attack, immediately lose all momentum tokens.

**Momentum Benefits:**
> - With 1+ tokens: Draw 2 cards (instead of 1) during your draw phase
> - With 2+ tokens: +3 bonus to your next attack or defense
> - With 3 tokens: Your next champion played costs 1 less luna (minimum 0)

### Order Powers Rules Text

**Activation:**
> Once per game per Order, you may discard a champion of that Order from your hand to activate its Order Power.

**Order Power Effects:**
> - **Order A (Dawn Light) - Resurrection:** Return any champion from your discard pile to your hand for free
> - **Order B (Verdant Light) - Growth:** All your champions in the current combat get +1 Attack Base
> - **Order C (Ember Light) - Fury:** Reroll all your dice in the current combat
> - **Order D (Eternal Light) - Timeless:** Draw 1 card and gain 2 lunas
> - **Order E (Moonlight) - Eclipse:** Your opponent discards 1 random card from their hand

---

## Changelog

**Version 1.0 (October 29, 2025):**
- Initial design document
- Champion Abilities: Berserker and Vampire defined
- Momentum System: Token mechanics established
- Order Powers: All 5 powers defined with balance adjustments
- Card distribution finalized (18 ability cards identified)
- Visual implementation guidelines established

---

*This document represents the approved design decisions for adding strategic depth to Oracle: Les Champions d'Arcadie while maintaining its core identity as a fast-paced, action-oriented card game.*

Let me analyze the distribution of Berserker and Vampire abilities by species and order:

## Distribution by Species

### Berserker Cards (9 total):
- **Human (Order A):** 0 cards
- **Elf (Order A):** 0 cards
- **Dwarf (Order A):** 0 cards
- **Hobbit (Order B):** 0 cards
- **Faun (Order B):** 0 cards
- **Centaur (Order B):** 0 cards
- **Orc (Order C):** 3 cards (18, 52, 86 - all d8+4)
- **Goblin (Order C):** 0 cards
- **Minotaur (Order C):** 0 cards
- **Dragon (Order D):** 3 cards (29, 63, 97 - all d12+5)
- **Cyclops (Order D):** 3 cards (19, 53, 87 - all d6+5)
- **Fairy (Order D):** 0 cards
- **Aven (Order E):** 0 cards
- **Koatl (Order E):** 0 cards
- **Lycan (Order E):** 0 cards

### Vampire Cards (9 total):
- **Human (Order A):** 3 cards (23, 57, 91 - all d6+6)
- **Elf (Order A):** 0 cards
- **Dwarf (Order A):** 0 cards
- **Hobbit (Order B):** 3 cards (21, 55, 89 - all d4+6)
- **Faun (Order B):** 0 cards
- **Centaur (Order B):** 0 cards
- **Orc (Order C):** 3 cards (31, 65, 99 - all d12+6)
- **Goblin (Order C):** 0 cards
- **Minotaur (Order C):** 0 cards
- **Dragon (Order D):** 0 cards
- **Cyclops (Order D):** 0 cards
- **Fairy (Order D):** 0 cards
- **Aven (Order E):** 0 cards
- **Koatl (Order E):** 0 cards
- **Lycan (Order E):** 0 cards

## Distribution by Order

| Order | Berserker | Vampire | Total |
|-------|-----------|---------|-------|
| **A (Dawn Light)** | 0 | 3 (Human) | 3 |
| **B (Verdant Light)** | 0 | 3 (Hobbit) | 3 |
| **C (Ember Light)** | 3 (Orc) | 3 (Orc) | 6 |
| **D (Eternal Light)** | 6 (Dragon, Cyclops) | 0 | 6 |
| **E (Moonlight)** | 0 | 0 | 0 |

## Distribution by Color

| Color | Berserker | Vampire | Total |
|-------|-----------|---------|-------|
| **Orange** | 3 | 3 | 6 |
| **Red** | 3 | 3 | 6 |
| **Indigo** | 3 | 3 | 6 |

## Summary

**Berserker Species (3 species, all different orders):**
- Orc (Order C) - 3 cards
- Dragon (Order D) - 3 cards
- Cyclops (Order D) - 3 cards

**Vampire Species (3 species, all different orders):**
- Human (Order A) - 3 cards
- Hobbit (Order B) - 3 cards
- Orc (Order C) - 3 cards

**Notable Observations:**
- Order E (Moonlight) has NO champions with abilities
- Order D (Eternal Light) is Berserker-heavy with 6 cards
- Order C (Ember Light) has both abilities (3 Berserker, 3 Vampire)
- Orc is the only species with both abilities
- Each ability appears on exactly 3 species
- Perfect color balance maintained (3+3 per color)

You're absolutely right to identify this potential balance concern. Let me summarize the imbalance risks:

## Potential Imbalance Issues

### Species Level
**Orc (Order C):**
- Only species with BOTH Berserker and Vampire abilities (6 total ability cards)
- Could make Orc-focused strategies dominant in monochrome decks
- In random decks, drawing multiple Orcs becomes disproportionately valuable

**Dragon & Cyclops (Order D):**
- Combined 6 Berserker cards
- Makes Order D species more attractive than others

**Human & Hobbit (Orders A & B):**
- 3 Vampire cards each
- Moderate boost, less concentrated than Orc

### Order Level
**Order D (Eternal Light) - 6 Berserker cards:**
- Strongest order for abilities
- Also has arguably the best Order Power (Timeless: draw + lunas)
- Double advantage could make it the "best" order

**Order C (Ember Light) - 6 total (3 Berserker + 3 Vampire):**
- Balanced mix of abilities
- Fury power (reroll dice) synergizes well with Berserker

**Order E (Moonlight) - 0 ability cards:**
- No champions with abilities at all
- Could make it the weakest order
- Eclipse power may not compensate enough

## Testing Priorities

When playtesting, watch for:

1. **Orc Dominance:** Are players prioritizing Orc champions over equivalent cards?
2. **Order D Preference:** In custom decks, is Order D over-represented?
3. **Order E Weakness:** Does Order E feel underpowered compared to others?
4. **Combo Synergies:** Does Berserker + Order C's Fury create overpowered combinations?
5. **Deck Construction:** In monochrome/custom, are certain species/orders mandatory picks?

## Potential Rebalancing Options (if needed)

If testing reveals issues, you could consider:

**Option 1: Redistribute some abilities**
- Move 1-2 Berserker cards from Order D to Order E
- Spread Vampire cards more evenly

**Option 2: Adjust ability power**
- If Order D dominates, slightly nerf Berserker effect
- If Order E is weak, buff Eclipse or add abilities

**Option 3: Accept strategic asymmetry**
- Different orders having different strengths can be interesting
- As long as win rates stay within ~45-55% range, it's likely fine

The fact that these are all low-power cards (≤3.25) means the abilities are meant to compensate for inefficiency. The real question is whether some combinations over-compensate. Good catch on identifying this - definitely worth tracking during testing!
