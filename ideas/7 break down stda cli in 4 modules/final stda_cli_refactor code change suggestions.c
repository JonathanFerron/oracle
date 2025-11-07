// ============================================================================
// FILE 1: ui/cli/cli_callbacks.c (~180 lines)
// Purpose: Bridge between game engine events and CLI display
// ============================================================================

#include "cli_callbacks.h"
#include "cli_display.h"
#include "core/game_engine.h"

typedef struct {
    config_t* cfg;
    bool verbose_mode;
} CLICallbackContext;

UICallbacks* cli_create_callbacks(config_t* cfg) {
    UICallbacks* callbacks = malloc(sizeof(UICallbacks));
    
    CLICallbackContext* ctx = malloc(sizeof(CLICallbackContext));
    ctx->cfg = cfg;
    ctx->verbose_mode = true;
    
    callbacks->on_card_drawn = cli_on_card_drawn;
    callbacks->on_card_played = cli_on_card_played;
    callbacks->on_combat_resolved = cli_on_combat_resolved;
    callbacks->on_phase_changed = cli_on_phase_changed;
    callbacks->on_discard_complete = cli_on_discard_complete;
    callbacks->on_energy_changed = cli_on_energy_changed;
    callbacks->ui_context = ctx;
    
    return callbacks;
}

void cli_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx) {
    CLICallbackContext* ctx = (CLICallbackContext*)ui_ctx;
    if (!ctx->verbose_mode) return;
    
    PlayerConfig* pconfig = (PlayerConfig*)ctx->cfg->player_config;
    const char* player_name = pconfig->player_names[player];
    const struct card* card = &fullDeck[card_id];
    
    cli_display_card_drawn(player_name, card);
}

void cli_on_card_played(PlayerID player, uint8_t card_id,
                        ActionType action_type, void* ui_ctx) {
    CLICallbackContext* ctx = (CLICallbackContext*)ui_ctx;
    if (!ctx->verbose_mode) return;
    
    PlayerConfig* pconfig = (PlayerConfig*)ctx->cfg->player_config;
    const char* player_name = pconfig->player_names[player];
    const struct card* card = &fullDeck[card_id];
    
    cli_display_card_played(player_name, card, action_type);
}

void cli_on_combat_resolved(const CombatResult* result, void* ui_ctx) {
    CLICallbackContext* ctx = (CLICallbackContext*)ui_ctx;
    cli_display_combat_resolution(result, ctx->cfg);
}

void cli_on_phase_changed(GamePhase old_phase, GamePhase new_phase,
                          void* ui_ctx) {
    CLICallbackContext* ctx = (CLICallbackContext*)ui_ctx;
    
    // Only display significant phase transitions
    if (new_phase == PHASE_BEGIN_TURN || 
        new_phase == PHASE_ATTACK_REQUEST ||
        new_phase == PHASE_DEFENSE_REQUEST) {
        cli_display_phase_transition(old_phase, new_phase);
    }
}

void cli_on_discard_complete(PlayerID player, uint8_t num_cards,
                             void* ui_ctx) {
    CLICallbackContext* ctx = (CLICallbackContext*)ui_ctx;
    
    PlayerConfig* pconfig = (PlayerConfig*)ctx->cfg->player_config;
    const char* player_name = pconfig->player_names[player];
    
    cli_display_discard_complete(player_name, num_cards);
}

void cli_on_energy_changed(PlayerID player, int16_t old_energy,
                           int16_t new_energy, void* ui_ctx) {
    CLICallbackContext* ctx = (CLICallbackContext*)ui_ctx;
    
    int16_t damage = old_energy - new_energy;
    if (damage > 0 && ctx->verbose_mode) {
        PlayerConfig* pconfig = (PlayerConfig*)ctx->cfg->player_config;
        const char* player_name = pconfig->player_names[player];
        cli_display_damage_taken(player_name, damage, new_energy);
    }
}

void cli_destroy_callbacks(UICallbacks* callbacks) {
    if (callbacks) {
        free(callbacks->ui_context);
        free(callbacks);
    }
}


// ============================================================================
// FILE 2: ui/cli/cli_display.c (~340 lines)
// Purpose: Pure presentation - format and print game information
// ============================================================================

#include "cli_display.h"
#include "game_types.h"
#include "game_constants.h"
#include "localization.h"

/* ANSI color codes */
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define GRAY    "\033[38;2;128;128;128m"
#define BOLD_WHITE   "\033[1;37m"
#define COLOR_P1     "\033[1;36m"
#define COLOR_P2     "\033[1;33m"
#define COLOR_ENERGY MAGENTA
#define COLOR_LUNA   CYAN

#define ICON_PROMPT  ">"
#define ICON_SUCCESS "[OK]"

void cli_display_player_status(PlayerID player, const GameState* gs,
                               const char* player_name, bool is_defense) {
    const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;
    const char* position = (player == PLAYER_A) ? "A" : "B";
    
    const char* phase_icon = is_defense ? 
        LOCALIZED_STRING("[DEF]", "[DEF]", "[DEF]") : 
        LOCALIZED_STRING("[ATK]", "[ATQ]", "[ATQ]");

    printf("%s%s (%s)" RESET " [" COLOR_ENERGY "HP:%d" RESET " "
           COLOR_LUNA "L:%d" RESET "] %s " ICON_PROMPT " ",
           player_color, player_name, position,
           gs->current_energy[player],
           gs->current_cash_balance[player],
           phase_icon);
}

