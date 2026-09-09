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



1. **`A11` IS-MCTS + NN** ("AlphaOracle Prime", `doc/ai_agents.md`'s A11
   section) -- ✅ done and
   **registered 2026-09-03** (see `doc/changelog.md`'s 2026-09-03 entry and
   `doc/ai_agents.md`'s A11 section). Stages 1-3 (state encoder, self-play corpus +
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
   Prime Plus I** (PUCT + policy head, item 4 below; revised 2026-09-04 from an
   earlier "Prime II" placeholder to its own "Plus" lineage, since this changes
   the search mechanism itself rather than adding one lookahead ply the way
   `A7`->`A9` "Grandmaster"->"Grandmaster II" did) is the next agent name in
   this lineage, not a corpus-size/technical suffix on the player-facing name.
   **Closes the original `A1`-`A11` ladder.**
2. **Interactive human-play match exporter** (rescoped 2026-08-28 from `ideas/4
   match results export/`'s original design). Purpose, per Jonathan: log true
   human-played games (vs AI and vs human) to mine heuristics from human play
   patterns and to feed this item's own neural network training data -- not seat-
   advantage tooling, which the mulligan/seat-advantage investigation
   (`doc/changelog.md`'s 2026-08-28 entry) already answered with purpose-built
   batch tooling instead. Sequenced alongside `A11`'s own family. `ideas/4`'s
   design needs updating before use: stale
   `GameState`/`PlayerType` types (the engine's are `struct gamestate`/
   `AIStrategyType`), a "does going first matter?" analysis that doesn't work as
   written (infers seat from `turns_played` parity, meaningless here since Player A
   always goes first in batch mode -- see the mulligan/seat-advantage investigation's
   finding, above, that this is intentional, not a gap), and gamestate instrumentation
   it assumes exists
   (`total_damage_dealt[2]`, `champions_played[2]`, etc.) that doesn't yet.
3. **SDL3 GUI**, together with save/load game state (`ideas/6 save and load gamestate/`)
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
4. **`A14` PUCT + policy head** ("AlphaOracle Prime Plus I") -- ✅ done and
   **registered 2026-09-08/09** (see `doc/changelog.md`'s 2026-09-09 entry and
   `doc/ai_agents.md`'s A14 section). PUCT (Predictor + UCT) selection
   replacing `A10`/`A11`'s plain UCT, directed by a learned policy prior from a
   retrained two-head value/policy net -- the first agent in this project
   whose tree structure differs from plain UCT, not just its leaf evaluator.
   Two roadmap corrections found while designing it: a slot-indexed policy
   head cannot work (the state encoder carries no hand-slot ordering), fixed
   by indexing over the same 105-type catalog the state encoder already uses;
   and `A11`'s own `leaf_value()` evaluates off-distribution at roughly half
   its leaves (a latent defect independent of PUCT, measured as a null result
   for `A11` itself and not shipped as a patch, but validating the
   side-to-move convention this agent depended on regardless). Value MSE
   plateaued genuinely better than `A11`'s own shipped 0.1705 floor
   (0.148-0.165 by matchup) -- the retrain itself worked. **The PUCT mechanism
   and its policy prior measured a genuine null result on strength**: 49.34%
   [47.82%, 50.87%] head-to-head vs `A11` (parity, not a win), an estimated
   Borealis rating of ~62 (vs `A11`'s 74, a real non-transitive result worth
   reading in full), and -- after the most statistically decisive
   dial-calibration effort in this project's history (four sweeps plus a
   properly-revalidated joint optimize pass, n=16,000 at the end) -- none of
   the four PUCT dials move win rate off the shipped defaults at all.
   `doc/ai_agents.md`'s A14 section gives the full reasoning for why: most
   likely the learned prior's ceiling is `A11` itself (one round of corpus
   generation, not a bootstrapped self-play loop), compounded by PUCT's own
   ~4x per-decision cost (1.7s mean, root-caused to its argmax selection
   lacking `A10`/`A11`'s "untried move always wins" guarantee) possibly
   eating into the very search breadth the prior needs to pay off. **Registered
   anyway, unconditionally** (Jonathan's call, 2026-09-08, made while the
   corpus was still generating) -- the mechanism itself is the milestone,
   matching `A13`'s own "registered for character, not strength" precedent.
   The `limit_iterations` re-sweep this item originally deferred to is now
   effectively answered: a sweet-spot sweep (1500-4000) found no clear
   dependence in range, so 4000 stays shipped and no further re-sweep is
   planned. Self-play round 2 stays a real, distinct future option (not
   attempted -- gated on a rising `prior_trust` signature that never
   materialized this round) if there's ever appetite to revisit it.
5. **3-4 player mode**. Jonathan has an existing physical/tabletop 3-4 player variant of
   the game; digitizing it is a genuine engine-level rework (`PlayerID` is binary
   throughout -- roughly two dozen files use a `1 - current_player`/`1 - defender`
   opponent-lookup pattern across `core/`, every `ai_strat/` file, and the roles layer --
   not a small feature), the single biggest architecture project on this list.
6. **`ideas/10 Draft Format and Game Depth Addition Ideas/`** -- a draft format as a
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
**rating 74, the new roster ceiling**, see "Next Up" item 1). See
`doc/ai_agents.md` for the canonical roster, flavour names, and ratings.

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
