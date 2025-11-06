Approach for displaying detailed combat information across different interfaces:

## General Design Principles

1. **Layer the information** - Show progressively more detail as needed
2. **Use consistent terminology** across CLI/TUI/GUI
3. **Color-code** by player and outcome
4. **Animate in GUI/TUI** for better understanding
5. **Make it skippable** for experienced players

## Recommended Approach for Each Interface

### CLI (Command Line Interface)

```c
// Add to combat.c or create combat_display.c
void display_combat_details_cli(struct gamestate* gstate, 
                                int16_t total_attack, 
                                int16_t total_defense,
                                CombatDetails* details)
{
    PlayerID attacker = gstate->current_player;
    PlayerID defender = 1 - attacker;

    printf("\n" BOLD_WHITE "╔══════════════════════════════════════╗\n");
    printf("║         COMBAT RESOLUTION            ║\n");
    printf("╚══════════════════════════════════════╝" RESET "\n\n");

    // Attacker's champions
    printf(COLOR_P1 "ATTACKER (%s):\n" RESET, PLAYER_NAMES[attacker]);
    for(int i = 0; i < details->num_attackers; i++) {
        printf("  • %s: D%d", 
               CHAMPION_SPECIES_NAMES[details->attacker_cards[i].species],
               details->attacker_dice[i]);
        printf(" rolled [%s%d" RESET "]", 
               details->attacker_rolls[i] >= details->attacker_dice[i]/2 ? GREEN : YELLOW,
               details->attacker_rolls[i]);
        printf(" + %d = %s%d" RESET "\n", 
               details->attacker_base[i],
               CYAN,
               details->attacker_total[i]);
    }

    if(details->attack_combo > 0)
        printf("  " GREEN "★ Combo Bonus: +%d" RESET "\n", details->attack_combo);

    printf("  " BOLD_WHITE "─────────────────────\n");
    printf("  TOTAL ATTACK: %s%d" RESET "\n\n", 
           BOLD_WHITE, total_attack);

    // Defender's champions (if any)
    if(details->num_defenders > 0) {
        printf(COLOR_P2 "DEFENDER (%s):\n" RESET, PLAYER_NAMES[defender]);
        for(int i = 0; i < details->num_defenders; i++) {
            printf("  • %s: D%d", 
                   CHAMPION_SPECIES_NAMES[details->defender_cards[i].species],
                   details->defender_dice[i]);
            printf(" rolled [%s%d" RESET "] = %s%d" RESET "\n",
                   details->defender_rolls[i] >= details->defender_dice[i]/2 ? GREEN : YELLOW,
                   details->defender_rolls[i],
                   CYAN,
                   details->defender_total[i]);
        }

        if(details->defense_combo > 0)
            printf("  " GREEN "★ Combo Bonus: +%d" RESET "\n", details->defense_combo);

        printf("  " BOLD_WHITE "─────────────────────\n");
        printf("  TOTAL DEFENSE: %s%d" RESET "\n\n", 
               BOLD_WHITE, total_defense);
    } else {
        printf(COLOR_P2 "DEFENDER: No defense\n\n" RESET);
    }

    // Combat result
    int damage = (total_attack > total_defense) ? (total_attack - total_defense) : 0;

    printf(BOLD_WHITE "═══════════════════════════════════════\n" RESET);
    if(damage > 0) {
        printf(RED "💥 DAMAGE: %d" RESET " (%d - %d)\n", 
               damage, total_attack, total_defense);
        printf(COLOR_P2 "%s" RESET ": " COLOR_ENERGY "%d" RESET " → " 
               COLOR_ENERGY "%d ❤\n" RESET,
               PLAYER_NAMES[defender],
               details->defender_energy_before,
               gstate->current_energy[defender]);
    } else {
        printf(GREEN "🛡 Attack blocked! No damage.\n" RESET);
    }
    printf(BOLD_WHITE "═══════════════════════════════════════\n\n" RESET);

    // Pause for user to read
    printf("Press Enter to continue...");
    getchar();
}
```

