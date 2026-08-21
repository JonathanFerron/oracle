# Deck Construction AI Facet — Implementation Handout

**Status**: Design finalized, ready for implementation
**Scope**: Adds a "deck construction" facet to each AI agent, alongside the existing in-game player facet
**Rating model**: Unified — one rating per AI across draft + in-game play (no separate deck-construction rating)

---

## 1. Background / Motivation

So far, each AI agent (`strat_*.c`) only implements the **in-game player** facet — deciding attacks, defenses, draw/cash plays during a match. This handout adds the **deck construction** facet: how each AI selects its 40-card deck when the format isn't `DECK_RANDOM` (i.e. Monochrome, Custom, or Draft formats).

The two facets share the AI's "identity" (e.g. Tactical's phase-awareness applies to both deck building and in-game play) but are implemented as separate functions, following the existing `strat_*.c` per-strategy file convention.

---

## 2. AI Roster (approx. strength order)

| # | AI | Deck Construction Approach | In-Game Approach (existing/reference) |
|---|----|------------------------------|----------------------------------------|
| 1 | Random | Random pick from options | Random moves |
| 2 | Value-Based (Greedy Power) | Highest standalone power | Highest power affordable |
| 3 | **Combo-Aware (Borealis)** — benchmark, rating = 50 | Power + species/order synergy | Existing benchmark logic |
| 4 | Balanced | Consistent power, low variance, resource targets | Existing resource-target logic |
| 5 | Heuristic | Multi-factor weighted score (power/synergy/flexibility) | Advantage function (ε·Energy + γ·Cards + δ·Cash) |
| 6 | Tactical | Phase-aware heuristic scoring — **no simulation** | Phase/aggression-based EV heuristics — **no simulation** |
| 7 | **HBT** (Heuristic-Balanced-Tactical Hybrid) | 3-layer: Balanced filters → Tactical phase/aggression weighting → Heuristic scoring | Same 3-layer hybrid |
| 8 | HBT-2ply | HBT + 2-ply lookahead, only in late-draft / critical picks | HBT + 2-ply lookahead in critical game phases |
| 9 | SimpleMonteCarlo | Simulate N games per candidate card, pick best win rate | Random rollouts, no tree search |
| 10 | Info Set MCTS (IS-MCTS) | Full MCTS tree search over draft picks (if feasible) | IS-MCTS |

**Important correctness notes carried over from design discussion:**
- **Tactical performs NO Monte Carlo simulation.** It is purely heuristic/rule-based (game phase + aggression factor + expected-value heuristics). Do not implement it with simulated rollouts.
- **HBT = Heuristic-Balanced-Tactical hybrid**, not a rating-aware/history-aware AI. It combines the three named rule-based strategies' techniques (filter → weight → score). It does not consult any historical rating data.
- Only **SimpleMonteCarlo** and **IS-MCTS** use simulation/tree search, both for deck construction and in-game play.
- Rating is **unified**: a single number per AI, derived from tournament results across both deck construction and in-game performance combined. Do not build a separate deck-construction-only rating track.

---

## 3. Core Data Structures

### 3.1 `DeckConstructionContext`

New file: `deck_construction.h` / `deck_construction.c`

```c
typedef struct {
    DeckType deck_type;            // DECK_RANDOM, DECK_MONOCHROME, DECK_CUSTOM, draft formats
    const Card* full_deck;         // Full 120-card pool (reference)
    Card* available_pool;          // Cards still available to draft/select
    uint8_t pool_size;

    Card* selected_cards[40];      // Cards chosen so far, building toward 40
    uint8_t selected_count;

    Card* current_options;         // Cards visible in the current pick/decision
    uint8_t options_count;

    GameContext* game_ctx;         // For RNG, config access
} DeckConstructionContext;

typedef void (*DeckConstructionFunc)(DeckConstructionContext* ctx);
```

### 3.2 `DeckProfile` (shared evaluation helper)

Tracks running composition of the deck being built — used by Combo-Aware, Balanced, Heuristic, Tactical, and HBT.

```c
typedef struct {
    uint8_t species_count[15];   // Per-species running count
    uint8_t order_count[5];      // Per-order running count
    uint8_t color_count[3];      // Per-color running count
    uint8_t cost_counts[4];      // Per-cost-tier running count
    float total_power;
    uint8_t card_count;
} DeckProfile;
```

### 3.3 Draft phase enum (shared by Tactical / HBT)

```c
typedef enum {
    DRAFT_PHASE_EARLY,   // picks 0–15: prioritize flexibility
    DRAFT_PHASE_MID,     // picks 16–30: prioritize synergy building
    DRAFT_PHASE_LATE      // picks 31–40: prioritize curve completion / desperate synergy
} DraftPhase;
```

### 3.4 Strategy set extension

Extend the existing `StrategySet` (in `strategy.h`) with a per-player deck construction function pointer, parallel to existing attack/defense pointers:

