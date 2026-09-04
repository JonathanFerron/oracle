// ai_strat_ismctsnn_state.h
// A11 IS-MCTS+NN ("AlphaOracle Prime") -- fixed-size info-set state encoding
// for the value net. See doc/ai_agents.md's A11 section)/
// about.md's "Confirmed plan" for the staged scope this feeds (Stage 1:
// value net replaces A10's heuristic rollout, no policy head/action encoding
// yet -- this file is that Stage 1 state encoder, nothing more).
//
// Card catalog: fullDeck[]'s 120 physical cards collapse to 105 distinct
// TYPES -- 102 unique champions (champion_id 1..102, no duplicates, verified
// against game_constants.c) plus 3 duplicated non-champion types (9x Draw-2,
// 6x Draw-3, 3x Cash, all internally identical per game_constants.c). A count
// vector is indexed by TYPE, not physical card, since duplicate physical
// copies of the same type are interchangeable for this purpose.
//
// Zones encoded: own hand, own/opp discard, own/opp combat zone -- all
// public except own hand, which only the deciding player (`observer`) sees.
// Deliberately does NOT encode either player's deck contents (own or
// opponent's) -- ai_strat_playout.c's mc_determinize() anti-clairvoyance rule
// applies here too: a player doesn't know their own deck's contents either,
// pre-reshuffle.
//
// Reshuffle-narrowing simplification (deliberate, not an oversight): the
// original design note (doc/ai_agents.md's A11 section's "reshuffle narrows the
// determinization" section) called for a separate known-composition-since-
// reshuffle feature. This encoder does NOT track that as its own
// snapshot/counter. Instead it relies on own_discard/opp_discard +
// own_deck_remaining/opp_deck_remaining already being present: a player's
// discard pile IS their known pre-reshuffle composition, and deck_remaining
// tells the net how close a reshuffle boundary is -- ai_strat_a13_belief.c's
// blend_reshuffle_value() uses exactly this same pair (discard contents +
// deck_size nearness) rather than a post-reshuffle snapshot, and A10's own
// Phase 0 finding (0/8000 real non-random games ever reshuffle) means the one
// gap this leaves -- right after an actual reshuffle, the discard is briefly
// empty and the net has no way to recover what composition just went back
// into the deck -- essentially never fires in real play. Revisit only if
// that finding stops holding for A11's self-play corpus specifically.
//
// All fields are `float` and the struct has no other member types, so it is
// safe to treat as a flat `float[ISMCTSNN_STATE_DIM]` array (corpus writing,
// tensor loading) -- enforced by a _Static_assert in the .c file. Values are
// intentionally already normalized/relativized here (never "champion_id" or
// "PLAYER_A/PLAYER_B", always "own"/"opp" from `observer`'s own seat) so the
// exact same function serves both corpus generation and live inference --
// no separate normalization step to keep in sync between Python training and
// the future hand-written-C inference path.

#ifndef AI_STRAT_ISMCTSNN_STATE_H
#define AI_STRAT_ISMCTSNN_STATE_H

#include "../core/game_types.h"

#define ISMCTSNN_CATALOG_SIZE 105 // 102 champions + Draw-2 + Draw-3 + Cash

// Normalization constants -- first-pass choices, not measured; the VECTOR
// SHAPE below is what's locked in, these are free to retune from training
// diagnostics without changing any corpus/inference code beyond this file.
#define ISMCTSNN_ENERGY_NORM 99.0f  // INITIAL_ENERGY_DEFAULT
#define ISMCTSNN_CASH_NORM   30.0f  // INITIAL_CASH_DEFAULT
#define ISMCTSNN_TURN_NORM   50.0f  // real games run ~7-42 turns (A10 Phase 0
// finding); MAX_NUMBER_OF_TURNS=500 would waste most of the net's input
// dynamic range on values that never occur in real play.

typedef struct
{ // -- Card-type count vectors (ISMCTSNN_CATALOG_SIZE each, raw counts) --
  float own_hand[ISMCTSNN_CATALOG_SIZE];
  float own_discard[ISMCTSNN_CATALOG_SIZE];
  float own_combat_zone[ISMCTSNN_CATALOG_SIZE];
  float opp_discard[ISMCTSNN_CATALOG_SIZE];
  float opp_combat_zone[ISMCTSNN_CATALOG_SIZE];

  // -- Scalars --
  float energy_me;             // current_energy[observer] / ISMCTSNN_ENERGY_NORM
  float energy_opp;
  float cash_me;                // current_cash_balance[observer] / ISMCTSNN_CASH_NORM
  float cash_opp;
  float turn;                   // gstate->turn / ISMCTSNN_TURN_NORM
  float phase;                   // 0.0 = ATTACK, 1.0 = DEFENSE
  float own_deck_remaining;      // deck[observer].top + 1, raw count
  float opp_deck_remaining;
  float opp_hand_size;           // raw count -- opponent hand SIZE is public, contents aren't
  float combo_bonus_random;      // combo_bonus_table one-hot (3 values)
  float combo_bonus_monochrome;
  float combo_bonus_custom;
} ISMCTSNNStateVector;

#define ISMCTSNN_STATE_DIM (5 * ISMCTSNN_CATALOG_SIZE + 12)

// Maps a card to its catalog type index (0..ISMCTSNN_CATALOG_SIZE-1):
// champions get their own slot (champion_id - 1, dense 0..101 since
// champion_id has no gaps/duplicates); Draw-2/Draw-3/Cash each collapse
// their duplicated physical copies onto one shared slot (102/103/104).
uint8_t ismctsnn_catalog_index(const struct card* card);

// Encodes `gstate`'s current information set from `observer`'s own seat into
// `out` -- see this file's header comment for the full rationale. `gstate`
// is read-only; `out` is fully overwritten (no need to zero it first).
void ismctsnn_encode_state(const struct gamestate* gstate, PlayerID observer,
                           ISMCTSNNStateVector* out);

#endif // AI_STRAT_ISMCTSNN_STATE_H
