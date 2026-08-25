# A4 — Balanced Rules · "Bean Counter" / "Compteur de Fèves" / "Contador de Frijoles"

|                      |                                                                                                              |
| -------------------- | ------------------------------------------------------------------------------------------------------------ |
| Enum                 | `AI_STRATEGY_BALANCED`                                                                                       |
| Shorthand            | `balanced`                                                                                                   |
| Est. Borealis rating | 62 — design intent, superseded by measurement (see below)                                                    |
| Measured rating      | **36** (`--stda.rating`, 2026-08-24) — below the Borealis anchor (50), a legitimate result, not a defect     |
| Source file          | `src/ai_strat/ai_strat_balanced_rules.c`/`.h` — implemented and calibrated 2026-08-24                        |
| Status               | implemented and calibrated                                                                                   |

## The one thing this agent does

Obsessive resource accounting: derives a target cash reserve and a target effective hand size directly from the opponent's current energy (linear formulas — see below), spends/holds to hit those targets, and defends by a variance-aware rule,
`E[Total Def] ≤ E[Total Attack] − β·σ`, rather than a flat threshold.

## Deliberately out of scope

- Game-phase classification or an aggression factor (`A6` Tactical) — this agent has one set of formulas, not a state machine.
- Combo bonus scoring as a primary signal — resource targets come first; combo awareness belongs to `A2`/`A3` and above. The shipped `combo_weight` parameter stays at `0.0`; calibration confirmed that pushing it above `0.5` measures stronger but erodes this boundary (see `doc/changelog.md`, 2026-08-24).
- A weighted multi-factor advantage function (`A5` Heuristic) — Balanced Rules is
  principled and formulaic, not a tunable weighted sum.

## Resource formulas (as shipped)

`target = slope·(opp_energy − 8) + intercept`, clamped at ≥ 0, divided by `late_game_aggro` once `opp_energy ≤ lethal_horizon`. **Two corrections to the formulas originally proposed in the design sources below**, found while implementing:

1. **Intercept**: the original stub's own numeric tables (cash 19→0, cards 5→0 as energy goes 99→8) fit `slope·(E−8)` with intercept **0** exactly — not the `+8`/`+3` stated in `balanced_tactical_hbt_comparison.md`/`ai_params_guide.md`, which misread the stub's inverse form. Shipped intercepts are non-zero after calibration (`target_cash_intercept=-2.73`, `target_cards_intercept=-0.99`), but that's a measured result, not a return to the `+8`/`+3` reading.
2. **Cash-ladder slope**: the stub's `19`-luna reserve at full opponent energy is a fossil of an obsolete starting-cash rule; today's `INITIAL_CASH_DEFAULT` is 30. Naively re-anchoring to `30/91 ≈ 0.33` turned out to be a genuine bug, not just an untuned guess — at full opponent energy it leaves ~0 spendable surplus, trapping the agent unable to attack for several early turns (confirmed by playtrace and a parameter sweep, `doc/changelog.md`). The shipped, calibrated slope is `0.081`.

## Calibration (2026-08-24)

Free `differential_evolution` search (`aicalibsrc/balanced/`) eroded this agent's identity — resource-target slopes toward 0, `defense_beta` toward "never defend" — while measuring stronger, mirroring `A2`'s rejected `aggression_level=2.21`. Shipped values instead come from `optimize --identity-safe`, which searches a narrower bound that keeps both slopes non-degenerate and `defense_beta ∈ [0.25, 2.0]`. Measured (validated): vs `borealis` 34.3%, vs `combo` ~59.7%, vs `value` ~58.1%, vs `rand` ~98.5%. Full details, shipped parameter values, and the personality-check rationale: `doc/changelog.md`, `aicalibsrc/balanced/README.md`.

## Design sources

- `src/ai_strat/ai_strat_balanced_rules.h` — the implemented spec, header comment (effective hand size, effective cash, priority-ordered play rules, and the two corrections above).
- `../G1 AI agent general info/balanced_tactical_hbt_comparison.md` — the original comparative analysis; its `+8`/`+3` intercepts and `19/91` cash slope are both superseded by the corrections above.
- `../G2 ai agent parameters storing and optimization/ai_params_guide.md` —
  `target_cash_slope` as a named, tunable parameter; its stated default value is superseded by the same corrections.
- `doc/changelog.md`'s 2026-08-24 entry — the full implementation and calibration writeup.