### TUI (Text UI with ncurses)

For ncurses-based TUI, create a more visual display:

```c
// In future stda_tui.c
void display_combat_details_tui(WINDOW* combat_win, 
                                struct gamestate* gstate,
                                CombatDetails* details)
{
    werase(combat_win);
    box(combat_win, 0, 0);

    // Title
    wattron(combat_win, A_BOLD | COLOR_PAIR(COLOR_HEADER));
    mvwprintw(combat_win, 1, 2, "⚔  COMBAT RESOLUTION  ⚔");
    wattroff(combat_win, A_BOLD | COLOR_PAIR(COLOR_HEADER));

    int row = 3;

    // Attacker section
    wattron(combat_win, COLOR_PAIR(COLOR_PLAYER_A));
    mvwprintw(combat_win, row++, 2, "ATTACKER:");
    wattroff(combat_win, COLOR_PAIR(COLOR_PLAYER_A));

    // Draw champion cards with animation (optional)
    for(int i = 0; i < details->num_attackers; i++) {
        draw_combat_card_tui(combat_win, row, 4, &details->attacker_cards[i],
                            details->attacker_rolls[i], 
                            details->attacker_total[i],
                            COLOR_PLAYER_A);
        row += 3;
    }

    // Show combo and total
    if(details->attack_combo > 0) {
        wattron(combat_win, COLOR_PAIR(COLOR_COMBO) | A_BOLD);
        mvwprintw(combat_win, row++, 4, "★ Combo: +%d", details->attack_combo);
        wattroff(combat_win, COLOR_PAIR(COLOR_COMBO) | A_BOLD);
    }

    wattron(combat_win, A_BOLD);
    mvwprintw(combat_win, row++, 4, "TOTAL: %d", details->total_attack);
    wattroff(combat_win, A_BOLD);

    // VS divider with animation
    row++;
    mvwprintw(combat_win, row++, getmaxx(combat_win)/2 - 3, "═══ VS ═══");
    row++;

    // Similar for defender...

    // Final result with color
    if(details->damage > 0) {
        wattron(combat_win, COLOR_PAIR(COLOR_DAMAGE) | A_BOLD);
        mvwprintw(combat_win, row++, 2, "💥 %d DAMAGE DEALT!", details->damage);
        wattroff(combat_win, COLOR_PAIR(COLOR_DAMAGE) | A_BOLD);
    }

    wrefresh(combat_win);

    // Wait for user input
    mvwprintw(combat_win, getmaxy(combat_win)-2, 2, "Press any key...");
    wgetch(combat_win);
}
```

### GUI (Future implementation)

For a graphical interface, consider:

```c
// Pseudo-code for GUI combat display
void display_combat_details_gui(GameWindow* win, CombatDetails* details)
{
    // 1. Fade in combat overlay
    fade_in_overlay(win->combat_overlay, 300ms);

    // 2. Slide in attacker's cards from left
    for(int i = 0; i < details->num_attackers; i++) {
        slide_in_card(details->attacker_cards[i], FROM_LEFT, 200ms);
        delay(100ms);
    }

    // 3. Animate dice rolls
    for(int i = 0; i < details->num_attackers; i++) {
        animate_dice_roll(details->attacker_dice[i], 
                         details->attacker_rolls[i], 800ms);
    }

    // 4. Show attack total with particle effect
    show_total_with_particles(details->total_attack, BLUE_PARTICLES);

    // 5. Slide in defender's cards from right (if any)
    // Similar animation...

    // 6. Collision animation
    if(details->num_defenders > 0) {
        animate_collision(details->total_attack, details->total_defense);
    } else {
        animate_direct_hit();
    }

    // 7. Show damage with screen shake
    if(details->damage > 0) {
        screen_shake(intensity: details->damage / 10);
        show_damage_number(details->damage, RED);
        animate_health_bar_decrease(defender, 
                                    details->defender_energy_before,
                                    details->defender_energy_after,
                                    1000ms);
    }

    // 8. Wait for user click or auto-advance after 3s
    wait_for_user_or_timeout(3000ms);
}
```

