// ai_strat_balanced_rules.c
// A4 Balanced Rules strategy ("Bean Counter") -- see ai_strat_balanced_rules.h
// for the full spec (resource formulas, priority-ordered attack/defense
// rules, and the corrections applied to the original design docs' cash-ladder
// slope and target intercepts). Calibrated 2026-08-24 -- see
// aicalibsrc/balanced/ and BALANCED_DEFAULTS's own comment below.

#include <math.h>

#include "ai_strat_balanced_rules.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// Denominator floor in both attack- and defense-efficiency ratios --
// prevents free (cost-0) cards from dominating purely from the denominator
// collapsing to zero, same role as A1's VB_COST_FLOOR (ai_strat_valuebased.c)
// and the same starting value; not (yet) a BalancedRulesParams field since
// nothing has calibrated whether this agent needs its own value.
#define BR_COST_FLOOR 1.3f

// Calibrated 2026-08-24 via aicalibsrc/balanced/calibrate_balanced.py's
// `optimize --identity-safe`. The spec-derived starting point --
// target_cash_slope = INITIAL_CASH_DEFAULT/91 (~0.3297), re-anchoring the
// original design docs' 19-luna-at-full-opponent-energy ladder to today's
// 30-luna starting stack (see ai_strat_balanced_rules.h) -- turned out to be
// a genuine bug in the *shape*, not just an untuned guess: at full opponent
// energy the cash surplus (effective_cash - target_cash) is ~0 by
// construction, and since target_cash tracks opponent energy (which barely
// falls while this agent is starved of the cash to attack with), the agent
// gets stuck unable to spend for many turns -- confirmed by hand (a single
// traced game showed 4 of 5 early turns passing outright) and by a
// target_cash_slope sweep vs `borealis` (0.0 -> 25.1%, 0.05 -> 26.1%, 0.33
// (the spec-derived value) -> 5.3%, 0.6 -> 0.0%, all at 1600 games/value).
//
// Two unconstrained `optimize` runs (all params free, vs `combo`) confirmed
// the failure mode by symmetry: both independently drove
// target_cash_slope/target_cards_slope toward 0 (spend-everything, ignore
// opponent energy) and defense_beta past 2.0 (rarely defend), each measuring
// stronger (up to 70.7% vs combo) but eroding "obsessive resource
// accounting" into a different, dumber agent -- flagged by
// check_personality_flags(), the same protocol as A2's rejected
// aggression_level=2.21 (doc/changelog.md 2026-08-22). Rather than
// hand-pick a compromise, `optimize --identity-safe` re-ran the search
// inside BOUNDS_IDENTITY_SAFE (calibrate_balanced.py), which keeps both
// slopes non-degenerate and defense_beta in [0.25, 2.0] by construction --
// i.e. the best this agent can do while still being this agent. Two such
// runs (targeting `combo`, then `borealis`) converged to statistically
// indistinguishable ~34-35% win rates vs `borealis` despite different
// starting points, and a 3-way self-play round-robin (defaults vs both
// candidates, 24000 games/pairing) picked the borealis-targeted one, 72.1%
// vs 71.4% BT-fit win rate against the other two -- shipped below.
//
// Measured (validated, both seats): vs `borealis` 5.8% -> 34.3% [33.8%,
// 34.7%] (40,000 games, `optimize --identity-safe --opponent borealis`); vs
// `combo` -> ~59.7% (9,000 games, manual both-seat check); vs `value` ->
// ~58.1% (6,000 games); vs `rand` -> ~98.5% (6,000 games, ceiling-effected
// like A1/A2/A3). The vs-`borealis` result lands the agent's Bradley-Terry
// rating below the anchor (below 50) rather than the design-intent estimate
// of 62 -- see ideas/A4 .../about.md and doc/changelog.md: a closed-form,
// no-search resource-conservation agent is not guaranteed to beat a
// lambda-tuned exhaustive 0-3-champion enumerator, and it doesn't here. That
// is a legitimate, informative outcome, not a defect (Finding 9,
// implementation plan 2026-08-24) -- run --stda.rating for the roster-wide
// fit before trusting this pairwise estimate as the shipped rating.
#define BALANCED_DEFAULTS \
  { .target_cash_slope = 0.08096868f, \
    .target_cash_intercept = -2.72849536f, \
    .target_cards_slope = 0.03572451f, \
    .target_cards_intercept = -0.99130504f, \
    .defense_beta = 1.93450222f, \
    .late_game_aggro = 2.09102475f, \
    .combo_weight = 0.0f, \
    .lethal_horizon = 9, \
    .draw2_hand_threshold = 6, \
    .draw3_hand_threshold = 6 }

