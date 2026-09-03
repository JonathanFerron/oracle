// ai_strat_ismctsnn.h
// A11 IS-MCTS+NN ("AlphaOracle Prime") -- see
// ideas/A11 ai agent is-mcts + nn (alphaoracle prime)/about.md.
//
// Stage 1 (value net) + Stage 2 (C inference + integration): reuses A10's
// exact SO-ISMCTS tree/search/determinization code
// (ai_strat_ismcts_search.c's ismcts_search_best_move()) completely
// unchanged -- this agent only supplies its own ISMCTSParams
// (nn_value_trust > 0.0f, see ai_strat_ismcts1.h) so the shared
// leaf_value() blend actually consults the trained net. Rollout policy
// (A5 Heuristic on both seats) and determinization are identical to A10 --
// "this agent changes what guides the tree, not the tree/determinization
// structure itself" (about.md). No PUCT/policy head yet (Stage 4, gated on
// Stage 3's measurement clearing both ship gates).
//
// Registered and shipping as of 2026-09-03 (Stage 3 cleared both ship
// gates -- 58.44% head-to-head vs A10, ~74 estimated Borealis rating, see
// about.md's "Stage 3 result"). g_params[2] starts at plain ISMCTS_DEFAULTS
// (nn_value_trust=0.0f, identical to A10, the safe baseline) until
// ismctsnn_load_weights() succeeds, at which point it promotes g_params[]
// to this agent's own measured-best default (nn_value_trust=1.0f, see
// ismctsnn_get_default_params()) -- so a successful load is what actually
// turns real play into AlphaOracle Prime rather than plain A10, and a
// failed load leaves both g_params[] and net-load state at the safe
// baseline together. ismctsnn_set_params() (used by calibration harnesses)
// can still override per player after that. main.c calls
// ismctsnn_load_weights() once at startup with the packaged asset
// (ISMCTSNN_DEFAULT_WEIGHTS_PATH below, overridable via --ai.weights); a
// missing/corrupt file leaves weights unloaded and decide_and_apply()
// forces nn_value_trust=0.0f for any decision made while unloaded,
// regardless of the configured value -- this agent is never worse than
// plain A10, even if the asset is missing.

#ifndef AI_STRAT_ISMCTSNN_H
#define AI_STRAT_ISMCTSNN_H

#include "ai_strat_ismcts1.h" // ISMCTSParams / ISMCTS_DEFAULTS
#include "../core/game_types.h"
#include "../core/game_context.h"

// Repo-root-relative, same assumption bin/expectedresults.txt already
// relies on (stda_auto.c, cli_game.c). Overridable via --ai.weights
// (cmdline.c); loaded once by main.c before mode dispatch.
#define ISMCTSNN_DEFAULT_WEIGHTS_PATH "assets/ismctsnn/prime_657k_weights.bin"

// Loads the trained value net's weights from `path` (see
// ai_strat_ismctsnn_net.h's file format note) -- must succeed before this
// agent's nn_value_trust can have any effect. Returns false on failure
// (missing file, size mismatch); any previously loaded weights are kept.
// On success, also promotes g_params[] to ismctsnn_get_default_params()
// (see header comment above) -- real play doesn't need a separate
// ismctsnn_set_params() call just to get out of the safe trust=0 baseline.
bool ismctsnn_load_weights(const char* path);

void ismctsnn_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void ismctsnn_defense_strategy(struct gamestate* gstate, GameContext* ctx);

// This agent's shipped default (ISMCTS_DEFAULTS + nn_value_trust=1.0f) --
// already the measured-best point (about.md's Stage 3 sweep: win rate
// rises monotonically with trust, 1.0 is both the strongest and cheapest
// point tested), not just a calibration starting point.
ISMCTSParams ismctsnn_get_default_params(void);
void ismctsnn_set_params(PlayerID player, const ISMCTSParams* params);
void ismctsnn_reset_params(void);

#endif // AI_STRAT_ISMCTSNN_H