void cli_display_hand(PlayerID player, const GameState* gs) {
    printf("\n%s\n", LOCALIZED_STRING("Your hand:", 
                                      "Votre main:", 
                                      "Tu mano:"));
    
    uint8_t* hand_array = HDCLL_toArray(&gs->hand[player]);

    for (uint8_t i = 0; i < gs->hand[player].size; i++) {
        uint8_t card_idx = hand_array[i];
        const struct card* c = &fullDeck[card_idx];

        if (c->card_type == CHAMPION_CARD) {
            const char* color = (c->color == COLOR_INDIGO) ? BLUE :
                              (c->color == COLOR_ORANGE) ? YELLOW : RED;
            printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "L%d" RESET ")\n",
                   i + 1, color, CHAMPION_SPECIES_NAMES[c->species],
                   c->defense_dice, c->attack_base, c->cost);
        }
        else if (c->card_type == DRAW_CARD) {
            printf("  [%d] " GREEN "%s %d" RESET " (" CYAN "L%d" RESET ")\n",
                   i + 1, 
                   LOCALIZED_STRING("Draw", "Piocher", "Robar"),
                   c->draw_num, c->cost);
        }
        else if (c->card_type == CASH_CARD) {
            printf("  [%d] " GRAY "%s %d %s" RESET 
                   " (" CYAN "L%d" RESET ")\n",
                   i + 1,
                   LOCALIZED_STRING("Exchange for", 
                                   "Echanger pour",
                                   "Cambiar por"),
                   c->exchange_cash,
                   LOCALIZED_STRING("lunas", "lunas", "lunas"),
                   c->cost);
        }
    }
    free(hand_array);
}

void cli_display_combat_zone(const GameState* gs, config_t* cfg) {
    printf("\n" RED "=== %s ===" RESET "\n",
           LOCALIZED_STRING("Combat! You are being attacked",
                           "Combat! Vous etes attaque",
                           "Combate! Estas siendo atacado"));
    printf("%s\n", 
           LOCALIZED_STRING("Attacker's champions in combat:",
                           "Champions de l'attaquant au combat:",
                           "Campeones del atacante en combate:"));
    
    PlayerID attacker = gs->current_player;
    struct LLNode* current = gs->combat_zone[attacker].head;

    for (uint8_t i = 0; i < gs->combat_zone[attacker].size; i++) {
        const struct card* c = &fullDeck[current->data];
        printf("  - %s (D%d+%d)\n", 
               CHAMPION_SPECIES_NAMES[c->species],
               c->defense_dice, c->attack_base);
        current = current->next;
    }
}

void cli_display_game_status(const GameState* gs, config_t* cfg) {
    printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
           LOCALIZED_STRING("Game Status", 
                           "Statut du jeu",
                           "Estado del juego"));
    
    PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
    
    printf(COLOR_P1 "%s (A)" RESET ": " COLOR_ENERGY "HP:%d" RESET
           " " COLOR_LUNA "L:%d" RESET " %s:%d %s:%d\n",
           pconfig->player_names[PLAYER_A],
           gs->current_energy[PLAYER_A],
           gs->current_cash_balance[PLAYER_A],
           LOCALIZED_STRING("Hand", "Main", "Mano"),
           gs->hand[PLAYER_A].size,
           LOCALIZED_STRING("Deck", "Paquet", "Mazo"),
           gs->deck[PLAYER_A].top + 1);
    
    printf(COLOR_P2 "%s (B)" RESET ": " COLOR_ENERGY "HP:%d" RESET
           " " COLOR_LUNA "L:%d" RESET " %s:%d %s:%d\n",
           pconfig->player_names[PLAYER_B],
           gs->current_energy[PLAYER_B],
           gs->current_cash_balance[PLAYER_B],
           LOCALIZED_STRING("Hand", "Main", "Mano"),
           gs->hand[PLAYER_B].size,
           LOCALIZED_STRING("Deck", "Paquet", "Mazo"),
           gs->deck[PLAYER_B].top + 1);
}

void cli_display_available_commands(GamePhase phase, config_t* cfg) {
    printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
           LOCALIZED_STRING("Commands", "Commandes", "Comandos"));
    
    if (phase == PHASE_DEFENSE_REQUEST) {
        printf("  cham <indices>  - %s\n",
               LOCALIZED_STRING("Defend with 1-3 champions",
                               "Defendre avec 1-3 champions",
                               "Defender con 1-3 campeones"));
        printf("  pass            - %s\n",
               LOCALIZED_STRING("Take damage without defending",
                               "Prendre des degats sans defendre",
                               "Recibir dano sin defender"));
    }
    else if (phase == PHASE_ATTACK_REQUEST) {
        printf("  cham <indices>  - %s\n",
               LOCALIZED_STRING("Attack with 1-3 champions",
                               "Attaquer avec 1-3 champions",
                               "Atacar con 1-3 campeones"));
        printf("  draw <index>    - %s\n",
               LOCALIZED_STRING("Play draw/recall card",
                               "Jouer carte piocher/rappeler",
                               "Jugar carta robar/recuperar"));
        printf("  cash <index>    - %s\n",
               LOCALIZED_STRING("Play exchange card",
                               "Jouer carte echange",
                               "Jugar carta intercambio"));
        printf("  pass            - %s\n",
               LOCALIZED_STRING("Pass your turn",
                               "Passer votre tour",
                               "Pasar tu turno"));
        printf("  gmst            - %s\n",
               LOCALIZED_STRING("Show game status",
                               "Afficher statut",
                               "Mostrar estado"));
    }
    
    printf("  help            - %s\n",
           LOCALIZED_STRING("Show this help",
                           "Afficher cette aide",
                           "Mostrar esta ayuda"));
    printf("  exit            - %s\n\n",
           LOCALIZED_STRING("Quit game",
                           "Quitter le jeu",
                           "Salir del juego"));
}

