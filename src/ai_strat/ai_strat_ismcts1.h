// ai_strat_ismcts1.h
// A10 IS-MCTS ("The Omniscient") -- see
// ideas/A10 ai agent is-mcts (the omniscient)/about.md.
//
// SO-ISMCTS (Single-Observer, Cowling et al.): one UCT tree over information
// sets, a fresh determinization every iteration, availability counts in the
// exploration denominator. Chosen over MO-ISMCTS because Oracle has no
// partially-hidden moves -- champions/draw/recall/cash are all played
// face-up (see ai_strat_ismcts_search.h for the full architecture note).
//
// Phase 2 (2026-08-27): attack/defense decisions are tree-driven
// (ai_strat_ismcts_tree.h/ai_strat_ismcts_search.h); mulligan/discard-to-7
// still fall back to the shared power-based default (strat_lib_mulligan/
// strat_lib_discard_to_7) via ai_strategy.c's registry, pending Phase 4's
// flat-rollout scoring (limit_flat_iterations/limit_flat_candidates below
// are declared now, unused until then).
//
// limit_iterations is deliberately a fixed count, never a clock -- per-PC
// wall-clock budgeting would make this agent's Bradley-Terry rating
// PC-dependent.
//
// Phase 3 (2026-08-27) originally pinned this to 100000 (~1s/decision on the
// reference machine, i7-11700, via aicalibsrc/ismcts/calib_ismcts_timing.c --
// timing lives only in that harness, never here) under the then-shipped
// random rollout policy. Phase 6's rating measurement (below) found that
// number badly wrong once the rollout policy changed: win rate vs Borealis
// actually PEAKS around 2000-8000 iterations (63-69%, noisy but consistently
// well above the anchor) and then declines hard at higher budgets -- 65.3%
// at 16k, 58.5% at 64k, 55.2% at 100k (quick sample). More search doesn't
// just stop helping past the peak, it actively hurts, plausibly by letting
// the tree over-exploit quirks of a deterministic (non-random) rollout
// policy rather than genuinely improving play. limit_iterations=4000 was
// chosen from that curve (67.1%, n=1992 -- the most rigorously sampled point
// in the plateau, a clean round number) -- see about.md and
// doc/changelog.md's 2026-08-27 entries for the full diagnostic. This makes
// per-decision latency dramatically cheaper than the original ~1s design
// target -- see calib_ismcts_timing.c's own numbers for the old 100k budget
// as a rough upper bound; 4000 is far below that.

#ifndef AI_STRAT_ISMCTS1_H
#define AI_STRAT_ISMCTS1_H

#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ // -- Compute budget: deterministic counts, never a clock --
  uint32_t limit_iterations;             // THE "1 second" dial; pinned in Phase 3
  uint32_t limit_playout_steps;          // backstop: max SELECT/EXPAND descent steps/iteration
  uint32_t limit_max_nodes;              // arena size

  // -- Candidate enumeration (MoveGenLimits, same three dials as A8) --
  uint8_t  limit_recall_variants;
  uint8_t  limit_cash_variants;
  uint8_t  limit_max_candidates;

  // -- Selection --
  float    search_exploration_constant;  // UCT c; default sqrtf(2.0f)
  bool     search_use_availability;      // false recovers plain UCT (ablation switch)
  uint16_t search_expand_threshold;      // visits before widening past the first child

  // -- Progressive widening (Oracle's branching factor is ~93 typical) --
  float    threshold_widening_k;
  float    threshold_widening_alpha;     // children allowed = ceil(k * visits^alpha)
  bool     prior_use_heuristic;          // unused in Phase 2 -- future move-ordering hook

  // -- Rollout --
  uint16_t rollout_max_turns;            // MAX_NUMBER_OF_TURNS
  uint16_t rollout_cutoff_depth;         // 0 = play to terminal; >0 = leaf-eval cutoff (unused, Phase 2)
  float    weight_energy_advantage;      // leaf eval, used only when cutoff_depth > 0
  float    weight_cash_advantage;
  float    weight_hand_advantage;

  // -- Flat-rollout decisions (mulligan / discard-to-7) -- unused until Phase 4 --
  uint32_t limit_flat_iterations;
  uint8_t  limit_flat_candidates;

  // -- A11 IS-MCTS+NN leaf-evaluation blend (Stage 2) --
  // 0.0 = pure A10 rollout-to-terminal result, bit-for-bit identical to
  // this agent (the superset guarantee -- see ideas/A11 .../about.md's
  // "Confirmed plan" step 2); 1.0 = pure NN value (skips the rollout
  // entirely, cheaper); in between blends both. A10 itself never sets this
  // above 0.0f (see ISMCTS_DEFAULTS) -- only ai_strat_ismctsnn.c does.
  float    nn_value_trust;
} ISMCTSParams;

#define ISMCTS_DEFAULTS { \
    .limit_iterations = 4000, \
    .limit_playout_steps = 200, \
    .limit_max_nodes = 200000, \
    .limit_recall_variants = 2, \
    .limit_cash_variants = 3, \
    .limit_max_candidates = 128, \
    .search_exploration_constant = 1.41421356f, \
    .search_use_availability = true, \
    .search_expand_threshold = 3, \
    .threshold_widening_k = 2.0f, \
    .threshold_widening_alpha = 0.5f, \
    .prior_use_heuristic = false, \
    .rollout_max_turns = MAX_NUMBER_OF_TURNS, \
    .rollout_cutoff_depth = 0, \
    .weight_energy_advantage = 0.0f, \
    .weight_cash_advantage = 0.0f, \
    .weight_hand_advantage = 0.0f, \
    .limit_flat_iterations = 2000, \
    .limit_flat_candidates = 36, \
    .nn_value_trust = 0.0f, \
  }

void ismcts_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void ismcts_defense_strategy(struct gamestate* gstate, GameContext* ctx);

ISMCTSParams ismcts_get_default_params(void);

// Internal accessor for ai_strat_ismcts_flat.c -- same pattern as A7's
// hbt_live_params()/ai_strat_hbt_cards.c.
const ISMCTSParams* ismcts_live_params(PlayerID player);

// Calibration-only override hook (see aicalibsrc/ismcts/, Phase 5), settable
// per player -- same pattern as simplemc_set_params()/hbt_set_params(). Not
// part of the general strategy framework: normal play always uses the
// compiled defaults, since nothing else calls these.
void ismcts_set_params(PlayerID player, const ISMCTSParams* params);
void ismcts_reset_params(void);

#endif // AI_STRAT_ISMCTS1_H
