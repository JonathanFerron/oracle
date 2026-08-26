// player_config.c
// Extended player configuration implementation

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "player_config.h"
#include "localization.h"
#include "../../util/rnd.h"
#include "ui_constants.h"
#include "../../ai_strat/ai_strategy.h"
#include "../../rating/rating.h"

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

// Menu label per AIStrategyType, in ideas/A1-A11 roster order. Availability
// (shown alongside each label) is no longer hardcoded here -- it comes from
// ai_strategy_is_implemented(), the same registry set_player_strategy_by_type()
// consults, so this menu can never drift from what actually plays. The
// bracketed flavour name (e.g. "[The Showboat]") is *not* part of this
// label -- format_menu_name() appends get_strategy_display_name() to it, so
// keep flavour names out of the literals below.
static const char* strategy_menu_label(AIStrategyType type, ui_language_t lang)
{ switch(type)
  { case AI_STRATEGY_RANDOM:
      return LOCALIZED_STRING_L(lang, "Random", "Aleatoire", "Aleatorio");
    case AI_STRATEGY_VALUE_BASED: // ideas/A1 ai agent value based (the apprentice)
      return LOCALIZED_STRING_L(lang, "Value Based", "Base sur la valeur",
                                "Basado en valor");
    case AI_STRATEGY_COMBO_THRESHOLD: // ideas/A2 ai agent combo threshold (the showboat)
      return LOCALIZED_STRING_L(lang, "Combo Threshold", "Seuil de combo",
                                "Umbral de combo");
    case AI_STRATEGY_BOREALIS: // ideas/A3 ai agent greedy power (borealis) -- the benchmark agent
      return LOCALIZED_STRING_L(lang, "Borealis", "Borealis", "Borealis");
    case AI_STRATEGY_BALANCED: // ideas/A4 ai agent balanced rules (bean counter)
      return LOCALIZED_STRING_L(lang, "Balanced Rules", "Regles equilibrees",
                                "Reglas equilibradas");
    case AI_STRATEGY_HEURISTIC: // ideas/A5 ai agent heuristic (eps-gam-del)
      return LOCALIZED_STRING_L(lang, "Heuristic", "Heuristique", "Heuristica");
    case AI_STRATEGY_TACTICAL: // ideas/A6 ai agent tactical (pressure cooker)
      return LOCALIZED_STRING_L(lang, "Tactical", "Tactique", "Tactico");
    case AI_STRATEGY_HYBRID_HBT: // ideas/A7 ai agent hybrid hbt (the grandmaster)
      return LOCALIZED_STRING_L(lang, "Hybrid (HBT)", "Hybride (HBT)",
                                "Hibrido (HBT)");
    case AI_STRATEGY_SIMPLE_MC: // ideas/A8 ai agent simple monte carlo (the soothsayer)
      return LOCALIZED_STRING_L(lang, "Simple Monte Carlo",
                                "Monte Carlo simple", "Monte Carlo simple");
    case AI_STRATEGY_HBT_2PLY: // ideas/A9 ai agent hbt 2 ply (grandmaster ii)
      return LOCALIZED_STRING_L(lang, "HBT 2-ply", "HBT 2-coups", "HBT 2-jugadas");
    case AI_STRATEGY_ISMCTS: // ideas/A10 ai agent is-mcts (the omniscient)
      return LOCALIZED_STRING_L(lang, "IS-MCTS", "IS-MCTS", "IS-MCTS");
    case AI_STRATEGY_ISMCTS_NN: // ideas/A11 ai agent is-mcts + nn (alphaoracle prime)
      return LOCALIZED_STRING_L(lang, "IS-MCTS + Neural Network",
                                "IS-MCTS + reseau de neurones",
                                "IS-MCTS + red neuronal");
    default:
      return "Unknown";
  }
}

// Borealis-scale rating per agent (1-99; the rating *is* the expected win
// percentage vs Borealis, the rating-50 anchor -- see src/rating/rating.h).
// `measured` distinguishes a real --stda.rating fit (doc/changelog.md's
// 2026-08-23 entry) from the design-intent estimate in
// ideas/G1 AI agent general info/oracle_ai_agent_names.md, which the menu
// prints with a "~" prefix. When a new agent is benchmarked, replace its
// estimate here and flip `measured` -- an implemented-but-unbenchmarked
// agent legitimately keeps measured = false.
typedef struct
{ int8_t rating;
  bool measured;
} AIStrategyRating;

