## Sort for Display Only

If you want sorted display:

```
void display_player_hand(PlayerID player, struct gamestate* gstate, 
                        config_t* cfg) {
    // Create temporary sorted view for display
    uint8_t display_order[15];
    memcpy(display_order, gstate->hand[player].cards, 
           gstate->hand[player].size);


    // Sort by whatever makes sense for display
    qsort(display_order, size, sizeof(uint8_t), compare_by_power);

    // Display in sorted order
    for (uint8_t i = 0; i < size; i++) {
        display_card(display_order[i], i+1);  // User sees position 1-based
    }

}
```

**Benefits**:

- Internal array stays unsorted (fast operations)
- User sees logical ordering
- Sorting cost: ~50 cycles once per display (acceptable)
