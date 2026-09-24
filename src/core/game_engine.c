// game_engine.c
// GameEngine's state machine -- see game_engine.h for the design rationale.
// Reuses turn_logic.c's begin_of_turn()/collect_1_luna()/change_current_player()
// as atomic step primitives (same primitives play_turn() itself is built
// from), the same way ai_strat_playout.c's mc_advance_to_decision() already
// does for tree search -- but never calls play_turn()/attack_phase()/
// defense_phase()/end_of_turn() themselves, since those each combine
// multiple wait-worthy steps into one call.

#include "game_engine.h"
#include "game_state.h"
#include "turn_logic.h"
#include "combat.h"
#include "card_actions.h"
#include "game_constants.h"
#include "../actions/move_apply.h"

static Hand cards_to_hand(const uint8_t* cards, uint8_t count)
{ Hand h = {0};
  for(uint8_t i = 0; i < count; i++) h.cards[i] = cards[i];
  h.size = count;
  return h;
} // cards_to_hand

// Exact for a hand-only change (mulligan discards, discard-to-7): whatever
// left `before` and isn't in `after` is what the player gave up, regardless
// of how many replacement cards were drawn in between.
static GameEvent hand_diff_event(GameEventType type, PlayerID player,
                                 const Hand* before, const Hand* after)
{ uint8_t removed[12];
  uint8_t n = cards_removed(before->cards, before->size, after->cards, after->size,
                            removed, 12);
  return (GameEvent)
  { .type = type, .player = player, .u.cards = cards_to_hand(removed, n)
  };
} // hand_diff_event

// Approximate for an AI attack/defense decision (no GameMove returned by a
// mutating strategy): champions newly committed to the combat zone, or
// MOVE_PASS if none -- see game_engine.h's own comment on why this doesn't
// recover draw/recall/cash sub-detail.
static GameEvent reconstruct_move_event(PlayerID player, const CombatZone* before,
                                        const CombatZone* after)
{ uint8_t added[3];
  uint8_t n = cards_added(before->cards, before->size, after->cards, after->size, added, 3);

  GameMove move = { .type = MOVE_PASS };
  if(n > 0)
  { move.type = MOVE_CHAMPIONS;
    move.count = n;
    for(uint8_t i = 0; i < n; i++) move.cards[i] = added[i];
  }
  return (GameEvent)
  { .type = EVT_MOVE_PLAYED, .player = player, .u.move = move
  };
} // reconstruct_move_event

static void enter_game_over(GameEngine* e, EventBuf* out)
{ PlayerID winner = (e->state.game_state == PLAYER_B_WINS) ? PLAYER_B : PLAYER_A;
  event_buf_push(out, (GameEvent)
  { .type = EVT_GAME_OVER, .player = winner
  });
  e->phase = ENG_GAME_OVER;
  e->pending = (PendingDecision)
  { .kind = DECISION_KIND_NONE, .player = winner
  };
} // enter_game_over

// begin_of_turn() draws for the attacker (except player A's turn 1) --
// reshuffle is detected exactly (deck was empty right before the draw,
// draw_1_card()'s own trigger condition), not inferred from a count diff.
static void advance_begin_turn(GameEngine* e, GameContext* ctx, EventBuf* out)
{ if(e->state.turn >= MAX_NUMBER_OF_TURNS)
  { e->state.game_state = DRAW;
    enter_game_over(e, out);
    return;
  }

  PlayerID attacker = e->state.current_player;
  bool deck_was_empty = DeckStk_isEmpty(&e->state.deck[attacker]);
  Hand hand_before = e->state.hand[attacker];

  begin_of_turn(&e->state, ctx);
  event_buf_push(out, (GameEvent)
  { .type = EVT_TURN_BEGAN, .player = attacker,
                              .u.turn = e->state.turn
  });

  uint8_t drawn[1];
  uint8_t n = cards_added(hand_before.cards, hand_before.size,
                          e->state.hand[attacker].cards, e->state.hand[attacker].size,
                          drawn, 1);
  if(n > 0)
  { if(deck_was_empty)
      event_buf_push(out, (GameEvent)
    { .type = EVT_DECK_RESHUFFLED, .player = attacker
    });
    event_buf_push(out, (GameEvent)
    { .type = EVT_CARD_DRAWN, .player = attacker,
                                .u.card = drawn[0]
    });
  }

  e->phase = ENG_ATTACK_WAIT;
  e->pending = (PendingDecision)
  { .kind = DECISION_KIND_ATTACK, .player = attacker
  };
} // advance_begin_turn

