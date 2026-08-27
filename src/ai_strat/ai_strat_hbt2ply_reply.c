// ai_strat_hbt2ply_reply.c
// A9 HBT 2-Ply's public-information-only surrogate hand and two-ply
// candidate scoring -- see ai_strat_hbt2ply_reply.h for the full
// construction and rationale of both.

#include <stdlib.h>

#include "ai_strat_hbt2ply_reply.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"
#include "../structures/card_collection.h"
#include "ai_strat_common.h"

static void mark_seen(bool* seen, const uint8_t* cards, uint8_t n)
{ for(uint8_t i = 0; i < n; i++) seen[cards[i]] = true;
} // mark_seen

// Every fullDeck[] index not currently in `pov`'s own hand or either
// player's discard/combat zone -- same pool definition as
// ai_strat_playout.c's build_unseen_pool(), duplicated locally rather than
// shared (see ai_strat_hbt2ply.h's header comment on why this agent takes
// no dependency on A8's playout infrastructure).
static uint8_t build_unseen_pool(const struct gamestate* gstate, PlayerID pov, uint8_t* pool)
{ bool seen[FULL_DECK_SIZE] = {0};

  mark_seen(seen, gstate->hand[pov].cards, gstate->hand[pov].size);
  mark_seen(seen, gstate->discard[PLAYER_A].cards, gstate->discard[PLAYER_A].size);
  mark_seen(seen, gstate->discard[PLAYER_B].cards, gstate->discard[PLAYER_B].size);
  mark_seen(seen, gstate->combat_zone[PLAYER_A].cards, gstate->combat_zone[PLAYER_A].size);
  mark_seen(seen, gstate->combat_zone[PLAYER_B].cards, gstate->combat_zone[PLAYER_B].size);

  uint8_t n = 0;
  for(uint8_t i = 0; i < FULL_DECK_SIZE; i++)
    if(!seen[i]) pool[n++] = i;
  return n;
} // build_unseen_pool

static int cmp_defense_desc(const void* a, const void* b)
{ uint8_t ca = *(const uint8_t*)a;
  uint8_t cb = *(const uint8_t*)b;
  float da = fullDeck[ca].expected_defense;
  float db = fullDeck[cb].expected_defense;

  if(da > db) return -1;
  if(da < db) return 1;
  return (int)ca - (int)cb; // deterministic tie-break: no two calls may disagree
} // cmp_defense_desc

// Splits `pool` into champions (sorted descending by expected_defense) and
// non-champions (pool order, i.e. ascending fullDeck index). Both output
// arrays must hold at least `pool_n` entries.
static void split_and_sort_pool(const uint8_t* pool, uint8_t pool_n,
                                uint8_t* champions, uint8_t* n_champs,
                                uint8_t* non_champions, uint8_t* n_non)
{ *n_champs = 0;
  *n_non = 0;
  for(uint8_t i = 0; i < pool_n; i++)
  { if(fullDeck[pool[i]].card_type == CHAMPION_CARD)
      champions[(*n_champs)++] = pool[i];
    else
      non_champions[(*n_non)++] = pool[i];
  }
  qsort(champions, *n_champs, sizeof(uint8_t), cmp_defense_desc);
} // split_and_sort_pool

// Window start into the descending-sorted champions array: 0 at
// surrogate_pessimism = 1.0 (top k, the best available blockers), the
// pool's own median offset at surrogate_pessimism = 0.0, linear and
// monotonic between the two -- see ai_strat_hbt2ply_reply.h.
static uint8_t pessimism_window_start(uint8_t n_champs, uint8_t k, float surrogate_pessimism)
{ uint8_t max_start = (uint8_t)(n_champs - k);
  uint8_t median_start = max_start / 2;
  uint8_t start = (uint8_t)((1.0f - surrogate_pessimism) * (float)median_start + 0.5f);
  return (start > max_start) ? max_start : start;
} // pessimism_window_start

