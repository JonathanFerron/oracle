// coin_wallet.h
// Physical-coin view of a player's lunas: how many Luna (1), Aureus (2) and
// Opal (3) coins they hold, and which coins pay a given cost. The plain
// luna count (gamestate.current_cash_balance) stays authoritative; this is
// what the GUI draws on the table.
//
// Payment rule: pay exactly if possible, spending Opals first, then Aureus,
// then Lunas; otherwise overpay by the smallest amount and the bank returns
// the overpay as Lunas (a "change event"). The overpay is proven to be at
// most 2 for any wallet and cost 0..9, and is 2 only for an Opals-only
// wallet paying 1, 4 or 7 -- see testsrc/test_coin_wallet.c, which checks
// both exhaustively.
//
// Depends only on libc, like src/rating/.

#ifndef COIN_WALLET_H
#define COIN_WALLET_H

#include <stdint.h>
#include <stdbool.h>

#define COIN_MAX_OVERPAY 2

// A wallet, or a payment drawn from one.
typedef struct
{ uint8_t luna;
  uint8_t aureus;
  uint8_t opal;
} CoinSet;

// Total value in lunas: luna + 2*aureus + 3*opal.
uint16_t coin_value(const CoinSet* c);

// Starting wallet for `value` lunas. The rules start every player with
// 5/5/5 (30 lunas); any other value is split as value/6 Opals, value/6
// Aureus and the rest in Lunas, which gives 5/5/5 for 30.
CoinSet coin_wallet_from_value(uint16_t value);

// Finds coins from `w` worth exactly `target`, preferring Opals, then
// Aureus. Returns false (and leaves `out` untouched) if no exact
// combination exists.
bool coin_find_payment(const CoinSet* w, uint16_t target, CoinSet* out);

// Picks the coins to pay `cost` from `w`: exact if possible, otherwise the
// smallest overpay. Sets *overpay (0..COIN_MAX_OVERPAY). Precondition:
// cost <= coin_value(w) and cost <= 9.
CoinSet coin_select_payment(const CoinSet* w, uint16_t cost, uint8_t* overpay);

// Removes payment `p` from `w` and adds `overpay` Lunas of change.
void coin_apply_payment(CoinSet* w, const CoinSet* p, uint8_t overpay);

#endif // COIN_WALLET_H