static void advance_combat(GameEngine* e, GameContext* ctx, EventBuf* out)
{ CombatDetails details;
  resolve_combat_with_details(&e->state, &details, ctx);
  event_buf_push(out, (GameEvent)
  { .type = EVT_COMBAT_RESOLVED, .player = e->state.current_player,
                                   .u.combat = details
  });

  if(e->state.someone_has_zero_energy)
  { enter_game_over(e, out);
    return;
  }
  e->phase = ENG_END_TURN;
} // advance_combat

static void advance_end_turn(GameEngine* e, EventBuf* out)
{ PlayerID attacker = e->state.current_player;
  collect_1_luna(&e->state);
  event_buf_push(out, (GameEvent)
  { .type = EVT_LUNA_COLLECTED, .player = attacker
  });

  if(e->state.hand[attacker].size > 7)
  { e->phase = ENG_DISCARD_WAIT;
    e->pending = (PendingDecision)
    { .kind = DECISION_KIND_DISCARD_TO_7, .player = attacker
    };
    return;
  }
  e->phase = ENG_SWITCH_PLAYER;
} // advance_end_turn

// Shared tail for both a human's engine_submit() and engine_run_ai(): the
// attacker's move is already applied to `e->state` by the caller. Matches
// attack_phase()'s own post-strategy transition (turn_logic.c) exactly --
// skip DEFENSE entirely (not just skip to no-op) when nothing was committed,
// matching play_turn()'s own `combat_zone[...].size > 0` guard.
static void after_attack_applied(GameEngine* e, GameEvent mv, EventBuf* out)
{ event_buf_push(out, mv);
  PlayerID attacker = e->state.current_player;
  e->state.turn_phase = DEFENSE;
  e->state.player_to_move = (PlayerID)(1 - attacker);

  if(e->state.combat_zone[attacker].size > 0)
  { e->phase = ENG_DEFENSE_WAIT;
    e->pending = (PendingDecision)
    { .kind = DECISION_KIND_DEFENSE, .player = e->state.player_to_move
    };
  }
  else
    e->phase = ENG_END_TURN;
} // after_attack_applied

static void after_defense_applied(GameEngine* e, GameEvent mv, EventBuf* out)
{ event_buf_push(out, mv);
  e->phase = ENG_COMBAT;
} // after_defense_applied

void engine_init(GameEngine* e, uint16_t initial_cash, GameContext* ctx, EventBuf* out)
{ setup_game(initial_cash, &e->state, ctx);

  // setup_game() doesn't initialize turn/turn_phase/player_to_move -- see
  // CLAUDE.md's "known architectural gaps" -- set them before the mulligan
  // wait, same ordering stda_auto.c's play_stda_auto_game() uses today.
  e->state.turn = 0;
  e->state.turn_phase = ATTACK;
  e->state.player_to_move = e->state.current_player;

  event_buf_push(out, (GameEvent)
  { .type = EVT_GAME_STARTED
  });

  e->phase = ENG_MULLIGAN_WAIT;
  e->pending = (PendingDecision)
  { .kind = DECISION_KIND_MULLIGAN, .player = PLAYER_B
  };
} // engine_init

PendingDecision engine_advance(GameEngine* e, GameContext* ctx, EventBuf* out)
{ for(;;)
  { switch(e->phase)
    { case ENG_MULLIGAN_WAIT:
      case ENG_ATTACK_WAIT:
      case ENG_DEFENSE_WAIT:
      case ENG_DISCARD_WAIT:
      case ENG_GAME_OVER:
      case ENG_SETUP:
        return e->pending;

      case ENG_BEGIN_TURN:
        advance_begin_turn(e, ctx, out);
        break;
      case ENG_COMBAT:
        advance_combat(e, ctx, out);
        break;
      case ENG_END_TURN:
        advance_end_turn(e, out);
        break;
      case ENG_SWITCH_PLAYER:
        change_current_player(&e->state);
        e->phase = ENG_BEGIN_TURN;
        break;
    }
  }
} // engine_advance