void cli_display_card_drawn(const char* player_name, 
                            const struct card* card) {
    printf("\n" GREEN ICON_SUCCESS " %s %s: %s" RESET "\n",
           player_name,
           LOCALIZED_STRING("draws", "pioche", "roba"),
           (card->card_type == CHAMPION_CARD) ? 
               CHAMPION_SPECIES_NAMES[card->species] : "card");
}

void cli_display_card_played(const char* player_name,
                             const struct card* card,
                             ActionType action_type) {
    printf(GREEN ICON_SUCCESS " %s %s: %s" RESET,
           player_name,
           LOCALIZED_STRING("plays", "joue", "juega"),
           (card->card_type == CHAMPION_CARD) ?
               CHAMPION_SPECIES_NAMES[card->species] : "card");
    
    if (action_type == ACTION_DRAW_CARD) {
        printf(" (%s)\n", LOCALIZED_STRING("Draw", "Piocher", "Robar"));
    } else if (action_type == ACTION_EXCHANGE) {
        printf(" (%s)\n", LOCALIZED_STRING("Exchange", 
                                          "Echange", 
                                          "Intercambio"));
    } else {
        printf("\n");
    }
}

void cli_display_combat_resolution(const CombatResult* result,
                                   config_t* cfg) {
    printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
           LOCALIZED_STRING("COMBAT RESOLUTION",
                           "RESOLUTION DU COMBAT",
                           "RESOLUCION DE COMBATE"));
    
    printf("%s: %d", LOCALIZED_STRING("Attack Total",
                                      "Total Attaque",
                                      "Total Ataque"),
           result->attack_base);
    for (int i = 0; i < result->attack_roll_count; i++) {
        printf(" + %d", result->attack_rolls[i]);
    }
    printf(" = " RED "%d" RESET "\n", result->total_attack);
    
    printf("%s: %d", LOCALIZED_STRING("Defense Total",
                                      "Total Defense",
                                      "Total Defensa"),
           result->defense_base);
    for (int i = 0; i < result->defense_roll_count; i++) {
        printf(" + %d", result->defense_rolls[i]);
    }
    printf(" = " BLUE "%d" RESET "\n", result->total_defense);
    
    int damage = result->total_attack - result->total_defense;
    if (damage > 0) {
        printf(MAGENTA "%s: %d" RESET "\n",
               LOCALIZED_STRING("Damage", "Degats", "Dano"),
               damage);
    } else {
        printf(GREEN "%s!" RESET "\n",
               LOCALIZED_STRING("Attack blocked",
                               "Attaque bloquee",
                               "Ataque bloqueado"));
    }
    printf("========================\n");
}

void cli_display_phase_transition(GamePhase old_phase, 
                                  GamePhase new_phase) {
    if (new_phase == PHASE_BEGIN_TURN) {
        printf("\n" BOLD_WHITE "--- %s ---" RESET "\n",
               LOCALIZED_STRING("NEW TURN", "NOUVEAU TOUR", "NUEVO TURNO"));
    } else if (new_phase == PHASE_ATTACK_REQUEST) {
        printf("\n" YELLOW "[%s]" RESET "\n",
               LOCALIZED_STRING("ATTACK PHASE",
                               "PHASE D'ATTAQUE",
                               "FASE DE ATAQUE"));
    } else if (new_phase == PHASE_DEFENSE_REQUEST) {
        printf("\n" BLUE "[%s]" RESET "\n",
               LOCALIZED_STRING("DEFENSE PHASE",
                               "PHASE DE DEFENSE",
                               "FASE DE DEFENSA"));
    }
}

void cli_display_discard_complete(const char* player_name,
                                  uint8_t num_cards) {
    printf(YELLOW ICON_SUCCESS " %s %s %d %s." RESET "\n",
           player_name,
           LOCALIZED_STRING("discarded", "defausse", "descarto"),
           num_cards,
           LOCALIZED_STRING("card(s)", "carte(s)", "carta(s)"));
}

void cli_display_damage_taken(const char* player_name, int16_t damage,
                              int16_t remaining_energy) {
    printf(RED "! %s %s %d %s (%s: %d)" RESET "\n",
           player_name,
           LOCALIZED_STRING("takes", "prend", "recibe"),
           damage,
           LOCALIZED_STRING("damage", "degats", "dano"),
           LOCALIZED_STRING("Remaining", "Restant", "Restante"),
           remaining_energy);
}

