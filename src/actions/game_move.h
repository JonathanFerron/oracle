// game_move.h
// The move representation shared by any AI that searches/simulates rather
// than scoring closed-form -- first consumer is A8 Simple Monte Carlo
// (ai_strat_simplemc1.c); A9-A11 (ideas/A9.../A10.../A11...) reuse it rather
// than re-deriving their own. See move_gen.h/move_apply.h for enumeration
// and application.

#ifndef GAME_MOVE_H
#define GAME_MOVE_H

#include <stdint.h>

// A move never spans both the attack and defense phase -- one MoveType
// covers exactly one phase's decision. MOVE_PASS covers both "attacker does
// nothing this turn" and "defender declines" (0 champions committed);
// MOVE_CHAMPIONS always commits 1-3 champions, legal in either phase. The
// other three types are attack-only (see move_gen.h).
typedef enum
{ MOVE_PASS = 0,
  MOVE_CHAMPIONS,
  MOVE_DRAW,
  MOVE_RECALL,
  MOVE_CASH
} MoveType;

// card/cards/recall fields are all fullDeck[] indices.
typedef struct
{ MoveType type;
  uint8_t  card;      // MOVE_DRAW/MOVE_RECALL/MOVE_CASH: the card played
  uint8_t  count;      // MOVE_CHAMPIONS: 1-3; MOVE_RECALL: choose_num; MOVE_CASH: 1
  uint8_t  cards[3];   // MOVE_CHAMPIONS: the subset; MOVE_CASH: cards[0] = champion exchanged
  uint8_t  recall[3];  // MOVE_RECALL: champions pulled from discard (choose_num is 1 or 2 in
  // fullDeck[] today, per game_constants.c -- sized 3 for margin)
} GameMove;

#endif // GAME_MOVE_H
