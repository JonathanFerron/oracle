# Custom Deck Construction — Design Handout

## Goal

Implement one or more **non-simulation-seeded, simulation-refined** methods to construct a 40-card custom deck that maximizes win probability, with explicit awareness of the combo bonus system. This is a design/architecture briefing for implementation — not code to paste directly.

---

## 1. Prerequisite: Fix a Real Bug Before Any Evaluation Work

**RESOLVED 2026-08-28** (as part of the project's general housekeeping pass, ahead
of `G3` itself being picked up -- see `doc/changelog.md`'s 2026-08-28 entry). The
fix landed as described below, with one change from this handout's own sketch:
`struct gamestate` gained the new field (not `GameContext` -- keeps every call site
unchanged, since MC/ISMCTS simulations already copy `struct gamestate` by value),
and the type was renamed `DeckType` -> `ComboBonusTable` (with `DECK_RANDOM`/
`DECK_MONOCHROME`/`DECK_CUSTOM` -> `COMBO_BONUS_RANDOM`/`COMBO_BONUS_MONOCHROME`/
`COMBO_BONUS_CUSTOM`) since it names which scoring table applies, not which
deck-construction method was used -- several of the ~15 methods in `ideas/10
Draft Format and Game Depth Addition Ideas/` are expected to share one. Original
description kept below for context.

`combat.c::calculate_total_attack()` and `calculate_total_defense()` currently call:

```c
int bonus = calculate_combo_bonus(combat_cards, num_cards, DECK_RANDOM);
```

This is **hardcoded to `DECK_RANDOM`** regardless of the actual deck type in play (see the `// Add combo bonus (assuming DECK_RANDOM for now)` comments in both functions). `combo_bonus.c` already has the correct branch (`calc_random_bonus` vs `calc_prebuilt_bonus`), but nothing currently passes the real `DeckType` down to `resolve_combat()`.

**This must be fixed first.** Any simulation-based deck evaluation (GA or MCTS below) is meaningless for custom decks until combat resolution actually applies `calc_prebuilt_bonus()` for custom-deck games. This likely means:
- Adding a `DeckType` (or similar) field to `struct gamestate` or `GameContext`, populated at game setup.
- Threading it through `resolve_combat()` → `calculate_total_attack()` / `calculate_total_defense()`.

Flag this as a blocking dependency in any handout to Claude Code implementing the deck builder.

---

## 2. Combo Bonus Rules That Apply to Custom Decks

From `combo_bonus.c`, custom/prebuilt decks use `calc_prebuilt_bonus()`, which is **species- and order-based only — it does NOT consider color**, unlike `calc_random_bonus()` which has a color tier. This is a meaningful asymmetry: for custom-deck design, color is only relevant for card *identity/uniqueness*, not for combo scoring.

**`calc_prebuilt_bonus()` values** (2–3 card combat groupings only; combos never apply to groups of 1):

| Condition | Bonus |
|---|---|
| 2 cards, same species | +7 |
| 3 cards, same species | +12 |
| 2 same species + 3rd matches their Order | +9 |
| 2 cards, same Order (no species match) | +4 |
| 3 cards, same Order (no species match) | +6 |

No color-based bonus exists in custom-deck combat.

---

## 3. Deck Composition Facts (from `game_types.h` / `game_constants.c`)

- **Species are color-exclusive.** Each of the 15 species exists in exactly one of the 3 colors (e.g., Hobbit only exists as Orange). Confirmed mapping:
  - Orange: Human (A), Hobbit (B), Orc (C), Dragon (D), Aven (E)
  - Red: Elf (A), Faun (B), Goblin (C), Cyclops (D), Koatl (E)
  - Indigo: Dwarf (A), Centaur (B), Minotaur (C), Fairy (D), Lycan (E)
- **Order groups 3 species across the 3 colors** (e.g., Order B = Hobbit + Faun + Centaur, one per color).
- Card counts per species and exact cost distribution should be **derived programmatically from `fullDeck[]`** rather than assumed — do not hardcode champion IDs or per-species counts in the deck builder; filter `fullDeck` by `species`/`order`/`cost` at runtime.
- Deck must total exactly **40 cards**, drawn from the 120-card `fullDeck` (champions, draw cards, cash cards), respecting whatever count/duplication limits the game rules impose (confirm max copies per unique card before implementation — not covered in the files reviewed so far).

---

## 4. Seed Deck: "Order B + Minotaur" Concentration Build

A deterministic, non-simulation starting point that maximizes expected combo frequency:

- **All Hobbit** cards (Order B, Orange)
- **All Faun** cards (Order B, Red)
- **All Centaur** cards (Order B, Indigo)
- **All Minotaur** cards (Order C, Indigo)
- Fill remainder to 40 with Draw/Recall and Cash cards, up to the game's allowed maximums.

**Rationale:**
- Hobbit/Faun/Centaur share Order B → any 2- or 3-card combat grouping mixing these species (without a species match) still scores the Order bonus (+4/+6).
- Any pair/triplet of the *same* species among the four scores the higher species bonus (+7/+9/+12).
- Minotaur adds a 4th species pool for species-match combos but does **not** share Order B, so Minotaur+Hobbit/Faun/Centaur groupings score 0 combo bonus (different species, different order). It's included purely to raise the odds of a same-species triplet, not for order synergy with the other three.
- This should be verified quantitatively (expected combo bonus per combat, hand-draw probabilities) once implemented — treat the rationale above as a hypothesis the simulation will test, not a proven optimum.

This deck is the **root/seed node** for the refinement methods below. A second seed worth generating for comparison purposes: a pure power-maximization deck (top-N champions by `power` field, 34 champions + max support cards, ignoring combos entirely) — useful as a sanity-check baseline, not as a candidate for "the" optimal deck.

---

## 5. Evaluation Methodology (applies to both GA and MCTS)

- **Both players in every simulated match use the identical in-game AI strategy** (e.g., both Random AI, or both Balanced AI once available). The only variable under test is the deck composition — strategy must never differ between the two sides in an evaluation match.
- Matches are always **parent deck vs. child deck** (i.e., the deck before a candidate change vs. the deck after), not vs. a fixed external opponent. This isolates the effect of each specific change.
- **Alternate which deck plays as PLAYER_A/PLAYER_B** across the match batch to cancel out any first-player advantage.
- Use a reasonably sized batch (order of 50–100 games) per comparison, and prefer a statistical significance check (e.g., confidence interval on win rate) over a raw win% threshold, to avoid accepting changes that are just simulation noise.

---

## 6. Primary Recommended Method: Genetic Algorithm

Preferred over greedy hill-climbing because population diversity reduces the risk of getting stuck in a locally-but-not-globally optimal deck.

**Design:**

- **Population** (e.g., 8–10 individuals): seed with the Order B + Minotaur deck, the power-max baseline deck, and the rest randomly generated valid 40-card decks.
- **Fitness**: round-robin tournament among the population (same strategy for all matches, per §5), win rate = fitness.
- **Selection**: tournament selection among the population for choosing parents.
- **Elitism**: carry the top few individuals unchanged into the next generation.
- **Crossover**: build a child deck by taking a subset of cards from parent 1 and filling the remainder with non-duplicate cards from parent 2; fill any shortfall with random valid cards to reach exactly 40.
- **Mutation**: with some probability, apply a small number of random swaps (remove a card, add a different valid card not already in the deck).
- **Termination**: fixed number of generations, or when best fitness stops improving for N consecutive generations.

**Validity constraints to enforce on every generated deck** (crossover, mutation, and random generation):
- Exactly 40 cards.
- No duplicate card beyond whatever max-copies rule the game defines (confirm this rule before implementation).
- Respect any minimum/maximum counts on Draw/Recall and Cash cards if such limits exist in the rules.

---

## 7. Secondary Method: MCTS-Based Local Refinement

Useful as a follow-up refinement pass on the GA's best result, or as an alternative if the GA is deferred.

**Node** = a candidate 40-card deck. **Edge/action** = a single swap (remove one card, add one valid card not already present). From any given deck there are up to (40 × candidates-not-in-deck) possible swaps — large, so only a handful should be evaluated per expansion rather than the full space.

**Selecting which swaps to try (in order of increasing sophistication):**

1. **Fully random** — pick a random card to remove and a random valid card to add. Simple, unbiased, but slow to converge; good for the early exploration phase.
2. **Stratified random** — bias removal toward the current deck's lowest-power cards and addition toward higher-power or same-species/order candidates, while still keeping some fraction fully random for exploration.
3. **Heuristic-scored** — score every possible swap on a combination of (a) power differential, (b) whether it increases concentration in a species/order the deck is already leaning into (more combo potential), and (c) mana-curve impact; then sample from the top-scoring candidates with some randomness (avoid being fully deterministic, which would collapse exploration).

A reasonable approach is to phase the search: random early, stratified in the middle, heuristic-guided late — trading exploration for exploitation as the search progresses.

**Selection between existing nodes**: use a UCB1-style formula (exploitation term = observed win rate, exploration term = a bonus that shrinks as a node accumulates more simulations) to decide which existing node to expand next, rather than always expanding the current best.

**Acceptance rule**: only keep a child as an improvement over its parent if its win rate beats the parent's by a margin large enough to be statistically meaningful (not just numerically higher) — see §5.

**Backpropagation**: when a child's evaluation completes, propagate its result up through its ancestor chain so ancestor win-rate estimates stay current for future UCB1 selection.

---

## 8. Implementation Notes (Oracle conventions)

- Follow existing conventions: target ~35 lines/function, ~400 lines/file.
- Suggested new files, consistent with existing naming patterns (`strat_*.c` for AI strategies):
  - `deck_builder.c` / `.h` — seed deck construction (Order B + Minotaur build, power-max baseline), shared validity-check helpers (40-card count, duplicate/limit checks).
  - `deck_genetic.c` / `.h` — GA population, fitness (round-robin), selection, crossover, mutation.
  - `deck_mcts.c` / `.h` — MCTS node structure, UCB1 selection, swap generation phases, backpropagation.
  - `deck_eval.c` / `.h` — shared parent-vs-child match evaluation harness (§5), used by both GA and MCTS.
- Reuse existing `GameContext`, `StrategySet`, and `run_simulation()`-style plumbing from `stda_auto.c` rather than duplicating game-loop logic — the evaluation harness is essentially "run N games between two fixed decks with one shared strategy" and should sit on top of the existing simulation infrastructure, not reimplement it.

---

## 9. Open Questions to Resolve Before Implementation

- What is the maximum number of copies of any single unique card allowed in a custom 40-card deck?
- Are there minimum/maximum counts mandated for Draw/Recall or Cash cards in a legal custom deck (the CLI/roadmap docs may already define this)?
- Once §1's `DeckType` threading fix is in place, confirm `resolve_combat()` receives the correct type for both stda.auto simulation games and any future interactive modes.