void cli_display_game_over(const GameState* gs, config_t* cfg) {
    printf("\n\n" BOLD_WHITE "=================================" RESET "\n");
    printf(BOLD_WHITE "        %s" RESET "\n",
           LOCALIZED_STRING("GAME OVER", "FIN DU JEU", "JUEGO TERMINADO"));
    printf(BOLD_WHITE "=================================" RESET "\n");
    
    PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
    const char* winner_name = NULL;
    
    if (gs->game_state == PLAYER_A_WINS) {
        winner_name = pconfig->player_names[PLAYER_A];
        printf(GREEN "%s: %s!" RESET "\n",
               LOCALIZED_STRING("Winner", "Gagnant", "Ganador"),
               winner_name);
    } else if (gs->game_state == PLAYER_B_WINS) {
        winner_name = pconfig->player_names[PLAYER_B];
        printf(GREEN "%s: %s!" RESET "\n",
               LOCALIZED_STRING("Winner", "Gagnant", "Ganador"),
               winner_name);
    } else if (gs->game_state == DRAW) {
        printf(YELLOW "%s" RESET "\n",
               LOCALIZED_STRING("Game ended in a draw",
                               "Partie terminee par un match nul",
                               "Juego termino en empate"));
    }
    
    printf("\n%s:\n",
           LOCALIZED_STRING("Final Status", "Statut final", "Estado final"));
    
    for (int i = 0; i < 2; i++) {
        PlayerID pid = (PlayerID)i;
        const char* name = pconfig->player_names[pid];
        const char* pos = (pid == PLAYER_A) ? "A" : "B";
        const char* color = (pid == PLAYER_A) ? COLOR_P1 : COLOR_P2;
        
        printf("  %s%s (%s)" RESET ": " COLOR_ENERGY "HP:%d" RESET
               " " COLOR_LUNA "L:%d" RESET " %s:%d\n",
               color, name, pos,
               gs->current_energy[pid],
               gs->current_cash_balance[pid],
               LOCALIZED_STRING("Cards", "Cartes", "Cartas"),
               gs->hand[pid].size);
    }
    
    printf("\n%s: %d (%s: %d)\n",
           LOCALIZED_STRING("Total turns", "Tours totaux", "Turnos totales"),
           gs->turn,
           LOCALIZED_STRING("Rounds", "Manches", "Rondas"),
           (uint16_t)((gs->turn - 1) * 0.5 + 1));
    printf(BOLD_WHITE "=================================" RESET "\n\n");
}


// ============================================================================
// FILE 3: ui/cli/cli_input.c (~290 lines)
// Purpose: Parse and validate user input, produce validated actions
// ============================================================================

#include "cli_input.h"
#include "actions/action.h"
#include "actions/action_validator.h"
#include "cli_display.h"
#include "localization.h"

#define MAX_COMMAND_LEN 256
#define EXIT_SIGNAL -1
#define ACTION_TAKEN 1
#define NO_ACTION 0

typedef struct {
    char command[32];
    int indices[9];
    int count;
} ParsedInput;

static bool parse_input_line(const char* input, ParsedInput* parsed) {
    parsed->count = 0;
    
    char buffer[256];
    strncpy(buffer, input, 255);
    buffer[255] = '\0';
    
    // Remove newline
    buffer[strcspn(buffer, "\n")] = 0;
    
    char* token = strtok(buffer, " \t");
    if (!token) return false;
    
    strncpy(parsed->command, token, 31);
    parsed->command[31] = '\0';
    
    // Parse numeric arguments
    while ((token = strtok(NULL, " \t")) != NULL) {
        int idx = atoi(token);
        if (idx > 0 && parsed->count < 9) {
            parsed->indices[parsed->count++] = idx - 1;  // 0-based
        }
    }
    
    return true;
}

static bool validate_card_indices(const int* indices, int count,
                                  int hand_size, config_t* cfg) {
    if (count == 0 || count > 3) return false;
    
    for (int i = 0; i < count; i++) {
        if (indices[i] < 0 || indices[i] >= hand_size) {
            printf(RED "%s %d (%s 1-%d)\n" RESET,
                   LOCALIZED_STRING("Error: Invalid card number",
                                   "Erreur: Numero de carte invalide",
                                   "Error: Numero de carta invalido"),
                   indices[i] + 1,
                   LOCALIZED_STRING("must be", "doit etre", "debe ser"),
                   hand_size);
            return false;
        }
        
        // Check for duplicates
        for (int j = i + 1; j < count; j++) {
            if (indices[i] == indices[j]) {
                printf(RED "%s\n" RESET,
                       LOCALIZED_STRING("Error: Duplicate card selected",
                                       "Erreur: Carte en double",
                                       "Error: Carta duplicada"));
                return false;
            }
        }
    }
    
    return true;
}

static Action* create_champion_action(const GameState* gs, PlayerID player,
                                      const int* indices, int count) {
    uint8_t* hand_array = HDCLL_toArray(&gs->hand[player]);
    uint8_t card_ids[3];
    
    for (int i = 0; i < count; i++) {
        card_ids[i] = hand_array[indices[i]];
    }
    free(hand_array);
    
    return action_create_play_champions(player, card_ids, count);
}

