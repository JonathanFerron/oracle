#include <stdint.h>
#include <stdio.h>

#include "deckstack.h"

// Function to check if the stack is empty
bool DeckStk_isEmpty(struct deck_stack *deck)
{ // If top is -1, the stack is empty
  return deck->top == -1;
}

// Function to check if the stack is full
bool DeckStk_isFull(struct deck_stack *deck)
{ // If top is MAX_SIZE - 1, the stack is full
  return deck->top == MAX_DECK_STACK_SIZE - 1;
}

// Function to push an element onto the stack
void DeckStk_push(struct deck_stack *deck, uint8_t value)
{ // Check for stack overflow
  if(DeckStk_isFull(deck))
  { printf("Deck Stack Overflow\n");
    return;
  }
  // Increment top and add the value to the top of the stack
  deck->card_indices[++deck->top] = value;
  //printf("Pushed %d onto the deck stack\n", value);
}

// Function to pop an element from the stack
uint8_t DeckStk_pop(struct deck_stack *deck)
{ // Check for stack underflow
  if(DeckStk_isEmpty(deck))
  { printf("Deck Stack Underflow\n");
    return -1;
  }
  // Return the top element
  uint8_t popped = deck->card_indices[deck->top];
  // decrement top pointer
  deck->top--;
  //printf("Popped %d from the deck stack\n", popped);
  // return the popped element
  return popped;
}

// empty out the entire deck and free heap memory
void DeckStk_emptyOut(struct deck_stack *deck)
{ deck->top = -1;
}

// Function to peek the top element of the stack
uint8_t DeckStk_peek(struct deck_stack *deck)
{ // Check if the stack is empty
  if(DeckStk_isEmpty(deck))
  { printf("Deck Stack is empty\n");
    return -1;
  }
  // Return the top element without removing it
  return deck->card_indices[deck->top];
}

void DeckStk_print(struct deck_stack *deck)
{ printf("(");
  for(int i = deck->top; i >= 0; i--)
    printf("%u, ", deck->card_indices[i]);
  printf(")");
}
