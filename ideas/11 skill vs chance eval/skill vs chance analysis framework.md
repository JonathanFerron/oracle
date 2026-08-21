# Oracle Skill vs. Chance Analysis Framework

Based on the two research papers you've provided, I'll propose a comprehensive approach to evaluate Oracle's position on the skill-chance spectrum. The papers present complementary methodologies that can be applied to your card game.

## Overview of the Two Approaches

### 1. Dreef et al. (2023) - Relative Skill Opportunity (RSO)
This paper proposes measuring skill opportunity by analyzing how much outcomes deviate from expected values when all players have equal skill versus when players have different skill levels.

**Key Metric**: RSO (Relative Skill Opportunity)
- RSO = 0: Pure chance game
- RSO = 1: Pure skill game
- RSO between 0 and 1: Mixed skill-chance game

### 2. Duersch et al. (2018) - Pairwise Comparison Method
This paper uses rating systems (specifically Elo) to measure skill by comparing actual win rates against predicted win rates based on skill ratings.

**Key Insight**: If ratings accurately predict outcomes, skill dominates; if ratings don't predict well, chance dominates.

## Proposed Implementation for Oracle

I'll create a comprehensive framework that combines both approaches and is tailored to Oracle's architecture.## Summary

I've created a comprehensive framework for evaluating Oracle's position on the skill-chance spectrum using the two research papers you provided. Here are the key points:

### Two Complementary Approaches:

1. **Relative Skill Opportunity (RSO)** - Compares outcome variance when players have equal vs. different skill levels
2. **Rating System Analysis** - Tests whether ratings can accurately predict match outcomes

### Expected Finding:
Oracle likely sits in the **balanced skill-chance category** with:
- RSO around 0.4-0.6 (similar to Poker, Backgammon)
- Rating prediction error (MAE) around 0.1-0.15
- Strong players beating weaker players ~70-80% of the time

### Implementation Requirements:
- ~160,000 simulated games (4-5 hours of computation)
- 3-5 AI strategies of different skill levels
- New `src/analysis/` module (~1,000 lines of code)
- Python visualization scripts

### Why This Matters:
1. **Game Design**: Validates whether Oracle achieves its intended balance
2. **Player Experience**: Confirms skill matters but isn't overwhelming
3. **Research**: Provides quantitative data for the skill-chance debate
4. **Comparison**: Positions Oracle relative to known games

The framework is designed to integrate cleanly with your existing codebase, requiring only new analysis modules while leveraging already-planned features like the rating system and CSV export.

Would you like me to elaborate on any specific aspect, such as the implementation details for the RSO calculation or the Python visualization scripts?
