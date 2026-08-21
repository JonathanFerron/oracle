# Handout — Implement the Value Based AI Agent

**Target:** `AI_STRATEGY_VALUE_BASED`
**Prerequisite:** `AI_STRATEGY_BOREALIS` ("Borealis") is implemented and
merged. This handout assumes its helpers exist and are available for reuse.
Design notes: `../A3 ai agent greedy power (borealis)/greedy_power_borealis_handout.md`.
(The retired name for this prerequisite was "The Hoarder" — see
`../G1 AI agent general info/oracle_ai_agent_names.md`, "Retired names".)
**Status to confirm:** enum, shorthand and display name are believed to already
exist in `player_config.c/h` with the agent stubbed as "not yet implemented".
Verify before touching anything.

---

## 1. Concept

A one-ply, no-simulation, **combo-blind** agent. Where Greedy Power enumerates
subsets and scores them with a λ-penalised linear model, Value Based ranks
individual cards by an **efficiency ratio** (contribution per luna) and takes
the best ones greedily until the budget or a card cap is hit.

It is deliberately simpler and deliberately weaker. Its roster value is as a
*floor*: an agent that plays "sensible-looking" cards with no understanding of
interaction. It should beat Random comfortably and lose to Greedy Power
consistently.

Deliberately **out of scope**:

- combo bonuses (this is the defining omission — see §2)
- subset enumeration of any kind
- resource targets, phase modelling, aggression modelling
- any lookahead or simulation

Expected strength: above Random, below Greedy Power (Borealis). Placeholder
rating **15**, per the canonical roster
(`../G1 AI agent general info/oracle_ai_agent_names.md`).

---

## 2. Design Decision — Why This Is Not Just "Greedy Power Minus Combos"

There is a real risk of the two agents collapsing into the same code with
different constants. Two distinct things are being conflated, and they need
different instruments:

**(a) Measuring how much combos are worth.** This is an *ablation*. It requires
holding everything else constant — same scoring model, same draw-card
heuristic, same defense logic, one bit flipped. The right tool is a temporary
compile-time or runtime flag on Greedy Power that zeroes `combo_bonus(S)`, run
head-to-head against the unmodified build. **Do not build a permanent roster
agent for this.** If you want the number, add the flag, take the measurement,
and remove the flag.

**(b) Adding a distinct low-tier agent to the roster.** This is what Value
Based is for. It should differ from Greedy Power in *character*, not just in
one term.

Value Based therefore differs on three structural axes, not one:

| Axis | Greedy Power (Borealis) | Value Based |
|---|---|---|
| Scoring | additive, λ-penalised: `Σcontrib + combo − λ·Σcost` | ratio: `contrib / (cost + k)` per card |
| Selection | exhaustive subsets 0–3, argmax | rank + greedy take under budget, cap 2 |
| Defense | play argmax subset, benefit capped at `E[incoming]` | play ≤1 champion, gated by a threat threshold |
| Combos | aware | blind |
| Tunables | `λ` | `VB_COST_FLOOR`, `VB_DEFEND_THRESHOLD` |

Note the ratio scoring is **scale-invariant** — it has no λ, so it cannot
express "lunas are worth more than damage right now". That is a genuine
weakness and a genuine difference in character, not an oversight. Say so in a
comment.

---

## 3. Files to Create

```
src/ai_strat/ai_strat_valuebased.c
src/ai_strat/ai_strat_valuebased.h
```

Source discovery is automatic via the Makefile — confirm this is still true.

Public API (header):

```c
void value_based_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void value_based_defense_strategy(struct gamestate* gstate, GameContext* ctx);
```

Everything else in the `.c` file is `static`.

---

## 4. Code Reuse — Extract Shared Helpers First

Greedy Power defines several `static` helpers that Value Based needs verbatim:

- `build_affordable_champions()` — filter hand → array of card indices
- `expected_incoming_attack()` — sum expected attack + combo over combat zone
- `try_play_draw_card()` — the §5 placeholder draw heuristic

Three shared helpers is past the threshold where duplication is the wrong
answer. **Do this as its own commit, before starting Value Based**, following
the `REFACTORING.md` four-part structure.

### Recommended: new internal helper module

```
src/ai_strat/ai_strat_common.c
src/ai_strat/ai_strat_common.h
```

Steps:

1. Move the three functions out of `ai_strat_greedypower.c`, dropping `static`.
2. Declare them in `ai_strat_common.h`.
3. `#include "ai_strat_common.h"` in `ai_strat_greedypower.c`; delete the
   local definitions.
