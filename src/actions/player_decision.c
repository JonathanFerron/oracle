// player_decision.c
// decision_is_legal() -- see player_decision.h for the design rationale.

#include "player_decision.h"
#include "move_gen.h"
#include "../core/game_constants.h"
#include "../ai_strat/ai_strat_lib_heuristics.h"

static bool has_duplicates(const uint8_t* cards, uint8_t n)
{ for(uint8_t i = 0; i < n; i++)
    for(uint8_t j = i + 1; j < n; j++)
      if(cards[i] == cards[j]) return true;
  return false;
} // has_duplicates

static bool array_contains(const uint8_t* arr, uint8_t n, uint8_t val)
{ for(uint8_t i = 0; i < n; i++)
    if(arr[i] == val) return true;
  return false;
} // array_contains

// Order-insensitive equality; correct only when both sides are duplicate-free
// (true for a GameMove's own cards[]/recall[], and checked separately for a
// submitted decision's cards[] before this is ever called on it).
static bool sets_match(const uint8_t* a, uint8_t na, const uint8_t* b, uint8_t nb)
{ if(na != nb) return false;
  for(uint8_t i = 0; i < na; i++)
    if(!array_contains(b, nb, a[i])) return false;
  return true;
} // sets_match

static bool legal_contains_card(const GameMove* legal, uint8_t n, MoveType type, uint8_t card)
{ for(uint8_t i = 0; i < n; i++)
    if(legal[i].type == type && legal[i].card == card) return true;
  return false;
} // legal_contains_card

static bool champions_move_is_legal(const GameMove* legal, uint8_t n, const GameMove* move)
{ if(move->count < 1 || move->count > 3) return false;
  if(has_duplicates(move->cards, move->count)) return false;

  for(uint8_t i = 0; i < n; i++)
  { if(legal[i].type != MOVE_CHAMPIONS) continue;
    if(sets_match(legal[i].cards, legal[i].count, move->cards, move->count)) return true;
  }
  return false;
} // champions_move_is_legal

// The recall sub-choice: legal_contains_card() already confirmed `card`
// itself is a playable draw/recall card (in hand, affordable, discard holds
// enough champions to fill at least one variant); this checks the specific
// champions named are legal -- exactly choose_num of them, no duplicates,
// each really sitting in `player`'s discard.
static bool recall_variant_is_legal(const struct gamestate* gstate, PlayerID player,
                                    const GameMove* move)
{ uint8_t choose_num = fullDeck[move->card].choose_num;
  if(move->count != choose_num) return false;
  if(has_duplicates(move->recall, move->count)) return false;

  const Discard* discard = &gstate->discard[player];
  for(uint8_t i = 0; i < move->count; i++)
  { uint8_t c = move->recall[i];
    if(fullDeck[c].card_type != CHAMPION_CARD) return false;
    if(!array_contains(discard->cards, discard->size, c)) return false;
  }
  return true;
} // recall_variant_is_legal

// The cash sub-choice: legal_contains_card() already confirmed `card` itself
// is playable; this checks the named exchange target really sits in hand.
static bool cash_variant_is_legal(const struct gamestate* gstate, PlayerID player,
                                  const GameMove* move)
{ if(move->count != 1) return false;
  uint8_t champion = move->cards[0];
  if(fullDeck[champion].card_type != CHAMPION_CARD) return false;
  return Hand_contains(&gstate->hand[player], champion);
} // cash_variant_is_legal

static bool move_is_legal(const struct gamestate* gstate, PlayerID player, const GameMove* move)
{ if(move->type == MOVE_PASS) return true;

  MoveGenLimits limits = { .max_recall_variants = 1, .max_cash_variants = 1 };
  GameMove legal[MOVE_GEN_MAX_MOVES];
  uint8_t n = get_available_moves(gstate, player, &limits, legal, MOVE_GEN_MAX_MOVES);

  switch(move->type)
  { case MOVE_CHAMPIONS:
      return champions_move_is_legal(legal, n, move);
    case MOVE_DRAW:
      return legal_contains_card(legal, n, MOVE_DRAW, move->card);
    case MOVE_RECALL:
      return legal_contains_card(legal, n, MOVE_RECALL, move->card)
             && recall_variant_is_legal(gstate, player, move);
    case MOVE_CASH:
      return legal_contains_card(legal, n, MOVE_CASH, move->card)
             && cash_variant_is_legal(gstate, player, move);
    default:
      return false;
  }
} // move_is_legal

// Mulligan is always player B, always before turn 1 (gstate->turn == 0, the
// value play_stda_auto_game()/stda_cli.c/stda_tui.c set prior to the first
// begin_of_turn() call -- see CLAUDE.md's "known architectural gaps").
static bool mulligan_is_legal(const struct gamestate* gstate, PlayerID player,
                              const PlayerDecision* d)
{ if(player != PLAYER_B || gstate->turn != 0) return false;
  if(d->count > mulligan_get_max_cards()) return false;
  if(has_duplicates(d->cards, d->count)) return false;

  for(uint8_t i = 0; i < d->count; i++)
    if(!Hand_contains(&gstate->hand[player], d->cards[i])) return false;

  return true;
} // mulligan_is_legal

// Discard-to-7 is always the current turn's attacker (card_actions.c's
// discard_to_7_cards() reads gstate->current_player, called before the
// player switches at end of turn) and always exactly hand.size - 7.
static bool discard_to_7_is_legal(const struct gamestate* gstate, PlayerID player,
                                  const PlayerDecision* d)
{ if(player != gstate->current_player) return false;

  const Hand* hand = &gstate->hand[player];
  if(hand->size <= 7) return false;
  if(d->count != (uint8_t)(hand->size - 7)) return false;
  if(has_duplicates(d->cards, d->count)) return false;

  for(uint8_t i = 0; i < d->count; i++)
    if(!Hand_contains(hand, d->cards[i])) return false;

  return true;
} // discard_to_7_is_legal

bool decision_is_legal(const struct gamestate* gstate, PlayerID player,
                       DecisionKind expected, const PlayerDecision* d)
{ if(d->kind != expected) return false;

  switch(d->kind)
  { case DECISION_KIND_MULLIGAN:
      return mulligan_is_legal(gstate, player, d);
    case DECISION_KIND_DISCARD_TO_7:
      return discard_to_7_is_legal(gstate, player, d);
    case DECISION_KIND_ATTACK:
      return gstate->turn_phase == ATTACK && gstate->player_to_move == player
             && move_is_legal(gstate, player, &d->move);
    case DECISION_KIND_DEFENSE:
      return gstate->turn_phase == DEFENSE && gstate->player_to_move == player
             && (d->move.type == MOVE_PASS || d->move.type == MOVE_CHAMPIONS)
             && move_is_legal(gstate, player, &d->move);
    default:
      return false;
  }
} // decision_is_legal
