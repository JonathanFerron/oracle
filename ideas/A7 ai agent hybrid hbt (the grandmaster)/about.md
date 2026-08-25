# A7 — Hybrid HBT · "The Grandmaster" / "Le Grand Maître" / "El Gran Maestro"

| | |
|---|---|
| Enum | `AI_STRATEGY_HYBRID_HBT` |
| Shorthand | `hbt` |
| Est. Borealis rating | 78 (design intent) — measured **62** (2026-08-25, roster-wide
  `--stda.rating` fit), the highest measured rating of any agent so far and above all
  three of its own ingredients (`A4` 34, `A5` 61, `A6` 53) in that same fit |
| Source file | `src/ai_strat/ai_strat_hbt.c` + `ai_strat_hbt_enum.{c,h}` +
  `ai_strat_hbt_cards.c` |
| Status | implemented and calibrated — see `doc/changelog.md`'s 2026-08-25 entry |

## The one thing this agent does

A fixed three-layer synthesis, in this order and no other: `A4` Balanced Rules
**weights** the advantage function via a soft resource-shortfall penalty (see
"Implementation note" below on why this is a penalty rather than a literal filter),
`A6` Tactical **weights** the same advantage function dynamically by game
phase/aggression, and `A5` Heuristic **ranks** every enumerated move by the resulting
weighted advantage. Named "The Grandmaster" as synthesis of the three approaches below
it on the ladder.

**Tension flagged, not a blocker:** guideline (b) for this sort asks each agent for one
distinct personality rather than "a little of everything." A7 (and `A9` HBT 2-Ply) are
*by roster design* syntheses — that's what "hybrid" and "Grandmaster" mean. The resolution
taken here: the personality **is** the fixed three-layer synthesis itself, and nothing
below is license to add a fourth mechanism.

## Deliberate exception: `A3`'s lethal-combo hold, added on purpose

The user requesting this agent explicitly asked for it to be "combo aware to a good
extent," beyond what `A4`/`A5`/`A6` provide on their own, so it wouldn't "feel dumb"
next to Borealis in human play. `A5`'s own enumeration already scores the *realized*
combo bonus for every candidate subset (genuine combo-aware selection, inherited for
free), but it cannot recognise a combo worth *holding back* for a finishing blow — a
1-move lookahead has no concept of "not yet." `A3` Borealis's `is_held_combo()` rule is
therefore ported verbatim (attack only), along with a local port of its
combo-protecting mulligan/discard-to-7 shape (not a call into `A3`'s own functions —
see `ai_strat_hbt.h`'s header comment for why). This is the one deliberate exception to
"deliberately out of scope" below.

## Implementation note: `A4` enters as a soft penalty, not a literal filter

This file's original text (and `hbt_design_notes.md`'s sketch) said `A4` "filters"
moves by resource constraints — i.e. drops any move outside a resource-derived budget
before ranking. The shipped agent instead applies `A4`'s resource-target formula as a
penalty subtracted from the advantage of every candidate move, never deleting a move
the ranking would otherwise pick. `A4` is the weakest of the three source agents
(rating 36) specifically because a hard filter can leave no legal move at all (`A4`'s
own `BALANCED_DEFAULTS` comment documents a traced game with 4 of 5 early turns
passing outright) — reusing that failure mode here would have undermined this agent's
whole reason for existing. See `doc/changelog.md`'s 2026-08-25 entry for the full
rationale and the corrected aggression-scaling sign versus
`ideas/G1 .../balanced_tactical_hbt_comparison.md`'s sketch.

## Deliberately out of scope

- Any mechanism not already present in `A3` (combo hold/mulligan-discard only, see
  above), `A4`, `A5`, or `A6`. If an idea doesn't trace to one of those, it belongs in a
  different agent, not bolted onto this one.
- Lookahead beyond the 1-move evaluation `A5` already does — 2-ply lookahead is `A9`'s job.
- Any sampling/simulation (`A8` Simple Monte Carlo and above).

## Design sources

- `hbt_design_notes.md`, `strat_hbt_sketch.h` (this folder) — the three-layer decision
  model, dynamic advantage-weight adjustment, `Move`/`StrategicState` struct sketches.
- `../G1 AI agent general info/balanced_tactical_hbt_comparison.md` — the original
  head-to-head reasoning across `A4`/`A5`/`A6` that produced this synthesis.
- `../A4 ai agent balanced rules (bean counter)/about.md`,
  `../A5 ai agent heuristic (eps-gam-del)/about.md`,
  `../A6 ai agent tactical (pressure cooker)/about.md` — the three source agents.
- `../A9 ai agent hbt 2 ply (grandmaster ii)/about.md` — the next rung, adds one
  opponent-response ply on top of this agent.
