// coin_wallet.c
// Coin payment selection -- see coin_wallet.h for the rule.

#include <assert.h>

#include "coin_wallet.h"

uint16_t coin_value(const CoinSet* c)
{ return (uint16_t)(c->luna + 2 * c->aureus + 3 * c->opal);
}

CoinSet coin_wallet_from_value(uint16_t value)
{ uint8_t big = (uint8_t)(value / 6);
  return (CoinSet)
  { .luna = (uint8_t)(value - 5 * big), .aureus = big, .opal = big
  };
} // coin_wallet_from_value

bool coin_find_payment(const CoinSet* w, uint16_t target, CoinSet* out)
{ int max3 = target / 3 < w->opal ? target / 3 : w->opal;
  for(int p3 = max3; p3 >= 0; p3--)
  { int rem3 = target - 3 * p3;
    int max2 = rem3 / 2 < w->aureus ? rem3 / 2 : w->aureus;
    for(int p2 = max2; p2 >= 0; p2--)
    { int p1 = rem3 - 2 * p2;
      if(p1 <= w->luna)
      { *out = (CoinSet)
        { .luna = (uint8_t)p1, .aureus = (uint8_t)p2, .opal = (uint8_t)p3
        };
        return true;
      }
    }
  }
  return false;
} // coin_find_payment

CoinSet coin_select_payment(const CoinSet* w, uint16_t cost, uint8_t* overpay)
{ CoinSet p = { 0 };
  for(uint8_t ov = 0; ov <= COIN_MAX_OVERPAY; ov++)
    if(coin_find_payment(w, cost + ov, &p))
    { *overpay = ov;
      return p;
    }
  assert(false && "coin_select_payment: unreachable for cost <= value <= 9");
  *overpay = 0;
  return p;
} // coin_select_payment

void coin_apply_payment(CoinSet* w, const CoinSet* p, uint8_t overpay)
{ w->luna = (uint8_t)(w->luna - p->luna + overpay);
  w->aureus = (uint8_t)(w->aureus - p->aureus);
  w->opal = (uint8_t)(w->opal - p->opal);
} // coin_apply_payment