## Recommended Data Structure

Add to `combat.h`:

```c
// Combat details structure for display
typedef struct {
    // Attacker info
    int num_attackers;
    CombatCard attacker_cards[3];
    uint8_t attacker_dice[3];      // Die size (4, 6, 8, etc.)
    uint8_t attacker_rolls[3];     // Actual roll results
    uint8_t attacker_base[3];      // Base attack values
    int16_t attacker_total[3];     // Die roll + base
    int16_t attack_combo;          // Combo bonus
    int16_t total_attack;          // Final total

    // Defender info
    int num_defenders;
    CombatCard defender_cards[3];
    uint8_t defender_dice[3];
    uint8_t defender_rolls[3];
    int16_t defender_total[3];
    int16_t defense_combo;
    int16_t total_defense;

    // Result
    int16_t damage;
    uint8_t defender_energy_before;
    uint8_t defender_energy_after;
} CombatDetails;
```

## Modified Combat Functions

Update `combat.c` to collect this information:

```c
void resolve_combat_with_details(struct gamestate* gstate, 
                                 CombatDetails* details, 
                                 GameContext* ctx)
{
    memset(details, 0, sizeof(CombatDetails));

    PlayerID attacker = gstate->current_player;
    PlayerID defender = 1 - attacker;

    // Collect attacker details
    details->num_attackers = gstate->combat_zone[attacker].size;
    struct LLNode* current = gstate->combat_zone[attacker].head;

    for(int i = 0; i < details->num_attackers; i++) {
        uint8_t card_idx = current->data;
        details->attacker_cards[i].species = fullDeck[card_idx].species;
        details->attacker_cards[i].color = fullDeck[card_idx].color;
        details->attacker_cards[i].order = fullDeck[card_idx].order;

        details->attacker_dice[i] = fullDeck[card_idx].defense_dice;
        details->attacker_rolls[i] = RND_dn(details->attacker_dice[i], ctx);
        details->attacker_base[i] = fullDeck[card_idx].attack_base;
        details->attacker_total[i] = details->attacker_rolls[i] + 
                                     details->attacker_base[i];

        current = current->next;
    }

    // Calculate attack with combo
    int16_t base_attack = 0;
    for(int i = 0; i < details->num_attackers; i++)
        base_attack += details->attacker_total[i];

    details->attack_combo = calculate_combo_bonus(details->attacker_cards,
                                                  details->num_attackers,
                                                  DECK_RANDOM);
    details->total_attack = base_attack + details->attack_combo;

    // Similar for defender...

    // Calculate damage
    details->defender_energy_before = gstate->current_energy[defender];
    apply_combat_damage(gstate, details->total_attack, 
                       details->total_defense, ctx);
    details->defender_energy_after = gstate->current_energy[defender];
    details->damage = details->defender_energy_before - 
                     details->defender_energy_after;

    // Clear zones
    clear_combat_zones(gstate, ctx);
}
```

## Usage in Game Modes

```c
// In stda_cli.c
if(gstate->combat_zone[gstate->current_player].size > 0) {
    CombatDetails details;
    resolve_combat_with_details(gstate, &details, ctx);

    if(ctx->config->verbose || is_interactive_mode(ctx->config->mode))
        display_combat_details_cli(gstate, details.total_attack,
                                   details.total_defense, &details);
}
```

This approach:

