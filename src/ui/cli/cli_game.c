/* ========================================================================
   CLI Game Logic
   ======================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cli_game.h"
#include "cli_display.h"
#include "cli_input.h"
#include "cli_io.h"
#include "../interactive/game_commands.h"
#include "../shared/ui_constants.h"
#include "../../core/game_constants.h"
#include "../../ai_strat/ai_strategy.h"
#include "../../core/game_state.h"
#include "../../core/turn_logic.h"
#include "../../core/combat.h"
#include "../shared/localization.h"
#include "../shared/player_config.h"
#include "../../core/card_actions.h" // TODO: this is only temporary as the AI agent fall back implementation of discard to 7 cards should be located in the AI code instead (e.g. strat_random.h)

/* ========================================================================
   Game Phase Handlers
   ======================================================================== */

int handle_interactive_attack(struct gamestate* gstate,
                              PlayerID player, GameContext* ctx, config_t* cfg)
{ char input_buffer[MAX_COMMAND_LEN];
  int action_taken = NO_ACTION;

  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  PlayerID opponent = 1 - player;
  char opponent_label[MAX_PLAYER_LABEL_LEN];
  format_player_label(opponent, pconfig, cfg->language, opponent_label, sizeof(opponent_label));

  while(!action_taken && !gstate->someone_has_zero_energy)
  { printf("\n=== (%s %d, %s %d) ===\n",
           LOCALIZED_STRING("Turn", "Tour", "Turno"),
           gstate->turn,
           LOCALIZED_STRING("Round", "Manche", "Ronda"),
           (uint16_t)((gstate->turn - 1) * 0.5 + 1));
    printf("\n=== %s (%s) ===\n",
           opponent_label,
           LOCALIZED_STRING("Defender", "Defenseur", "Defensor"));
    display_player_prompt(opponent, gstate, 1, cfg);
    printf(" %s:%d\n",
           LOCALIZED_STRING("Hand", "Main", "Mano"),
           gstate->hand[opponent].size);
    printf("\n");
    display_player_prompt(player, gstate, 0, cfg);
    display_player_hand(player, gstate, cfg);
    printf("\n%s\n" ICON_PROMPT " ",
           LOCALIZED_STRING("Commands: cham <indices>, draw <index>, cash <index>, pass, gmst, shod, help, exit",
                            "Commandes: cham <indices>, draw <index>, cash <index>, pass, gmst, shod, help, exit",
                            "Comandos: cham <indices>, draw <index>, cash <index>, pass, gmst, shod, help, exit"));

    if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    { printf("%s\n", LOCALIZED_STRING("Error reading input.",
                                      "Erreur de lecture.",
                                      "Error al leer entrada."));
      return EXIT_SIGNAL;
    }

    action_taken = process_attack_command(input_buffer, gstate, player, ctx, cfg);
    if(action_taken == EXIT_SIGNAL) return EXIT_SIGNAL;
  }

  return EXIT_SUCCESS;
}

int handle_interactive_defense(struct gamestate* gstate,
                               PlayerID player, GameContext* ctx, config_t* cfg)
{ char input_buffer[MAX_COMMAND_LEN];

  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  PlayerID opponent = 1 - player;
  char opponent_label[MAX_PLAYER_LABEL_LEN];
  format_player_label(opponent, pconfig, cfg->language, opponent_label, sizeof(opponent_label));

  printf("\n=== (%s %d, %s %d) ===\n",
         LOCALIZED_STRING("Turn", "Tour", "Turno"),
         gstate->turn,
         LOCALIZED_STRING("Round", "Manche", "Ronda"),
         (uint16_t)((gstate->turn - 1) * 0.5 + 1));
  display_attack_state(gstate, cfg);

  printf("\n=== %s (%s) ===\n",
         opponent_label,
         LOCALIZED_STRING("Attacker", "Attaquant", "Atacante"));
  display_player_prompt(opponent, gstate, 0, cfg);
  printf(" %s:%d\n",
         LOCALIZED_STRING("Hand", "Main", "Mano"),
         gstate->hand[opponent].size);

  printf("\n\n");
  display_player_prompt(player, gstate, 1, cfg);

  /* Check if defender has any cards before showing hand/prompt
     If hand is empty, automatically pass (take damage) */
  if(gstate->hand[player].size == 0)
  { printf("\n%s\n",
           LOCALIZED_STRING("No cards in hand - taking damage without defending",
                            "Aucune carte en main - prendre des degats sans defendre",
                            "No hay cartas en mano - recibir dano sin defender"));
    return EXIT_SUCCESS;
  }

  display_player_hand(player, gstate, cfg);
  printf("\n%s\n" ICON_PROMPT " ",
         LOCALIZED_STRING("Defend: 'cham <indices>' (e.g., 'cham 1 2') or 'pass' to take damage",
                          "Defendre: 'cham <indices>' (ex: 'cham 1 2') ou 'pass' pour prendre des degats",
                          "Defender: 'cham <indices>' (ej: 'cham 1 2') o 'pass' para recibir dano"));

  if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    return EXIT_SUCCESS;

  int result = process_defense_command(input_buffer, gstate, player, ctx, cfg);
  return (result == EXIT_SIGNAL) ? EXIT_SIGNAL : EXIT_SUCCESS;
}