static BalancedRulesParams g_params[2] = { BALANCED_DEFAULTS, BALANCED_DEFAULTS };

BalancedRulesParams balanced_rules_get_default_params(void)
{ BalancedRulesParams defaults = BALANCED_DEFAULTS;
  return defaults;
} // balanced_rules_get_default_params

void balanced_rules_set_params(PlayerID player, const BalancedRulesParams* params)
{ g_params[player] = *params;
} // balanced_rules_set_params

void balanced_rules_reset_params(void)
{ BalancedRulesParams defaults = BALANCED_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // balanced_rules_reset_params

/* ========================================================================
   Resource accounting: effective hand size, effective cash (promoted to
   ai_strat_common.{c,h} once A6 Tactical needed the identical formula --
   see doc/changelog.md), and the opponent-energy-driven targets
   (ai_strat_balanced_rules.h's header comment).
   ======================================================================== */

// target = slope*(opp_energy - 8) + intercept, clamped at >= 0, divided by
// late_game_aggro once opp_energy drops to/below lethal_horizon (spends
// down harder once a kill is close). Both intercepts ship at 0.0 -- see
// ai_strat_balanced_rules.h's header comment for why the design docs'
// stated +8/+3 don't match the original spec's own numeric tables.
static void resource_targets(uint8_t opp_energy, const BalancedRulesParams* params,
                             float* out_target_cash, float* out_target_cards)
{ float e = (float)((int)opp_energy - 8);

  float target_cash = params->target_cash_slope * e + params->target_cash_intercept;
  float target_cards = params->target_cards_slope * e + params->target_cards_intercept;

  if(target_cash < 0.0f) target_cash = 0.0f;
  if(target_cards < 0.0f) target_cards = 0.0f;

  if((int)opp_energy <= params->lethal_horizon && params->late_game_aggro > 0.0f)
  { target_cash /= params->late_game_aggro;
    target_cards /= params->late_game_aggro;
  }

  *out_target_cash = target_cash;
  *out_target_cards = target_cards;
} // resource_targets

/* ========================================================================
   Draw step (attack phase only) -- the stub's two-threshold rule. Skipped
   entirely once the opponent is at/below lethal_horizon: spending the
   turn's one action (doc/game_rules_doc.md; a draw card ends the action
   phase) on drawing when a finishing attack might be available instead is
   exactly the waste this agent's resource discipline exists to avoid.
   ======================================================================== */

// First affordable held DRAW_CARD of the given draw_num tier (2 -> cost-1
// "Draw 2", 3 -> cost-2 "Draw 3"), or UINT8_MAX if none.
static uint8_t find_affordable_draw_card(const struct gamestate* gstate, PlayerID player,
                                         uint8_t draw_num)
{ const Hand* hand = &gstate->hand[player];
  uint16_t budget = gstate->current_cash_balance[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type == DRAW_CARD &&
       fullDeck[card_idx].draw_num == draw_num &&
       fullDeck[card_idx].cost <= budget)
      return card_idx;
  }
  return UINT8_MAX;
} // find_affordable_draw_card

// The stub lists "hand < 7 -> Draw-2" ahead of "hand < 6 -> Draw-3", but
// since only one card can be played per turn, a hand small enough to
// qualify for both is better served by the bigger draw -- so Draw-3 is
// checked first here when both thresholds are satisfied. Returns whether a
// draw card was played (and therefore the turn's one action was consumed).
static bool try_play_draw_step(struct gamestate* gstate, PlayerID player,
                               uint8_t opp_energy, const BalancedRulesParams* params,
                               GameContext* ctx)
{ if((int)opp_energy <= params->lethal_horizon) return false;

  uint8_t hand_size = gstate->hand[player].size;

  if(hand_size < (uint8_t)params->draw3_hand_threshold)
  { uint8_t card_idx = find_affordable_draw_card(gstate, player, 3);
    if(card_idx != UINT8_MAX)
    { play_draw_card(gstate, player, card_idx, ctx);
      return true;
    }
  }

  if(hand_size < (uint8_t)params->draw2_hand_threshold)
  { uint8_t card_idx = find_affordable_draw_card(gstate, player, 2);
    if(card_idx != UINT8_MAX)
    { play_draw_card(gstate, player, card_idx, ctx);
      return true;
    }
  }

  return false;
} // try_play_draw_step

