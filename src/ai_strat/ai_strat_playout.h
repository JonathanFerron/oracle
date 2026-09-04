// ai_strat_playout.h
// Forked-RNG-stream playout infrastructure for search-based AI agents (A8
// Simple Monte Carlo and later A9-A11) -- see ai_strat_hbt.h/
// ai_strat_heuristic.h for why every earlier agent refused to
// clone-and-apply at all: cloning gstate is free (pure POD), but running a
// simulated turn through the engine draws dice/cards from GameContext's RNG,
// and every prior agent needed that stream untouched to stay reproducible.
// mc_fork_context() is what breaks that constraint: a simulation drives the
// engine through a separate MTRand stream instead of the live game's own.

#ifndef AI_STRAT_PLAYOUT_H
#define AI_STRAT_PLAYOUT_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "../actions/game_move.h"
#include "ai_strategy.h" // StrategySet

// A GameContext with its own independent RNG stream, seeded separately from
// `ctx`. Every call that threads the returned context through the engine
// (mc_determinize(), mc_playout(), or any play_turn()/resolve_combat() a
// caller drives directly) draws only from that fork -- ctx->rng is left
// byte-identical.
GameContext mc_fork_context(const GameContext* ctx, uint32_t seed);

// Re-deals the information hidden from `observer` in `sim`: everything
// except observer's own hand and both players' discards/combat zones (all
// public). Consistent with setup_game()'s per-player 40-card allocation
// (hand+discard+combat_zone+deck always sums to 40 for a given player), the
// unseen pool always splits exactly into opponent's hand + observer's own
// remaining deck + opponent's remaining deck, with the 40 cards
// setup_game() never dealt to begin with left untouched. Deliberately not
// reshuffle-aware (see doc/ai_agents.md's A10 section's
// narrowing note) -- that refinement belongs to A10, not this agent.
void mc_determinize(struct gamestate* sim, PlayerID observer, GameContext* sim_ctx);

// Reads a terminal `sim`'s outcome from `me`'s perspective: 1.0 (won), 0.5
// (draw, or non-terminal -- callers only call this once someone_has_zero_
// energy is known true, or after the max_turns cap forced sim.game_state to
// DRAW), or 0.0 (lost). Exposed for A10/A11's own terminal checks (e.g. a
// tree search whose mc_advance_to_decision() call just returned false) --
// mc_playout()/mc_playout_from() use it internally too.
float mc_outcome_for(const struct gamestate* sim, PlayerID me);

// Plays `first` for `me` from `root`'s current phase, finishes that turn,
// then continues playing out the rest of the game via `rollout_strats`
// (both attack_strategy/defense_strategy AND mulligan_strategy/
// discard_strategy must be populated -- discard_to_7_cards() dereferences
// the latter even mid-rollout) until someone's energy hits 0 or `max_turns`
// is reached. Returns 1.0 (me wins), 0.5 (draw or the max_turns cap was hit
// without a winner), or 0.0 (me loses). `root` is never modified;
// `sim_ctx`'s RNG stream advances -- pass a forked context
// (mc_fork_context()), never the live game's own. Callers needing plain
// uniformly-random rollouts on both seats (A8's own use) can build that
// StrategySet with set_player_strategy_by_type(&s, p, AI_STRATEGY_RANDOM)
// for both players.
float mc_playout(const struct gamestate* root, PlayerID me, const GameMove* first,
                 const StrategySet* rollout_strats, GameContext* sim_ctx,
                 uint16_t max_turns);

// Like mc_playout(), but with no leading move to apply -- `root`'s own
// pending decision (whichever of ATTACK/DEFENSE its turn_phase is) is played
// out via `rollout_strats` too, not supplied by the caller. For A9-A11's
// tree search: a newly expanded leaf has no "first move" of its own (the
// move that created it was already applied via mc_advance_to_decision()),
// so the leaf's own pending decision must come from the rollout policy like
// everything after it. Same contract otherwise: `root` untouched, `sim_ctx`
// advances, returns 1.0/0.5/0.0 from `me`'s perspective.
float mc_playout_from(const struct gamestate* root, PlayerID me,
                      const StrategySet* rollout_strats, GameContext* sim_ctx,
                      uint16_t max_turns);

// The tree's DoMove(): applies `move` for `player` at sim's current decision
// (sim's turn_phase says which), then advances the engine through exactly
// the strategy-free transitions play_turn() itself would perform --
// resolve_combat() once both sides' moves are in, end_of_turn() (still
// dispatches to `strats`' mulligan/discard-to-7 hooks -- those are outside
// the tree, see about.md), begin_of_turn() -- stopping the instant another
// player must decide something, or the game ends. Returns true and leaves
// `sim` sitting at that next decision (ATTACK: attacker to move; DEFENSE:
// defender to move, only reachable when the attacker just committed
// champions); returns false if the game ended instead (check
// sim->someone_has_zero_energy / sim->game_state). Never touches `strats`'
// attack_strategy/defense_strategy -- the tree supplies `move` itself, which
// is the entire point of not calling attack_phase()/defense_phase()
// directly. `sim` is modified in place (this is a fork's working state, not
// `root`); `sim_ctx` advances.
bool mc_advance_to_decision(struct gamestate* sim, PlayerID player, const GameMove* move,
                            StrategySet* strats, GameContext* sim_ctx);

// Plays out from a turn boundary -- no phase pending at all, e.g. right
// after apply_mulligan() (before turn 1 has begun) or after a manual
// change_current_player() following a simulated discard-to-7 (see
// ai_strat_ismcts_flat.c, A10's flat mulligan/discard-to-7 scorer) -- via
// `rollout_strats` until someone's energy hits 0 or `max_turns` is reached.
// Same contract as mc_playout()/mc_playout_from() otherwise: `root`
// untouched, `sim_ctx` advances, returns 1.0/0.5/0.0 from `me`'s
// perspective.
float mc_playout_from_turn_boundary(const struct gamestate* root, PlayerID me,
                                    const StrategySet* rollout_strats, GameContext* sim_ctx,
                                    uint16_t max_turns);

#endif // AI_STRAT_PLAYOUT_H
