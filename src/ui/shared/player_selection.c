// player_selection.c
// Player type selection implementation

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "player_selection.h"
#include "localization.h"
#include "player_config.h"
#include "../cli/cli_constants.h"

void display_player_selection_menu(config_t* cfg)
{ printf("\n" "=== %s ===" "\n",
         LOCALIZED_STRING("Player Configuration",
                          "Configuration des joueurs",
                          "Configuracion de jugadores"));

  printf("\n%s:\n",
         LOCALIZED_STRING("Select game mode",
                          "Selectionnez le mode de jeu",
                          "Selecciona el modo de juego"));

  printf("  [1] %s\n",
         LOCALIZED_STRING("Human vs AI (default)",
                          "Humain vs IA (par defaut)",
                          "Humano vs IA (predeterminado)"));

  printf("  [2] %s\n",
         LOCALIZED_STRING("Human vs Human",
                          "Humain vs Humain",
                          "Humano vs Humano"));

  printf("  [3] %s\n",
         LOCALIZED_STRING("AI vs AI",
                          "IA vs IA",
                          "IA vs IA"));

  printf("\n%s [1]: ",
         LOCALIZED_STRING("Enter choice",
                          "Entrez le choix",
                          "Ingrese la opcion"));
}

int get_player_type_choice(config_t* cfg)
{ char input[MAX_INPUT_LEN_SHORT];

  if(fgets(input, sizeof(input), stdin) == NULL)
    return 1; // Default to Human vs AI

  // Remove newline
  input[strcspn(input, "\n")] = 0;

  // Empty input defaults to option 1
  if(strlen(input) == 0)
    return 1;

  // Parse choice
  int choice = atoi(input);

  // Validate range
  if(choice < 1 || choice > 3)
  { printf("%s\n",
           LOCALIZED_STRING("Invalid choice. Using default (Human vs AI).",
                            "Choix invalide. Utilisation par defaut (Humain vs IA).",
                            "Opcion invalida. Usando predeterminado (Humano vs IA)."));
    return 1;
  }

  return choice;
}

void apply_player_selection(PlayerConfig* pconfig, config_t* cfg, int choice)
{ switch(choice)
  { case 1: // Human vs AI (default)
      pconfig->player_types[PLAYER_A] = INTERACTIVE_PLAYER;
      pconfig->player_types[PLAYER_B] = AI_PLAYER;
      break;

    case 2: // Human vs Human
      pconfig->player_types[PLAYER_A] = INTERACTIVE_PLAYER;
      pconfig->player_types[PLAYER_B] = INTERACTIVE_PLAYER;
      break;

    case 3: // AI vs AI
      pconfig->player_types[PLAYER_A] = AI_PLAYER;
      pconfig->player_types[PLAYER_B] = AI_PLAYER;
      break;

    default: // Should not reach here due to validation
      pconfig->player_types[PLAYER_A] = INTERACTIVE_PLAYER;
      pconfig->player_types[PLAYER_B] = AI_PLAYER;
      break;
  }
}