/* ========================================================================
   Attack: greedy selection by attack efficiency, up to the count the
   resource formula calls for, staying within the cash surplus above target.
   ======================================================================== */

// contribution/(cost+floor), nudged by combo_weight*(marginal combo bonus
// this card would add to the champions already picked this turn).
// combo_weight ships at 0.0 (see the header's field comment), so this is a
// plain efficiency ratio unless calibration ships otherwise.
static float attack_selection_score(uint8_t card_idx, const uint8_t* already_selected,
                                    uint8_t selected_count, const BalancedRulesParams* params)
{ float base = fullDeck[card_idx].expected_attack / (fullDeck[card_idx].cost + BR_COST_FLOOR);
  if(params->combo_weight == 0.0f) return base;

  uint8_t trial[3];
  for(uint8_t i = 0; i < selected_count; i++) trial[i] = already_selected[i];
  trial[selected_count] = card_idx;

  int with_bonus = combo_bonus_for_selection(trial, (uint8_t)(selected_count + 1));
  int without_bonus = (selected_count >= 2) ?
                      combo_bonus_for_selection(already_selected, selected_count) : 0;

  return base + params->combo_weight * (float)(with_bonus - without_bonus);
} // attack_selection_score

// Greedily plays up to max_count champions affordable within cash_cap,
// re-ranking the remaining candidates after each pick (so combo_weight's
// marginal term, when non-zero, sees the actually-chosen set). Writes chosen
// card indices to out (>= 3 slots) and returns how many were picked.
static uint8_t select_attack_champions(const struct gamestate* gstate, PlayerID player,
                                       uint8_t max_count, uint16_t cash_cap,
                                       const BalancedRulesParams* params, uint8_t* out)
{ uint8_t candidates[12];
  uint8_t candidate_count = build_affordable_champions(gstate, player, cash_cap, candidates);

  uint8_t chosen_count = 0;
  int32_t budget_left = cash_cap;

  while(chosen_count < max_count)
  { int8_t best_slot = -1;
    float best_score = -1.0f;

    for(uint8_t i = 0; i < candidate_count; i++)
    { uint8_t card_idx = candidates[i];
      if(card_idx == UINT8_MAX) continue; // already chosen
      if(fullDeck[card_idx].cost > budget_left) continue;

      float score = attack_selection_score(card_idx, out, chosen_count, params);
      if(best_slot < 0 || score > best_score)
      { best_score = score;
        best_slot = (int8_t)i;
      }
    }

    if(best_slot < 0) break;

    uint8_t card_idx = candidates[best_slot];
    out[chosen_count++] = card_idx;
    budget_left -= fullDeck[card_idx].cost;
    candidates[best_slot] = UINT8_MAX; // consumed
  }

  return chosen_count;
} // select_attack_champions

void balanced_rules_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  const BalancedRulesParams* params = &g_params[player];
  uint8_t opp_energy = gstate->current_energy[1 - player];

  if(try_play_draw_step(gstate, player, opp_energy, params, ctx))
    return; // playing a draw card consumes the turn's one action

  float effective_hand, effective_cash;
  effective_hand_and_cash(gstate, player, &effective_hand, &effective_cash);

  float target_cash, target_cards;
  resource_targets(opp_energy, params, &target_cash, &target_cards);

  long cards_to_play = lroundf(effective_hand - target_cards);
  if(cards_to_play < 0) cards_to_play = 0;
  if(cards_to_play > 3) cards_to_play = 3;

  float cash_surplus = effective_cash - target_cash;
  uint16_t cash_cap = (cash_surplus > 0.0f) ? (uint16_t)cash_surplus : 0;

  uint8_t chosen[3];
  uint8_t chosen_count = select_attack_champions(gstate, player, (uint8_t)cards_to_play,
                                                 cash_cap, params, chosen);

  if(chosen_count == 0)
  { uint8_t affordable[12];
    uint8_t count = build_affordable_champions(gstate, player, cash_cap, affordable);
    try_play_cash_fallback(gstate, player, count, ctx);
    return;
  }

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, player, chosen[i], ctx);
} // balanced_rules_attack_strategy

/* ========================================================================
   Defense: rank by defense efficiency (tie-break: worst attack efficiency
   first -- spend the cards that are worst on offence), then walk the
   ranking adding defenders while the running total (actual combo bonus
   included) stays at or below E[Attack] - beta*sigma. A candidate that
   would breach the cap is skipped, not a stopping point -- one big
   champion early in the ranking must not block smaller ones behind it.
   ======================================================================== */

