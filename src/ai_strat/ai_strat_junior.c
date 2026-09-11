// ai_strat_junior.c
// Junior gap-filler strategy ("Naive Greedy" / Junior) -- see
// doc/ai_agents.md's Junior section
//
// A deliberately stripped-down A1 Value Based (ai_strat_valuebased.c),
// built to sit between Random (rating 2) and A1 (rating 24). It shares A1's
// shape -- rank affordable champions, greedily take the best one(s) -- but
// drops the two things that make A1 non-trivial:
//   - ranks by raw power (expected_attack/expected_defense) instead of
//     power-per-luna efficiency, so it is cost-blind;
//   - commits at most one champion per attack, instead of A1's
//     VB_MAX_ATTACK_CARDS-capped knapsack;
//   - defends unconditionally with its best available blocker instead of
//     A1's threshold-gated should_defend() -- Junior never reasons about
//     whether blocking is worth it, it just reacts to whatever is
//     incoming (safe to do unconditionally here: defense_strategy is only
//     invoked once turn_logic.c's defense_phase() has already confirmed
//     the attacker's combat zone is non-empty).
//
// Target Borealis rating ~7 -- the log-strength midpoint between Random (2)
// and A1 (24), not the linear rating midpoint (13) -- see doc/ai_agents.md's
// Junior section for why the two differ.
//
// Deliberately never calls calculate_combo_bonus() on its own hand/selection,
// same reasoning as A1 -- combo-blindness is a floor property shared by
// every agent below A3 Borealis; do not "fix" it here.

#include "ai_strat_junior.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// Highest-power affordable champion, ignoring cost entirely -- the opposite
// of A1's card_efficiency() (contribution/cost). Ties broken by lower card
// index; this agent consumes no RNG and must be fully deterministic given
// game state, same convention as A1's ranks_before().
static uint8_t best_affordable_champion(const struct gamestate* gstate,
                                        PlayerID player, bool for_attack)
{ uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, player,
                                             gstate->current_cash_balance[player],
                                             affordable);
  if(count == 0) return UINT8_MAX;

  uint8_t best = affordable[0];
  for(uint8_t i = 1; i < count; i++)
  { float power = for_attack ? fullDeck[affordable[i]].expected_attack
                  : fullDeck[affordable[i]].expected_defense;
    float best_power = for_attack ? fullDeck[best].expected_attack
                       : fullDeck[best].expected_defense;
    if(power > best_power)
      best = affordable[i];
  }

  return best;
} // best_affordable_champion

// Same draw-card gating as A1 (VB_DRAW_HAND_THRESHOLD/VB_DRAW_ENERGY_FLOOR in
// ai_strat_valuebased.c) -- not this agent's own tunable, just the shared
// Borealis-derived default (try_play_draw_card()).
#define JR_DRAW_HAND_THRESHOLD 4
#define JR_DRAW_ENERGY_FLOOR 20

// Play the single highest-power affordable champion, cost-blind and with no
// multi-card budget optimization. No pass option, same known limitation A1
// documents: if a champion is affordable, this agent always plays one.
void junior_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;

  if(try_play_draw_card(gstate, attacker, JR_DRAW_HAND_THRESHOLD,
                        JR_DRAW_ENERGY_FLOOR, ctx))
    return;

  uint8_t card_idx = best_affordable_champion(gstate, attacker, true);
  if(card_idx == UINT8_MAX) return;

  play_champion(gstate, attacker, card_idx, ctx);
} // junior_attack_strategy

// Block unconditionally with the single highest-defense affordable
// champion -- no cost-benefit gate like A1's should_defend(). Junior never
// declines by choice, only when it truly has no affordable defender.
void junior_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  uint8_t card_idx = best_affordable_champion(gstate, defender, false);
  if(card_idx == UINT8_MAX) return;

  play_champion(gstate, defender, card_idx, ctx);
} // junior_defense_strategy
