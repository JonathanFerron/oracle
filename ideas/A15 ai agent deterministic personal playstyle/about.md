# A15 — "The Daredevil" / "Le Casse-Cou" / "El Temerario" (design draft, not yet implemented)

**Status**: design/documentation only, captured 2026-09-08 from Jonathan's own
description of how he plays Oracle at the table. No code exists yet. This is a
pre-implementation scoping doc in the style `ideas/A10 …/`, `ideas/A11 …/` used before
those agents were built — expect it to get folded into `doc/ai_agents.md` once
implemented, calibrated, and registered, per this project's usual consolidation
pattern.

**What makes this agent different from `A1`-`A13`**: every prior agent's rules were
*designed* toward a target rating/character, then measured. `A15`'s rules are instead a
direct transcription of one specific human's actual decision procedure — the design
goal is behavioral fidelity to Jonathan's own play, not a rating target. Its measured
Borealis rating (once calibrated) is diagnostic, not a pass/fail gate the way it was for
`A11`/`A14` — a faithful transcription of a strong human player's heuristics is the
deliverable regardless of where it lands on the ladder.

**Scope**: all rules below describe Jonathan's play under the **random deck
distribution** specifically (`doc/game_rules_doc.md`'s "Random Distribution", the
default/recommended setup, deck-type-dependent combo bonus table #1). Monochrome/custom
deck play is out of scope for this design pass — a real, separate question if this
agent is ever extended to those deck types, not assumed to transfer automatically.

**Naming**: "The Daredevil" / "Le Casse-Cou" / "El Temerario" (confirmed 2026-09-08) —
the name reflects R4's calculated-risk defense rule: this agent rarely defends, but the
threshold it defends by is a precisely computed loss probability, not a reckless gut
call. Bold, not careless.

**Sources consulted**: Jonathan's own rule descriptions (this Q&A pass) plus
`doc/game_rules_doc.md` and direct reads of `src/core/combat.c`,
`src/core/game_constants.c`, `src/ai_strat/ai_strat_lib_heuristics.c` for anything
stated as a mechanical fact below. `doc/game_rules_doc.md`'s own "Strategy Tips"
section (generic advice, e.g. "don't hoard lunas", "defend when attack > 15 damage")
is **not** what this doc encodes — R3/R4 deliberately diverge from that generic advice
in favor of Jonathan's actual decision procedure; noted here so a future reader isn't
confused by the difference.

**Deck-type dependency note**: combo bonus values differ by `DeckType` (random vs.
monochrome vs. custom — see `doc/game_rules_doc.md`'s Combo Bonuses section, three
different bonus tables). This is already handled at the engine level
(`combo_bonus.c`/`combo_bonus_for_selection()` thread `DeckType` through correctly per
a prior housekeeping fix) — R6 and every other combo-potential judgment below should
call that existing utility rather than re-deriving bonus values, so this design is
deck-type-correct automatically without extra work.

---

## Rule catalog

Each rule below is stated as clarified through a Q&A pass (2026-09-08); the original
one-line rule is kept as a quote, followed by the clarified/expanded version actually
meant to be implemented.

### R1 — Turn 1 as first attacker (Player A never draws on turn 1)

> "since the first player does not pick a card on their first turn, I would only play
> cards during my first turn if: I have a draw n card, in which case I'll use it, have
> 8 cards on hand, and thus discard one (the one which gives me the least chance of
> landing a combo later down the road, or in a tie, the one with the worst expected
> attack to luna cost ratio, meaning also that I would very rarely if ever discard a
> zero cost card)."

Two **independent** triggers (not sequential — either can fire on its own):

1. **Holding a draw card** → play it. The deck has exactly two draw-card types
   (verified against `fullDeck[]`, `src/core/game_constants.c`):
   - cost 1, `draw_num=2`, `choose_num=1` (draw 2, or recall exactly 1 champion)
   - cost 2, `draw_num=3`, `choose_num=2` (draw 3, or recall exactly 2 champions)

   If holding **both** on turn 1: play the cost-1 (draw-2) card now, and hold the
   cost-2 card back. Reasoning (Jonathan, Q1): the cost-2 card's *recall-2* facet has
   good potential later in the game for pulling back zero-cost or combo-relevant
   champions from discard, and recalling 2 is worth meaningfully more than recalling 1
   — so the cheaper card's draw effect is spent now, preserving the more valuable
   recall option for a future opportunity (see R6).

2. **Hand at 8 cards** (reachable on turn 1 specifically via the draw-3 facet, since
   `INITIAL_HAND_SIZE_DEFAULT` is 6: 6 − 1 (played) + 3 (drawn) = 8) → discard exactly
   one card, chosen by:
   - primary: least combo potential looking forward
   - tiebreak: worst expected-attack-to-luna-cost ratio
   - constraint: avoid discarding zero-cost cards (applies generally, see R3 below —
     confirmed by Jonathan as a general aversion, not turn-1-specific)

### R2 — Mulligan (Player B only — `apply_mulligan()` always dispatches to Player B)

> "unless my hand is very strong with combos on hand, or zero cost cards, I would
> mulligan, typically 2 cards, to maximize my combo potential."

Skip the mulligan only if either holds:
- the hand already contains an **achievable combo right now** (Q4), or
- mulliganing would **break an existing 2-or-3-card combo of any kind already in
  hand** — species, color, *or* order (Q4: "don't break any 2 or 3 card in hand combo
  of any kind via a mulligan"), even if that combo isn't yet complete/playable.

Otherwise, mulligan up to the hard cap of 2 cards (`MULLIGAN_DEFAULT_MAX_CARDS`),
selecting cards to discard that maximize resulting combo potential, subject to the same
zero-cost-card protection as R1 (Q3: "avoid discarding zero-cost cards applies here
too").

Mechanical note: `strat_lib_mulligan()`'s existing default redraw is **blind** (discard
chosen cards, then draw random replacements) — this is a real, unavoidable game
mechanic, not a design choice `A15` can route around.

### R3 — Luna budget indifference

> "I typically play the whole game will very little regards to luna budget as we start
> with an ample budget anyway."

Clarified (Q5) to be sharper than simple indifference: **essentially never play Cash
cards at all.** Preferred handling of a Cash card in hand: let the hand grow toward 8
and discard the Cash card during the discard-to-7 phase (R5) rather than spending a
turn playing it. Luna-cost only enters the strategy as R1's attack/luna tiebreak
signal, never as a budget constraint on what to play.

### R4 — Defense threshold

> "I generally do not defend, unless not doing so means I have a chance of more than p%
> of loosing in the current combat given the known attacker cards in the combat zone (p
> could be an optimized dial, likely starting with something like 30%)."

Precisely clarified (Q8): "losing" means **being defeated by this specific
undefended combat** — i.e., `P(damage from this attack, if left undefended, ≥ my
current energy)`. Not a broader game-loss estimate; a concrete, computable
per-combat probability over the attacker's already-committed cards' known dice
distribution (`RND_dn(defense_dice)`) plus their combo bonus.

Decision rule: **decline to defend unless that probability ≥ p%** (dial, default
guess ~30%, to be calibrated). If defending is triggered, R7 governs which champions
to commit.

### R5 — Hand-size preference / discard-to-7

> "My strategy typically works best, given that I seek maximum combo potentials, when I
> keep my hand full with 7 cards, though I make sure I never 'waste' a card by
> discarding one in the 'discard to 7' phase, unless that's clearly to my advantage."

Target hand size: 7. At the discard-to-7 phase, only discard a card if doing so is
clearly advantageous — the given example is discarding a card that a *held* recall-2
card (see R1) can later pull back, when that card is either zero-cost or sets up a
strong future combo.

Clarified (Q6): "planning a recall" does **not** mean deliberately discarding
combo/zero-cost cards into the discard pile as bait. Cards reach the discard pile
through ordinary play (attacking or defending with them); "planning a recall" means
**holding the recall-capable card in hand until a good recall opportunity naturally
presents itself** from what's already in the discard pile — not engineering the
discard pile's contents in advance.

### R6 — Immediate combo attack

> "if I have a full 3 card of the same species combo in my hand, I play it right away
> to attack, as there is no point in waiting."

Same-species specifically is the trigger — clarified (Q9) as the highest-value combo
type, not a stand-in for "any 3-card combo of any kind." This is an unconditional,
non-deferring rule: no waiting for a hypothetically better combo later.

This also resolves the apparent tension with R5's "never waste a discard" principle: if
a turn-1 draw (or any draw) brings the hand to 8 and a same-species 3-combo is present,
**play that combo immediately** rather than holding for something better — "pull the
trigger... I won't wait for 'a better combo'" (Q9).

**Resolved**: color/order combos get **no** general "play now" preference — confirmed.
The one exception: if *not* playing a complete color/order combo now would force
discarding one of its cards at the discard-to-7 phase, play it now rather than lose a
card to the discard. (Species combos never face this tension in the first place, since
R6 already plays them immediately, before hand size becomes a factor.)

### R7 — Defense card selection: "+0" cards

> "if ever I have to defend, I focus on using cards with 'plus 0' to the dice roll, as
> those cards have the same attack and defence efficiency."

Verified precisely against `src/core/combat.c`: attack contributes
`attack_base + roll(defense_dice)`; defense contributes `roll(defense_dice)` alone (no
base added) — both draw from the *same* `defense_dice` field. A champion with
`attack_base == 0` therefore has **numerically identical** expected attack and defense
contribution, which is exactly why `attack_efficiency == defense_efficiency` for those
cards (spot-checked: champion `#1` in `fullDeck[]` has `attack_base=0`,
`expected_attack == expected_defense == 2.5`, `attack_efficiency == defense_efficiency
== 10`).

Rule: **when defending, prefer committing champions with `attack_base == 0`** over
others.

**Resolved — fallback when no `attack_base == 0` champion is available (or isn't
enough to clear R4's threshold)**, in priority order:
1. Maximize combo bonus on the defending selection, if that's what it takes to push
   the loss probability below `p%` — this is the only circumstance where combo-forming
   is a defensive rather than offensive consideration.
2. Otherwise, prefer the lowest available `attack_base` champions (closest to the
   ideal, even if not exactly 0).
3. **Never break a combo currently being held for next turn's attack** to build this
   turn's defense — the overriding priority stays "attack next turn," not "defend
   optimally now." The only job of defending at all is to avoid dying *this* turn
   (R4); once that's achieved, stop spending resources on it.

### R8 — Endgame ("kill sequence") mode

> "once the probability of being able to 'finish off' my opponent within the next 4
> attacks... is equal to or greater than a threshold of say q1, I 'kick off' the
> 'endgame mode'... going all-out over a period of 4 consecutive attacks with the goal
> to win the game within the next 4 attacks."

Added 2026-09-09, verified arithmetically consistent: from 8 cards at the triggering
turn *i* (after that turn's draw), exactly 3 more of *my own* draws arrive over the
next 3 of my own turns — 8 + 3 = 11 = 2+3+3+3 champions across 4 attacks (respecting
the 3-champion attack cap), so the hand empties exactly on the 4th, final attack, in
the idealized case where every card drawn/held through the window is a champion.

**Trigger, per my own attack turn, evaluated right after that turn's draw**:

- Turn *i*: if `P(finish opponent within next 4 attacks) ≥ q1` → enter endgame mode,
  commit to the 2-3-3-3 attack pattern above.
- Turn *i+1* (already in endgame mode): re-check `P(finish within next 3 attacks) ≥
  q2`. **If it now falls below q2, abort back to normal R1-R7 play immediately**
  (confirmed 2026-09-09) — the commitment is contingent each turn, not a one-way
  door.
- Turn *i+2*: same re-check against `q3` (2-attack horizon).
- Turn *i+3* (final attack): same re-check against `q4` (1-attack horizon).

**Defense stays active, unchanged, throughout** (confirmed 2026-09-09): R4's p%
death-probability threshold keeps operating normally on every combat during the
endgame window. Going "all-out" means committing hand resources to offense, not
abandoning the one safeguard against dying before landing the kill — this agent is a
calculated risk-taker (R4), not a reckless one, even in its endgame gambit.

**Probability calculation**: `P(finish opponent within next N attacks)` weighs my own
expected attack output against the opponent's expected defense capacity, over the
next N of my own attacks. **Correction (2026-09-09)**: my own future draws are *not*
privileged knowledge, even though it's my own deck — in the random distribution
format, neither player ever sees their own 40-card deck's composition up front; a
card's identity is only known once actually revealed (drawn to hand, or played to
discard/combat zone). So my own future draws and the opponent's future draws are
**both** expectations over the exact same shared unseen pool, symmetrically — no
special-cased "I know my own deck" branch. My currently-*held* cards (already drawn,
so known) are the one asymmetry: I know my own hand exactly; the opponent's hand is
known only by *count*.

**This reuses existing, already-verified infrastructure**:
`strat_common_unseen_pool()` (`src/ai_strat/ai_strat_common.h`, pool = 120 − own hand
− both discards − both combat zones) is precisely `A13` Cartographer's own machinery
for expectation-over-the-exact-unseen-pool reasoning
(`src/ai_strat/ai_strat_a13.h`'s `belief_draw_weight`/`belief_reshuffle_trust`
mechanism) — no new statistical infrastructure needed, just a new use of an existing,
battle-tested utility, applied symmetrically to both sides' future draws.

**Dial**: `q1, q2, q3, q4` (per-horizon endgame thresholds, 4-attack down to 1-attack),
sibling to R4's `p%` — both are calibration targets for a future sweep/optimize pass
once implemented, same methodology.

*Open, not yet resolved*: the specific 2-3-3-3 ordering (small attack first, then
committing fully) was Jonathan's stated pattern, but whether that ordering is a fixed
schedule or should flex based on actual combo composition available each turn (e.g.
playing a ready 3-combo before a non-combo 2-card play, regardless of position in the
sequence) hasn't been asked directly — default assumption for a first implementation
is combo-quality still governs *which* champions are chosen each attack (R6 keeps
applying), only the champion *count* per attack follows the 2-3-3-3 schedule.

---

## Summary of open items

One, added with R8: whether the 2-3-3-3 endgame attack-count schedule is rigid or
should flex around combo availability (default assumption: count follows the
schedule, card *selection* within each attack still follows R6/combo-quality).
Everything else from the original 7-rule draft is resolved: color/order combo
exception, defense-fallback priority order, `p% = 30` starting value, naming
("The Daredevil" / "Le Casse-Cou" / "El Temerario"). Ready to move to implementation
whenever Jonathan wants to start Stage 1.

## Non-goals

- No luna-budget optimization (R3 makes this explicit — deliberately simpler than
  every prior agent in this respect).
- Not designed to maximize Borealis rating — designed to match Jonathan's own
  real-table decisions. A low or high measured rating are both legitimate outcomes;
  neither invalidates the design.
