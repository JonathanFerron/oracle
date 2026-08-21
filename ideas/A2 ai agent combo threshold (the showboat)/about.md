# A2 — Combo Threshold · "The Showboat" / "Le Frimeur" / "El Fanfarrón"

| | |
|---|---|
| Enum | `AI_STRATEGY_COMBO_THRESHOLD` (retired name: `AI_STRATEGY_COMBO_AWARE`) |
| Shorthand | `showboat`, alias `combo` |
| Est. Borealis rating | 37 — design intent, unmeasured |
| Source file | `src/ai_strat/ai_strat_combo_threshold.c` (not yet written) |
| Status | design notes only |

## The one thing this agent does

Chases champion combinations whose combo bonus clears a tunable threshold, hoards big
combos for a finishing blow, and defends probabilistically — sometimes declining a defence
it should take. This was the original candidate for the rating-scale benchmark; it lost
that role to `A3` Borealis precisely because these traits are exploitable rather than
diffuse (see this folder's handout §3), which is exactly what makes it a legible,
teachable rung below the benchmark instead.

## Deliberately out of scope

- Exhaustive subset enumeration or a single monotone strength dial — that's `A3` Borealis.
- Reliable defence — the probabilistic decline is the point, not a bug to fix.
- Resource targets tied to opponent energy (`A4` Balanced Rules), phase/aggression
  modelling (`A6` Tactical), advantage functions over energy/cards/cash (`A5` Heuristic),
  any lookahead or simulation.

## Design sources

- `combo_threshold_handout.md` (this folder) — full spec; §3 explains the benchmark
  reassignment, §8 explains why the weaknesses are kept as personality.
- `../A3 ai agent greedy power (borealis)/greedy_power_borealis_handout.md` §8 — the λ-dial
  argument this design lost to.
- `../G1 AI agent general info/oracle_ai_agent_names.md` — canonical roster/ratings, "What
  Changed and Why" section.