typedef struct
{ uint8_t card_index;
  float defense_score;
  float attack_score; // tie-break only, ascending
} DefenseCandidate;

static bool defense_ranks_before(const DefenseCandidate* a, const DefenseCandidate* b)
{ if(a->defense_score != b->defense_score) return a->defense_score > b->defense_score;
  if(a->attack_score != b->attack_score) return a->attack_score < b->attack_score;
  return a->card_index < b->card_index;
} // defense_ranks_before

static uint8_t build_ranked_defenders(const struct gamestate* gstate, PlayerID defender,
                                      uint16_t cash_cap, DefenseCandidate* out)
{ uint8_t candidates[12];
  uint8_t count = build_affordable_champions(gstate, defender, cash_cap, candidates);

  for(uint8_t i = 0; i < count; i++)
  { uint8_t card_idx = candidates[i];
    out[i] = (DefenseCandidate)
    { .card_index = card_idx,
        .defense_score = fullDeck[card_idx].expected_defense /
                         (fullDeck[card_idx].cost + BR_COST_FLOOR),
                         .attack_score = fullDeck[card_idx].expected_attack /
                                         (fullDeck[card_idx].cost + BR_COST_FLOOR)
    };
  }

  for(uint8_t i = 1; i < count; i++)
  { DefenseCandidate key = out[i];
    int j = i - 1;
    while(j >= 0 && defense_ranks_before(&key, &out[j]))
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }

  return count;
} // build_ranked_defenders

static uint8_t select_defenders(const struct gamestate* gstate, PlayerID defender,
                                uint8_t max_count, uint16_t cash_cap, float cap,
                                uint8_t* out)
{ DefenseCandidate ranked[12];
  uint8_t count = build_ranked_defenders(gstate, defender, cash_cap, ranked);

  uint8_t chosen_count = 0;
  float total_defense = 0.0f;
  int32_t budget_left = cash_cap;

  for(uint8_t i = 0; i < count && chosen_count < max_count; i++)
  { uint8_t card_idx = ranked[i].card_index;
    if(fullDeck[card_idx].cost > budget_left) continue;

    uint8_t trial[3];
    for(uint8_t j = 0; j < chosen_count; j++) trial[j] = out[j];
    trial[chosen_count] = card_idx;

    float trial_total = total_defense + fullDeck[card_idx].expected_defense +
                        (float)combo_bonus_for_selection(trial, (uint8_t)(chosen_count + 1));
    if(trial_total > cap) continue; // would overdefend -- skip, keep walking

    out[chosen_count++] = card_idx;
    total_defense += fullDeck[card_idx].expected_defense;
    budget_left -= fullDeck[card_idx].cost;
  }

  return chosen_count;
} // select_defenders

void balanced_rules_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  PlayerID attacker = gstate->current_player;
  const BalancedRulesParams* params = &g_params[defender];

  float expected_attack = expected_incoming_attack(gstate, defender);

  float attack_variance = 0.0f;
  const CombatZone* zone = &gstate->combat_zone[attacker];
  for(uint8_t i = 0; i < zone->size; i++)
    attack_variance += champion_variance(zone->cards[i]);

  float cap = expected_attack - params->defense_beta * sqrtf(attack_variance);
  if(cap < 0.0f) cap = 0.0f;

  uint8_t opp_energy = gstate->current_energy[attacker]; // attacker is defender's opponent here
  float target_cash, target_cards;
  resource_targets(opp_energy, params, &target_cash, &target_cards);

  float effective_hand, effective_cash;
  effective_hand_and_cash(gstate, defender, &effective_hand, &effective_cash);

  long cards_available = lroundf(effective_hand - target_cards);
  if(cards_available < 0) cards_available = 0;
  if(cards_available > 3) cards_available = 3;

  float cash_surplus = effective_cash - target_cash;
  uint16_t cash_cap = (cash_surplus > 0.0f) ? (uint16_t)cash_surplus : 0;

  uint8_t chosen[3];
  uint8_t chosen_count = select_defenders(gstate, defender, (uint8_t)cards_available,
                                          cash_cap, cap, chosen);

  for(uint8_t i = 0; i < chosen_count; i++)
    play_champion(gstate, defender, chosen[i], ctx);
} // balanced_rules_defense_strategy
