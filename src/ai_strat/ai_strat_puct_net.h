// ai_strat_puct_net.h
// A14 AlphaOracle Prime Plus I -- hand-written plain-C forward pass for the
// trained two-head (value + policy) net. See doc/ai_agents.md's A14
// section for the full design.
//
// Same shared 537-float state encoder as A11 (ismctsnn_encode_state(),
// ai_strat_ismctsnn_state.h) -- unchanged, not duplicated -- but its own
// weights, its own file, its own architecture constants: this module never
// touches ai_strat_ismctsnn_net.c/A11's shipped weights, so A11's own
// measured behaviour stays bit-for-bit untouched by anything here.
//
// Architecture: the same 537->256->128->64 trunk as A11's value net, but
// with TWO heads off the shared 64-wide trunk output instead of one --
// value (64->1, sigmoid, same 0.0/0.5/1.0 scale as mc_outcome_for()) and
// policy (64->PUCT_POLICY_DIM, RAW logits, no activation here -- the
// softmax happens in puct_compose_priors() (ai_strat_puct_policy.c), which
// needs the legal move list to mask over, something this module never
// sees). One shared trunk pass costs ~8% more than A11's own single-head
// forward pass (one extra 64xPUCT_POLICY_DIM matmul for the policy head),
// not roughly double -- this is the "one forward pass yields both" design
// that makes PUCT affordable at this agent's expansion rate.
//
// Weight file format: flat, headerless float32 -- same philosophy as
// ai_strat_ismctsnn_net.h. Order: W1,b1,W2,b2,W3,b3 (trunk), Wv,bv (value
// head), Wp,bp (policy head). puctnet_load() asserts the file's byte size
// matches the compiled-in shape exactly.

#ifndef AI_STRAT_PUCT_NET_H
#define AI_STRAT_PUCT_NET_H

#include "ai_strat_ismctsnn_state.h" // ISMCTSNNStateVector, ismctsnn_encode_state()
#include "ai_strat_puct_policy.h" // PUCT_POLICY_DIM
#include "../core/game_types.h"

#define PUCT_NET_HIDDEN1 256
#define PUCT_NET_HIDDEN2 128
#define PUCT_NET_HIDDEN3 64

// Loads weights from `path` (see file format note above) into static
// storage. Returns false (leaving any previously loaded weights alone) on
// a missing file or a size mismatch against the compiled-in shape.
bool puctnet_load(const char* path);
bool puctnet_is_loaded(void);

// The pure forward pass on an already-encoded state vector -- exposed
// separately so it can be cross-checked directly against
// export_puct_weights.py's reference predictions on real corpus records,
// mirroring ismctsnn_net_forward()'s own role. If no weights are loaded,
// writes 0.5f to *value_out and all-zero logits to policy_logits_out (which
// puct_compose_priors()/puct_move_score() reduce to a uniform distribution
// regardless of the legal move list -- no separate "net not loaded"
// fallback needed at call sites).
void puctnet_forward(const ISMCTSNNStateVector* state, float* value_out,
                     float policy_logits_out[PUCT_POLICY_DIM]);

// Encodes gstate's information set from observer's own seat
// (ismctsnn_encode_state(), unchanged from A11) and runs the forward pass,
// writing both outputs. This is the ONE call site doc/ai_agents.md's A14
// section's "one forward pass yields both value and priors" design relies
// on -- ai_strat_puct_search.c's puct_leaf_value()/puct_evaluate_root_policy()
// never call puctnet_forward() directly with a second, independent state.
void puctnet_value_and_policy(const struct gamestate* gstate, PlayerID observer,
                              float* value_out, float policy_logits_out[PUCT_POLICY_DIM]);

#endif // AI_STRAT_PUCT_NET_H
