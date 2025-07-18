#ifndef __DECKSTACK_H
#define __DECKSTACK_H

#define MAX_DECK_STACK_SIZE 39

struct deck_stack {
  uint8_t card_indices[MAX_DECK_STACK_SIZE];
  int8_t top;
};

void DeckStk_push(struct deck_stack *, uint8_t ) ;
uint8_t DeckStk_pop(struct deck_stack *) ;
bool DeckStk_isEmpty(struct deck_stack *);
void DeckStk_emptyOut(struct deck_stack *);
void DeckStk_print(struct deck_stack *deck);

#endif /* #ifndef __DECKSTACK_H */