/* ========================================================================
   Game Turn Execution
   ======================================================================== */

// When the AI attacker commits zero champions, the combat block below never
// runs, so neither a Turn header nor anything else gets printed for that
// turn -- it silently vanishes from the transcript even though gstate->turn
// did advance normally. Distinguishes "drew/recalled" (hand grew), "played a
// cash card" (hand shrank), and "passed outright" (hand unchanged) purely
// from the hand-size delta, since attack_phase() returns no signal of its
// own about what the strategy did. Only called for a human-vs-AI game (the
// caller gates on the opponent being interactive), matching this file's
// existing "only narrate when a human is watching" scoping for the detailed
// combat breakdown below.
static void report_ai_no_combat_action(struct gamestate* gstate, PlayerID attacker,
                                       uint8_t hand_before, config_t* cfg)
{ PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  char label[MAX_PLAYER_LABEL_LEN];
  format_player_label(attacker, pconfig, cfg->language, label, sizeof(label));
  uint8_t hand_after = gstate->hand[attacker].size;

  printf("\n=== (%s %d, %s %d) ===\n",
         LOCALIZED_STRING("Turn", "Tour", "Turno"),
         gstate->turn,
         LOCALIZED_STRING("Round", "Manche", "Ronda"),
         (uint16_t)((gstate->turn - 1) * 0.5 + 1));

  if(hand_after > hand_before)
    printf("%s %s.\n", label,
           LOCALIZED_STRING("drew/recalled cards instead of attacking",
                            "a pioche/rappele des cartes au lieu d'attaquer",
                            "robo/recupero cartas en lugar de atacar"));
  else if(hand_after < hand_before)
    printf("%s %s.\n", label,
           LOCALIZED_STRING("exchanged a champion for cash instead of attacking",
                            "a echange un champion contre des lunas au lieu d'attaquer",
                            "cambio un campeon por lunas en lugar de atacar"));
  else
    printf("%s %s.\n", label,
           LOCALIZED_STRING("passed (no affordable or worthwhile play)",
                            "a passe (aucun coup jouable ou utile)",
                            "paso (sin jugada asequible o util)"));
} // report_ai_no_combat_action

int execute_game_turn(struct gamestate* gstate, StrategySet* strategies,
                      GameContext* ctx, config_t* cfg)
{ begin_of_turn(gstate, ctx);

  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;

  /* Attack phase - check if current player is interactive */
  if(pconfig->player_types[gstate->current_player] == INTERACTIVE_PLAYER)
  { int result = handle_interactive_attack(gstate, gstate->current_player, ctx, cfg);
    if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
  }
  else
  { PlayerID attacker = gstate->current_player;
    uint8_t hand_before = gstate->hand[attacker].size;
    attack_phase(gstate, strategies, ctx);

    /* AI committed no champions: the combat block below never runs, so
       narrate the turn here instead -- but only when the opponent is
       human (AI-vs-AI CLI play stays silent, matching the combat-detail
       gate a few lines down). */
    if(gstate->combat_zone[attacker].size == 0 &&
       pconfig->player_types[1 - attacker] == INTERACTIVE_PLAYER)
      report_ai_no_combat_action(gstate, attacker, hand_before, cfg);
  }

  /* Defense phase - check if defender is interactive */
  if(gstate->combat_zone[gstate->current_player].size > 0)
  { PlayerID attacker = gstate->current_player;
    PlayerID defender = 1 - attacker;
    if(pconfig->player_types[defender] == INTERACTIVE_PLAYER)
    { int result = handle_interactive_defense(gstate, defender, ctx, cfg);
      if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
    }
    else
      defense_phase(gstate, strategies, ctx);

    /* Show the detailed combat breakdown whenever a human is involved;
       stda_auto (both players AI) always takes the plain path below so its
       RNG-dependent results (bin/expectedresults.txt) stay unaffected. */
    if(pconfig->player_types[attacker] == INTERACTIVE_PLAYER ||
       pconfig->player_types[defender] == INTERACTIVE_PLAYER)
    { CombatDetails details;
      resolve_combat_with_details(gstate, &details, ctx);
      display_combat_details_cli(gstate, &details, cfg);
    }
    else
      resolve_combat(gstate, ctx);
  }

  return EXIT_SUCCESS;
}