4. Rebuild and re-run the Greedy Power test suite from its handout §9. Win
   rates against Random must be **unchanged** — this is a pure move.
5. Only then start Value Based.

### Explicitly not `strat_lib`

`strat_lib.c/h` is scoped to non-turn heuristics lifted out of
`card_actions.c` / `stda_auto.c` (mulligan, discard-to-7, champion selection
for cash exchange). Turn-strategy helpers shared between agents are a
different concern and belong in `ai_strat_common`. Do not merge the two.

### If you disagree with the extraction

The fallback is to duplicate `build_affordable_champions()` (~15 lines) and
skip the draw heuristic in Value Based. That is defensible but leaves you
unable to hold the draw-card behaviour constant between agents, which
compromises head-to-head interpretation. Prefer the extraction.

---

## 5. Scoring Model

For a single champion card `c`:

```
efficiency(c) = contribution(c) / (cost(c) + VB_COST_FLOOR)
```

where `contribution(c)` is expected attack (attack phase) or expected defense
(defense phase).

`VB_COST_FLOOR` (suggested: `1.0`) does two jobs: it prevents division by zero
on free cards, and it shrinks the ratio advantage of very cheap cards toward
the population mean — a low-cost card with tiny contribution should not
dominate the ranking purely on denominator effects. Define it as a single named
constant at the top of the file with that rationale in a comment. Do not
scatter magic numbers.

**Do not call the combo-bonus function anywhere in this file.** Its absence is
the point. Add a comment saying so, otherwise the next reader will "fix" it.

---

## 6. Attack Phase

1. If hand size < `VB_DRAW_HAND_THRESHOLD` (start with 4) and an affordable
   draw card is in hand, play the cheapest one and return.
   Use the shared `try_play_draw_card()` — behaviour must match Greedy Power
   exactly so head-to-head results isolate champion selection.
2. Build the affordable-champion list.
3. Compute `efficiency()` for each with attack contribution.
4. Sort descending.
5. Walk the sorted list, playing each card whose cost fits the *remaining*
   budget, until either the budget is exhausted or `VB_MAX_ATTACK_CARDS`
   (= 2) cards have been played.
6. If the list is non-empty, the agent always plays at least the top card. It
   has no pass option — ratio scoring provides no natural zero to compare
   against. This is a known limitation; note it in a comment as the first
   thing a stronger agent would fix.

Step 5 is the fractional-knapsack greedy rule applied to a 0/1 problem. It is
not optimal, and it will occasionally leave budget on the table where a
different pair would have fit. That is acceptable and expected for this tier —
do not add a repair pass or a second-chance loop.

---

## 7. Defense Phase

1. Build the affordable-champion list. If empty, decline.
2. Compute `efficiency()` for each with **defense** contribution; sort
   descending; take the top card as the candidate defender.
3. Compute `E[incoming attack]` via the shared helper.
4. Defend iff:

```
E[incoming attack]  >=  VB_DEFEND_THRESHOLD * expected_defense(candidate)
```

Play exactly one champion. Never more.

Rationale for the threshold form: both sides are in damage units, so the rule
is scale-free and needs no knowledge of the energy scale. Semantically it says
"do not spend a champion unless the incoming threat justifies at least a
`VB_DEFEND_THRESHOLD` fraction of what that champion can absorb." Suggested
starting value `0.5`; this is the calibration target for the agent.

Note the asymmetry with Greedy Power: that agent caps *benefit* at incoming
damage (`min(...)`) to avoid over-committing; this one gates *entry* on
incoming damage and then commits fully. Both address over-defending; they fail
differently, which is the intended contrast.

---

## 8. Structural Constraints

Project limits: **≤35 lines of code per function** (hard limit 100),
**≤400 lines per file** (soft limit 500).

Suggested decomposition:

| Function | Responsibility |
|---|---|
| `card_efficiency()` | the §5 ratio, parameterised by phase |
| `build_ranked_champions()` | affordable filter → efficiency → sort |
| `play_attack_selection()` | §6 steps 5–6 |
| `should_defend()` | §7 step 4 |
| `value_based_attack_strategy()` | orchestration only |
| `value_based_defense_strategy()` | orchestration only |

Plus `build_affordable_champions()`, `expected_incoming_attack()` and
`try_play_draw_card()` from `ai_strat_common`.

**Sorting:** build an array of `{ uint8_t card_index; float efficiency; }`
and insertion-sort it. Hand is capped at 7 by the discard-to-7 rule, so O(n²)
is fine and avoids a `qsort` comparator that would need global state to reach
`fullDeck[]`. Precomputing the efficiency also makes the tie-break explicit.