void build_surrogate_hand(const struct gamestate* gstate, PlayerID pov,
                          float surrogate_pessimism, Hand* out)
{ PlayerID opponent = 1 - pov;
  // Read the target size BEFORE Hand_init(out): a caller building the
  // surrogate directly into a gamestate clone's own hand[opponent] slot
  // (A9's own two-ply scoring does exactly this, to avoid a copy) passes
  // `out == &gstate->hand[opponent]`, so Hand_init(out) would otherwise
  // zero this same field before it's read.
  uint8_t requested_size = gstate->hand[opponent].size;
  Hand_init(out);

  uint8_t pool[FULL_DECK_SIZE];
  uint8_t pool_n = build_unseen_pool(gstate, pov, pool);
  if(pool_n == 0) return;

  uint8_t size = requested_size;
  if(size > pool_n) size = pool_n;
  if(size == 0) return;

  uint8_t champions[FULL_DECK_SIZE];
  uint8_t non_champions[FULL_DECK_SIZE];
  uint8_t n_champs, n_non;
  split_and_sort_pool(pool, pool_n, champions, &n_champs, non_champions, &n_non);

  float champion_fraction = (float)n_champs / (float)pool_n;
  uint8_t k = (uint8_t)(champion_fraction * (float)size + 0.5f);
  if(k > size) k = size;
  if(k > n_champs) k = n_champs;

  uint8_t start = pessimism_window_start(n_champs, k, surrogate_pessimism);
  for(uint8_t i = 0; i < k; i++)
    Hand_add(out, champions[start + i]);

  uint8_t non_needed = size - k;
  if(non_needed > n_non) non_needed = n_non;
  for(uint8_t i = 0; i < non_needed; i++)
    Hand_add(out, non_champions[i]);
} // build_surrogate_hand

// Clones `gstate`, commits `cards`/`count` to `player`'s combat zone, and
// replaces the opponent's hand with the public surrogate -- the position
// A7's own hbt_best_defense_move() is asked to reply to. `int_cost` is the
// caller's already-computed integer cost of the subset (avoids re-summing).
static void simulate_reply_position(const struct gamestate* gstate, PlayerID player,
                                    const uint8_t* cards, uint8_t count,
                                    uint16_t int_cost, float surrogate_pessimism,
                                    struct gamestate* sim)
{ PlayerID opponent = 1 - player;
  *sim = *gstate;

  for(uint8_t i = 0; i < count; i++)
  { Hand_remove(&sim->hand[player], cards[i]);
    CombatZone_add(&sim->combat_zone[player], cards[i]);
  }
  sim->current_cash_balance[player] -= int_cost;
  sim->turn_phase = DEFENSE;
  sim->player_to_move = opponent;

  build_surrogate_hand(sim, player, surrogate_pessimism, &sim->hand[opponent]);
} // simulate_reply_position

HBTBestMove hbt2ply_reply_defense_move(const struct gamestate* gstate, PlayerID defender,
                                       const HBTParams* params, const HBTState* state)
{ PlayerID attacker = 1 - defender;
  float own_energy = (float)gstate->current_energy[defender];
  float opp_energy = (float)gstate->current_energy[attacker];
  float own_hand = (float)gstate->hand[defender].size;
  float opp_hand = (float)gstate->hand[attacker].size;
  float own_cash = (float)gstate->current_cash_balance[defender];
  float opp_cash = (float)gstate->current_cash_balance[attacker];
  float incoming = variance_aware_incoming(gstate, defender, attacker, params);

  // The one difference from A7's own hbt_best_defense_move(): PASS charges
  // the full incoming attack against own_energy, instead of scoring the
  // decline option at the undamaged own_energy (see this file's header
  // comment on why A7's own baseline can't be reused here).
  float undamaged_energy = own_energy - incoming;
  if(undamaged_energy < 0.0f) undamaged_energy = 0.0f;

  HBTBestMove best =
  { .advantage = hbt_advantage(undamaged_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, params, state),
    .type = HBT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, defender,
                                         gstate->current_cash_balance[defender], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_defense_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                            own_cash, opp_cash, incoming, params, state, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_defense_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, incoming, params, state, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_defense_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                                own_cash, opp_cash, incoming, params, state, &best);
      }
    }
  }

  return best;
} // hbt2ply_reply_defense_move

