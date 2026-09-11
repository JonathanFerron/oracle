// ai_strat_a15_endgame.c
// See ai_strat_a15_endgame.h.

#include "ai_strat_a15_endgame.h"
#include "ai_strat_a15_prob.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// q_N for horizon N (1-4) -- ai_strat_a15.h's declared field order maps q1 to
// the widest horizon (4) down to q4 at the tightest (1), per the design doc.
static float threshold_for_horizon(const A15Params* params, uint8_t horizon)
{ switch(horizon)
  { case 4:
      return params->endgame_q1;
    case 3:
      return params->endgame_q2;
    case 2:
      return params->endgame_q3;
    default: // 1
      return params->endgame_q4;
  }
} // threshold_for_horizon

// The smallest N in [1,4] for which P(finish within N) clears q_N, or 0 if
// none do. Checked directly at every horizon rather than derived from hand
// size (see ai_strat_a15_endgame.h's header comment for why the original
// hand-size proxy was replaced 2026-09-10) -- the smallest satisfying N is
// the tightest, most immediate real opportunity, not just the first one
// tried.
static uint8_t find_triggering_horizon(const struct gamestate* gstate, PlayerID player,
                                       const A15Params* params)
{ for(uint8_t horizon = 1; horizon <= 4; horizon++)
  { float p = a15_p_finish_within(gstate, player, horizon);
    if(p >= threshold_for_horizon(params, horizon)) return horizon;
  }
  return 0;
} // find_triggering_horizon

// Best affordable combo (any tier -- species/order/color all compete
// equally here, unlike R9a's order/color-only restriction, since R8 has no
// sibling rule already claiming species). Same triple-nested C(n,<=3)
// enumeration idiom as every other candidate scan in this agent.
static bool find_best_affordable_combo(const struct gamestate* gstate, PlayerID player,
                                       uint8_t* out, uint8_t* out_count)
{ uint8_t affordable[12];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t n = build_affordable_champions(gstate, player, budget, affordable);

  int best_bonus = 0;
  uint8_t best[3] = {0};
  uint8_t best_count = 0;

  for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = i + 1; j < n; j++)
    { uint16_t cost2 = fullDeck[affordable[i]].cost + fullDeck[affordable[j]].cost;
      if(cost2 <= budget)
      { uint8_t pair[2] = { affordable[i], affordable[j] };
        int b = combo_bonus_for_selection(pair, 2);
        if(b > best_bonus)
        { best_bonus = b;
          best[0] = pair[0];
          best[1] = pair[1];
          best_count = 2;
        }
      }
      for(uint8_t k = j + 1; k < n; k++)
      { uint16_t cost3 = cost2 + fullDeck[affordable[k]].cost;
        if(cost3 > budget) continue;
        uint8_t triple[3] = { affordable[i], affordable[j], affordable[k] };
        int b = combo_bonus_for_selection(triple, 3);
        if(b > best_bonus)
        { best_bonus = b;
          best[0] = triple[0];
          best[1] = triple[1];
          best[2] = triple[2];
          best_count = 3;
        }
      }
    }

  if(best_count == 0) return false;
  for(uint8_t i = 0; i < best_count; i++) out[i] = best[i];
  *out_count = best_count;
  return true;
} // find_best_affordable_combo

// No combo available -- "going all out" still means playing as much value
// as affordable, up to 3 champions, ranked by raw expected_attack.
static uint8_t pick_best_value_attackers(const struct gamestate* gstate, PlayerID player,
                                         uint8_t* out)
{ uint8_t affordable[12];
  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t n = build_affordable_champions(gstate, player, budget, affordable);

  for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = i + 1; j < n; j++)
      if(fullDeck[affordable[j]].expected_attack > fullDeck[affordable[i]].expected_attack)
      { uint8_t tmp = affordable[i];
        affordable[i] = affordable[j];
        affordable[j] = tmp;
      }

  uint8_t count = 0;
  uint16_t spent = 0;
  for(uint8_t i = 0; i < n && count < 3; i++)
  { if(fullDeck[affordable[i]].cost + spent > budget) continue;
    out[count++] = affordable[i];
    spent += fullDeck[affordable[i]].cost;
  }
  return count;
} // pick_best_value_attackers

bool a15_try_endgame_attack(struct gamestate* gstate, PlayerID player,
                            const A15Params* params, GameContext* ctx)
{ if(find_triggering_horizon(gstate, player, params) == 0) return false;

  uint8_t cards[3];
  uint8_t count;
  if(!find_best_affordable_combo(gstate, player, cards, &count))
    count = pick_best_value_attackers(gstate, player, cards);
  if(count == 0) return false; // nothing playable -- let the normal chain handle it

  for(uint8_t i = 0; i < count; i++)
    play_champion(gstate, player, cards[i], ctx);
  return true;
} // a15_try_endgame_attack
