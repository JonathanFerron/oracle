# Oracle: Custom Deck Construction — Combinatorics Analysis

**Date:** August 2026
**Purpose:** Quantify the size of the custom 40-card deck construction space, both unconstrained and under the game's deck-building rules.

---

## 1. Card Pool Composition

Derived from `src/game_constants.c` (`fullDeck[FULL_DECK_SIZE]`, 120 cards total):

| Category | Count | Notes |
|---|---|---|
| **Champions (total)** | 102 | 34 per color × 3 colors (Orange, Red, Indigo) |
| — Cost 0 | 9 | 3 per color |
| — Cost 1 | 51 | 17 per color |
| — Cost 2 | 27 | 9 per color |
| — Cost 3 | 15 | 5 per color |
| **Draw 2 cards** | 9 | cost 1 luna, draw 2 |
| **Draw 3 cards** | 6 | cost 2 lunas, draw 3 |
| **Exchange (Cash) cards** | 3 | exchange for 5 lunas |
| **Total** | **120** | |

## 2. Custom Deck Construction Constraints

A custom 40-card deck is subject to:

| Constraint | Limit | Pool available |
|---|---|---|
| Draw 2 cards | max 6 | 9 |
| Draw 3 cards | max 4 | 6 |
| Exchange cards | max 2 | 3 |
| Cost-0 champions | max 4 | 9 |
| Cost-1/2/3 champions | no limit | 93 |
| Deck size | exactly 40 | — |

## 3. Unconstrained Combinatorics

Choosing any 40 cards from the full 120-card pool, order irrelevant:

$$
\binom{120}{40} \approx 8.31 \times 10^{32}
$$

This is the baseline combinatorial size with no deck-building rules applied.

## 4. Constrained Combinatorics — Method

The valid deck space was computed by summing over every legal combination of non-champion cards, then filling remaining slots with champions (respecting the cost-0 cap):

$$
N = \sum_{\substack{n_2=0..\min(6,9) \\ n_3=0..\min(4,6) \\ n_e=0..\min(2,3)}} \binom{9}{n_2}\binom{6}{n_3}\binom{3}{n_e} \times \sum_{n_0=0}^{\min(4,\,s)} \binom{9}{n_0}\binom{93}{s-n_0}
$$

where $s = 40 - (n_2+n_3+n_e)$ is the number of champion slots remaining, and 93 = 51+27+15 (cost 1/2/3 champion pool).

Implemented and evaluated as a Python script (`comb` from `math`), iterating all valid $(n_2, n_3, n_e, n_0)$ tuples.

## 5. Key Findings

- **Unconstrained space:** $\binom{120}{40} \approx 8.31 \times 10^{32}$ decks.
- **Constrained space:** several orders of magnitude smaller, but still astronomically large (well beyond exhaustive search or enumeration) — full precision figure to be read off the script's final run rather than approximated by hand.
- **Cost-0 cap is only mildly restrictive in practice.** With just 9 cost-0 champions total and a cap of 4, the constraint excludes access to at most 5 of the 9 (44%) — far less restrictive than an earlier (incorrect) draft assumption of 36 cost-0 champions, which would have made this the dominant constraint. With the corrected pool of 9, most natural deck builds (0–3 cost-0 champions) aren't meaningfully affected by the cap at all.
- **Champion slot count varies** with how many Draw 2 / Draw 3 / Exchange cards are included — from a minimum (max utility cards: 6+4+2=12 non-champions → 28 champion slots) to a maximum (0 utility cards → 40 champion slots, capped by the 4 cost-0 limit).
- **Deck diversity remains enormous** under all realistic constraint sets — no practical concern about deck-space exhaustion for playtesting or AI training purposes.

## 6. Distribution Analysis (Configuration Breakdown)

A second script decomposes the constrained deck space by **non-champion card configuration** $(n_2, n_3, n_e)$, to identify:

- The most numerous individual configurations (by count of legal decks each permits)
- How deck count is distributed across total non-champion card counts (0–12)
- How deck count is distributed across champion slot counts (28–40)
- The concentration of deck space among top configurations (e.g., top 10 vs. top 20 share of total)

**Method:** for each valid $(n_2, n_3, n_e)$ triple, compute $\binom{9}{n_2}\binom{6}{n_3}\binom{3}{n_e} \times \sum_{n_0} \binom{9}{n_0}\binom{93}{s-n_0}$, tabulate, and rank.

**Qualitative expectation** (pending exact run output): configurations with a moderate number of Draw 2 cards (2–5) and few Draw 3/Exchange cards dominate deck count, since $\binom{93}{s-n_0}$ grows very quickly with champion slot count $s$ — meaning configurations that leave more room for champions contribute disproportionately more decks to the total, even though they may not represent typical *competitive* choices.

## 7. Practical Deck-Building Implications

- A typical 40-card deck likely carries **~4–8 non-champion cards** (Draw 2/3, Exchange) and **~32–36 champions**.
- Within champions, a natural spread favors cost-1 (largest pool, 51 cards) and cost-2 (27 cards), with cost-0 (9) and cost-3 (15) as smaller complementary tiers.
- The cost-0 cap (max 4 of 9) is a soft guardrail rather than a binding constraint for most sensible deck archetypes.
- These figures support treating custom deck construction as an effectively unbounded design space for AI deck-building work (Genetic Algorithm / MCTS optimization, per the open design question on deck construction), since the combinatorial space vastly exceeds any feasible search budget.

## 8. Follow-Up / Open Items

- Run the distribution-analysis script to completion and record exact figures (total constrained deck count, top-20 configuration table, median champion-slot count) for permanent reference.
- Consider whether a "viable deck" filter (e.g., minimum total power threshold) would be a more useful metric than raw combinatorial count for AI training purposes.
- No code changes to the game engine were made in this session — this was a pure combinatorics/analysis exercise using two standalone Python scripts (not part of the C codebase).
