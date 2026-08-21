# Information Set Monte Carlo Tree Search (IS-MCTS)

Split out of the original combined `ismcts_nn_overview.md` (2026-08-21) — this file now
covers IS-MCTS on its own; NN+MCTS moved to
`../A11 ai agent is-mcts + nn (alphaoracle prime)/nn_mcts_overview.md`.

## Overview
IS-MCTS extends traditional MCTS to handle imperfect information games like Oracle, where players cannot see their opponent's hand or deck composition. Unlike games like Chess or Go where all information is visible, Oracle requires the AI to reason about hidden information.

## Core Concept: Determinization
Since the AI cannot see the opponent's hidden cards, it uses **determinization**:
1. **Sample** a possible opponent hand/deck consistent with observed information
2. **Search** the game tree as if this sample were the true game state
3. **Repeat** with many different samples to build a robust strategy
4. **Aggregate** results across all determinizations

## Key Components

**Tree Structure**
- Nodes represent game states from the AI's perspective (information sets)
- Each node tracks: visit count, total reward, available actions
- UCT (Upper Confidence bounds applied to Trees) balances exploration vs exploitation

**Four Phases per Iteration**
1. **Selection**: Traverse tree using UCT formula to pick promising nodes
2. **Expansion**: Add new child node when reaching tree frontier
3. **Simulation**: Play out the game randomly (rollout) from new node to terminal state
4. **Backpropagation**: Update all ancestor nodes with simulation result

**Handling Hidden Information**
- Maintain observer's information set (what the AI knows)
- Clone game state and randomize unknown cards (opponent's hand/deck)
- Re-determinize periodically as new information is revealed
- Use consistent determinization within each tree search

## Oracle-Specific Challenges
- **Deck composition unknown**: After initial random distribution, neither player knows what's in their deck
- **Hand hidden**: Cannot see opponent's current hand (0-7 cards)
- **Discard pile visible**: Public information that constrains possible remaining cards
- **Stochastic elements**: Dice rolls during combat add randomness beyond hidden information — see `mcts_depth_strategy.md` in this folder for why dice can be handled in closed form rather than sampled.

## Expected Performance
Well-tuned IS-MCTS should achieve strong strategic play by:
- Reasoning probabilistically about opponent's possible hands
- Planning multi-turn sequences
- Balancing resource management (lunas, hand size, energy)
- Adapting strategy based on revealed information

---

## References & Further Reading

- **IS-MCTS**: "Information Set Monte Carlo Tree Search" (Cowling et al., 2012)
- **Oracle Design**: See `doc/oracle_design.md` and `doc/oracle_roadmap.md` in this repository

## Quick Decision Guide

**Choose IS-MCTS if you want:**
- Strong AI without machine learning complexity
- CPU-only solution
- Faster development cycle
- More interpretable decision-making

**Best approach for Oracle**: Implement IS-MCTS first (necessary foundation per the roadmap's A1→A11 ladder), then decide whether to add the neural network — see the A11 folder.