/* ========================================================================
   Game Initialization and Cleanup
   ======================================================================== */

struct gamestate* initialize_cli_game(uint16_t initial_cash,
                                      StrategySet** strategies_out,
                                      config_t* cfg,
                                      GameContext* ctx)
{ PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  StrategySet* strategies = create_strategy_set();
  set_player_strategy_by_type(strategies, PLAYER_A, pconfig->ai_strategies[PLAYER_A]);
  set_player_strategy_by_type(strategies, PLAYER_B, pconfig->ai_strategies[PLAYER_B]);

  struct gamestate* gstate = malloc(sizeof(struct gamestate));
  setup_game(initial_cash, gstate, ctx);

  *strategies_out = strategies;
  return gstate;
}

void cleanup_cli_game(struct gamestate* gstate, StrategySet* strategies,
                      GameContext* ctx)
{ DeckStk_emptyOut(&gstate->deck[PLAYER_A]);
  DeckStk_emptyOut(&gstate->deck[PLAYER_B]);

  free(gstate);
  free_strategy_set(strategies);
  destroy_game_context(ctx);
}

/* ========================================================================
   Mulligan Phase
   ======================================================================== */

static int process_mulligan_command(char* input_buffer,
                                    struct gamestate* gstate,
                                    GameContext* ctx, config_t* cfg)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strcmp(input_buffer, "help") == 0)
  { display_mulligan_prompt(gstate, PLAYER_B, cfg);
    return 0;
  }

  UiIO io = cli_io_create(cfg);
  int done = game_process_mulligan_command(input_buffer, gstate, ctx, cfg, &io);

  if(done && strncmp(input_buffer, "mull ", 5) == 0)
  { printf("\n%s:\n",
           LOCALIZED_STRING("New hand", "Nouvelle main", "Nueva mano"));
    display_player_hand(PLAYER_B, gstate, cfg);
  }

  return done;
}

int handle_interactive_mulligan(struct gamestate* gstate,
                                GameContext* ctx, config_t* cfg)
{ char input_buffer[MAX_COMMAND_LEN];
  int mulligan_done = 0;

  display_mulligan_prompt(gstate, PLAYER_B, cfg);

  while(!mulligan_done)
  { if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    { printf(YELLOW "%s\n" RESET,
             LOCALIZED_STRING("Input error, keeping current hand",
                              "Erreur, conservation de la main",
                              "Error, manteniendo mano actual"));
      return 0;
    }
    mulligan_done = process_mulligan_command(input_buffer, gstate, ctx, cfg);
  }

  return 0;
}

/* ========================================================================
   Discard-to-7 Phase
   ======================================================================== */

static int process_discard_command(char* input_buffer,
                                   struct gamestate* gstate,
                                   int cards_to_discard,
                                   GameContext* ctx, config_t* cfg)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;
  PlayerID player = gstate->current_player;

  if(strcmp(input_buffer, "help") == 0)
  { display_discard_prompt(gstate, player, cfg);
    return 0;
  }

  UiIO io = cli_io_create(cfg);
  int done = game_process_discard_command(input_buffer, gstate, cards_to_discard,
                                          ctx, cfg, &io);

  if(done && strncmp(input_buffer, "disc ", 5) == 0)
  { printf("\n%s (%d %s):\n",
           LOCALIZED_STRING("Remaining hand", "Main restante", "Mano restante"),
           gstate->hand[player].size,
           LOCALIZED_STRING("cards", "cartes", "cartas"));
    display_player_hand(player, gstate, cfg);
  }

  return done;
}

int handle_interactive_discard_to_7(struct gamestate* gstate, StrategySet* strategies,
                                    GameContext* ctx, config_t* cfg)
{ if(gstate->hand[gstate->current_player].size <= 7)
    return 0;

  char input_buffer[MAX_COMMAND_LEN];
  int discard_done = 0;
  int cards_to_discard = gstate->hand[gstate->current_player].size - 7;

  display_discard_prompt(gstate, gstate->current_player, cfg);

  while(!discard_done)
  { if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    { printf(YELLOW "%s\n" RESET,
             LOCALIZED_STRING("Input error, auto-discarding",
                              "Erreur, defausse automatique",
                              "Error, descarte automatico"));
      discard_to_7_cards(gstate, strategies, ctx);
      return 0;
    }
    discard_done = process_discard_command(input_buffer, gstate,
                                           cards_to_discard, ctx, cfg);
  }

  return 0;
}