**Tie-breaks:** equal efficiency → lower cost → lower card index. Same
convention as Greedy Power. The agent must consume no RNG and be fully
deterministic given the game state.

The whole agent should land well under 300 lines.

---

## 9. Integration Points

1. **`src/ui/shared/player_config.c`** — `get_strategy_display_name()`: set the
   flavour name for `AI_STRATEGY_VALUE_BASED`, matching the accent-stripping
   convention already used there for Borealis.
2. **`src/ui/shared/player_config.c`** — interactive strategy menu: drop the
   "not yet implemented" suffix from the Value Based entry.
3. **Strategy dispatch** — register both functions in the `AIStrategyType` →
   `StrategySet` mapping (the site where Random and Greedy Power are wired).
4. **Shorthand** — confirm the entry in `AI_STRATEGY_SHORTHANDS[]`; likely
   already present, likely `value`. Verify, don't assume.
5. **`doc/changelog.md`** and the AI section of **`doc/oracle_todo.md`** —
   record the new agent.
6. **Bash completion** — if the self-reporting `print_completion_list()` work
   has landed, no change needed; confirm.

---

## 10. Verify Before Coding

Assumed from design notes; **check against actual source**:

- Whether `attack_efficiency` / `defense_efficiency` fields exist on
  `fullDeck[]`. **They probably do not.** Earlier chat notes referred to them
  as if they were real deck fields; treat that as unverified. The likely
  reality is that you must derive expected attack/defense from dice specs, as
  Greedy Power already does — reuse whatever accessor that agent settled on.
  If a precomputed field does exist, use it and note the discrepancy.
- Exact `fullDeck[]` field names for cost and card type.
- How the combat zone is represented (needed by the shared
  `expected_incoming_attack()`; should already be resolved by Greedy Power).
- `play_card()` / `play_champion()` signatures, and whether playing two
  champions means two calls or one batched call.
- `HDCLL_toArray()` allocates — every path must `free()` it, including early
  returns. `ai_strat_random.c` is the reference.

If any assumption proves wrong, flag it rather than working around it silently.

---

## 11. Testing

```bash
./bin/oracle --ai value --ai rand   -n 10000   # confirm exact flag syntax
./bin/oracle --ai value --ai greedy -n 10000
./bin/oracle --ai value --ai value  -n 10000
```

Expected results:

| Matchup | Expected | Diagnosis if violated |
|---|---|---|
| vs Random | clear win, ~60–70% | efficiency sign, or affordability constraint |
| vs Greedy Power | clear loss, ~30–40% | if near 50%, combos are not being scored in Greedy Power, or the two agents have converged — check §2 |
| vs itself | ~50% | any deviation should be attributable to first-player advantage only |

**Sample size.** Win rate is binomial; SE ≈ √(p(1−p)/n). At n = 1000 that is
~1.6 pp, so a 5 pp effect is resolvable but a 2 pp effect is not. Use
n = 10 000 (SE ≈ 0.5 pp) for anything you intend to draw a conclusion from,
and quote an interval rather than a point estimate when comparing agents.

**Parameter sweeps.**

- `VB_DEFEND_THRESHOLD`: sweep 0.0, 0.25, 0.5, 1.0, 2.0 vs Random. 0.0 means
  always defend when able; large values mean never defend. Win rate should be
  unimodal. A flat curve means `expected_incoming_attack()` is not wired in.
- `VB_COST_FLOOR`: sweep 0.5, 1.0, 2.0, 5.0. Effect should be mild — large
  values push the ranking toward raw contribution, i.e. toward "play the
  biggest card", which is a useful sanity anchor.

**Determinism check.** Run the same seed twice and diff the output. Any
difference means RNG leaked into the agent.

---

## 12. Open Question for the Human

Value Based has no pass option in the attack phase (§6 step 6). This makes it
strictly more aggressive than Greedy Power, which can score the empty set
highest and decline to play. Two readings:

- **Feature.** "Always commits" is a legible, distinct personality for a
  low-tier agent, and it makes the head-to-head loss against Borealis
  interpretable as *combos + restraint* rather than combos alone.
- **Bug.** It muddies the combo attribution, since two things differ.

**Ship it without the pass option.** If the vs-Greedy-Power gap turns out
larger than ~40/60, revisit — at that point add a minimum-efficiency floor
(`VB_MIN_EFFICIENCY`, default 0.0) as the pass mechanism rather than porting
Greedy Power's empty-set scoring, which would pull the two agents back
together.