static bool validate_champion_action(const Action* action,
                                     const GameState* gs,
                                     config_t* cfg) {
    if (!validate_action(action, gs)) {
        printf(RED "%s\n" RESET,
               LOCALIZED_STRING("Error: Invalid action",
                               "Erreur: Action invalide",
                               "Error: Accion invalida"));
        return false;
    }
    
    // Check affordability
    int total_cost = 0;
    for (int i = 0; i < action->data.play_champions.num_cards; i++) {
        uint8_t card_id = action->data.play_champions.card_ids[i];
        total_cost += fullDeck[card_id].cost;
    }
    
    if (total_cost > gs->current_cash_balance[action->player_id]) {
        printf(RED "%s (%s %d, %s %d)\n" RESET,
               LOCALIZED_STRING("Error: Not enough lunas",
                               "Erreur: Pas assez de lunas",
                               "Error: No hay suficientes lunas"),
               LOCALIZED_STRING("need", "besoin", "necesita"),
               total_cost,
               LOCALIZED_STRING("have", "avoir", "tienes"),
               gs->current_cash_balance[action->player_id]);
        return false;
    }
    
    return true;
}

Action* cli_get_attack_action(const GameState* gs, PlayerID player,
                              config_t* cfg) {
    char input_buffer[MAX_COMMAND_LEN];
    ParsedInput parsed;
    
    while (true) {
        // Display prompt
        cli_display_player_status(player, gs,
            ((PlayerConfig*)cfg->player_config)->player_names[player],
            false);
        
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
            printf("%s\n", LOCALIZED_STRING("Error reading input.",
                                          "Erreur de lecture.",
                                          "Error al leer entrada."));
            continue;
        }
        
        if (!parse_input_line(input_buffer, &parsed)) {
            continue;
        }
        
        // Handle commands
        if (strcmp(parsed.command, "pass") == 0) {
            return action_create_pass(player);
        }
        else if (strcmp(parsed.command, "gmst") == 0) {
            cli_display_game_status(gs, cfg);
            continue;
        }
        else if (strcmp(parsed.command, "help") == 0) {
            cli_display_available_commands(PHASE_ATTACK_REQUEST, cfg);
            continue;
        }
        else if (strcmp(parsed.command, "exit") == 0) {
            return action_create_exit(player);
        }
        else if (strcmp(parsed.command, "cham") == 0) {
            if (!validate_card_indices(parsed.indices, parsed.count,
                                      gs->hand[player].size, cfg)) {
                continue;
            }
            
            Action* action = create_champion_action(gs, player,
                                                    parsed.indices,
                                                    parsed.count);
            
            if (validate_champion_action(action, gs, cfg)) {
                return action;
            }
            
            free_action(action);
            continue;
        }
        else if (strcmp(parsed.command, "draw") == 0) {
            if (parsed.count != 1) {
                printf(RED "%s\n" RESET,
                       LOCALIZED_STRING("Usage: draw <index>",
                                       "Utilisation: draw <index>",
                                       "Uso: draw <index>"));
                continue;
            }
            
            uint8_t* hand_array = HDCLL_toArray(&gs->hand[player]);
            uint8_t card_id = hand_array[parsed.indices[0]];
            free(hand_array);
            
            Action* action = action_create_draw_card(player, card_id);
            
            if (validate_action(action, gs)) {
                return action;
            }
            
            printf(RED "%s\n" RESET,
                   LOCALIZED_STRING("Error: Cannot play that card",
                                   "Erreur: Impossible de jouer cette carte",
                                   "Error: No se puede jugar esa carta"));
            free_action(action);
            continue;
        }
        else if (strcmp(parsed.command, "cash") == 0) {
            if (parsed.count != 1) {
                printf(RED "%s\n" RESET,
                       LOCALIZED_STRING("Usage: cash <index>",
                                       "Utilisation: cash <index>",
                                       "Uso: cash <index>"));
                continue;
            }
            
            uint8_t* hand_array = HDCLL_toArray(&gs->hand[player]);
            uint8_t card_id = hand_array[parsed.indices[0]];
            free(hand_array);
            
            Action* action = action_create_exchange(player, card_id);
            
            if (validate_action(action, gs)) {
                return action;
            }
            
            printf(RED "%s\n" RESET,
                   LOCALIZED_STRING("Error: Cannot play that card",
                                   "Erreur: Impossible de jouer cette carte",
                                   "Error: No se puede jugar esa carta"));
            free_action(action);
            continue;
        }
        else {
            printf(RED "%s: %s\n" RESET,
                   LOCALIZED_STRING("Unknown command",
                                   "Commande inconnue",
                                   "Comando desconocido"),
                   parsed.command);
            printf("%s\n",
                   LOCALIZED_STRING("Type 'help' for available commands",
                                   "Tapez 'help' pour les commandes",
                                   "Escribe 'help' para comandos"));
            continue;
        }
    }
}