static const AIStrategyRating AI_STRATEGY_RATINGS[AI_STRATEGY_COUNT] =
{ [AI_STRATEGY_RANDOM]           = {  2, true  },
  [AI_STRATEGY_VALUE_BASED]      = { 24, true  },
  [AI_STRATEGY_COMBO_THRESHOLD]  = { 30, true  },
  [AI_STRATEGY_BOREALIS]         = { BOREALIS_RATING, true },
  [AI_STRATEGY_BALANCED]         = { 36, true  },
  [AI_STRATEGY_HEURISTIC]        = { 60, true  },
  [AI_STRATEGY_TACTICAL]         = { 52, true  },
  [AI_STRATEGY_HYBRID_HBT]       = { 62, true  },
  [AI_STRATEGY_SIMPLE_MC]        = { 35, true  },
  [AI_STRATEGY_HBT_2PLY]         = { 85, false },
  [AI_STRATEGY_ISMCTS]           = { 92, false },
  [AI_STRATEGY_ISMCTS_NN]        = { 97, false },
};

// "Random [The Gambler]", or just "Borealis" when the technical label and
// the flavour name (get_strategy_display_name()) are identical strings.
static void format_menu_name(AIStrategyType type, ui_language_t lang,
                             char* buf, size_t n)
{ const char* label = strategy_menu_label(type, lang);
  const char* flavour = get_strategy_display_name(type, lang);

  if(strcmp(label, flavour) == 0)
    snprintf(buf, n, "%s", label);
  else
    snprintf(buf, n, "%s [%s]", label, flavour);
}

// "50" for a measured rating, "~62" for a design-intent estimate.
static void format_menu_rating(AIStrategyType type, char* buf, size_t n)
{ const AIStrategyRating* r = &AI_STRATEGY_RATINGS[type];

  snprintf(buf, n, "%s%d", r->measured ? "" : "~", r->rating);
}

