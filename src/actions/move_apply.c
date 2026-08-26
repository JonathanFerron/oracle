// move_apply.c
// Applies a GameMove to a game state -- see move_apply.h.

#include "move_apply.h"
#include "../core/card_actions.h"

void apply_move(struct gamestate* gstate, PlayerID player, const GameMove* move,
                GameContext* ctx)
{ switch(move->type)
  { case MOVE_PASS:
      return;

    case MOVE_CHAMPIONS:
      for(uint8_t i = 0; i < move->count; i++)
        play_champion(gstate, player, move->cards[i], ctx);
      return;

    case MOVE_DRAW:
      play_draw_card(gstate, player, move->card, ctx);
      return;

    case MOVE_RECALL:
      play_recall_card(gstate, player, move->card, move->recall, move->count, ctx);
      return;

    case MOVE_CASH:
      play_cash_card_interactive(gstate, player, move->card, move->cards[0], ctx);
      return;
  }
} // apply_move
