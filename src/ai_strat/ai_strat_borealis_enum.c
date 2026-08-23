// ai_strat_borealis_enum.c
// A3 Borealis's candidate enumeration and scoring -- split out of
// ai_strat_borealis.c per its handout's Sec.10 file-length guidance ("if
// the file exceeds ~400 lines after decomposition, split enumeration and
// scoring into a separate translation unit"). See
// ideas/A3 ai agent greedy power (borealis)/greedy_power_borealis_handout.md
// Sec.4-6 for the scoring model and epsilon tie-break implemented here.
//
// collect_candidates()'s three nested loops are the ONLY enumeration site
// (handout Sec.5: "do not add loop caps, sampling, or combo-bonus
// thresholds" -- pruning here is exactly what makes A2 Combo Threshold the
// weaker, more exploitable design). The empty set is never special-cased:
// it is emitted like any other candidate and scores exactly 0, which is
// what makes lambda interpretable (Sec.4).

#include "ai_strat_borealis_enum.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../util/mtwister.h"

float borealis_expected_attack_of(uint8_t card_idx)
{ return fullDeck[card_idx].expected_attack;
} // borealis_expected_attack_of

float borealis_expected_defense_of(uint8_t card_idx)
{ return fullDeck[card_idx].expected_defense;
} // borealis_expected_defense_of

// One candidate's raw value (Sigma contribution + combo bonus, uncapped --
// what is_held_combo() tests against opponent energy), its ranking score
// (raw value capped at `cap` when cap >= 0, minus lambda*cost -- Sec.4), and
// total cost (for the cumulative-affordability check, Sec.5).
typedef struct
{ float raw_value;
  float score;
  uint16_t cost;
} CandidateEval;

static CandidateEval evaluate_candidate(const uint8_t* cards, uint8_t count,
                                        BorealisContributionFunc contribution,
                                        float lambda, float cap)
{ CandidateEval eval = { 0 };

  for(uint8_t i = 0; i < count; i++)
  { eval.raw_value += contribution(cards[i]);
    eval.cost += fullDeck[cards[i]].cost;
  }
  eval.raw_value += (float)combo_bonus_for_selection(cards, count);

  float capped = (cap >= 0.0f && eval.raw_value > cap) ? cap : eval.raw_value;
  eval.score = capped - lambda * (float)eval.cost;

  return eval;
} // evaluate_candidate

// Sec.7: hold candidate S back (exclude it from consideration) when its
// combo bonus clears lethal_combo_bonus, it wouldn't finish the opponent off
// right now (raw_value < their current energy), and their energy is still
// comfortably above lethal_hold_ceiling. Attack-only -- callers gate this
// via apply_holding, since holding back a defensive play makes no sense.
static bool is_held_combo(const uint8_t* cards, uint8_t count, float raw_value,
                          PlayerID opponent, const struct gamestate* gstate,
                          const BorealisParams* params)
{ if(!params->hold_lethal_combos || count < 2) return false;

  int bonus = combo_bonus_for_selection(cards, count);
  if(bonus < params->lethal_combo_bonus) return false;

  uint8_t opp_energy = gstate->current_energy[opponent];
  if((int)opp_energy <= params->lethal_hold_ceiling) return false;
  if(raw_value >= (float)opp_energy) return false;

  return true;
} // is_held_combo

// Hand is capped at 12 cards (Hand struct); worst-case affordable-champion
// count is therefore 12, giving C(12,0)+C(12,1)+C(12,2)+C(12,3) = 299
// candidates across all subset sizes 0-3.
#define BOREALIS_MAX_CANDIDATES 300

typedef struct
{ uint8_t cards[3];
  uint8_t count;
  float score;
} BorealisCandidate;

