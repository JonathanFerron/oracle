# Advanced AI Agents: IS-MCTS and NN+MCTS

## Information Set Monte Carlo Tree Search (IS-MCTS)

### Overview
IS-MCTS extends traditional MCTS to handle imperfect information games like Oracle, where players cannot see their opponent's hand or deck composition. Unlike games like Chess or Go where all information is visible, Oracle requires the AI to reason about hidden information.

### Core Concept: Determinization
Since the AI cannot see the opponent's hidden cards, it uses **determinization**:
1. **Sample** a possible opponent hand/deck consistent with observed information
2. **Search** the game tree as if this sample were the true game state
3. **Repeat** with many different samples to build a robust strategy
4. **Aggregate** results across all determinizations

### Key Components

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

### Oracle-Specific Challenges
- **Deck composition unknown**: After initial random distribution, neither player knows what's in their deck
- **Hand hidden**: Cannot see opponent's current hand (0-7 cards)
- **Discard pile visible**: Public information that constrains possible remaining cards
- **Stochastic elements**: Dice rolls during combat add randomness beyond hidden information

### Expected Performance
Well-tuned IS-MCTS should achieve strong strategic play by:
- Reasoning probabilistically about opponent's possible hands
- Planning multi-turn sequences
- Balancing resource management (lunas, hand size, energy)
- Adapting strategy based on revealed information

---

## Neural Network + MCTS (AlphaZero-Style)

### Overview
Combining deep neural networks with MCTS creates superhuman game-playing AI, as demonstrated by AlphaZero (Chess), AlphaGo (Go), and KataGo. The neural network **guides** the MCTS search toward promising positions, making it vastly more efficient than pure MCTS.

### The Hybrid Approach

**Traditional MCTS Problem**: Random rollouts waste computation on bad moves

**NN+MCTS Solution**: Neural network provides two critical insights:
1. **Policy**: Which actions are most promising? (focuses search)
2. **Value**: How good is this position? (reduces rollout depth)

### Architecture Components

**Neural Network**
- **Input**: Oracle game state representation
  - Energy, lunas, hand composition, combat zone, visible cards, turn number
  - Encoded as feature planes (similar to how AlphaGo encodes Go boards)
- **Outputs**:
  - **Policy head**: Move quality scores for all legal actions
    - Outputs normalized scores (sum to 1.0) indicating relative move strength
    - Guides MCTS search: higher scores → explore more
    - Based on what succeeded in millions of training games
  - **Value head**: Position evaluation from current player's perspective
    - Typically -1 (certain loss) to +1 (certain win), with 0 = even
    - Can also be represented as win probability 0 to 1; both are equivalent

**MCTS with Neural Network**
1. **Determinization**: Sample opponent's hidden cards (still required!)
2. **Selection**: Use neural net policy + UCT to pick promising branches
3. **Expansion**: Neural net evaluates new position (no random rollout needed!)
4. **Backpropagation**: Use neural net value estimate instead of rollout result
5. **Repeat**: Re-determinize and search again with different samples

### Training Process (Self-Play Loop)

**Phase 1: Self-Play Data Generation**
- AI plays against itself using current neural network
- Each position generates training data: (state, MCTS_policy, game_outcome)
- Store millions of training examples

**Phase 2: Network Training**
- Train neural network on self-play data
- Policy head learns to predict MCTS-improved move probabilities
- Value head learns to predict actual game outcomes
- Use standard supervised learning (cross-entropy for policy, MSE for value)

**Phase 3: Iteration**
- New network plays against old network
- Keep new network if it wins >55% of games
- Repeat cycle: self-play → train → evaluate → iterate

### Why This Works for Oracle

**Advantages**
- **Learns game patterns**: Network discovers combo synergies, resource management strategies
- **Handles complexity**: Learns when to attack/defend without hand-coded heuristics
- **Improves over time**: Gets stronger with more training games
- **Fast inference**: Once trained, neural net evaluates positions instantly

**Oracle-Specific Considerations**
- **Smaller state space than Go**: Faster training (days vs weeks)
- **Hidden information**: Still requires determinization, but NN learns to reason probabilistically
- **Stochastic combat**: Network learns expected values of dice-based combat
- **Resource management**: Network discovers optimal luna spending patterns
- **Training advantage**: Network sees full game state during self-play, learns patterns that help with uncertainty during actual play

### Expected Training Requirements

Based on KataGo's results (which trained strong Go AI in 19 days on 28 GPUs):
- **Hardware**: 1× RTX 4090 (24GB VRAM)
- **Timeline**: 5-10 days of continuous training
- **Training games**: 1-5 million self-play games
- **Data generated**: 10-50 million training positions
- **Result**: Superhuman Oracle play

### Comparison to Pure IS-MCTS

| Aspect | IS-MCTS | NN+MCTS |
|--------|---------|---------|
| **Development time** | Weeks | Weeks setup + days training |
| **Computational cost** | Low (CPU only) | High (GPU for training) |
| **Play strength** | Strong | Superhuman |
| **Inference speed** | Slower (many rollouts) | Faster (fewer, better-guided evaluations) |
| **Explainability** | Moderate | Low (black box) |
| **Requires training** | No | Yes (but one-time) |
| **Determinization** | Required | Also required, but more efficient |

### Implementation Path for Oracle

**Stage 1: IS-MCTS Foundation** (Prerequisite)
- Build working IS-MCTS agent
- Establish baseline performance
- Create game state cloning and determinization

**Stage 2: Neural Network Integration**
- Design state representation (feature encoding)
- Implement small network architecture (~10-20 residual blocks)
- Set up self-play infrastructure
- Implement training pipeline (PyTorch/TensorFlow)

**Stage 3: Training & Iteration**
- Generate initial training data with random/IS-MCTS play
- Train first network version
- Run self-play with network + MCTS
- Iterate and improve

**Stage 4: Evaluation**
- Compare NN+MCTS vs pure IS-MCTS
- Measure Elo rating improvement
- Test against human players
- Analyze learned strategies

---

## References & Further Reading

- **IS-MCTS**: "Information Set Monte Carlo Tree Search" (Cowling et al., 2012)
- **AlphaZero**: "Mastering Chess and Shogi by Self-Play with a General Reinforcement Learning Algorithm" (Silver et al., 2017)
- **KataGo**: "Accelerating Self-Play Learning in Go" (Wu, 2019) - achieved amateur dan strength in days, not months
- **Oracle Design**: See `doc/oracle_design.md` and `doc/oracle_roadmap.md` in this repository

---

## Quick Decision Guide

**Choose IS-MCTS if you want:**
- Strong AI without machine learning complexity
- CPU-only solution
- Faster development cycle
- More interpretable decision-making

**Choose NN+MCTS if you want:**
- Strongest possible AI (superhuman play)
- Cutting-edge AI research experience
- Fast inference once trained
- Don't mind GPU requirements and training time

**Best approach for Oracle**: Implement IS-MCTS first (necessary foundation), then add neural network if you want to push to superhuman levels.
