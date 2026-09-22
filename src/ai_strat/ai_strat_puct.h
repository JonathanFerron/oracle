// ai_strat_puct.h
// A14 AlphaOracle Prime Plus I -- PUCT + policy head. See
// doc/ai_agents.md's A14 section for the full design.
//
// Reuses A10/A11's exact ISMCTSParams/ismcts_search_best_move() for
// compute budget, candidate enumeration, and rollout/determinization
// machinery. This agent's own SELECTION mechanism is PUCT (replacing
// plain UCT, ai_strat_puct_search.c) and its LEAF EVALUATION is a
// two-head net (ai_strat_puct_net.c, supplying both a value and a policy
// prior from one shared forward pass) -- never A10/A11's own
// tree/determinization structure itself. **As of 2026-09-22, PUCT
// selection is measured and shipped OFF by default** (`use_puct=false`,
// see PUCT_DEFAULTS below and this field's own comment) -- the shipped
// default is plain UCT selection over this agent's own two-head net, not
// PUCT selection. The PUCT mechanism remains fully implemented and
// tested; `use_puct=true` is a real, exercised configuration, just not
// the default one. See doc/ai_agents.md's A14 section, 2026-09-22
// addendum, for the measurement that drove this. This agent's leaf
// evaluation never calls the shared ai_strat_ismcts_search.c's own
// leaf_value() (it has its own puct_leaf_value(), ai_strat_puct_search.c)
// -- so its own ISMCTSParams instance leaves nn_value_trust/
// nn_value_use_mover_seat entirely unread, dead weight kept only because
// A10/A11 still need those fields in the struct they share.
//
// PUCTParams below is a genuinely DISJOINT struct from ISMCTSParams --
// unlike A11 (which shares ISMCTSParams with A10, see ai_strat_ismcts1.h's
// nn_value_trust comment and the "shared-struct gotcha" documented in
// aicalibsrc/ismctsnn/README.md), a THIRD agent adding fields to that same
// struct would compound an already-documented calibration risk.

#ifndef AI_STRAT_PUCT_H
#define AI_STRAT_PUCT_H

#include "ai_strat_ismcts1.h" // ISMCTSParams / ISMCTS_DEFAULTS
#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ // -- Selection --
  bool use_puct; // SHIPPED DEFAULT (2026-09-22): false -> plain UCT
  // SELECTION (ai_strat_ismcts_search.c's own select_or_expand()) while
  // leaf evaluation still uses this agent's own two-head net regardless.
  // Originally built as an ablation rung isolating the net's own
  // contribution from PUCT's, this measured a decisive 57.15%
  // [55.63%, 58.66%] head-to-head win vs A11 (n=4110) -- A14's own
  // original null result decomposes into "good net, bad selection rule",
  // so this became the shipped default rather than staying an ablation
  // (doc/ai_agents.md's A14 section, 2026-09-22 addendum). true restores
  // real PUCT selection -- still fully implemented and tested, just no
  // longer the default. Neither setting is a full A10/A11 restoration --
  // that only happens when the CALLER passes puct_params == NULL entirely
  // (ai_strat_ismcts1.c's/ai_strat_ismctsnn.c's own call sites, and this
  // agent's own decide_and_apply() when weights aren't loaded).
  float c_puct; // PUCT exploration constant
  float fpu_reduction; // value assigned to an unvisited child (parent's
  // own Q minus this), replacing plain UCT's +infinity-for-unvisited rule

  // -- Learned prior --
  float prior_trust; // 0.0 = uniform prior (PUCT formula, no learned
  // signal); 1.0 = pure learned prior; in between blends both
  float policy_temperature; // softmax temperature on composed logits
  // (puct_compose_priors(), ai_strat_puct_policy.c)

  // -- Progressive widening ablation --
  bool use_widening; // false (default): a learned prior makes widening
  // largely redundant for this agent, unlike A10/A11 where it's load-
  // bearing; true reuses ISMCTSParams' own threshold_widening_k/_alpha/
  // search_expand_threshold fields

  // -- Self-play exploration (declared, NOT wired -- read by nothing) --
  // Originally intended to mix Dirichlet noise into the root's own priors
  // during self-play generation (AlphaZero-style), parsed/pinned by
  // calib_puct.c/calibrate_puct.py but never consumed by
  // compose_node_priors() or anywhere else in src/ (confirmed by grep,
  // 2026-09-22, A16 Session 2 item 2.2 -- see doc/ai_agents.md's A14
  // section, 2026-09-22 addendum, for why: compose_node_priors() lives
  // inside puct_select_or_expand(), which the shipped default
  // (use_puct=false) never calls, so wiring this would only matter for an
  // explicit use_puct=true configuration, not for anything that ships).
  // Also, root widening already structurally prevents the blind-spot
  // failure mode this noise exists to fix, at least at the shipped
  // limit_iterations=4000 (widening_cap = ceil(2*sqrt(4000)) = 127,
  // comfortably above Oracle's ~93 typical legal moves -- every legal root
  // move gets expanded at least once under plain UCT, by construction).
  float root_dirichlet_alpha; // meaningful only if this ever gets wired
  // AND use_puct=true is explicitly set -- currently inert either way
  float root_noise_frac; // same caveat as root_dirichlet_alpha above
} PUCTParams;

#define PUCT_DEFAULTS { \
    .use_puct = false, \
    .c_puct = 1.5f, \
    .fpu_reduction = 0.2f, \
    .prior_trust = 1.0f, \
    .policy_temperature = 1.0f, \
    .use_widening = false, \
    .root_dirichlet_alpha = 0.0f, \
    .root_noise_frac = 0.0f, \
  }

// Repo-root-relative, same assumption ISMCTSNN_DEFAULT_WEIGHTS_PATH already
// relies on. Not yet wired into main.c/cmdline.c -- Stage 7's registration.
#define PUCT_DEFAULT_WEIGHTS_PATH "assets/puct/plus1_weights.bin"

// Loads the trained two-head net's weights (ai_strat_puct_net.h's file
// format). Returns false on failure (missing file, size mismatch); any
// previously loaded weights are kept. decide_and_apply() (ai_strat_puct.c)
// checks puctnet_is_loaded() itself and, if false, forces a full degrade
// to plain A10 (puct_params passed as NULL, ISMCTSParams.nn_value_trust
// forced to 0.0f) rather than running PUCT against an unloaded net's
// uninformative value=0.5/uniform-policy output -- the same "never worse
// than plain A10, even if the asset is missing" guarantee A11's own
// ismctsnn_load_weights() gives, not merely "never crashes".
bool puct_load_weights(const char* path);

void puct_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void puct_defense_strategy(struct gamestate* gstate, GameContext* ctx);

PUCTParams puct_get_default_params(void);
void puct_set_params(PlayerID player, const PUCTParams* params);
void puct_reset_params(void);

// This agent's own ISMCTSParams instance (compute budget/candidate
// enumeration/rollout fields) -- see the header comment above for why
// nn_value_trust/nn_value_use_mover_seat specifically are unread here.
// limit_max_nodes defaults to limit_iterations + 8, not A10/A11's 200000:
// at most one node is created per iteration regardless of algorithm, so
// that headroom was always generous, not required -- shrinking it here is
// a bonus (a smaller per-decision malloc), not a correctness need.
ISMCTSParams puct_get_default_ismcts_params(void);
void puct_set_ismcts_params(PlayerID player, const ISMCTSParams* params);
void puct_reset_ismcts_params(void);

#endif // AI_STRAT_PUCT_H