// Scores one candidate and, if legal (affordable and, on attack, not held
// back per Sec.7), appends it to candidates[] and updates *out_best_score.
// The single call site for evaluate_candidate()/is_held_combo() -- every
// candidate size below funnels through here so the legality rule can't
// drift between sizes.
static void emit_candidate(const uint8_t* cards, uint8_t n,
                           const struct gamestate* gstate, PlayerID player,
                           BorealisContributionFunc contribution, float cap,
                           bool apply_holding, const BorealisParams* params,
                           BorealisCandidate* candidates, uint16_t* out_n,
                           float* out_best_score)
{ CandidateEval eval = evaluate_candidate(cards, n, contribution,
                                          params->luna_value, cap);
  if(eval.cost > gstate->current_cash_balance[player]) return;

  if(apply_holding &&
     is_held_combo(cards, n, eval.raw_value, 1 - player, gstate, params))
    return;

  BorealisCandidate* c = &candidates[*out_n];
  for(uint8_t i = 0; i < n; i++) c->cards[i] = cards[i];
  c->count = n;
  c->score = eval.score;
  (*out_n)++;

  if(eval.score > *out_best_score) *out_best_score = eval.score;
} // emit_candidate

// The three nested loops (handout Sec.5): every 0-3 subset of
// affordable[0..count). Enforces cumulative affordability per-candidate
// (inside the loops, not just at the leaves) via emit_candidate().
static uint16_t collect_candidates(const uint8_t* affordable, uint8_t count,
                                   const struct gamestate* gstate, PlayerID player,
                                   BorealisContributionFunc contribution, float cap,
                                   bool apply_holding, const BorealisParams* params,
                                   BorealisCandidate* candidates, float* out_best_score)
{ uint16_t n = 0;
  *out_best_score = -1e9f;

  uint8_t empty[1] = { 0 };
  emit_candidate(empty, 0, gstate, player, contribution, cap, apply_holding,
                 params, candidates, &n, out_best_score);

  for(uint8_t i = 0; i < count; i++)
  { uint8_t c1[1] = { affordable[i] };
    emit_candidate(c1, 1, gstate, player, contribution, cap, apply_holding,
                   params, candidates, &n, out_best_score);

    for(uint8_t j = i + 1; j < count; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      emit_candidate(c2, 2, gstate, player, contribution, cap, apply_holding,
                     params, candidates, &n, out_best_score);

      for(uint8_t k = j + 1; k < count; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        emit_candidate(c3, 3, gstate, player, contribution, cap, apply_holding,
                       params, candidates, &n, out_best_score);
      }
    }
  }

  return n;
} // collect_candidates

// Sec.6: uniformly at random among every candidate scoring within epsilon of
// the best -- even at epsilon == 0, ties must still be broken randomly, so
// this never just takes candidates[0]. Uses genRandLong() directly rather
// than RND_randn() (rnd.c): the epsilon band can hold up to 299 candidates,
// past RND_randn()'s uint8_t count parameter.
static void pick_epsilon_band(const BorealisCandidate* candidates, uint16_t n,
                              float best_score, float epsilon, GameContext* ctx,
                              uint8_t* out_cards, uint8_t* out_count)
{ uint16_t band_idx[BOREALIS_MAX_CANDIDATES];
  uint16_t band_n = 0;

  for(uint16_t i = 0; i < n; i++)
    if(candidates[i].score >= best_score - epsilon)
      band_idx[band_n++] = i;

  uint16_t chosen = (uint16_t)(genRandLong(&ctx->rng) % band_n);
  const BorealisCandidate* winner = &candidates[band_idx[chosen]];

  for(uint8_t i = 0; i < winner->count; i++)
    out_cards[i] = winner->cards[i];
  *out_count = winner->count;
} // pick_epsilon_band

void borealis_best_champion_set(const uint8_t* affordable, uint8_t count,
                                const struct gamestate* gstate, PlayerID player,
                                BorealisContributionFunc contribution, float cap,
                                bool apply_holding, const BorealisParams* params,
                                GameContext* ctx, uint8_t* out_cards, uint8_t* out_count)
{ BorealisCandidate candidates[BOREALIS_MAX_CANDIDATES];
  float best_score;

  uint16_t n = collect_candidates(affordable, count, gstate, player, contribution,
                                  cap, apply_holding, params, candidates, &best_score);

  pick_epsilon_band(candidates, n, best_score, params->tiebreak_epsilon, ctx,
                    out_cards, out_count);
} // borealis_best_champion_set
