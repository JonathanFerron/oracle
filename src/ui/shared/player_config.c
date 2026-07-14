// player_config.c
// Extended player configuration implementation

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "player_config.h"
#include "localization.h"
#include "../../util/rnd.h"
#include "../cli/cli_constants.h"

void init_player_config(PlayerConfig* pconfig)
{ // Default player types (Human vs AI)
  pconfig->player_types[0] = INTERACTIVE_PLAYER;
  pconfig->player_types[1] = AI_PLAYER;

  // Default names
  strncpy(pconfig->player_names[0], "Player1", MAX_PLAYER_NAME_LEN - 1);
  strncpy(pconfig->player_names[1], "Player2", MAX_PLAYER_NAME_LEN - 1);
  pconfig->player_names[0][MAX_PLAYER_NAME_LEN - 1] = '\0';
  pconfig->player_names[1][MAX_PLAYER_NAME_LEN - 1] = '\0';

  // Default strategies
  pconfig->ai_strategies[0] = AI_STRATEGY_RANDOM;
  pconfig->ai_strategies[1] = AI_STRATEGY_RANDOM;

  // Default assignment
  pconfig->assignment_mode = ASSIGN_DIRECT;
}

static void trim_whitespace(char* str)
{ // Trim leading whitespace
  char* start = str;
  while(isspace((unsigned char)*start)) start++;

  // Trim trailing whitespace
  char* end = start + strlen(start) - 1;
  while(end > start && isspace((unsigned char)*end)) end--;
  *(end + 1) = '\0';

  // Move trimmed string to beginning
  if(start != str)
    memmove(str, start, strlen(start) + 1);
}

void get_player_names(config_t* cfg, PlayerConfig* pconfig)
{ char input[MAX_INPUT_LEN_MEDIUM];

  // Get Player 1 name
  printf("\n%s [%s]: ",
         LOCALIZED_STRING_L(cfg->language,
                            "Enter name for Player 1",
                            "Entrez le nom du Joueur 1",
                            "Ingrese el nombre del Jugador 1"),
         pconfig->player_names[0]);

  if(fgets(input, sizeof(input), stdin) != NULL)
  { input[strcspn(input, "\n")] = 0;
    trim_whitespace(input);

    if(strlen(input) > 0)
    { strncpy(pconfig->player_names[0], input, MAX_PLAYER_NAME_LEN - 1);
      pconfig->player_names[0][MAX_PLAYER_NAME_LEN - 1] = '\0';
    }
  }

  // Get Player 2 name only if Human vs Human
  if(pconfig->player_types[0] == INTERACTIVE_PLAYER &&
     pconfig->player_types[1] == INTERACTIVE_PLAYER)
  { printf("%s [%s]: ",
           LOCALIZED_STRING_L(cfg->language,
                              "Enter name for Player 2",
                              "Entrez le nom du Joueur 2",
                              "Ingrese el nombre del Jugador 2"),
           pconfig->player_names[1]);

    if(fgets(input, sizeof(input), stdin) != NULL)
    { input[strcspn(input, "\n")] = 0;
      trim_whitespace(input);

      if(strlen(input) > 0)
      { strncpy(pconfig->player_names[1], input,
                MAX_PLAYER_NAME_LEN - 1);
        pconfig->player_names[1][MAX_PLAYER_NAME_LEN - 1] = '\0';
      }
    }
  }
}

