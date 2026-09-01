// ai_strat_playout.c
// Forked-RNG-stream playout infrastructure -- see ai_strat_playout.h.

#include <stddef.h>

#include "ai_strat_playout.h"
#include "ai_strategy.h"
#include "ai_strat_common.h"
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

void mc_determinize(struct gamestate* sim, PlayerID observer, GameContext* sim_ctx)
{ PlayerID opponent = 1 - observer;

  uint8_t pool[FULL_DECK_SIZE];
  uint8_t pool_n = strat_common_unseen_pool(sim, observer, pool);
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

float mc_outcome_for(const struct gamestate* sim, PlayerID me)
{ if(sim->game_state == PLAYER_A_WINS) return (me == PLAYER_A) ? 1.0f : 0.0f;
  if(sim->game_state == PLAYER_B_WINS) return (me == PLAYER_B) ? 1.0f : 0.0f;
  return 0.5f; // DRAW, or the max_turns cap was hit without a winner
} // mc_outcome_for

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
                 const StrategySet* rollout_strats, GameContext* sim_ctx,
                 uint16_t max_turns)
{ struct gamestate sim = *root;
  PlayerID cur = sim.current_player;
  StrategySet local_strats = *rollout_strats; // turn_logic.c's calls need non-const

  apply_move(&sim, me, first, sim_ctx);
  resolve_first_move(&sim, cur, &local_strats, sim_ctx);

  if(!sim.someone_has_zero_energy)
    end_of_turn(&sim, &local_strats, sim_ctx);

  while(!sim.someone_has_zero_energy && sim.turn < max_turns)
    play_turn(NULL, &sim, &local_strats, sim_ctx);

  if(!sim.someone_has_zero_energy)
    sim.game_state = DRAW; // hit max_turns without a winner

  return mc_outcome_for(&sim, me);
} // mc_playout

// Mirrors play_turn()'s ATTACK/DEFENSE dispatch for whichever phase `sim` is
// currently sitting in, since there is no leading move here to have already
// picked a branch for us (contrast resolve_first_move(), which only ever
// resolves what's left *after* one).
static void play_out_pending_decision(struct gamestate* sim, StrategySet* rollout_strats,
                                      GameContext* sim_ctx)
{ if(sim->turn_phase == ATTACK)
    attack_phase(sim, rollout_strats, sim_ctx);
  else
    defense_phase(sim, rollout_strats, sim_ctx);

  if(sim->combat_zone[sim->current_player].size > 0)
    resolve_combat(sim, sim_ctx);
} // play_out_pending_decision

float mc_playout_from(const struct gamestate* root, PlayerID me,
                      const StrategySet* rollout_strats, GameContext* sim_ctx,
                      uint16_t max_turns)
{ struct gamestate sim = *root;
  StrategySet local_strats = *rollout_strats; // turn_logic.c's calls need non-const

  play_out_pending_decision(&sim, &local_strats, sim_ctx);

  if(!sim.someone_has_zero_energy)
    end_of_turn(&sim, &local_strats, sim_ctx);

  while(!sim.someone_has_zero_energy && sim.turn < max_turns)
    play_turn(NULL, &sim, &local_strats, sim_ctx);

  if(!sim.someone_has_zero_energy)
    sim.game_state = DRAW; // hit max_turns without a winner

  return mc_outcome_for(&sim, me);
} // mc_playout_from

bool mc_advance_to_decision(struct gamestate* sim, PlayerID player, const GameMove* move,
                            StrategySet* strats, GameContext* sim_ctx)
{ PlayerID cur = sim->current_player;
  bool was_attack = (sim->turn_phase == ATTACK);

  apply_move(sim, player, move, sim_ctx);

  if(was_attack)
  { sim->turn_phase = DEFENSE;
    sim->player_to_move = 1 - cur;
    if(sim->combat_zone[cur].size > 0)
      return true; // defender must decide next
  }
  else // DEFENSE -- the attacker's combat_zone[cur] is already committed
    resolve_combat(sim, sim_ctx);

  if(sim->someone_has_zero_energy)
    return false;

  end_of_turn(sim, strats, sim_ctx);
  begin_of_turn(sim, sim_ctx);
  return true; // attacker must decide next
} // mc_advance_to_decision

float mc_playout_from_turn_boundary(const struct gamestate* root, PlayerID me,
                                    const StrategySet* rollout_strats, GameContext* sim_ctx,
                                    uint16_t max_turns)
{ struct gamestate sim = *root;
  StrategySet local_strats = *rollout_strats; // turn_logic.c's calls need non-const

  while(!sim.someone_has_zero_energy && sim.turn < max_turns)
    play_turn(NULL, &sim, &local_strats, sim_ctx);

  if(!sim.someone_has_zero_energy)
    sim.game_state = DRAW; // hit max_turns without a winner

  return mc_outcome_for(&sim, me);
} // mc_playout_from_turn_boundary
