// ai_strat_hbt_enum.h
// A7 Hybrid HBT's per-turn state derivation (Layers T and B) and move
// enumeration/scoring (Layer H) -- split out of ai_strat_hbt.c/.h once the
// combined file passed ~400 lines, same file-length reasoning as A3
// Borealis's ai_strat_borealis/ai_strat_borealis_enum split
// (ai_strat_borealis_enum.c's own header comment). Internal to the A7/A9
// agent family: nothing outside ai_strat_hbt.c/ai_strat_hbt2ply*.c should
// include this. hbt_advantage()/predicted_damage()/predicted_block()/
// is_held_combo() are exposed (non-static) specifically so A9 HBT 2-Ply can
// score its opponent-reply ply with A7's own scoring, rather than
// reimplementing or drifting from it -- see ai_strat_hbt2ply.h.

#ifndef AI_STRAT_HBT_ENUM_H
#define AI_STRAT_HBT_ENUM_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_hbt.h"

// Per-turn derived state, computed once per call into hbt_attack_strategy()/
// hbt_defense_strategy() and threaded through move scoring -- Layer T's
// aggression-modulated H weights and Layer B's aggression-scaled resource
// targets (ai_strat_hbt.h's header comment has the full formulas).
typedef struct
{ float eps_eff;
  float gamma_eff;
  float delta_eff;
  float target_cash;
  float target_cards;
} HBTState;

// player's own aggression/phase state, evaluated against 1-player (its
// opponent) -- same direction as A6's evaluate_aggression_factor(). Call
// once per turn with `player` = the acting side (attacker on attack,
// defender on defense; ai_strat_hbt.c's two entry points pass the right one).
HBTState hbt_evaluate_state(struct gamestate* gstate, PlayerID player,
                            const HBTParams* params);

// Internal accessor for the live per-player params (g_params, private to
// ai_strat_hbt.c) -- used by ai_strat_hbt_cards.c's mulligan/discard
// overrides, which need the same calibrated values attack/defense use
// (hbt_set_params()'s per-player override included). Not part of the public
// ai_strat_hbt.h API.
const HBTParams* hbt_live_params(PlayerID player);

typedef enum
{ HBT_MOVE_PASS = 0,
  HBT_MOVE_CHAMPIONS,
  HBT_MOVE_DRAW,
  HBT_MOVE_CASH
} HBTMoveType;

typedef struct
{ float advantage;
  HBTMoveType type;
  uint8_t cards[3];
  uint8_t count;
} HBTBestMove;

// A5-shaped exhaustive enumeration (every affordable 1-3 champion subset,
// every affordable draw card, every affordable cash card, plus pass),
// scored by the state-modulated advantage function with A3's combo hold
// excluding held-back subsets. Deterministic argmax, first-enumerated wins
// ties (no RNG).
HBTBestMove hbt_best_attack_move(struct gamestate* gstate, PlayerID player,
                                 const HBTParams* params, const HBTState* state);

// Same exhaustive-enumeration shape restricted to champion subsets (0-3,
// decline is size 0), scored against a variance-aware incoming-attack
// estimate (ai_strat_hbt.h's defense_stdev_mult).
//
// NOT used directly by A9 HBT 2-Ply as its opponent-reply oracle, despite
// the original design intent (ai_strat_hbt2ply.h's header comment has the
// full story): this function's own PASS/decline baseline scores the
// undamaged own_energy, never charging the incoming attack against
// declining. Worked through hbt_advantage()'s algebra, this makes PASS
// mathematically dominate every blocking option under the shipped
// HBTParams (a hand card's positive weight always outweighs the energy
// saved, even for a perfect block) -- confirmed empirically too (HBT-vs-HBT
// and Heuristic-vs-Heuristic both average 6 turns to burn 99 energy, vs
// Random-vs-Random's 26). This is a pre-existing property of this shipped,
// calibrated, measured function (and A5's identical best_defense_move(),
// which this one's shape was copied from) -- not something A9 introduced,
// and not fixed here: that would move A7's own measured rating and is
// deferred to its own task. A9 instead uses its own local
// hbt2ply_reply_defense_move() (ai_strat_hbt2ply_reply.c), built from the
// two shared pieces below with a corrected PASS baseline.
HBTBestMove hbt_best_defense_move(const struct gamestate* gstate, PlayerID defender,
                                  const HBTParams* params, const HBTState* state);

// Variance-aware incoming-attack estimate (ai_strat_hbt.h's
// defense_stdev_mult) and per-subset defense scoring -- the two pieces of
// hbt_best_defense_move() that are NOT the buggy PASS baseline (see above),
// exposed so A9's hbt2ply_reply_defense_move() can reuse them verbatim and
// differ from A7's own function only in that one baseline.
float variance_aware_incoming(const struct gamestate* gstate, PlayerID defender,
                              PlayerID attacker, const HBTParams* params);
void evaluate_defense_subset(const uint8_t* cards, uint8_t count, float own_energy,
                             float opp_energy, float own_hand, float opp_hand,
                             float own_cash, float opp_cash, float incoming,
                             const HBTParams* params, const HBTState* state,
                             HBTBestMove* best);

// -- Shared scoring primitives, exposed for A9 HBT 2-Ply's opponent-reply
// ply (ai_strat_hbt_enum.c has the full comment on each) --

// The Layer H advantage function: A5's weighted-sum shape re-parameterised
// on an HBTState's effective weights/targets. NOT sign-symmetric -- the
// resource-shortfall penalty is one-sided (own shortfall only) and `state`
// must be derived for whichever player's advantage is being scored via
// hbt_evaluate_state(gstate, that_player, params). Negating one player's
// score to get the other's silently drops the penalty and the aggression
// modulation -- always call hbt_evaluate_state() for the side being scored.
float hbt_advantage(float own_energy, float opp_energy, float own_hand,
                    float opp_hand, float own_cash, float opp_cash,
                    const HBTParams* params, const HBTState* state);

// Sigma(expected_attack) + combo bonus, clamped to opp_energy. Models no
// opponent block -- the exact gap A9's second ply exists to correct.
float predicted_damage(const uint8_t* cards, uint8_t count, float opp_energy);

// Sigma(expected_defense) + combo bonus -- used to score a (real or
// simulated) defense subset's block amount.
float predicted_block(const uint8_t* cards, uint8_t count);

// A3's lethal-combo-hold rule, ported verbatim: true if `cards` should be
// excluded from attack consideration because it's a big combo worth saving
// for a finishing blow rather than playing now.
bool is_held_combo(const uint8_t* cards, uint8_t count, float raw_damage,
                   PlayerID opponent, const struct gamestate* gstate,
                   const HBTParams* params);

#endif // AI_STRAT_HBT_ENUM_H