bool hbt2ply_score_attack_subset(const struct gamestate* gstate, PlayerID player,
                                 const uint8_t* cards, uint8_t count,
                                 const HBT2PlyParams* params, const HBTState* my_state,
                                 float* out_score)
{ PlayerID opponent = 1 - player;
  float own_energy = (float)gstate->current_energy[player];
  float opp_energy = (float)gstate->current_energy[opponent];
  float own_hand = (float)gstate->hand[player].size;
  float opp_hand = (float)gstate->hand[opponent].size;
  float own_cash = (float)gstate->current_cash_balance[player];
  float opp_cash = (float)gstate->current_cash_balance[opponent];

  uint16_t int_cost = 0;
  for(uint8_t i = 0; i < count; i++) int_cost += fullDeck[cards[i]].cost;
  float cost = (float)int_cost;
  if(cost > own_cash) return false;

  float dmg = predicted_damage(cards, count, opp_energy);
  if(is_held_combo(cards, count, dmg, opponent, gstate, &params->base)) return false;

  float one_ply = hbt_advantage(own_energy, opp_energy - dmg, own_hand - (float)count,
                                opp_hand, own_cash - cost, opp_cash,
                                &params->base, my_state);

  float two_ply = one_ply; // ply not applied: gated off, or reply_trust == 0
  if(params->reply_trust > 0.0f && opp_energy <= (float)params->ply_energy_ceiling)
  { struct gamestate sim;
    simulate_reply_position(gstate, player, cards, count, int_cost,
                            params->surrogate_pessimism, &sim);

    HBTState opp_state = hbt_evaluate_state(&sim, opponent, &params->base);
    HBTBestMove reply = hbt2ply_reply_defense_move(&sim, opponent, &params->base, &opp_state);

    float net_damage = dmg - predicted_block(reply.cards, reply.count);
    if(net_damage < 0.0f) net_damage = 0.0f;

    // Deliberately NOT crediting the reply's own hand/cash cost here (only
    // the damage reduction) -- opp_hand/opp_cash stay exactly as they are
    // in one_ply. An earlier version subtracted reply.count/reply_int_cost
    // from them too, which credited "forced the opponent to spend a
    // resource blocking" via gamma_eff/delta_eff (~1.8-2.1) on top of the
    // damage reduction via eps_eff (~0.35) -- roughly 5x overweighting the
    // resource-trade side of a block relative to the damage side. A
    // controlled test (both sides given a rational, blocking defense,
    // attack-side logic the only variable) showed that version losing to
    // A7's own undefended attack 43% to 57%, i.e. the bias made the ply
    // actively harmful even when its block prediction was accurate.
    two_ply = hbt_advantage(own_energy, opp_energy - net_damage, own_hand - (float)count,
                            opp_hand, own_cash - cost, opp_cash, &params->base, my_state);
  }

  *out_score = (1.0f - params->reply_trust) * one_ply + params->reply_trust * two_ply;
  return true;
} // hbt2ply_score_attack_subset

/* ========================================================================
   Full attack enumeration: A7's own shape (pass + every affordable 1-3
   champion subset + every affordable draw/cash card), champion subsets
   routed through hbt2ply_score_attack_subset() and the ply_beam_width
   ranking below instead of A7's undefended formula.
   ======================================================================== */

// Hand's own 12-card cap (card_collection.h) bounds the affordable-champion
// list, so C(12,3)+C(12,2)+C(12,1) = 298 is the true maximum; a little
// margin costs nothing on the stack.
#define HBT2PLY_MAX_CANDIDATES 300

typedef struct
{ uint8_t cards[3];
  uint8_t count;
  float score; // one-ply-only until rescored inside the beam, see below
} HBT2PlyCandidate;

static void hbt2ply_consider(HBTBestMove* best, float advantage, HBTMoveType type,
                             const uint8_t* cards, uint8_t count)
{ if(advantage <= best->advantage) return;

  best->advantage = advantage;
  best->type = type;
  best->count = count;
  for(uint8_t i = 0; i < count; i++) best->cards[i] = cards[i];
} // hbt2ply_consider

static int cmp_candidate_score_desc(const void* a, const void* b)
{ float sa = ((const HBT2PlyCandidate*)a)->score;
  float sb = ((const HBT2PlyCandidate*)b)->score;
  if(sa > sb) return -1;
  if(sa < sb) return 1;
  return 0;
} // cmp_candidate_score_desc

// Records one candidate's cheap one-ply-only score (reply_trust forced to
// 0, so hbt2ply_score_attack_subset() skips the simulation) if it's
// applicable at all (affordable, not a held combo). A no-op otherwise, so
// held/unaffordable subsets never enter the beam ranking.
static void record_candidate(const struct gamestate* gstate, PlayerID player,
                             const uint8_t* cards, uint8_t count,
                             const HBT2PlyParams* one_ply_only, const HBTState* my_state,
                             HBT2PlyCandidate* candidates, uint16_t* n)
{ float score;
  if(!hbt2ply_score_attack_subset(gstate, player, cards, count, one_ply_only, my_state, &score))
    return;
  if(*n >= HBT2PLY_MAX_CANDIDATES) return; // unreachable given the 12-card hand cap

  for(uint8_t i = 0; i < count; i++) candidates[*n].cards[i] = cards[i];
  candidates[*n].count = count;
  candidates[*n].score = score;
  (*n)++;
} // record_candidate