Action* cli_get_defense_action(const GameState* gs, PlayerID player,
                               config_t* cfg) {
    char input_buffer[MAX_COMMAND_LEN];
    ParsedInput parsed;
    
    // Display combat state
    cli_display_combat_zone(gs, cfg);
    
    while (true) {
        cli_display_player_status(player, gs,
            ((PlayerConfig*)cfg->player_config)->player_names[player],
            true);
        
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
            printf("%s\n", LOCALIZED_STRING("Error reading input.",
                                          "Erreur de lecture.",
                                          "Error al leer entrada."));
            continue;
        }
        
        if (!parse_input_line(input_buffer, &parsed)) {
            continue;
        }
        
        // Handle commands
        if (strcmp(parsed.command, "pass") == 0) {
            printf(YELLOW "%s\n" RESET,
                   LOCALIZED_STRING("Taking damage without defending",
                                   "Prendre des degats sans defendre",
                                   "Recibir dano sin defender"));
            return action_create_pass(player);
        }
        else if (strcmp(parsed.command, "help") == 0) {
            cli_display_available_commands(PHASE_DEFENSE_REQUEST, cfg);
            continue;
        }
        else if (strcmp(parsed.command, "exit") == 0) {
            return action_create_exit(player);
        }
        else if (strcmp(parsed.command, "cham") == 0) {
            if (!validate_card_indices(parsed.indices, parsed.count,
                                      gs->hand[player].size, cfg)) {
                continue;
            }
            
            Action* action = create_champion_action(gs, player,
                                                    parsed.indices,
                                                    parsed.count);
            
            if (validate_champion_action(action, gs, cfg)) {
                return action;
            }
            
            free_action(action);
            continue;
        }
        else {
            printf(RED "%s\n" RESET,
                   LOCALIZED_STRING("Unknown command. Use 'cham <indices>' or 'pass'",
                                   "Commande inconnue. Utilisez 'cham <indices>' ou 'pass'",
                                   "Comando desconocido. Usa 'cham <indices>' o 'pass'"));
            continue;
        }
    }
}


// ============================================================================
// FILE 4: roles/stda/stda_game.c (~380 lines)
// Purpose: Orchestrate CLI game flow using unified state machine
// ============================================================================

#include "stda_game.h"
#include "core/game_engine.h"
#include "ui/cli/cli_display.h"
#include "ui/cli/cli_input.h"
#include "ui/cli/cli_callbacks.h"
#include "ui/shared/player_selection.h"
#include "ui/shared/player_config.h"
#include "strategies/strategy.h"

#ifdef _WIN32
  #include <windows.h>
#endif

// Initialize game with CLI callbacks
static GameEngine* init_cli_game(config_t* cfg, UICallbacks** callbacks_out) {
    GameConfig game_cfg;
    game_cfg.initial_cash = INITIAL_CASH_DEFAULT;
    game_cfg.player_types[PLAYER_A] = cfg->player_types[PLAYER_A];
    game_cfg.player_types[PLAYER_B] = cfg->player_types[PLAYER_B];
    
    GameEngine* engine = engine_create(&game_cfg);
    
    // Create CLI callbacks
    UICallbacks* callbacks = cli_create_callbacks(cfg);
    
    // Register callbacks with engine
    engine_register_callbacks(engine, callbacks);
    
    *callbacks_out = callbacks;
    return engine;
}

// Display turn header
static void display_turn_header(const GameState* gs, PlayerID player,
                                config_t* cfg) {
    PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
    const char* player_name = pconfig->player_names[player];
    PlayerID opponent = 1 - player;
    const char* opponent_name = pconfig->player_names[opponent];
    
    printf("\n=== %s's %s (%s %d, %s %d) ===\n",
           player_name,
           LOCALIZED_STRING("Turn", "Tour", "Turno"),
           LOCALIZED_STRING("Turn", "Tour", "Turno"),
           gs->turn,
           LOCALIZED_STRING("Round", "Manche", "Ronda"),
           (uint16_t)((gs->turn - 1) * 0.5 + 1));
    
    printf("\n=== %s (%s) ===\n",
           opponent_name,
           LOCALIZED_STRING("Defender", "Defenseur", "Defensor"));
    printf("  %s:%d\n",
           LOCALIZED_STRING("Hand", "Main", "Mano"),
           gs->hand[opponent].size);
}

// Handle human player turn (blocking)
static Action* get_human_action(GameEngine* engine, PlayerID player,
                               config_t* cfg) {
    GameState* gs = engine_get_state(engine);
    GamePhase phase = engine_get_phase(engine);
    
    // Display current state
    if (phase == PHASE_ATTACK_REQUEST) {
        display_turn_header(gs, player, cfg);
        cli_display_hand(player, gs);
        return cli_get_attack_action(gs, player, cfg);
    }
    else if (phase == PHASE_DEFENSE_REQUEST) {
        cli_display_hand(player, gs);
        return cli_get_defense_action(gs, player, cfg);
    }
    else if (phase == PHASE_DISCARD_REQUEST) {
        // TODO: Implement discard selection
        return cli_get_discard_action(gs, player, cfg);
    }
    
    return NULL;
}

// Handle AI player turn (non-blocking)
static Action* get_ai_action(GameEngine* engine, PlayerID player,
                            StrategySet* strategies,
                            GameContext* ctx) {
    GameState* gs = engine_get_state(engine);
    GamePhase phase = engine_get_phase(engine);
    
    // Convert to visible state
    VisibleGameState vgs;
    gamestate_get_visible(gs, player, &vgs);
    
    // Get action from strategy
    if (phase == PHASE_ATTACK_REQUEST) {
        return strategies->attack_strategy[player](&vgs, ctx);
    }
    else if (phase == PHASE_DEFENSE_REQUEST) {
        return strategies->defense_strategy[player](&vgs, ctx);
    }
    else if (phase == PHASE_DISCARD_REQUEST) {
        return strategies->discard_strategy[player](&vgs, ctx);
    }
    
    return NULL;
}

