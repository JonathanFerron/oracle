// ai_strat_puct_policy.h
// A14 AlphaOracle Prime Plus I -- action encoding for the policy head. See
// doc/ai_agents.md's A14 section for the full rationale.
//
// The head emits PUCT_POLICY_DIM logits, indexed by the SAME 105-type card
// catalog ismctsnn_catalog_index() already uses (ai_strat_ismctsnn_state.h)
// -- never a hand-slot position. ismctsnn_encode_state()'s state vector
// carries no slot ordering anywhere in its input, so a slot-indexed head
// would be predicting over positions the net cannot see; a card-type-
// indexed head instead composes naturally with the legal move list
// get_available_moves() (move_gen.h) already produces, and the softmax
// over that legal list at composition time IS the mask -- no separate
// masking step.
//
// Layout:
//   [0, ISMCTSNN_CATALOG_SIZE)      L_play   -- this card type is played
//                                              from hand (champions
//                                              committed, or the draw/
//                                              recall/cash card itself)
//   [.., 2*ISMCTSNN_CATALOG_SIZE)   L_target -- this card type is NAMED AS
//                                              A TARGET (recall's discard
//                                              pick, cash's exchange
//                                              victim) -- kept separate
//                                              from L_play because
//                                              "commit this champion" and
//                                              "give this champion up" are
//                                              semantically opposite
//   [.., +5)                       L_type   -- one slot per MoveType
//   [.., +3)                       L_count  -- subset size 1/2/3
//                                              (MOVE_CHAMPIONS/MOVE_RECALL
//                                              only)
//
// Composition, for one GameMove m from get_available_moves():
//   score(m) = L_type[m.type]
//            + L_count[m.count-1]                      (CHAMPIONS/RECALL)
//            + mean(L_play[t]   for t in played(m))
//            + mean(L_target[t] for t in targets(m))   (0 if none)
//   P = softmax(score / temperature) over the legal move list
//
// mean, not sum, so subset size is expressed by L_count rather than
// smuggled into the card term. Used by both live PUCT selection
// (ai_strat_puct_search.c) and corpus generation
// (aicalibsrc/puct/gen_policy_corpus.c) -- the same function serves both,
// so training and inference cannot drift, the same discipline
// ismctsnn_encode_state() already follows.

#ifndef AI_STRAT_PUCT_POLICY_H
#define AI_STRAT_PUCT_POLICY_H

#include <stdint.h>

#include "ai_strat_ismctsnn_state.h" // ISMCTSNN_CATALOG_SIZE
#include "../actions/game_move.h"

#define PUCT_POLICY_PLAY_OFFSET   0
#define PUCT_POLICY_TARGET_OFFSET (ISMCTSNN_CATALOG_SIZE)
#define PUCT_POLICY_TYPE_OFFSET   (2 * ISMCTSNN_CATALOG_SIZE)
#define PUCT_POLICY_TYPE_COUNT    5 // MOVE_PASS/CHAMPIONS/DRAW/RECALL/CASH
#define PUCT_POLICY_COUNT_OFFSET  (PUCT_POLICY_TYPE_OFFSET + PUCT_POLICY_TYPE_COUNT)
#define PUCT_POLICY_COUNT_SLOTS   3 // subset size 1/2/3
#define PUCT_POLICY_DIM           (PUCT_POLICY_COUNT_OFFSET + PUCT_POLICY_COUNT_SLOTS)

#define PUCT_MOVE_MAX_PLAY   3 // matches GameMove.cards[3]'s own sizing
#define PUCT_MOVE_MAX_TARGET 3 // matches GameMove.recall[3]/.cards[3]'s own
// margin (choose_num is only ever 1 or 2 today, per game_move.h's own
// comment -- sized for the same future headroom, not today's tighter bound)

// A GameMove decomposed into catalog-indexed play/target sets -- the same
// decomposition puct_move_score() scores internally, exposed separately so
// corpus generation (aicalibsrc/puct/gen_policy_corpus.c) can log it
// without re-deriving move_gen.c's own per-MoveType field knowledge. Unused
// play/target slots are -1, never a valid catalog index (0..104).
typedef struct
{ MoveType type;
  uint8_t count; // subset size (CHAMPIONS/RECALL only); 0 otherwise
  int8_t play[PUCT_MOVE_MAX_PLAY];
  int8_t target[PUCT_MOVE_MAX_TARGET];
} PUCTMoveFeatures;

// Decomposes `move` per this file's header comment's played(m)/targets(m)
// rules -- the fullDeck[]-index card fields translated to catalog indices
// via ismctsnn_catalog_index(), never fullDeck[] indices themselves (so
// neither the corpus format nor the training pipeline ever needs the deck
// table).
void puct_move_features(const GameMove* move, PUCTMoveFeatures* out);

// Composes one move's raw (pre-softmax) score from a PUCT_POLICY_DIM-length
// logit vector, via puct_move_features() above. Pure function of its
// arguments -- no gamestate access, so it can run identically over a live
// GameMove during search or a logged one during corpus generation/training.
float puct_move_score(const float* logits, const GameMove* move);

// Softmaxes puct_move_score() over moves[0..n) at `temperature` (<= 0.0f
// treated as 1.0f), writing n probabilities summing to 1.0 into out_probs.
// n == 0 is a caller bug (get_available_moves() never returns 0 -- MOVE_PASS
// is always legal); defensively a no-op.
void puct_compose_priors(const float* logits, const GameMove* moves, uint8_t n,
                         float temperature, float* out_probs);

// Uniform distribution over n moves -- the prior_trust==0.0 ablation and
// the arena-full/missing-row fallback both want this, not a zeroed logit
// vector run back through puct_compose_priors() (which would still shape
// the distribution by L_type/L_count/subset size).
void puct_uniform_priors(uint8_t n, float* out_probs);

#endif // AI_STRAT_PUCT_POLICY_H