```c
typedef struct {
    AttackStrategyFunc attack_strategy[2];
    DefenseStrategyFunc defense_strategy[2];
    DeckConstructionFunc deck_construction[2];   // NEW
} StrategySet;
```

---

## 4. Shared Helper Functions (new: `deck_evaluation.h/c`)

To avoid duplication across `strat_*.c` files, implement these once:

- `build_deck_profile(Card** selected, uint8_t count, DeckProfile* out)` — populate a `DeckProfile` from current selections.
- `add_card_to_deck(DeckConstructionContext* ctx, Card card)` — append to `selected_cards`, update `selected_count`.
- `evaluate_mana_curve_penalty(uint8_t cost, const DeckProfile* profile)` — penalty for oversaturating a cost tier vs. target distribution (target ratios: ~30% cost 0, ~40% cost 1, ~20% cost 2, ~10% cost 3 — confirm exact targets against `oracle_design.md` before hardcoding).
- `get_draft_phase(uint8_t picks_so_far)` — maps pick count to `DraftPhase` (0–15 / 16–30 / 31–40).
- `select_monochrome_deck(DeckConstructionContext* ctx, ChampionColor color)` — existing/likely-existing helper for building a full monochrome deck once a color is chosen; verify if this already exists before writing a new one.

**Verify against actual source before implementing**: field names (`power`, `cost`, `species`, `color`, `order`, `attack_efficiency`, `defense_efficiency`), the `Card` struct definition, `ChampionColor` enum values, and `DeckType` enum values. Do not assume the names above are exact — check `card_actions.c` / relevant headers first.

---

## 5. Per-AI Implementation Specs

### 5.1 Random — `strat_random.c` (extend existing file)

```c
void random_deck_construction(DeckConstructionContext* ctx) {
    if (ctx->deck_type == DECK_RANDOM) return;   // nothing to do, setup handles it

    if (ctx->deck_type == DECK_MONOCHROME) {
        ChampionColor color = RND_randn(3, ctx->game_ctx);
        select_monochrome_deck(ctx, color);
        return;
    }

    if (ctx->options_count == 0) return;
    uint8_t choice = RND_randn(ctx->options_count, ctx->game_ctx);
    add_card_to_deck(ctx, ctx->current_options[choice]);
}
```

### 5.2 Value-Based — new `strat_valuebased.c`

- Monochrome: pick the color with the highest average champion power.
- Draft/Custom: pick the single highest-power card among `current_options`.
- No synergy, no curve awareness.

### 5.3 Combo-Aware / Borealis — new `strat_comboaware.c` (benchmark, rating anchor = 50)

- Monochrome: pick color with most balanced species distribution.
- Draft/Custom: score = power + synergy bonus (species pair/trio bonus scaled by pick phase: early picks favor starting new pairs, later picks favor completing pairs/trios) − mana curve penalty.
- This is the reference implementation all others compare against.

### 5.4 Balanced — `strat_balanced.c` (extend existing file)

- Monochrome: pick color with lowest power variance (most consistent), not highest average.
- Draft/Custom: similar to Combo-Aware's scoring but weight power more heavily (~70/30 power/synergy split vs. Combo-Aware's ~60/40) — reflects Balanced's more conservative, less synergy-hungry identity.

### 5.5 Heuristic — `strat_heuristic.c` (extend existing file)