// Main CLI game loop (game owns loop)
void stda_game_loop_cli(config_t* cfg, GameContext* ctx,
                        StrategySet* strategies) {
    UICallbacks* callbacks;
    GameEngine* engine = init_cli_game(cfg, &callbacks);
    
    bool exit_requested = false;
    
    while (engine_get_phase(engine) != PHASE_GAME_OVER && !exit_requested) {
        // Step engine until needs input
        engine_run_until_input(engine, ctx);
        
        if (engine_needs_input(engine)) {
            PlayerID active = engine_get_active_player(engine);
            Action* action = NULL;
            
            // Get action from human or AI
            if (cfg->player_types[active] == INTERACTIVE_PLAYER) {
                action = get_human_action(engine, active, cfg);
                
                // Check for exit request
                if (action && action->type == ACTION_EXIT) {
                    exit_requested = true;
                    free_action(action);
                    break;
                }
            } else {
                action = get_ai_action(engine, active, strategies, ctx);
            }
            
            // Submit action
            if (action) {
                if (engine_submit_action(engine, action)) {
                    // Move to resolve phase
                    advance_to_resolve_phase(engine);
                } else {
                    printf(RED "Error: Failed to submit action\n" RESET);
                    free_action(action);
                }
            }
        }
    }
    
    // Display final results
    if (!exit_requested) {
        cli_display_game_over(engine_get_state(engine), cfg);
    }
    
    // Cleanup
    cli_destroy_callbacks(callbacks);
    engine_destroy(engine);
}

// Player configuration and game initialization
int run_mode_stda_cli(config_t* cfg) {
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    printf("%s\n", 
           LOCALIZED_STRING("Running in command line interface mode...",
                           "Execution en mode interface de ligne de commande...",
                           "Ejecutando en modo interfaz de linea de comandos..."));

    // Get player type selection
    display_player_selection_menu(cfg);
    int choice = get_player_type_choice(cfg);
    apply_player_selection(cfg, choice);
    
    // Initialize player configuration
    PlayerConfig pconfig;
    init_player_config(&pconfig);
    cfg->player_config = &pconfig;

    // Get player names
    get_player_names(cfg, &pconfig);

    // Get AI strategies for AI players
    get_ai_strategies(cfg, &pconfig);

    // Create game context
    GameContext* ctx = create_game_context(cfg->prng_seed, cfg);
    if (ctx == NULL) {
        fprintf(stderr, "%s\n", 
               LOCALIZED_STRING("Failed to create game context",
                               "Echec de creation du contexte",
                               "Error al crear contexto"));
        return EXIT_FAILURE;
    }

    // Get player assignment
    get_player_assignment(&pconfig, cfg);
    apply_player_assignment(&pconfig, cfg, ctx);

    // Create strategies
    StrategySet* strategies = create_strategy_set();
    for (int i = 0; i < 2; i++) {
        if (cfg->player_types[i] == AI_PLAYER) {
            set_player_strategy_by_type(strategies, (PlayerID)i,
                                       pconfig.ai_strategies[i]);
        } else {
            // Human players still need a strategy for fallback
            set_player_strategy(strategies, (PlayerID)i,
                              random_attack_strategy,
                              random_defense_strategy);
        }
    }
    
    // Display configuration summary
    printf("\n=== %s ===\n",
           LOCALIZED_STRING("Game Configuration",
                           "Configuration du jeu",
                           "Configuracion del juego"));
    
    for (int i = 0; i < 2; i++) {
        PlayerID pid = (PlayerID)i;
        const char* pos = (pid == PLAYER_A) ? "A" : "B";
        const char* name = pconfig.player_names[pid];
        
        if (cfg->player_types[pid] == INTERACTIVE_PLAYER) {
            printf("%s %s: %s (%s)\n",
                   LOCALIZED_STRING("Player", "Joueur", "Jugador"),
                   pos, name,
                   LOCALIZED_STRING("Human", "Humain", "Humano"));
        } else {
            const char* strat = get_strategy_display_name(
                                  pconfig.ai_strategies[pid],
                                  cfg->language);
            printf("%s %s: %s (AI - %s)\n",
                   LOCALIZED_STRING("Player", "Joueur", "Jugador"),
                   pos, name, strat);
        }
    }

    printf("\n=== %s ===\n", 
           LOCALIZED_STRING("Game Start", 
                           "Début du jeu",
                           "Inicio del juego"));

    // Run main game loop
    stda_game_loop_cli(cfg, ctx, strategies);

    // Cleanup
    free_strategy_set(strategies);
    destroy_game_context(ctx);
    
    return EXIT_SUCCESS;
}


// ============================================================================
// HEADER FILES
// ============================================================================

// ui/cli/cli_callbacks.h
#ifndef CLI_CALLBACKS_H
#define CLI_CALLBACKS_H

#include "game_types.h"
#include "ui/shared/ui_callbacks.h"

UICallbacks* cli_create_callbacks(config_t* cfg);
void cli_destroy_callbacks(UICallbacks* callbacks);

// Callback implementations
void cli_on_card_drawn(PlayerID player, uint8_t card_id, void* ui_ctx);
void cli_on_card_played(PlayerID player, uint8_t card_id,
                        ActionType action_type, void* ui_ctx);
void cli_on_combat_resolved(const CombatResult* result, void* ui_ctx);
void cli_on_phase_changed(GamePhase old_phase, GamePhase new_phase,
                          void* ui_ctx);
void cli_on_discard_complete(PlayerID player, uint8_t num_cards,
                             void* ui_ctx);
void cli_on_energy_changed(PlayerID player, int16_t old_energy,
                           int16_t new_energy, void* ui_ctx);

#endif