static void display_ai_strategy_menu(ui_language_t lang)
{ printf("\n%s:\n",
         LOCALIZED_STRING_L(lang,
                            "Available AI Strategies",
                            "Strategies IA disponibles",
                            "Estrategias IA disponibles"));

  for(AIStrategyType type = 0; type < AI_STRATEGY_COUNT; type++)
  { const char* availability = ai_strategy_is_implemented(type)
                               ? LOCALIZED_STRING_L(lang, "available", "disponible", "disponible")
                               : LOCALIZED_STRING_L(lang, "not yet implemented",
                                                    "pas encore implemente", "no implementado");
    char name[80], rating[8];
    format_menu_name(type, lang, name, sizeof(name));
    format_menu_rating(type, rating, sizeof(rating));

    printf("  [%d] %s (%s) [%s] (%s)\n", type + 1, name, availability,
           rating, get_ai_strategy_shorthand(type));
  }
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
  AIStrategyType chosen = (AIStrategyType)(choice - 1);
  if(!ai_strategy_is_implemented(chosen))
  { printf("%s\n",
           LOCALIZED_STRING_L(lang,
                              "Warning: Strategy not yet implemented. Using Random.",
                              "Attention: Strategie pas encore implementee. Utilisation Aleatoire.",
                              "Advertencia: Estrategia no implementada. Usando Aleatorio."));
    return AI_STRATEGY_RANDOM;
  }

  return chosen;
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

// Flavour names from ideas/G1 AI agent general info/oracle_ai_agent_names.md
// (canonical roster). ASCII-safe variants are used throughout, matching that
// file's "ASCII-Safe Variants" table, since print_ai_agent_shorthand_list()
// pads with %-16s, which counts bytes rather than glyphs.
const char* get_strategy_display_name(AIStrategyType strategy,
                                      ui_language_t lang)
{ switch(strategy)
  { case AI_STRATEGY_RANDOM:
      return LOCALIZED_STRING_L(lang, "The Gambler", "Le Parieur",
                                "El Apostador");
    case AI_STRATEGY_VALUE_BASED:
      return LOCALIZED_STRING_L(lang, "The Apprentice", "L'Apprenti",
                                "El Aprendiz");
    case AI_STRATEGY_COMBO_THRESHOLD:
      return LOCALIZED_STRING_L(lang, "The Showboat", "Le Frimeur",
                                "El Fanfarron");
    case AI_STRATEGY_BOREALIS:
      return LOCALIZED_STRING_L(lang, "Borealis", "Borealis", "Borealis");
    case AI_STRATEGY_BALANCED:
      return LOCALIZED_STRING_L(lang, "Bean Counter", "Compteur de Feves",
                                "Contador de Frijoles");
    case AI_STRATEGY_HEURISTIC:
      return LOCALIZED_STRING_L(lang, "Eps-Gam-Del", "Eps-Gam-Del",
                                "Eps-Gam-Del");
    case AI_STRATEGY_TACTICAL:
      return LOCALIZED_STRING_L(lang, "Pressure Cooker", "Cocotte-Minute",
                                "Olla a Presion");
    case AI_STRATEGY_HYBRID_HBT:
      return LOCALIZED_STRING_L(lang, "The Grandmaster", "Le Grand Maitre",
                                "El Gran Maestro");
    case AI_STRATEGY_SIMPLE_MC:
      return LOCALIZED_STRING_L(lang, "The Soothsayer", "Le Devin",
                                "El Adivino");
    case AI_STRATEGY_HBT_2PLY:
      return LOCALIZED_STRING_L(lang, "Grandmaster II", "Grand Maitre II",
                                "Gran Maestro II");
    case AI_STRATEGY_ISMCTS:
      return LOCALIZED_STRING_L(lang, "The Omniscient", "L'Omniscient",
                                "El Omnisciente");
    case AI_STRATEGY_ISMCTS_NN:
      return LOCALIZED_STRING_L(lang, "AlphaOracle Prime", "AlphaOracle Prime",
                                "AlphaOracle Prime");
    default:
      return "Unknown";
  }
}

const char* get_player_display_name(PlayerID player, PlayerConfig* pconfig)
{ return pconfig->player_names[player];
}

void format_player_label(PlayerID player, PlayerConfig* pconfig, ui_language_t lang,
                         char* buf, size_t n)
{ if(pconfig->player_types[player] == INTERACTIVE_PLAYER)
  { snprintf(buf, n, "%s", pconfig->player_names[player]);
    return;
  }

  const char* flavour = get_strategy_display_name(pconfig->ai_strategies[player], lang);
  snprintf(buf, n, "%s - %s", LOCALIZED_STRING_L(lang, "AI", "IA", "IA"), flavour);
} // format_player_label

// Shorthand code for `-A`/`--ai` (lowercase letters/digits, <=10 chars).
// Exactly one per strategy -- AI_STRATEGY_BOREALIS and
// AI_STRATEGY_COMBO_THRESHOLD used to also accept their pre-rename/flavour
// aliases `greedy` and `showboat`; those were retired so every agent has a
// single canonical code (see player_config.h's shorthand-alias note).
typedef struct
{ AIStrategyType strategy;
  const char* shorthand;
} AIStrategyShorthand;

static const AIStrategyShorthand AI_STRATEGY_SHORTHANDS[] =
{ { AI_STRATEGY_RANDOM,           "rand" },
  { AI_STRATEGY_VALUE_BASED,      "value" },
  { AI_STRATEGY_COMBO_THRESHOLD,  "combo" },
  { AI_STRATEGY_BOREALIS,         "borealis" },
  { AI_STRATEGY_BALANCED,         "balanced" },
  { AI_STRATEGY_HEURISTIC,        "heuristic" },
  { AI_STRATEGY_TACTICAL,         "tactical" },
  { AI_STRATEGY_HYBRID_HBT,       "hbt" },
  { AI_STRATEGY_SIMPLE_MC,        "simplemc" },
  { AI_STRATEGY_HBT_2PLY,         "hbt2ply" },
  { AI_STRATEGY_ISMCTS,           "ismcts" },
  { AI_STRATEGY_ISMCTS_NN,        "ismctsnn" },
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
  { if(strcmp(lower, AI_STRATEGY_SHORTHANDS[i].shorthand) == 0)
      return AI_STRATEGY_SHORTHANDS[i].strategy;
  }

  return AI_STRATEGY_COUNT;
}

void print_ai_agent_shorthand_list(config_t* cfg)
{ printf("%s\n", LOCALIZED_STRING("Available AI agents (for -A/--ai):",
                                  "Agents IA disponibles (pour -A/--ai) :",
                                  "Agentes IA disponibles (para -A/--ai):"));

  for(size_t i = 0; i < AI_STRATEGY_SHORTHAND_COUNT; i++)
  { const AIStrategyShorthand* e = &AI_STRATEGY_SHORTHANDS[i];
    printf("  %-16s %s\n", e->shorthand,
           get_strategy_display_name(e->strategy, cfg->language));
  }
}

void print_ai_agent_shorthand_codes(void)
{ for(size_t i = 0; i < AI_STRATEGY_SHORTHAND_COUNT; i++)
    printf("%s\n", AI_STRATEGY_SHORTHANDS[i].shorthand);
}

// Inverse of parse_ai_strategy_shorthand() -- e.g. for naming a
// src/rating/ entrant after the agent that plays it (stda_rating.c).
// NULL for AI_STRATEGY_COUNT or any other out-of-range value.
const char* get_ai_strategy_shorthand(AIStrategyType strategy)
{ for(size_t i = 0; i < AI_STRATEGY_SHORTHAND_COUNT; i++)
    if(AI_STRATEGY_SHORTHANDS[i].strategy == strategy)
      return AI_STRATEGY_SHORTHANDS[i].shorthand;

  return NULL;
}

// Borealis-scale rating lookup (see AI_STRATEGY_RATINGS's comment above).
// Returns -1 and sets *is_measured = false for an out-of-range strategy;
// is_measured may be NULL.
int8_t get_ai_strategy_rating(AIStrategyType strategy, bool* is_measured)
{ if(strategy < 0 || strategy >= AI_STRATEGY_COUNT)
  { if(is_measured) *is_measured = false;
    return -1;
  }

  const AIStrategyRating* r = &AI_STRATEGY_RATINGS[strategy];
  if(is_measured) *is_measured = r->measured;
  return r->rating;
}