static void display_ai_strategy_menu(ui_language_t lang)
{ printf("\n%s:\n",
         LOCALIZED_STRING_L(lang,
                            "Available AI Strategies",
                            "Strategies IA disponibles",
                            "Estrategias IA disponibles"));

  printf("  [1] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Random", "Aleatoire", "Aleatorio"),
         LOCALIZED_STRING_L(lang, "available", "disponible", "disponible"));

  // ideas/A1 ai agent value based
  printf("  [2] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Value Based", "Base sur la valeur",
                            "Basado en valor"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A3 ai agent greedy power
  printf("  [3] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Greedy Power", "Puissance gloutonne",
                            "Poder codicioso"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A4 ai agent combo aware -- the Borealis benchmark agent
  printf("  [4] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Combo Aware [Borealis benchmark]",
                            "Sensible aux combos [reference Borealis]",
                            "Consciente de combos [referencia Borealis]"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A5 ai agent balanced
  printf("  [5] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Balanced Rules", "Regles equilibrees",
                            "Reglas equilibradas"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A6 ai agent heuristics
  printf("  [6] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Heuristic", "Heuristique", "Heuristica"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A7 ai agent tactical and hbt (Heuristics+Balanced+Tactical hybrid)
  printf("  [7] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Hybrid (HBT)", "Hybride (HBT)",
                            "Hibrido (HBT)"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A8 ai agent hbt 2 ply
  printf("  [8] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "HBT 2-ply", "HBT 2-coups", "HBT 2-jugadas"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A9 ai agent simple MC
  printf("  [9] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "Simple Monte Carlo",
                            "Monte Carlo simple", "Monte Carlo simple"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A10 ai agent is mcts
  printf("  [10] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "IS-MCTS", "IS-MCTS", "IS-MCTS"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));

  // ideas/A11 ai agent is mcts with neural network
  printf("  [11] %s (%s)\n",
         LOCALIZED_STRING_L(lang, "IS-MCTS + Neural Network",
                            "IS-MCTS + reseau de neurones",
                            "IS-MCTS + red neuronal"),
         LOCALIZED_STRING_L(lang, "not yet implemented",
                            "pas encore implemente", "no implementado"));
}

static AIStrategyType get_ai_strategy_choice(ui_language_t lang)
{ char input[MAX_INPUT_LEN_SHORT];

  printf("\n%s [1]: ",
         LOCALIZED_STRING_L(lang,
                            "Enter choice",
                            "Entrez le choix",
                            "Ingrese la opcion"));

  if(fgets(input, sizeof(input), stdin) == NULL)
    return AI_STRATEGY_RANDOM;

  input[strcspn(input, "\n")] = 0;

  if(strlen(input) == 0)
    return AI_STRATEGY_RANDOM;

  int choice = atoi(input);

  if(choice < 1 || choice > AI_STRATEGY_COUNT)
  { printf("%s\n",
           LOCALIZED_STRING_L(lang,
                              "Invalid choice. Using Random.",
                              "Choix invalide. Utilisation Aleatoire.",
                              "Opcion invalida. Usando Aleatorio."));
    return AI_STRATEGY_RANDOM;
  }

  // Warn if strategy not implemented
  if(choice > 1)
  { printf("%s\n",
           LOCALIZED_STRING_L(lang,
                              "Warning: Strategy not yet implemented. Using Random.",
                              "Attention: Strategie pas encore implementee. Utilisation Aleatoire.",
                              "Advertencia: Estrategia no implementada. Usando Aleatorio."));
    return AI_STRATEGY_RANDOM;
  }

  return (AIStrategyType)(choice - 1);
}

void get_ai_strategies(config_t* cfg, PlayerConfig* pconfig)
{ // Get strategy for AI Player 1 (if applicable)
  if(pconfig->player_types[0] == AI_PLAYER)
  { printf("\n=== %s 1 ===\n",
           LOCALIZED_STRING_L(cfg->language,
                              "AI Configuration for Player",
                              "Configuration IA pour le Joueur",
                              "Configuracion IA para Jugador"));
    display_ai_strategy_menu(cfg->language);
    pconfig->ai_strategies[0] = get_ai_strategy_choice(cfg->language);
  }

  // Get strategy for AI Player 2 (if applicable)
  if(pconfig->player_types[1] == AI_PLAYER)
  { printf("\n=== %s 2 ===\n",
           LOCALIZED_STRING_L(cfg->language,
                              "AI Configuration for Player",
                              "Configuration IA pour le Joueur",
                              "Configuracion IA para Jugador"));
    display_ai_strategy_menu(cfg->language);
    pconfig->ai_strategies[1] = get_ai_strategy_choice(cfg->language);
  }
}

void get_player_assignment(PlayerConfig* pconfig, config_t* cfg)
{ char input[MAX_INPUT_LEN_SHORT];

  printf("\n=== %s ===\n",
         LOCALIZED_STRING_L(cfg->language,
                            "Player Assignment",
                            "Attribution des joueurs",
                            "Asignacion de jugadores"));

  printf("\n%s:\n",
         LOCALIZED_STRING_L(cfg->language,
                            "How should players be assigned to game positions?",
                            "Comment attribuer les joueurs aux positions?",
                            "Como asignar jugadores a las posiciones?"));

  printf("  [1] %s (%s A, %s B)\n",
         LOCALIZED_STRING_L(cfg->language, "Direct", "Direct", "Directo"),
         pconfig->player_names[0],
         pconfig->player_names[1]);

  printf("  [2] %s (%s B, %s A)\n",
         LOCALIZED_STRING_L(cfg->language, "Inverted", "Inverse", "Invertido"),
         pconfig->player_names[0],
         pconfig->player_names[1]);

  printf("  [3] %s\n",
         LOCALIZED_STRING_L(cfg->language,
                            "Random (first player chosen randomly)",
                            "Aleatoire (premier joueur choisi aleatoirement)",
                            "Aleatorio (primer jugador elegido al azar)"));

  printf("\n%s [1]: ",
         LOCALIZED_STRING_L(cfg->language,
                            "Enter choice",
                            "Entrez le choix",
                            "Ingrese la opcion"));

  if(fgets(input, sizeof(input), stdin) == NULL)
  { pconfig->assignment_mode = ASSIGN_DIRECT;
    return;
  }

  input[strcspn(input, "\n")] = 0;

  if(strlen(input) == 0)
  { pconfig->assignment_mode = ASSIGN_DIRECT;
    return;
  }

  int choice = atoi(input);

  if(choice < 1 || choice > 3)
  { printf("%s\n",
           LOCALIZED_STRING_L(cfg->language,
                              "Invalid choice. Using Direct assignment.",
                              "Choix invalide. Attribution directe.",
                              "Opcion invalida. Asignacion directa."));
    pconfig->assignment_mode = ASSIGN_DIRECT;
    return;
  }

  pconfig->assignment_mode = (PlayerAssignmentMode)(choice - 1);
}

void apply_player_assignment(PlayerConfig* pconfig, config_t* cfg,
                             GameContext* ctx)
{ bool swap = false;

  switch(pconfig->assignment_mode)
  { case ASSIGN_DIRECT:
      swap = false;
      printf("\n%s: %s -> A, %s -> B\n",
             LOCALIZED_STRING_L(cfg->language,
                                "Assignment",
                                "Attribution",
                                "Asignacion"),
             pconfig->player_names[0],
             pconfig->player_names[1]);
      break;

    case ASSIGN_INVERTED:
      swap = true;
      printf("\n%s: %s -> B, %s -> A\n",
             LOCALIZED_STRING_L(cfg->language,
                                "Assignment",
                                "Attribution",
                                "Asignacion"),
             pconfig->player_names[0],
             pconfig->player_names[1]);
      break;

    case ASSIGN_RANDOM:
      swap = (RND_randn(2, ctx) == 1);
      printf("\n%s: %s -> %s, %s -> %s\n",
             LOCALIZED_STRING_L(cfg->language,
                                "Random assignment",
                                "Attribution aleatoire",
                                "Asignacion aleatoria"),
             pconfig->player_names[0],
             swap ? "B" : "A",
             pconfig->player_names[1],
             swap ? "A" : "B");
      break;
  }

  // Apply swap if needed
  if(swap)
  { // Swap player types
    PlayerType temp_type = pconfig->player_types[0];
    pconfig->player_types[0] = pconfig->player_types[1];
    pconfig->player_types[1] = temp_type;

    // Swap names
    char temp_name[MAX_PLAYER_NAME_LEN];
    strncpy(temp_name, pconfig->player_names[0], MAX_PLAYER_NAME_LEN);
    strncpy(pconfig->player_names[0], pconfig->player_names[1],
            MAX_PLAYER_NAME_LEN);
    strncpy(pconfig->player_names[1], temp_name, MAX_PLAYER_NAME_LEN);

    // Swap AI strategies
    AIStrategyType temp_strat = pconfig->ai_strategies[0];
    pconfig->ai_strategies[0] = pconfig->ai_strategies[1];
    pconfig->ai_strategies[1] = temp_strat;
  }
}

const char* get_strategy_display_name(AIStrategyType strategy,
                                      ui_language_t lang)
{ switch(strategy)
  { case AI_STRATEGY_RANDOM:
      return LOCALIZED_STRING_L(lang, "Random", "Aleatoire", "Aleatorio");
    case AI_STRATEGY_VALUE_BASED:
      return LOCALIZED_STRING_L(lang, "Value Based", "Base sur la valeur",
                                "Basado en valor");
    case AI_STRATEGY_GREEDY_POWER:
      return LOCALIZED_STRING_L(lang, "Greedy Power", "Puissance gloutonne",
                                "Poder codicioso");
    case AI_STRATEGY_COMBO_AWARE:
      return LOCALIZED_STRING_L(lang, "Combo Aware [Borealis]",
                                "Sensible aux combos [Borealis]",
                                "Consciente de combos [Borealis]");
    case AI_STRATEGY_BALANCED:
      return LOCALIZED_STRING_L(lang, "Balanced", "Equilibre", "Equilibrado");
    case AI_STRATEGY_HEURISTIC:
      return LOCALIZED_STRING_L(lang, "Heuristic", "Heuristique",
                                "Heuristica");
    case AI_STRATEGY_HYBRID_HBT:
      return LOCALIZED_STRING_L(lang, "Hybrid (HBT)", "Hybride (HBT)",
                                "Hibrido (HBT)");
    case AI_STRATEGY_HBT_2PLY:
      return LOCALIZED_STRING_L(lang, "HBT 2-ply", "HBT 2-coups",
                                "HBT 2-jugadas");
    case AI_STRATEGY_SIMPLE_MC:
      return LOCALIZED_STRING_L(lang, "SimpleMC", "MC-Simple", "MC-Simple");
    case AI_STRATEGY_ISMCTS:
      return LOCALIZED_STRING_L(lang, "IS-MCTS", "IS-MCTS", "IS-MCTS");
    case AI_STRATEGY_ISMCTS_NN:
      return LOCALIZED_STRING_L(lang, "IS-MCTS + NN", "IS-MCTS + RN",
                                "IS-MCTS + RN");
    default:
      return "Unknown";
  }
}

const char* get_player_display_name(PlayerID player, PlayerConfig* pconfig)
{ return pconfig->player_names[player];
}

// Shorthand codes for `-A`/`--ai` (lowercase letters/digits, <=10 chars).
// AI_STRATEGY_COMBO_AWARE has two aliases (its own name and the Borealis
// benchmark codename); every other strategy has exactly one.
typedef struct
{ AIStrategyType strategy;
  const char* shorthands[2];
} AIStrategyShorthand;

static const AIStrategyShorthand AI_STRATEGY_SHORTHANDS[] =
{ { AI_STRATEGY_RANDOM,       { "rand", NULL } },
  { AI_STRATEGY_VALUE_BASED,  { "value", NULL } },
  { AI_STRATEGY_GREEDY_POWER, { "greedy", NULL } },
  { AI_STRATEGY_COMBO_AWARE,  { "combo", "borealis" } },
  { AI_STRATEGY_BALANCED,     { "balanced", NULL } },
  { AI_STRATEGY_HEURISTIC,    { "heuristic", NULL } },
  { AI_STRATEGY_HYBRID_HBT,   { "hbt", NULL } },
  { AI_STRATEGY_HBT_2PLY,     { "hbt2ply", NULL } },
  { AI_STRATEGY_SIMPLE_MC,    { "simplemc", NULL } },
  { AI_STRATEGY_ISMCTS,       { "ismcts", NULL } },
  { AI_STRATEGY_ISMCTS_NN,    { "ismctsnn", NULL } },
};
#define AI_STRATEGY_SHORTHAND_COUNT \
  (sizeof(AI_STRATEGY_SHORTHANDS) / sizeof(AI_STRATEGY_SHORTHANDS[0]))

AIStrategyType parse_ai_strategy_shorthand(const char* shorthand)
{ if(!shorthand) return AI_STRATEGY_COUNT;

  char lower[16] = {0};
  size_t i;
  for(i = 0; i < sizeof(lower) - 1 && shorthand[i]; i++)
    lower[i] = (char)tolower((unsigned char)shorthand[i]);

  for(i = 0; i < AI_STRATEGY_SHORTHAND_COUNT; i++)
  { const AIStrategyShorthand* e = &AI_STRATEGY_SHORTHANDS[i];
    if((e->shorthands[0] && strcmp(lower, e->shorthands[0]) == 0) ||
       (e->shorthands[1] && strcmp(lower, e->shorthands[1]) == 0))
      return e->strategy;
  }

  return AI_STRATEGY_COUNT;
}

void print_ai_agent_shorthand_list(config_t* cfg)
{ printf("%s\n", LOCALIZED_STRING("Available AI agents (for -A/--ai):",
                                  "Agents IA disponibles (pour -A/--ai) :",
                                  "Agentes IA disponibles (para -A/--ai):"));

  for(size_t i = 0; i < AI_STRATEGY_SHORTHAND_COUNT; i++)
  { const AIStrategyShorthand* e = &AI_STRATEGY_SHORTHANDS[i];
    char codes[24];

    if(e->shorthands[1])
      snprintf(codes, sizeof(codes), "%s, %s", e->shorthands[0], e->shorthands[1]);
    else
      snprintf(codes, sizeof(codes), "%s", e->shorthands[0]);

    printf("  %-16s %s\n", codes,
           get_strategy_display_name(e->strategy, cfg->language));
  }
}