static uint16_t collect_champion_candidates(const struct gamestate* gstate, PlayerID player,
                                            const uint8_t* affordable, uint8_t n,
                                            const HBT2PlyParams* one_ply_only,
                                            const HBTState* my_state,
                                            HBT2PlyCandidate* candidates)
{ uint16_t count = 0;
  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    record_candidate(gstate, player, c1, 1, one_ply_only, my_state, candidates, &count);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      record_candidate(gstate, player, c2, 2, one_ply_only, my_state, candidates, &count);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        record_candidate(gstate, player, c3, 3, one_ply_only, my_state, candidates, &count);
      }
    }
  }
  return count;
} // collect_champion_candidates

// Applies the real ply_beam_width restriction: candidates are already
// sorted descending by their cheap one-ply score, so the top
// `effective_beam` get rescored with the caller's real params (the full
// two-ply blend); the rest keep the one-ply score they were ranked by --
// exactly what they'd score anyway, since a subset outside the beam never
// gets the ply, same as one not applied for any other gating reason.
static void score_champion_candidates(const struct gamestate* gstate, PlayerID player,
                                      const HBT2PlyCandidate* candidates, uint16_t n,
                                      const HBT2PlyParams* params, const HBTState* my_state,
                                      HBTBestMove* best)
{ uint16_t effective_beam = (params->ply_beam_width == 0 || params->ply_beam_width >= n)
                            ? n : params->ply_beam_width;

  for(uint16_t i = 0; i < n; i++)
  { float score = candidates[i].score;
    if(i < effective_beam)
      hbt2ply_score_attack_subset(gstate, player, candidates[i].cards, candidates[i].count,
                                  params, my_state, &score);
    hbt2ply_consider(best, score, HBT_MOVE_CHAMPIONS, candidates[i].cards, candidates[i].count);
  }
} // score_champion_candidates

HBTBestMove hbt2ply_best_attack_move(struct gamestate* gstate, PlayerID player,
                                     const HBT2PlyParams* params, const HBTState* my_state)
{ PlayerID opp = 1 - player;
  float own_energy = (float)gstate->current_energy[player];
  float opp_energy = (float)gstate->current_energy[opp];
  float own_hand = (float)gstate->hand[player].size;
  float opp_hand = (float)gstate->hand[opp].size;
  float own_cash = (float)gstate->current_cash_balance[player];
  float opp_cash = (float)gstate->current_cash_balance[opp];

  HBTBestMove best =
  { .advantage = hbt_advantage(own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, &params->base, my_state),
    .type = HBT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, player,
                                         gstate->current_cash_balance[player], affordable);

  HBT2PlyParams one_ply_only = *params;
  one_ply_only.reply_trust = 0.0f;

  HBT2PlyCandidate candidates[HBT2PLY_MAX_CANDIDATES]; // ~2.4KB, fine on the stack
  uint16_t n_candidates = collect_champion_candidates(gstate, player, affordable, n,
                                                      &one_ply_only, my_state, candidates);
  qsort(candidates, n_candidates, sizeof(HBT2PlyCandidate), cmp_candidate_score_desc);
  score_champion_candidates(gstate, player, candidates, n_candidates, params, my_state, &best);

  bool has_champion = has_champion_in_hand(&gstate->hand[player]);
  const Hand* hand = &gstate->hand[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].cost > gstate->current_cash_balance[player]) continue;

    if(fullDeck[card_idx].card_type == DRAW_CARD)
    { float new_hand = own_hand - 1.0f + (float)fullDeck[card_idx].draw_num;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost;
      float adv = hbt_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                new_cash, opp_cash, &params->base, my_state);
      hbt2ply_consider(&best, adv, HBT_MOVE_DRAW, &card_idx, 1);
    }
    else if(fullDeck[card_idx].card_type == CASH_CARD && has_champion)
    { float new_hand = own_hand - 2.0f;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost +
                       (float)fullDeck[card_idx].exchange_cash;
      float adv = hbt_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                new_cash, opp_cash, &params->base, my_state);
      hbt2ply_consider(&best, adv, HBT_MOVE_CASH, &card_idx, 1);
    }
  }

  return best;
} // hbt2ply_best_attack_move
