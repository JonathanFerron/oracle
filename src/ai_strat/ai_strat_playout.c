// ai_strat_playout.c
// Forked-RNG-stream playout infrastructure -- see ai_strat_playout.h.

#include <stddef.h>

#include "ai_strat_playout.h"
#include "ai_strategy.h"
#include "../core/game_constants.h"
#include "../core/combat.h"
#include "../core/turn_logic.h"
#include "../util/rnd.h"
#include "../actions/move_apply.h"

GameContext mc_fork_context(const GameContext* ctx, uint32_t seed)
{ GameContext sim_ctx = *ctx;
  sim_ctx.rng = seedRand(seed);
  return sim_ctx;
} // mc_fork_context

static void mark_seen(bool* seen, const uint8_t* cards, uint8_t n)
{ for(uint8_t i = 0; i < n; i++) seen[cards[i]] = true;
} // mark_seen

// Every fullDeck[] index not currently in observer's hand or either
// player's discard/combat zone. Returns the pool size.
static uint8_t build_unseen_pool(const struct gamestate* sim, PlayerID observer, uint8_t* pool)
{ bool seen[FULL_DECK_SIZE] = {0};

  mark_seen(seen, sim->hand[observer].cards, sim->hand[observer].size);
  mark_seen(seen, sim->discard[PLAYER_A].cards, sim->discard[PLAYER_A].size);
  mark_seen(seen, sim->discard[PLAYER_B].cards, sim->discard[PLAYER_B].size);
  mark_seen(seen, sim->combat_zone[PLAYER_A].cards, sim->combat_zone[PLAYER_A].size);
  mark_seen(seen, sim->combat_zone[PLAYER_B].cards, sim->combat_zone[PLAYER_B].size);

  uint8_t n = 0;
  for(uint8_t i = 0; i < FULL_DECK_SIZE; i++)
    if(!seen[i]) pool[n++] = i;
  return n;
} // build_unseen_pool

void mc_determinize(struct gamestate* sim, PlayerID observer, GameContext* sim_ctx)
{ PlayerID opponent = 1 - observer;

  uint8_t pool[FULL_DECK_SIZE];
  uint8_t pool_n = build_unseen_pool(sim, observer, pool);
  RND_partial_shuffle(pool, pool_n, pool_n, sim_ctx);

  uint8_t opp_hand_size = sim->hand[opponent].size;
  uint8_t my_deck_size = (uint8_t)(sim->deck[observer].top + 1);
  uint8_t opp_deck_size = (uint8_t)(sim->deck[opponent].top + 1);
  uint8_t cursor = 0;

  Hand_init(&sim->hand[opponent]);
  for(uint8_t i = 0; i < opp_hand_size; i++)
    Hand_add(&sim->hand[opponent], pool[cursor++]);

  sim->deck[observer].top = -1;
  for(uint8_t i = 0; i < my_deck_size; i++)
    DeckStk_push(&sim->deck[observer], pool[cursor++]);

  sim->deck[opponent].top = -1;
  for(uint8_t i = 0; i < opp_deck_size; i++)
    DeckStk_push(&sim->deck[opponent], pool[cursor++]);
} // mc_determinize

static float outcome_for(const struct gamestate* sim, PlayerID me)
{ if(sim->game_state == PLAYER_A_WINS) return (me == PLAYER_A) ? 1.0f : 0.0f;
  if(sim->game_state == PLAYER_B_WINS) return (me == PLAYER_B) ? 1.0f : 0.0f;
  return 0.5f; // DRAW, or the max_turns cap was hit without a winner
} // outcome_for

// Mirrors attack_phase()/defense_phase()'s own post-strategy transitions
// (turn_logic.c) for the single manually-driven `first` move, so the
// while(play_turn()) loop below can take over from a state indistinguishable
// from one play_turn() itself would have produced.
static void resolve_first_move(struct gamestate* sim, PlayerID cur,
                               StrategySet* rollout_strats, GameContext* sim_ctx)
{ if(sim->turn_phase == ATTACK)
  { sim->turn_phase = DEFENSE;
    sim->player_to_move = 1 - cur;
    if(sim->combat_zone[cur].size > 0)
    { defense_phase(sim, rollout_strats, sim_ctx);
      resolve_combat(sim, sim_ctx);
    }
  }
  else // DEFENSE -- the attacker's combat_zone[cur] is already committed
    resolve_combat(sim, sim_ctx);
} // resolve_first_move

float mc_playout(const struct gamestate* root, PlayerID me, const GameMove* first,
                 GameContext* sim_ctx, uint16_t max_turns)
{ struct gamestate sim = *root;
  PlayerID cur = sim.current_player;

  StrategySet rollout_strats = {0};
  set_player_strategy_by_type(&rollout_strats, PLAYER_A, AI_STRATEGY_RANDOM);
  set_player_strategy_by_type(&rollout_strats, PLAYER_B, AI_STRATEGY_RANDOM);

  apply_move(&sim, me, first, sim_ctx);
  resolve_first_move(&sim, cur, &rollout_strats, sim_ctx);

  if(!sim.someone_has_zero_energy)
    end_of_turn(&sim, &rollout_strats, sim_ctx);

  while(!sim.someone_has_zero_energy && sim.turn < max_turns)
    play_turn(NULL, &sim, &rollout_strats, sim_ctx);

  if(!sim.someone_has_zero_energy)
    sim.game_state = DRAW; // hit max_turns without a winner

  return outcome_for(&sim, me);
} // mc_playout
