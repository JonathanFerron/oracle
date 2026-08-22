// ai_strat_valuebased.c
// A1 Value Based strategy ("The Apprentice") -- see
// ideas/A1 ai agent value based (the apprentice)/value_based_handout.md
//
// A one-ply, no-simulation, combo-blind agent. It ranks individual champion
// cards by an efficiency ratio (contribution per luna) and greedily takes the
// best ones under budget. It is deliberately simpler and weaker than later
// agents -- its roster role is a floor above Random with no understanding of
// card interaction.
//
// Deliberately never calls calculate_combo_bonus() on its own hand/selection
// (combo_bonus.h) -- that omission *is* the agent; do not "fix" it by adding
// combo awareness here. (expected_incoming_attack(), used only to size the
// defense-phase threat, does score the opponent's already-committed combo --
// that's public information about a board state that already exists, not a
// combo evaluation of this agent's own card choices.)
//
// The efficiency ratio is deliberately scale-invariant: it has no lambda
// term, so unlike a lambda-penalised additive scorer it cannot express
// "lunas are worth more than damage right now". That's a genuine weakness
// and a genuine difference in character from a stronger agent, not an
// oversight -- do not silently add one.

#include "ai_strat_valuebased.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// Denominator floor in the efficiency ratio. Prevents division by zero on
// free (cost-0) cards, and shrinks their ratio advantage toward the
// population mean -- without it, a cost-0 card with tiny contribution would
// dominate the ranking purely from the denominator collapsing to zero.
#define VB_COST_FLOOR 1.0f

// Defense entry gate (see should_defend()): defend only when the incoming
// threat is worth at least this fraction of the candidate's expected
// defense. This is the agent's primary calibration target.
#define VB_DEFEND_THRESHOLD 0.5f

// Attack-phase greedy take cap -- combat zone holds up to 3, but this agent
// commits at most 2, leaving headroom deliberately unused (no subset search).
#define VB_MAX_ATTACK_CARDS 2

// Play a draw card once hand size drops below this...
#define VB_DRAW_HAND_THRESHOLD 4
// ...but not once the opponent is this close to 0 energy -- don't spend a
// turn drawing when a lethal-adjacent attack might be available instead.
#define VB_DRAW_ENERGY_FLOOR 20

typedef struct
{ uint8_t card_index;
  float efficiency;
} RankedChampion;

// contribution(c) / (cost(c) + VB_COST_FLOOR). contribution is expected
// attack or expected defense depending on `for_attack`. Deliberately not
// fullDeck[].attack_efficiency/defense_efficiency -- those precomputed
// fields divide by cost directly (with a 0.25 floor baked in for cost-0
// cards instead of a tunable one), which is exactly the denominator
// blow-up VB_COST_FLOOR exists to control, and isn't sweepable.
static float card_efficiency(uint8_t card_idx, bool for_attack)
{ float contribution = for_attack ? fullDeck[card_idx].expected_attack
                       : fullDeck[card_idx].expected_defense;
  return contribution / (fullDeck[card_idx].cost + VB_COST_FLOOR);
} // card_efficiency

// Ordering used by the insertion sort below: true if `a` ranks ahead of `b`
// (higher efficiency; tie-break lower cost, then lower card index -- the
// agent consumes no RNG and must be fully deterministic given game state).
static bool ranks_before(const RankedChampion* a, const RankedChampion* b)
{ if(a->efficiency != b->efficiency) return a->efficiency > b->efficiency;

  uint8_t cost_a = fullDeck[a->card_index].cost;
  uint8_t cost_b = fullDeck[b->card_index].cost;
  if(cost_a != cost_b) return cost_a < cost_b;

  return a->card_index < b->card_index;
} // ranks_before

// Affordable champions -> efficiency -> descending sort. Hand is capped at
// 12 (Hand.cards[12]) and typically <=7 by the discard-to-7 rule, so O(n^2)
// insertion sort is fine and avoids a qsort comparator needing global state
// to reach fullDeck[]. `out` must have room for gstate->hand[player].size.
static uint8_t build_ranked_champions(const struct gamestate* gstate, PlayerID player,
                                      bool for_attack, RankedChampion* out)
{ uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, player,
                                             gstate->current_cash_balance[player],
                                             affordable);

  for(uint8_t i = 0; i < count; i++)
    out[i] = (RankedChampion)
  { .card_index = affordable[i],
      .efficiency = card_efficiency(affordable[i], for_attack)
  };

  for(uint8_t i = 1; i < count; i++)
  { RankedChampion key = out[i];
    int j = i - 1;
    while(j >= 0 && ranks_before(&key, &out[j]))
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }

  return count;
} // build_ranked_champions

// Fractional-knapsack greedy applied to a 0/1 problem: walk the ranked list,
// playing each card that fits the *remaining* budget, until either the
// budget is exhausted or VB_MAX_ATTACK_CARDS is played. Not optimal -- it
// will occasionally leave budget on the table where a different pair would
// have fit -- and that's acceptable and expected for this tier; no repair
// pass or second-chance loop.
//
// No pass option: if `ranked` is non-empty the agent always plays at least
// the top card. Ratio scoring has no natural zero to decline against. This
// is a known limitation -- the first thing a stronger agent should fix.
static void play_attack_selection(struct gamestate* gstate, PlayerID player,
                                  GameContext* ctx, const RankedChampion* ranked,
                                  uint8_t count)
{ uint16_t remaining_budget = gstate->current_cash_balance[player];
  uint8_t played = 0;

  for(uint8_t i = 0; i < count && played < VB_MAX_ATTACK_CARDS; i++)
  { uint8_t card_idx = ranked[i].card_index;
    if(fullDeck[card_idx].cost > remaining_budget) continue;

    play_champion(gstate, player, card_idx, ctx);
    remaining_budget -= fullDeck[card_idx].cost;
    played++;
  }
} // play_attack_selection

// Defend iff the incoming threat justifies at least VB_DEFEND_THRESHOLD of
// what `candidate` can absorb. Both sides are damage units, so this is
// scale-free and needs no knowledge of the energy scale.
static bool should_defend(const struct gamestate* gstate, PlayerID defender,
                          uint8_t candidate)
{ return expected_incoming_attack(gstate, defender) >=
         VB_DEFEND_THRESHOLD * fullDeck[candidate].expected_defense;
} // should_defend

void value_based_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;

  if(try_play_draw_card(gstate, attacker, VB_DRAW_HAND_THRESHOLD,
                        VB_DRAW_ENERGY_FLOOR, ctx))
    return;

  RankedChampion ranked[12];
  uint8_t count = build_ranked_champions(gstate, attacker, true, ranked);
  if(count == 0) return;

  play_attack_selection(gstate, attacker, ctx, ranked, count);
} // value_based_attack_strategy

// Plays exactly one champion, never more -- unlike an argmax-subset
// defender, this agent gates *entry* on the incoming threat and then, once
// past the gate, commits fully to a single candidate.
void value_based_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  RankedChampion ranked[12];
  uint8_t count = build_ranked_champions(gstate, defender, false, ranked);
  if(count == 0) return;

  uint8_t candidate = ranked[0].card_index;
  if(!should_defend(gstate, defender, candidate)) return;

  play_champion(gstate, defender, candidate, ctx);
} // value_based_defense_strategy
