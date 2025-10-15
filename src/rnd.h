#ifndef __RND_H
#define __RND_H

uint8_t RND_randn(uint8_t);
uint8_t RND_dn(uint8_t);
void RND_swap(uint8_t*, uint8_t*);
void RND_partial_shuffle(uint8_t A[], uint8_t, uint8_t);

#endif /* #ifndef __RND_H */
