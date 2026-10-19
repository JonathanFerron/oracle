// test_coin_wallet.c
// Exhaustive test of the coin payment rule (src/core/coin_wallet.c): every
// wallet with 0..30 of each coin, every cost 0..min(9, value). Checks each
// payment against a brute-force reachable-sum set, so "minimum overpay" is
// verified rather than assumed.

#include "../src/core/coin_wallet.h"
#include <stdio.h>

#define TEST_PASS "\033[32m✓ PASS\033[0m"
#define TEST_FAIL "\033[31m✗ FAIL\033[0m"

#define MAX_COST 9
#define MAX_TARGET (MAX_COST + COIN_MAX_OVERPAY)

typedef struct
{ int passed;
  int failed;
} TestSuite;

typedef struct
{ long cases;
  long change_cases;
  int max_overpay;
  long bad_fit;          // payment uses coins the wallet doesn't hold
  long bad_value;        // coin_value(P) != cost + overpay
  long bad_minimum;      // a smaller overpay was reachable
  long bad_ov2_shape;    // overpay 2 outside Opals-only, cost 1/4/7
  long bad_apply;        // wallet value after paying != value - cost
} SweepStats;

static void check(TestSuite* suite, const char* name, bool ok)
{ printf("  %s: %s\n", ok ? TEST_PASS : TEST_FAIL, name);
  if(ok) suite->passed++;
  else suite->failed++;
} // check

// reach[s] is true if some sub-multiset of `w` sums to exactly s, for
// s <= MAX_TARGET (larger sums never matter here).
static void reachable_sums(const CoinSet* w, bool reach[MAX_TARGET + 1])
{ for(int s = 0; s <= MAX_TARGET; s++) reach[s] = false;
  for(int c = 0; c <= w->opal && 3 * c <= MAX_TARGET; c++)
    for(int b = 0; b <= w->aureus && 3 * c + 2 * b <= MAX_TARGET; b++)
      for(int a = 0; a <= w->luna && 3 * c + 2 * b + a <= MAX_TARGET; a++)
        reach[3 * c + 2 * b + a] = true;
} // reachable_sums

static void check_one(const CoinSet* w, uint16_t cost, const bool* reach, SweepStats* st)
{ uint8_t ov = 0xFF;
  CoinSet p = coin_select_payment(w, cost, &ov);
  st->cases++;
  if(ov > 0) st->change_cases++;
  if(ov > st->max_overpay) st->max_overpay = ov;

  if(p.luna > w->luna || p.aureus > w->aureus || p.opal > w->opal) st->bad_fit++;
  if(coin_value(&p) != cost + ov) st->bad_value++;
  for(int k = 0; k < ov; k++)
    if(reach[cost + k]) st->bad_minimum++;
  if(ov == 2 && !(w->luna == 0 && w->aureus == 0 && (cost == 1 || cost == 4 || cost == 7)))
    st->bad_ov2_shape++;

  CoinSet after = *w;
  coin_apply_payment(&after, &p, ov);
  if(coin_value(&after) != coin_value(w) - cost) st->bad_apply++;
} // check_one

static SweepStats sweep(int max_coins)
{ SweepStats st = { 0 };
  bool reach[MAX_TARGET + 1];
  for(int a = 0; a <= max_coins; a++)
    for(int b = 0; b <= max_coins; b++)
      for(int c = 0; c <= max_coins; c++)
      { CoinSet w = { (uint8_t)a, (uint8_t)b, (uint8_t)c };
        uint16_t value = coin_value(&w);
        uint16_t top = value < MAX_COST ? value : MAX_COST;
        reachable_sums(&w, reach);
        for(uint16_t cost = 0; cost <= top; cost++)
          check_one(&w, cost, reach, &st);
      }
  return st;
} // sweep

static void report_sweep(TestSuite* suite, int max_coins, const SweepStats* st)
{ printf("\n[sweep] wallets 0..%d of each coin: %ld cases, %ld need change, max overpay %d\n",
         max_coins, st->cases, st->change_cases, st->max_overpay);
  check(suite, "payment always fits in the wallet", st->bad_fit == 0);
  check(suite, "payment value == cost + overpay", st->bad_value == 0);
  check(suite, "overpay is the true minimum", st->bad_minimum == 0);
  check(suite, "overpay <= COIN_MAX_OVERPAY", st->max_overpay <= COIN_MAX_OVERPAY);
  check(suite, "overpay 2 only for Opals-only wallets paying 1/4/7", st->bad_ov2_shape == 0);
  check(suite, "applying the payment leaves value - cost", st->bad_apply == 0);
} // report_sweep

static void test_python_port_agreement(TestSuite* suite)
{ // Same range as the spec's Python port, which reported 33614 cases, 403
  // needing change, max overpay 2.
  SweepStats st = sweep(14);
  report_sweep(suite, 14, &st);
  check(suite, "case count matches the Python port (33614)", st.cases == 33614);
  check(suite, "change count matches the Python port (403)", st.change_cases == 403);
  check(suite, "max overpay matches the Python port (2)", st.max_overpay == 2);
} // test_python_port_agreement

static void test_specific_payments(TestSuite* suite)
{ printf("\n[specific]\n");
  CoinSet w = coin_wallet_from_value(30);
  check(suite, "30 lunas -> 5 Lunas, 5 Aureus, 5 Opals",
        w.luna == 5 && w.aureus == 5 && w.opal == 5);

  uint8_t ov = 0xFF;
  CoinSet p = coin_select_payment(&w, 0, &ov);
  check(suite, "cost 0 pays nothing", ov == 0 && coin_value(&p) == 0);

  p = coin_select_payment(&w, 3, &ov);
  check(suite, "cost 3 from 5/5/5 is one Opal", ov == 0 && p.opal == 1 && p.aureus == 0 && p.luna == 0);

  p = coin_select_payment(&w, 5, &ov);
  check(suite, "cost 5 from 5/5/5 is Opal + Aureus", ov == 0 && p.opal == 1 && p.aureus == 1 && p.luna == 0);

  CoinSet opals = { 0, 0, 3 };
  p = coin_select_payment(&opals, 1, &ov);
  coin_apply_payment(&opals, &p, ov);
  check(suite, "cost 1 from Opals only: one Opal, 2 Lunas change",
        ov == 2 && p.opal == 1 && opals.luna == 2 && opals.opal == 2);
} // test_specific_payments

int main(void)
{ TestSuite suite = { 0, 0 };
  printf("\n=== ORACLE COIN WALLET TEST SUITE ===\n");

  test_specific_payments(&suite);
  test_python_port_agreement(&suite);
  SweepStats st = sweep(30);
  report_sweep(&suite, 30, &st);

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d, Failed: %d, Total: %d\n",
         suite.passed, suite.failed, suite.passed + suite.failed);
  return suite.failed > 0 ? 1 : 0;
}
