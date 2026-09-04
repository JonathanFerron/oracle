# Oracle Development Roadmap

**Project**: Les Champions d'Arcadie / The Arcadian Champions of Light
**Type**: Open source hobby/research project
**Focus**: Strategic dueling card game AI research, C programming patterns, game
architecture

**Scope of this document**: long-horizon phases, ordering, and status-at-a-glance. For
actionable near-term checkboxes see `doc/oracle_todo.md`.

---

### What Needs Work

- Automated simulation mode (`stda_auto.c`) needs a refactor (see `doc/oracle_todo.md`).
- No save/load, no config file system, no general match-result CSV export (the rating
  system's own CSV persistence is separate and done), no network, no GUI, no `stda.sim`.

---

## Next Up



1. **`A11` IS-MCTS + NN** ("AlphaOracle Prime",
   `ideas/A11 ai agent is-mcts + nn (alphaoracle prime)/`) -- ✅ done and
   **registered 2026-09-03** (see `doc/changelog.md`'s 2026-09-03 entry and
   `ideas/A11 .../about.md`). Stages 1-3 (state encoder, self-play corpus +
   training, hand-written C inference, tree integration, measurement) cleared
   both ship gates 2026-09-02: 58.44% head-to-head vs `A10` [56.93%, 59.94%]
   (Gate 2, the real bar) and an estimated ~74 Borealis rating [72.68%, 75.36%]
   (Gate 1, context) vs `A10`'s own 69 -- a real, well-powered win, not the null
   result `A13` hit. The three registration steps landed 2026-09-03: (1)
   `player_config.c`'s rating-table entry updated to the real `{74, true}`; (2)
   trained weights packaged as a committed asset at `assets/ismctsnn/` (a new
   top-level, category-scoped directory); (3) a default `ismctsnn_load_weights()`
   call wired into real `main.c` startup (`--ai.weights` override), which also
   fixed a related latent bug (`g_params[]` needed an explicit promotion to this
   agent's own default on a successful load, or real play would have silently
   stayed at plain `A10` even with the load wired in). Naming decided: flavor
   name stays "AlphaOracle Prime" for this UCT+value-net lineage; **AlphaOracle
   Prime II** (PUCT + policy head, item 8 below) reuses this project's own
   `A7`->`A9` "Grandmaster"->"Grandmaster II" precedent rather than a
   corpus-size/technical suffix on the player-facing name. **Closes the
   original `A1`-`A11` ladder.**
2. **AlphaOracle Prime -- bigger training corpus** (agreed 2026-09-03, the first
   of three AlphaOracle-family strengthening options discussed that session --
   see this item and item 8 below for the comparison; not written up separately
   in `doc/changelog.md`, which only covers completed work). Retrain the shipped
   value net on the original 12-hour
   full-run corpus (`aicalibsrc/ismctsnn/run_selfplay.sh`), not the 1-hour/657K
   pilot that actually shipped -- realistically ~6-8x more records once the
   measured ~44% real-vs-naive-extrapolation throughput gap (see the pilot's
   own corpus-generation numbers, `ideas/A11 .../about.md`) is accounted for.
   Picked to go *first*, ahead of item 6 below, because it's the best
   risk-adjusted bet of the three options: the shipped net needed fairly aggressive regularization (`dropout=0.4`, `weight_decay=1e-3`) just to survive past epoch 1 without catastrophic overfitting, and its val MSE (0.1705 vs a 0.2451 baseline) is not
   yet plateaued -- both point at a data-starved net, not a saturated one. The
   full pipeline (`gen_corpus.c`, `run_selfplay.sh`, `train_value_net.py`,
   `export_weights.py`, `calib_ismctsnn.c`) is already built and proven end to
   end, so this is a rerun against the existing Stage 3 ship gates (58.44% vs
   `ismcts`, ~74 Borealis rating), not new engineering -- lowest cost, most
   directly evidenced payoff of the three options. Ship only if the retrained
   net clears the *existing* candidate's own measured numbers by a real margin,
   not just noise (see `aicalibsrc/ismctsnn/README.md`'s shared-struct
   measurement gotcha before writing a comparison harness).
3. **Interactive human-play match exporter** (rescoped 2026-08-28 from `ideas/4
   match results export/`'s original design). Purpose, per Jonathan: log true
   human-played games (vs AI and vs human) to mine heuristics from human play
   patterns and to feed this item's own neural network training data -- not seat-
   advantage tooling, which item 2 above already answered with purpose-built batch
   tooling instead. Sequenced alongside `A11`'s own family (right after item 5's
   bigger-corpus retrain) since that's when the training-data need actually
   arrives. `ideas/4`'s design needs updating before use: stale
   `GameState`/`PlayerType` types (the engine's are `struct gamestate`/
   `AIStrategyType`), a "does going first matter?" analysis that doesn't work as
   written (infers seat from `turns_played` parity, meaningless here since Player A
   always goes first in batch mode -- see item 2's finding that this is intentional,
   not a gap), and gamestate instrumentation it assumes exists
   (`total_damage_dealt[2]`, `champions_played[2]`, etc.) that doesn't yet.
4. **SDL3 GUI**, together with save/load game state (`ideas/6 save and load gamestate/`)
   and the configuration file system (`ideas/7 config file/`) -- promoted out of
   "long-horizon" status (2026-08-28, see `CLAUDE.md`'s "Out of scope" section) because
   it addresses a concrete, named pain point: reading board state and deciding moves is
   slower in CLI/TUI's text card representation than it would be with a closer visual
   analog to the physical cards. Target platform is Kubuntu Linux only for now --
   Windows/iOS are explicitly not goals -- kept reasonably portable toward a future
   Android build (SDL3 has an official Android target) where that costs little, rather
   than a dedicated mobile pass now. Deliberately sequenced *before* the 3-4 player
   engine rework below, not after: the two are more independent than they first look --
   3-4 player support is an engine-level change (`PlayerID`, `GameContext`, `combat.c`,
   every AI strategy's opponent lookup, see the "3-4 player mode" item below), none of
   which lives in the GUI layer, so a GUI built for the current 2-player engine doesn't
   get thrown away when that rework lands -- it needs *extending* to draw N players
   instead of 2. **Design discipline for this reason**: write the renderer to loop over
   players rather than hardcoding a 2-player ("my side / their side") layout, so that
   later extension is additive, not a rewrite. **Asset location**: champion artwork
   (PNGs) belongs under the top-level `assets/` directory (sibling to `src/`/`bin/`/
   `doc/`), first established 2026-09-02 for `A11`'s shipped NN weights
   (`assets/ismctsnn/`) -- that folder is deliberately category-scoped per subfolder
   (`assets/<category>/...`), not a flat dump, specifically so this GUI work has a
   ready-made home (e.g. `assets/champions/`) rather than needing to invent the
   convention from scratch.
5. **AlphaOracle Prime II -- PUCT + policy head** (Stage 4, gated on Stage 3's
   ship-gate pass -- now technically unlocked, agreed 2026-09-03 to schedule it
   here, after SDL3 GUI and before the 3-4 player rework). Of the three
   AlphaOracle-family options discussed that session (see item 5 above for the
   other two), this is the highest-ceiling but highest-cost/highest-uncertainty
   one: it changes *what* `A11`'s search
   explores (a learned prior directing simulations toward promising moves)
   rather than just how much, which is the mechanism AlphaZero-class engines
   actually derive most of their strength from -- and it directly addresses
   the pathology behind `A10`'s own measured iteration-budget curve (win rate
   vs `Borealis` peaking around 2000-8000 iterations, then *declining* at
   higher budgets, diagnosed as unfocused UCT over-committing to noise in
   under-explored branches once the leaf evaluator is deterministic-ish).
   Needs, in order: the action-encoding problem solved (fixed-size logits over
   hand-card-slot "include in subset" plus pass/draw/recall/cash-target, masked
   to `get_available_moves()`'s legal set); a policy-head architecture and a
   training-data change to visit-count targets, not just terminal outcomes;
   PUCT selection replacing plain UCT in `ai_strat_ismcts_search.c`. This
   project's own track record with "add a smart-sounding mechanism on top of an
   already-good agent" is genuinely mixed (`A9`'s `reply_trust`, `A13`'s
   `hplus_trust`, and Layer R's race-aware `defense_stdev_mult` all measured at
   parity or worse despite sound reasoning going in) -- real chance of a null
   result here too, budget accordingly. **Pushing the per-decision time budget
   (`limit_iterations`, currently 4000, ~439ms at `-O2`) is folded in as this
   item's own natural follow-on step, not a separate roadmap line**: sweeping
   it *before* a policy prior exists is retesting a mechanism `A10`'s own data
   already argues against on this exact search code, but *after* PUCT lands,
   more focused search may finally pay off where raw budget alone would not --
   re-sweep `limit_iterations`/decision-time once the policy head is in, not
   before.
6. **3-4 player mode**. Jonathan has an existing physical/tabletop 3-4 player variant of
   the game; digitizing it is a genuine engine-level rework (`PlayerID` is binary
   throughout -- roughly two dozen files use a `1 - current_player`/`1 - defender`
   opponent-lookup pattern across `core/`, every `ai_strat/` file, and the roles layer --
   not a small feature), the single biggest architecture project on this list.
7. **`ideas/10 Draft Format and Game Depth Addition Ideas/`** -- a draft format as a
   third deck type alongside random/custom. Sequenced right after 3-4 player mode per
   Jonathan's call (2026-08-28); related to but distinct from `G3`'s "AI constructs a
   custom deck" facet below.

**Bottom of the list** (still intended, least urgent, distinct from the back burner
below): `ideas/11 skill vs chance eval/` -- an analytical framework for game balance,
unstarted, no active plan to pick it up soon.

**Back burner** (no active plan): `G3 ai agent deck construction/` (design already
finished -- two handouts, `custom_deck_construction_handout.md` and
`deck_construction_ai_handout.md` -- but deprioritized behind everything above, 2026-08-28);
TUI fine-tuning (`doc/oracle_todo.md`'s TUI Mode section already lists the specific
polish items); `stda.sim` (simulation UI) -- effectively superseded in spirit by the
`aicalibsrc/*/calibrate_*.py` tooling, which already covers what `stda.sim` was
originally meant to provide (sweeps, results, comparison), so this is closer to resolved
than merely deferred; client/server / networking (`ideas/8 client server/`).

---

## Long-Term Vision

### Research Goals

1. **AI Development**: progress from random → rule-based → heuristic → Monte Carlo →
   Information Set MCTS.
2. **Rating System**: Bradley-Terry model to measure AI strength objectively.
3. **Architecture**: clean client/server separation for future multiplayer.
4. **Simulation**: CSV export framework for statistical analysis of strategies.
5. **Cross-Platform**: terminal (ncurses, done), desktop (SDL3, future), mobile
   (long-term).

### Learning Objectives

Advanced AI techniques (MCTS, information sets); network programming patterns;
statistical modeling (rating systems); GUI programming (SDL3); build systems and
cross-platform development.

---

## Development Phases

Each phase below names its `ideas/` home; see `doc/oracle_todo.md` for the actionable task breakdown within whichever phase is currently active.

### Phase: Complete Game Loop — mostly done

Core turn/combat/card-action logic and all interactive-mode features (recall, cash
exchange, mulligan, discard-to-7, combat/discard display) are implemented. Remaining:
error-handling polish (see `doc/oracle_todo.md`).

### Phase: Standalone Modes — partial

- `stda.auto` (automated simulation): working, needs a refactor + CSV export
  (`ideas/2 engine and action system design/stda_auto_split_plan.md`,
  `ideas/4 match results export/`).
- `stda.cli` (interactive CLI): done except save/load.
- `stda.tui` (ncurses TUI): Milestones 1–2 done and its design-exploration folder
  archived accordingly — see `doc/changelog.md`.
- `stda.sim` (simulation UI): not started.

### Phase: AI Development — `A1`–`A11` done, ladder complete, `A11` the new roster ceiling (74)

→ `A11` IS-MCTS + neural network (AlphaOracle Prime, Stages 1-3 done 2026-09-02,
registered 2026-09-03 -- **both ship gates PASS**, 58.44% head-to-head vs `A10`,
**rating 74, the new roster ceiling**, see "Next Up" item 4). See
`ideas/G1 AI agent general info/oracle_ai_agent_names.md` for the canonical roster, flavour names, and ratings.

### Phase: Simulation & Analysis Tools — spec complete, implementation pending

CSV export (`ideas/4 match results export/`); interactive simulation UI, `stda.sim`
(no dedicated `ideas/` folder yet, see `ideas/2 …/target_folder_structure_v4.md` for
scoping notes); configuration file system (`ideas/7 config file/`, back-burnered).



### Phase: Client/Server Architecture — design complete, major refactor required

Protocol design, server (full state + validation + broadcast), client (visible state +
action submission), code separation (`sh_`/`sr_`/`cl_`/`pr_`-style modules). Depends on
the engine state-machine/action-system rework in
`ideas/2 engine and action system design/` landing first.
`ideas/8 client server/` for the client/server-specific design.

### Phase: Cross-Platform GUI — plan exists, major undertaking

SDL3 desktop GUI (`ideas/9 gui/oracle_sdl3_gui_plan.md`): card rendering, font/texture
management, responsive layout, input handling; asset pipeline (champion artwork, frames,
species/order icons); mobile ports (iOS/Android) as a long-term stretch goal.

---

## Research Questions to Explore

**AI Development**: What's the skill ceiling with perfect information?

**Game Balance**: What's the optimal starting cash amount?

**System Design**: best way to serialize game state for network play? How to handle
reconnection in multiplayer? Efficient card representation for GUI rendering? Optimal
strategy framework for pluggable AIs?

---



### Longer-Term

- [ ] Network multiplayer works reliably
- [ ] - [ ] Cross-platform GUI runs on Linux/Android
- [ ] Project serves as a good portfolio/learning showcase

---

## References

- Game rules: `doc/game_rules_doc.md`
- Architecture: `doc/oracle_design.md`
- Actionable backlog: `doc/oracle_todo.md`
- History: `doc/changelog.md`
- Contributing workflow: `CLAUDE.md`, `doc/REFACTORING.md`
- GitHub repo: https://github.com/JonathanFerron/oracle/
- Design notes: `ideas/` directory

---

*Last Updated: September 2026*
