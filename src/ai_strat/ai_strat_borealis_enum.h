// ai_strat_borealis_enum.h
// A3 Borealis's candidate enumeration and scoring, split out of
// ai_strat_borealis.c/.h per the handout's Sec.10 file-length guidance --
// see ai_strat_borealis_enum.c's header comment and
// doc/ai_agents.md's A3 section
// Sec.4-6. Internal to the Borealis agent: nothing outside
// ai_strat_borealis.c should include this.

#ifndef AI_STRAT_BOREALIS_ENUM_H
#define AI_STRAT_BOREALIS_ENUM_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_borealis.h" // BorealisParams

// Per-card contribution picker -- fullDeck[].expected_attack (attack phase)
// or fullDeck[].expected_defense (defense phase). Lets
// borealis_best_champion_set() serve both phases off one enumeration
// instead of duplicating it (handout Sec.10).
typedef float (*BorealisContributionFunc)(uint8_t card_idx);

float borealis_expected_attack_of(uint8_t card_idx);
float borealis_expected_defense_of(uint8_t card_idx);

// Enumerates every legal 0-3 champion subset of affordable[0..count)
// (handout Sec.5: three nested loops, no pruning), scores each via
// `contribution` and `cap` (Sec.4: cap < 0 means uncapped, for attack;
// defense passes expected_incoming_attack() so value(S) is capped at the
// incoming threat), and returns the epsilon tie-break winner (Sec.6) in
// out_cards/out_count (0-3 cards, out_cards must hold at least 3).
// apply_holding gates Sec.7's lethal-combo-holding exclusion -- pass true
// only for attack; holding back a defensive play makes no sense.
void borealis_best_champion_set(const uint8_t* affordable, uint8_t count,
                                const struct gamestate* gstate, PlayerID player,
                                BorealisContributionFunc contribution, float cap,
                                bool apply_holding, const BorealisParams* params,
                                GameContext* ctx, uint8_t* out_cards, uint8_t* out_count);

#endif // AI_STRAT_BOREALIS_ENUM_H