static void submit_mulligan(GameEngine* e, PlayerID player, const PlayerDecision* d,
                            GameContext* ctx, EventBuf* out)
{ mulligan_apply(&e->state, player, d->cards, d->count, ctx);
  event_buf_push(out, (GameEvent)
  { .type = EVT_MULLIGAN_DONE, .player = player, .u.cards = cards_to_hand(d->cards, d->count)
  });
  e->phase = ENG_BEGIN_TURN;
} // submit_mulligan

static void submit_discard(GameEngine* e, PlayerID player, const PlayerDecision* d, EventBuf* out)
{ discard_to_7_apply(&e->state, player, d->cards, d->count);
  event_buf_push(out, (GameEvent)
  { .type = EVT_DISCARDED_TO_7, .player = player, .u.cards = cards_to_hand(d->cards, d->count)
  });
  e->phase = ENG_SWITCH_PLAYER;
} // submit_discard

bool engine_submit(GameEngine* e, PlayerID player, const PlayerDecision* d,
                   GameContext* ctx, EventBuf* out)
{ if(player != e->pending.player) return false;
  if(!decision_is_legal(&e->state, player, e->pending.kind, d)) return false;

  GameEvent mv = { .type = EVT_MOVE_PLAYED, .player = player, .u.move = d->move };

  switch(d->kind)
  { case DECISION_KIND_MULLIGAN:
      submit_mulligan(e, player, d, ctx, out);
      return true;
    case DECISION_KIND_ATTACK:
      apply_move(&e->state, player, &d->move, ctx);
      after_attack_applied(e, mv, out);
      return true;
    case DECISION_KIND_DEFENSE:
      apply_move(&e->state, player, &d->move, ctx);
      after_defense_applied(e, mv, out);
      return true;
    case DECISION_KIND_DISCARD_TO_7:
      submit_discard(e, player, d, out);
      return true;
    default:
      return false;
  }
} // engine_submit

static void run_ai_mulligan(GameEngine* e, const StrategySet* strategies, GameContext* ctx,
                            EventBuf* out, PlayerID player)
{ Hand before = e->state.hand[player];
  strategies->mulligan_strategy[player](&e->state, player, ctx);
  event_buf_push(out, hand_diff_event(EVT_MULLIGAN_DONE, player, &before, &e->state.hand[player]));
  e->phase = ENG_BEGIN_TURN;
} // run_ai_mulligan

static void run_ai_discard(GameEngine* e, const StrategySet* strategies, GameContext* ctx,
                           EventBuf* out, PlayerID player)
{ Hand before = e->state.hand[player];
  strategies->discard_strategy[player](&e->state, player, ctx);
  event_buf_push(out, hand_diff_event(EVT_DISCARDED_TO_7, player, &before, &e->state.hand[player]));
  e->phase = ENG_SWITCH_PLAYER;
} // run_ai_discard

void engine_run_ai(GameEngine* e, const StrategySet* strategies, GameContext* ctx,
                   EventBuf* out)
{ PlayerID player = e->pending.player;

  switch(e->pending.kind)
  { case DECISION_KIND_MULLIGAN:
      run_ai_mulligan(e, strategies, ctx, out, player);
      return;

    case DECISION_KIND_ATTACK:
    { CombatZone before = e->state.combat_zone[player];
      strategies->attack_strategy[player](&e->state, ctx);
      after_attack_applied(e, reconstruct_move_event(player, &before, &e->state.combat_zone[player]), out);
      return;
    }

    case DECISION_KIND_DEFENSE:
    { CombatZone before = e->state.combat_zone[player];
      strategies->defense_strategy[player](&e->state, ctx);
      after_defense_applied(e, reconstruct_move_event(player, &before, &e->state.combat_zone[player]), out);
      return;
    }

    case DECISION_KIND_DISCARD_TO_7:
      run_ai_discard(e, strategies, ctx, out, player);
      return;

    default:
      return;
  }
} // engine_run_ai