Multi-factor weighted scoring per candidate card:
- 40% standalone power
- 35% combo potential with current `DeckProfile`
- 15% flexibility (average of attack/defense efficiency, bonus for low cost)
- 10% denial value if opponent strategy is known (stub if opponent modeling isn't available yet — return 0 and flag as a placeholder)

Monochrome: predict likely opponent color if possible, otherwise fall back to Combo-Aware's approach for color selection.

### 5.6 Tactical — `strat_tactical.c` (extend existing file)

**No simulation.** Pure heuristic, phase-aware:

- Determine `DraftPhase` from `selected_count` via `get_draft_phase()`.
- Score = power + efficiency terms, with phase-dependent bonuses:
  - **Early**: favor attack/defense efficiency and starting new species pairs.
  - **Mid**: favor completing species pairs/trios, order synergy.
  - **Late**: apply strong curve-completion penalty, heavily reward completing trios (last chance).
- Monochrome: evaluate each color via a pure heuristic quality score (average power, species distribution variance, curve balance) — **not** simulated games.

### 5.7 HBT (Heuristic-Balanced-Tactical Hybrid) — new `strat_hbt.c`

Three-layer approach, mirroring the in-game HBT design:

1. **Layer 1 (Balanced) — Filter**: `filter_viable_cards_balanced()` removes candidates that violate resource/curve constraints:
   - Reject if cost tier is oversaturated (current count > target + margin).
   - Reject weak cards (below a power threshold) during early draft.
   - Reject cards from an already-saturated species (e.g. ≥4 of same species) to preserve flexibility.
2. **Layer 2 (Tactical) — Weight**: compute `DraftPhase` and a `draft_aggression` factor (0.0–1.0) based on current deck's average power and attack/defense efficiency balance.
3. **Layer 3 (Heuristic) — Score**: evaluate each *viable* (post-filter) card using the multi-factor formula from §5.5, with phase-dependent synergy bonuses from §5.6, and an aggression-weighted attack/defense efficiency adjustment.

Monochrome: compute Balanced/Heuristic/Tactical color-quality scores independently, then combine with fixed weights (suggested starting point: 30% Balanced / 40% Heuristic / 30% Tactical — tune later via tournament data).

### 5.8 HBT-2ply — new `strat_hbt2ply.c`

- For picks before the late-draft threshold (e.g. `selected_count < 35`): delegate directly to `hbt_deck_construction()`.
- For late-draft picks (e.g. `selected_count >= 35`): for each candidate card, hypothetically add it to a test deck and evaluate resulting deck quality via `evaluate_deck_quality()` (avg power + combo potential − curve imbalance); pick the candidate yielding the best resulting deck.
- This lookahead only needs to reason about *our own* resulting deck quality, not a full opponent-response simulation — keep it cheap.

### 5.9 SimpleMonteCarlo — new `strat_simplemc.c` (defer until in-game MC works)

- For each candidate card, build a hypothetical deck, pad to 40 with random filler, run N (~50) quick partial-game simulations (first ~10 turns or until a clear leader emerges) against a random opponent, and track win rate.
- Pick the candidate with the best win rate.
- **Do not implement before the in-game Monte Carlo rollout infrastructure exists** — this shares that simulation engine.

### 5.10 IS-MCTS — new `strat_ismcts.c` (defer until in-game IS-MCTS works)

- Full tree search over draft picks using the same IS-MCTS infrastructure as in-game play.
- Lowest priority; implement last.

---

## 6. File Organization

```
src/
├── ai/  (or wherever strat_*.c currently live — verify actual path)
│   ├── deck_construction.h/c      # NEW: context struct, add_card_to_deck, etc.
│   ├── deck_evaluation.h/c        # NEW: DeckProfile, shared scoring helpers
│   │
│   ├── strat_random.c             # EXTEND: add random_deck_construction()
│   ├── strat_valuebased.c         # NEW
│   ├── strat_comboaware.c         # NEW (Borealis benchmark)
│   ├── strat_balanced.c           # EXTEND: add balanced_deck_construction()
│   ├── strat_heuristic.c          # EXTEND: add heuristic_deck_construction()
│   ├── strat_tactical.c           # EXTEND: add tactical_deck_construction() — no simulation
│   ├── strat_hbt.c                # NEW: 3-layer hybrid
│   ├── strat_hbt2ply.c            # NEW: HBT + late-pick lookahead
│   │
│   └── (deferred until in-game MC/MCTS exist)
│       ├── strat_simplemc.c
│       └── strat_ismcts.c
```

**Note**: verify actual existing file locations for `strat_*.c` (project knowledge / GitHub connector) before creating new files — the paths above are illustrative based on naming convention, not confirmed against the repo.

---

## 7. Implementation Order

1. **Foundation**: `deck_construction.h/c`, `deck_evaluation.h/c`, extend `StrategySet`.
2. **Baseline tier**: Random → Value-Based → Combo-Aware/Borealis (benchmark).
3. **Multi-factor tier**: Balanced → Heuristic → Tactical (confirm no simulation creeps in).
4. **Hybrid tier**: HBT → HBT-2ply.
5. **Testing**: tournament harness running full drafts + games, unified rating updates.
6. **Simulation tier** (later, separate work item): SimpleMonteCarlo → IS-MCTS, after in-game simulation infrastructure exists.

Each numbered step should be a separate implementation pass / Claude Code session, consistent with incremental, function-scoped delivery.

---

## 8. Testing Notes

- Unit-test each `*_deck_construction()` function in isolation with a constructed `DeckConstructionContext` (mirrors existing `strat_*` unit test patterns if any exist — check for a `test_strat_*.c` convention first).
- Integration test: run a full draft for each AI and assert the resulting deck is legal (40 cards, respects draw-2/draw-3/cash card limits per existing game rules).
- Tournament test: full draft + play, all AIs vs. all AIs, confirm Borealis anchors at rating 50 and relative ordering roughly matches §2's expected strength order (this is a sanity check, not a hard requirement — real tournament data should be allowed to shift the ordering).

---

## 9. Open Items to Resolve Before/During Implementation

1. Confirm exact `Card` struct field names and `DeckType`/`ChampionColor` enum values against source (do not assume names in this handout are exact).
2. Confirm target mana-curve distribution against `oracle_design.md` rather than the illustrative 30/40/20/10 split used above.
3. Confirm whether `select_monochrome_deck()` or an equivalent already exists, to avoid duplicating it.
4. Decide tuning method for HBT's layer weights and HBT-2ply's late-pick threshold (fixed constants for v1, tournament-tuned later).
5. Opponent-visibility rules for draft formats (e.g. Solomon draft) — how much of the opponent's picks should Heuristic's "denial value" or HBT be allowed to see? Stub as 0/unused until this is clarified.
