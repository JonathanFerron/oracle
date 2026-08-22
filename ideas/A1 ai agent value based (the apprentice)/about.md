# A1 — Value Based · "The Apprentice" / "L'Apprenti" / "El Aprendiz"

|                      |                                                        |
| -------------------- | ------------------------------------------------------ |
| Enum                 | `AI_STRATEGY_VALUE_BASED`                              |
| Shorthand            | `value`                                                |
| Est. Borealis rating | 15 — design intent; measured win rate vs Random is ~92.4% after calibration (`VB_COST_FLOOR=1.3`, `VB_DEFEND_THRESHOLD=0.8`, see `doc/changelog.md`, 2026-08-21), well above the design intent, but a Bradley-Terry rating needs `A3` Borealis as benchmark first |
| Source file          | `src/ai_strat/ai_strat_valuebased.c` (implemented 2026-08-21) |
| Status               | implemented -- see `doc/changelog.md`, 2026-08-21                       |

## The one thing this agent does

Ranks individual champion cards by an efficiency ratio, `contribution / (cost + k)`, and
greedily takes the top ones (cap 2) under the cash budget. No subset enumeration, no
lookahead — a pure per-card ranking.

## Deliberately out of scope

- Touching the combo bonus in any way — the omission *is* the agent. It "knows one thing (efficiency) and nothing else" (roster naming rationale, #2).
- Subset/combination enumeration of any kind (that's `A3` Borealis).
- A pass option in the attack phase — ratio scoring has no natural zero to compare against; this agent always commits at least its top card.
- Resource targets, phase modelling, aggression modelling, any simulation.

## Design sources

- `value_based_handout.md` (this folder) — full spec, scoring model, structural constraints.
- Prerequisite: `../A3 ai agent greedy power (borealis)/greedy_power_borealis_handout.md`
  (`build_affordable_champions()`, `expected_incoming_attack()`, `try_play_draw_card()` are
  meant to be extracted into a shared `ai_strat_common` module and reused here).
- `../G1 AI agent general info/oracle_ai_agent_names.md` — canonical roster/ratings.