- ✅ Keeps functions under 30 lines (display functions can be longer as they're mostly formatting)
- ✅ Separates data collection from display
- ✅ Allows different display methods for CLI/TUI/GUI
- ✅ Maintains compatibility with your existing code structure
- ✅ Supports verbose/quiet modes via config
- ✅ Provides all necessary combat details for player understanding

Would you like me to elaborate on any specific interface implementation?

You're absolutely right! They're redundant since `CombatDetails` already contains `total_attack` and `total_defense` fields. Here's the corrected function signature and usage:

```c
// Corrected function signature
void display_combat_details_cli(struct gamestate* gstate, 
                                CombatDetails* details)
{
    PlayerID attacker = gstate->current_player;
    PlayerID defender = 1 - attacker;

    printf("\n" BOLD_WHITE "╔══════════════════════════════════════╗\n");
    printf("║         COMBAT RESOLUTION            ║\n");
    printf("╚══════════════════════════════════════╝" RESET "\n\n");

    // Attacker's champions
    printf(COLOR_P1 "ATTACKER (%s):\n" RESET, PLAYER_NAMES[attacker]);
    for(int i = 0; i < details->num_attackers; i++) {
        printf("  • %s: D%d", 
               CHAMPION_SPECIES_NAMES[details->attacker_cards[i].species],
               details->attacker_dice[i]);
        printf(" rolled [%s%d" RESET "]", 
               details->attacker_rolls[i] >= details->attacker_dice[i]/2 ? GREEN : YELLOW,
               details->attacker_rolls[i]);
        printf(" + %d = %s%d" RESET "\n", 
               details->attacker_base[i],
               CYAN,
               details->attacker_total[i]);
    }

    if(details->attack_combo > 0)
        printf("  " GREEN "★ Combo Bonus: +%d" RESET "\n", details->attack_combo);

    printf("  " BOLD_WHITE "─────────────────────\n");
    printf("  TOTAL ATTACK: %s%d" RESET "\n\n", 
           BOLD_WHITE, details->total_attack);  // Use details->total_attack

    // Defender's champions (if any)
    if(details->num_defenders > 0) {
        printf(COLOR_P2 "DEFENDER (%s):\n" RESET, PLAYER_NAMES[defender]);
        for(int i = 0; i < details->num_defenders; i++) {
            printf("  • %s: D%d", 
                   CHAMPION_SPECIES_NAMES[details->defender_cards[i].species],
                   details->defender_dice[i]);
            printf(" rolled [%s%d" RESET "] = %s%d" RESET "\n",
                   details->defender_rolls[i] >= details->defender_dice[i]/2 ? GREEN : YELLOW,
                   details->defender_rolls[i],
                   CYAN,
                   details->defender_total[i]);
        }

        if(details->defense_combo > 0)
            printf("  " GREEN "★ Combo Bonus: +%d" RESET "\n", details->defense_combo);

        printf("  " BOLD_WHITE "─────────────────────\n");
        printf("  TOTAL DEFENSE: %s%d" RESET "\n\n", 
               BOLD_WHITE, details->total_defense);  // Use details->total_defense
    } else {
        printf(COLOR_P2 "DEFENDER: No defense\n\n" RESET);
    }

    // Combat result - use details->damage directly
    printf(BOLD_WHITE "═══════════════════════════════════════\n" RESET);
    if(details->damage > 0) {
        printf(RED "💥 DAMAGE: %d" RESET " (%d - %d)\n", 
               details->damage, details->total_attack, details->total_defense);
        printf(COLOR_P2 "%s" RESET ": " COLOR_ENERGY "%d" RESET " → " 
               COLOR_ENERGY "%d ❤\n" RESET,
               PLAYER_NAMES[defender],
               details->defender_energy_before,
               details->defender_energy_after);
    } else {
        printf(GREEN "🛡 Attack blocked! No damage.\n" RESET);
    }
    printf(BOLD_WHITE "═══════════════════════════════════════\n\n" RESET);

    // Pause for user to read
    printf("Press Enter to continue...");
    getchar();
}
```

And the corrected usage:

```c
// In stda_cli.c
if(gstate->combat_zone[gstate->current_player].size > 0) {
    CombatDetails details;
    resolve_combat_with_details(gstate, &details, ctx);

    if(ctx->config->verbose || is_interactive_mode(ctx->config->mode))
        display_combat_details_cli(gstate, &details);  // Only 2 parameters needed
}
```

Much cleaner! The `CombatDetails` struct is the single source of truth for all combat information.
