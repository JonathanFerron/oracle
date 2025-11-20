

## Optimization Bonus: Unordered Remove

In hand/discard, you can make removal O(1):

```c
bool Hand_remove_unordered(Hand* hand, uint8_t card) {
    for (uint8_t i = 0; i < hand->size; i++) {
        if (hand->cards[i] == card) {
            // Swap with last element instead of shifting!
            hand->cards[i] = hand->cards[hand->size - 1];
            hand->size--;
            return true;
        }
    }
    return false;
}
```






