# A9 — HBT 2-Ply · "Grandmaster II" / "Grand Maître II" / "Gran Maestro II"

| | |
|---|---|
| Enum | `AI_STRATEGY_HBT_2PLY` |
| Shorthand | `hbt2ply` |
| Est. Borealis rating | 85 (design intent) — see "Measured" below |
| Source file | `src/ai_strat/ai_strat_hbt2ply.c` + `ai_strat_hbt2ply_reply.{c,h}` |
| Status | implemented and calibrated (2026-08-26) — see `doc/changelog.md`'s 2026-08-26 entry |

## The one thing this agent does

`A7` Hybrid HBT plus one opponent-response ply: for each candidate champion-subset move,
clone the position, commit the subset, and estimate the opponent's best reply against a
deterministic public-information surrogate hand — then blend that net-of-reply score
against `A7`'s own undefended score via `reply_trust`. "The Grandmaster, now anticipating
your reply" (names file).

**Same flagged tension as `A7`:** this agent is a synthesis by design (Grandmaster II).
The distinct personality here is specifically *the added ply*, not a new evaluation
mechanism — see `A7`'s `about.md` for the fuller discussion.

## Measured: below the design target, root cause isolated

Validated **47.2%** [46.7%, 47.7%] head-to-head vs `A7` (`hbt`, 40,000 games) — below the
`>55%` target set for this agent. Two independent calibration searches (`optimize`
against `borealis`, then directly against `hbt`) both converge to genuine, non-degenerate
`reply_trust`/`surrogate_pessimism` values and both plateau in this same neighborhood —
not a tuning gap.

Root cause, isolated via a controlled test (both sides given a corrected,
actually-blocking defense so only the attack-side logic varies): the two-ply mechanism
reaches **near-parity** with `A7` there (49.6% vs 50.5%), so the model is sound in
principle. The gap against the real `hbt` opponent traces to a pre-existing property of
`A7`'s *own* shipped defense formula (`hbt_best_defense_move()`, `ai_strat_hbt_enum.c`):
its PASS/decline baseline never charges the incoming attack, which makes declining
mathematically dominate every blocking option under `A7`'s shipped weights — found while
building this agent, not introduced by it (see `ai_strat_hbt_enum.h`'s comment on that
function, and `doc/changelog.md`'s 2026-08-26 entry for the full diagnosis, including a
resource-trading-bias defect this agent's own scoring formula had and was fixed for
along the way). Since `A7`'s real defense essentially never blocks, this agent's ply has
nothing real to correct for against the actual opponent it is measured against — a
genuine prerequisite gap, not a design dead end. Fixing `A7`'s (and `A5`'s, which has the
identical defect) defense formula is believed necessary before this agent's own design
target is reachable; that fix is deferred to its own task (see memory:
`project_a5_a7_defense_pass_dominance`), conditioned on `A5`/`A7` measuring at least as
strong against `Borealis` afterward, not merely "more realistic."

Shipped anyway as a playable, calibrated roster member (matching the `A4`/`A8`/`A12`
precedent of reporting a below-target result honestly rather than withholding it).

## Deliberately out of scope

- Any evaluation mechanism not already in `A4`/`A5`/`A6`/`A7` — the only new thing this
  agent adds over `A7` is the second ply.
- Sampling/rollouts (`A8`) or a real search tree with backpropagation (`A10`) — this stays
  a fixed 2-ply minimax-on-expectations, not a general search.
- Determinization / hidden-information reasoning: this agent instead uses a
  deterministic, non-sampling public-information surrogate hand
  (`ai_strat_hbt2ply_reply.c`'s `build_surrogate_hand()`) rather than `A8`'s
  `mc_determinize()` — keeps this agent fully deterministic like `A7`, and keeps its cost
  close to `A7`'s own (measured ~3200 games/sec via the calibration harness) rather than
  `A8`'s ~100x rollout overhead. (`A8`'s own determinization, added 2026-08-25, predates
  this note; the "not yet built" framing this line originally carried is stale.)
- Fixing `A7`'s (or `A5`'s) own defense formula — deliberately left untouched here even
  after being identified as this agent's own blocker; see "Measured" above.

## Design sources

- `../A7 ai agent hybrid hbt (the grandmaster)/hbt_design_notes.md`,
  `../A7 ai agent hybrid hbt (the grandmaster)/strat_hbt_sketch.h` — the base agent this
  one extends by one ply.
- `../G1 AI agent general info/oracle_ai_agent_names.md` — confirms ordering:
  "Simple Monte Carlo is weaker than HBT 2-Ply."
