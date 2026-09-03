// ai_strat_ismctsnn_net.h
// A11 IS-MCTS+NN ("AlphaOracle Prime") Stage 2 -- hand-written plain-C
// forward pass for the trained value net. See "Confirmed plan" step 2 in
// ideas/A11 ai agent is-mcts + nn (alphaoracle prime)/about.md: training
// stays Python/PyTorch offline; this is the C side that loads the exported
// weights and evaluates a position during real (or calibration) play.
//
// Architecture is fixed at compile time (matches
// aicalibsrc/ismctsnn/train_value_net.py's HIDDEN=(256,128,64) and
// export_weights.py's output) -- no generic/self-describing network format,
// consistent with this project's manual-code-over-macro-magic convention
// for a net this small. If the trained architecture ever changes, these
// constants and export_weights.py change together.
//
// Weight file format: a flat, headerless float32 file -- same philosophy as
// ai_strat_ismctsnn_state.h's corpus record format. export_weights.py fuses
// the trained model's BatchNorm1d into the first Linear layer's weights (an
// exact transformation in eval mode, verified numerically against the live
// PyTorch model before export), so this file holds exactly W1,b1,W2,b2,W3,
// b3,W4,b4 back to back in that order, nothing else. ismctsnn_net_load()
// asserts the file's byte size matches the compiled-in shape exactly.

#ifndef AI_STRAT_ISMCTSNN_NET_H
#define AI_STRAT_ISMCTSNN_NET_H

#include "../core/game_types.h"
#include "ai_strat_ismctsnn_state.h"

#define ISMCTSNN_NET_HIDDEN1 256
#define ISMCTSNN_NET_HIDDEN2 128
#define ISMCTSNN_NET_HIDDEN3 64

// Loads weights from `path` (see file format note above) into static
// storage. Returns false (and leaves any previously loaded weights alone)
// on a missing file or a size mismatch against the compiled-in shape --
// callers must check this before trusting ismctsnn_net_value().
bool ismctsnn_net_load(const char* path);
bool ismctsnn_net_is_loaded(void);

// The pure forward pass on an already-encoded state vector -- exposed
// separately from ismctsnn_net_value() below so it can be cross-checked
// directly against export_weights.py's reference predictions on real
// corpus records (which are already-encoded vectors on disk, not
// reconstructible gamestates). Returns 0.5f if no weights are loaded.
float ismctsnn_net_forward(const ISMCTSNNStateVector* state);

// Encodes gstate's information set from observer's own seat
// (ismctsnn_encode_state()) and runs the forward pass. Returns a value in
// [0,1] on the same 0.0/0.5/1.0 scale as mc_outcome_for() -- undefined
// (returns 0.5f) if no weights are loaded.
float ismctsnn_net_value(const struct gamestate* gstate, PlayerID observer);

#endif // AI_STRAT_ISMCTSNN_NET_H
