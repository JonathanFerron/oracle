# A4 — Balanced Rules · "Bean Counter" / "Compteur de Fèves" / "Contador de Frijoles"

| | |
|---|---|
| Enum | `AI_STRATEGY_BALANCED` |
| Shorthand | `balanced` |
| Est. Borealis rating | 62 — design intent, unmeasured |
| Source file | `src/ai_strat/ai_strat_balancedrules1.c` (design-comment stub exists, ~120 lines; logic not yet implemented) |
| Status | design notes only |

## The one thing this agent does

Obsessive resource accounting: derives a target cash reserve and a target effective hand
size directly from the opponent's current energy (linear formulas — see design sources),
spends/holds to hit those targets, and defends by a variance-aware rule,
`E[Total Def] ≤ E[Total Attack] − β·σ`, rather than a flat threshold.

## Deliberately out of scope

- Game-phase classification or an aggression factor (`A6` Tactical) — this agent has one
  set of formulas, not a state machine.
- Combo bonus scoring as a primary signal — resource targets come first; combo awareness
  belongs to `A2`/`A3` and above.
- A weighted multi-factor advantage function (`A5` Heuristic) — Balanced Rules is
  principled and formulaic, not a tunable weighted sum.

## Design sources

- `src/ai_strat/ai_strat_balancedrules1.c` — the only written spec: effective hand size,
  effective cash, and priority-ordered play rules, in code comments.
- `../G1 AI agent general info/balanced_tactical_hbt_comparison.md` — the explicit
  `target_cash = (opp_energy - 8) * 19/91 + 8`, `target_cards = (opp_energy - 8) * 5/91 + 3`
  formulas, and how this agent compares to `A6` Tactical and `A7` Hybrid HBT.
- `../G2 ai agent parameters storing and optimization/ai_params_guide.md` —
  `target_cash_slope` (0.2088 ≈ 19/91) as a named, tunable parameter.
- `doc/oracle_todo.md` "A5 Balanced Rules Strategy" section (todo numbering there predates
  this folder renumbering — cross-reference by content, not number).