// ui/cli/cli_display.h
#ifndef CLI_DISPLAY_H
#define CLI_DISPLAY_H

#include "game_types.h"
#include "core/game_engine.h"

// Status and state display
void cli_display_player_status(PlayerID player, const GameState* gs,
                               const char* player_name, bool is_defense);
void cli_display_hand(PlayerID player, const GameState* gs);
void cli_display_combat_zone(const GameState* gs, config_t* cfg);
void cli_display_game_status(const GameState* gs, config_t* cfg);
void cli_display_available_commands(GamePhase phase, config_t* cfg);

// Event notifications
void cli_display_card_drawn(const char* player_name, 
                            const struct card* card);
void cli_display_card_played(const char* player_name,
                             const struct card* card,
                             ActionType action_type);
void cli_display_combat_resolution(const CombatResult* result,
                                   config_t* cfg);
void cli_display_phase_transition(GamePhase old_phase, 
                                  GamePhase new_phase);
void cli_display_discard_complete(const char* player_name,
                                  uint8_t num_cards);
void cli_display_damage_taken(const char* player_name, int16_t damage,
                              int16_t remaining_energy);

// Game summary
void cli_display_game_over(const GameState* gs, config_t* cfg);

#endif

// ui/cli/cli_input.h
#ifndef CLI_INPUT_H
#define CLI_INPUT_H

#include "game_types.h"
#include "actions/action.h"
#include "core/game_engine.h"

// Get action from human player (blocking)
Action* cli_get_attack_action(const GameState* gs, PlayerID player,
                              config_t* cfg);
Action* cli_get_defense_action(const GameState* gs, PlayerID player,
                               config_t* cfg);
Action* cli_get_discard_action(const GameState* gs, PlayerID player,
                               config_t* cfg);

#endif

// roles/stda/stda_game.h
#ifndef STDA_GAME_H
#define STDA_GAME_H

#include "game_types.h"
#include "game_context.h"
#include "strategies/strategy.h"

// Main CLI game loop (blocking - game owns loop)
void stda_game_loop_cli(config_t* cfg, GameContext* ctx,
                        StrategySet* strategies);

// Entry point
int run_mode_stda_cli(config_t* cfg);

#endif


// ============================================================================
// SHARED UI CALLBACK INTERFACE
// ============================================================================

// ui/shared/ui_callbacks.h
#ifndef UI_CALLBACKS_H
#define UI_CALLBACKS_H

#include "game_types.h"
#include "core/game_engine.h"

// Generic callback structure (UI-agnostic)
typedef struct {
    void (*on_card_drawn)(PlayerID player, uint8_t card_id, void* ui_ctx);
    void (*on_card_played)(PlayerID player, uint8_t card_id,
                          ActionType action_type, void* ui_ctx);
    void (*on_combat_resolved)(const CombatResult* result, void* ui_ctx);
    void (*on_phase_changed)(GamePhase old_phase, GamePhase new_phase,
                            void* ui_ctx);
    void (*on_discard_complete)(PlayerID player, uint8_t num_cards,
                               void* ui_ctx);
    void (*on_energy_changed)(PlayerID player, int16_t old_energy,
                             int16_t new_energy, void* ui_ctx);
    void* ui_context;  // UI-specific data
} UICallbacks;

#endif


// ============================================================================
// MIGRATION NOTES
// ============================================================================

/*
 * MIGRATION STRATEGY:
 * 
 * Phase 1: Create New Files (Week 1)
 * ----------------------------------
 * 1. Create ui/cli/ directory
 * 2. Create roles/stda/ directory
 * 3. Create empty files with headers
 * 4. Add to build system (Makefile)
 * 
 * Phase 2: Extract Display Functions (Week 1-2)
 * ---------------------------------------------
 * 1. Copy all display_* functions to cli_display.c
 * 2. Update function signatures (take GameState* instead of gamestate*)
 * 3. Remove ANSI code duplication
 * 4. Test in isolation with mock GameState
 * 
 * Phase 3: Extract Input Functions (Week 2)
 * -----------------------------------------
 * 1. Create Action structures (if not existing)
 * 2. Move parse_* and validate_* to cli_input.c
 * 3. Convert to return Action* instead of modifying state
 * 4. Test with mock GameState
 * 
 * Phase 4: Create Callback System (Week 2-3)
 * ------------------------------------------
 * 1. Define UICallbacks interface in ui/shared/
 * 2. Implement CLI callbacks in cli_callbacks.c
 * 3. Thread callbacks through engine functions
 * 4. Verify callbacks fire at correct times
 * 
 * Phase 5: Integrate State Machine (Week 3-4)
 * -------------------------------------------
 * 1. Refactor stda_game.c to use GameEngine
 * 2. Replace direct turn_logic calls with engine_step()
 * 3. Replace handle_interactive_* with get_*_action()
 * 4. Test full integration
 * 
 * Phase 6: Cleanup Original File (Week 4)
 * ---------------------------------------
 * 1. Remove old stda_cli.c
 * 2. Update all includes
 * 3. Final integration testing
 * 4. Performance testing
 * 
 * BENEFITS ACHIEVED:
 * - Each file < 400 lines (well under 500 limit)
 * - Clear separation of concerns
 * - Easy to add TUI (swap cli_display/input)
 * - Ready for client/server (action-based)
 * - Callbacks prepare for event broadcasting
 * - State machine enables both blocking and event-driven modes
 */