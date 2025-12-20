#ifndef __RND_H
#define __RND_H

#include "../core/game_context.h"

uint8_t RND_randn(uint8_t n, GameContext* ctx);
uint8_t RND_dn(uint8_t n, GameContext* ctx);
void RND_swap(uint8_t*, uint8_t*);
void RND_partial_shuffle(uint8_t A[], uint8_t n, uint8_t k, GameContext* ctx);

#endif
