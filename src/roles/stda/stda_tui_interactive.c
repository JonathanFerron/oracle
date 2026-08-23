// stda_tui_interactive.c
// TUI Milestone 2 human-turn handlers -- see stda_tui_interactive.h.

#include <stdio.h>
#include <string.h>

#include "stda_tui_interactive.h"
#include "../../ui/tui/tui_input.h"
#include "../../ui/interactive/game_commands.h"
#include "../../ui/shared/ui_constants.h"
#include "../../ui/shared/localization.h"
#include "../../core/turn_logic.h"
#include "../../core/combat.h"
#include "../../core/game_state.h"
#include "../../core/card_actions.h"

/* ========================================================================
   PLAY-mode digit-staging (attack/defense champion selection)
   ======================================================================== */

typedef enum
{ STAGE_CONTINUE,     // redraw and keep polling PLAY-mode keys
  STAGE_CONFIRM,       // Enter: play the staged champions
  STAGE_PASS,          // P: pass
  STAGE_COMMAND_MODE,  // TAB: switch to full COMMAND-mode line editing
  STAGE_QUIT           // q: quit the game
} TuiStageResult;

// Stage-then-confirm champion selection: digit keys (1-9, mapped to hand
// position) toggle a card in/out of the staged set (max 3, matching the
// "cham <i1> <i2> <i3>" grammar's cap); Esc clears the staged set.
static TuiStageResult tui_play_mode_step(TuiScreen* screen, int hand_size,
                                         uint8_t* staged, int* staged_count)
{ int ch = tui_get_input();

  if(tui_input_is_quit(ch)) return STAGE_QUIT;
  if(tui_input_is_resize(ch))
  { tui_layout(screen);
    return STAGE_CONTINUE;
  }
  if(tui_input_is_tab(ch)) return STAGE_COMMAND_MODE;
  if(tui_input_is_escape(ch))
  { *staged_count = 0;
    return STAGE_CONTINUE;
  }
  if(tui_input_is_enter(ch) && *staged_count > 0) return STAGE_CONFIRM;
  if(ch == 'p' || ch == 'P') return STAGE_PASS;

  if(ch >= '1' && ch <= '9')
  { int idx = ch - '1';
    if(idx < hand_size)
    { bool found = false;
      for(int i = 0; i < *staged_count; i++)
      { if(staged[i] == idx)
        { staged[i] = staged[--(*staged_count)];
          found = true;
          break;
        }
      }
      if(!found && *staged_count < 3) staged[(*staged_count)++] = (uint8_t)idx;
    }
  }

  return STAGE_CONTINUE;
}

// Short form only -- this now renders in the narrower command window (it
// shares the bottom row with Player A's status bar), and the full key hints
// already live in the always-visible Shortcuts box.
static void tui_format_staged_status(const uint8_t* staged, int staged_count,
                                     char* buf, size_t bufsize)
{ char list[32] = "";
  for(int i = 0; i < staged_count; i++)
  { char num[8];
    snprintf(num, sizeof(num), "%s%d", i == 0 ? "" : ",", staged[i] + 1);
    strncat(list, num, sizeof(list) - strlen(list) - 1);
  }

  snprintf(buf, bufsize, "PLAY [%s]", list);
}

/* ========================================================================
   Attack / Defense
   ======================================================================== */

static bool tui_handle_interactive_attack(TuiScreen* screen, struct gamestate* gstate,
                                          PlayerID player, GameContext* ctx, config_t* cfg)
{ TuiIoContext tctx = { screen, gstate, cfg };
  UiIO io = tui_io_create(&tctx);
  uint8_t staged[3];
  int staged_count = 0;

  while(!gstate->someone_has_zero_energy)
  { tui_draw_all(screen, gstate, cfg);

    char status[32];
    tui_format_staged_status(staged, staged_count, status, sizeof(status));
    tui_draw_command_line(screen, status, false);

    TuiStageResult result = tui_play_mode_step(screen, gstate->hand[player].size,
                                               staged, &staged_count);

    if(result == STAGE_QUIT) return false;
    if(result == STAGE_CONTINUE) continue;

    if(result == STAGE_COMMAND_MODE)
    { char input[MAX_COMMAND_LEN];
      if(!io.read_line(&io, "> ", input, sizeof(input))) continue; // Esc: back to PLAY
      int action = game_process_attack_command(input, gstate, player, ctx, cfg, &io);
      if(action == EXIT_SIGNAL) return false;
      if(action == ACTION_TAKEN) return true;
      continue;
    }

    if(result == STAGE_PASS)
    { char pass_cmd[8] = "pass";
      game_process_attack_command(pass_cmd, gstate, player, ctx, cfg, &io);
      return true;
    }

    // STAGE_CONFIRM
    uint8_t indices[3];
    memcpy(indices, staged, (size_t)staged_count * sizeof(uint8_t));
    int action = validate_and_play_champions(gstate, player, indices, staged_count,
                                             ctx, cfg, &io);
    if(action == ACTION_TAKEN) return true;
    staged_count = 0; // validation error already messaged; let the player restage
  }

  return true;
}

// Single-shot, matching the CLI: one command/confirmation attempt, then the
// phase ends (an empty/invalid response just means "take damage").
static bool tui_handle_interactive_defense(TuiScreen* screen, struct gamestate* gstate,
                                           PlayerID player, GameContext* ctx, config_t* cfg)
{ TuiIoContext tctx = { screen, gstate, cfg };
  UiIO io = tui_io_create(&tctx);

  if(gstate->hand[player].size == 0)
  { io.message(&io, UI_MSG_WARNING, "%s",
               LOCALIZED_STRING("No cards in hand - taking damage without defending",
                                "Aucune carte en main - prendre des degats sans defendre",
                                "No hay cartas en mano - recibir dano sin defender"));
    return true;
  }

  uint8_t staged[3];
  int staged_count = 0;

  for(;;)
  { tui_draw_all(screen, gstate, cfg);

    char status[32];
    tui_format_staged_status(staged, staged_count, status, sizeof(status));
    tui_draw_command_line(screen, status, false);

    TuiStageResult result = tui_play_mode_step(screen, gstate->hand[player].size,
                                               staged, &staged_count);

    if(result == STAGE_QUIT) return false;
    if(result == STAGE_CONTINUE) continue;

    if(result == STAGE_COMMAND_MODE)
    { char input[MAX_COMMAND_LEN];
      if(!io.read_line(&io, "> ", input, sizeof(input))) continue; // Esc: back to PLAY
      int action = game_process_defense_command(input, gstate, player, ctx, cfg, &io);
      return (action != EXIT_SIGNAL);
    }

    if(result == STAGE_PASS)
    { char pass_cmd[8] = "pass";
      game_process_defense_command(pass_cmd, gstate, player, ctx, cfg, &io);
      return true;
    }

    // STAGE_CONFIRM
    uint8_t indices[3];
    memcpy(indices, staged, (size_t)staged_count * sizeof(uint8_t));
    validate_and_play_champions(gstate, player, indices, staged_count, ctx, cfg, &io);
    return true;
  }
}

/* ========================================================================
   Mulligan / Discard-to-7 (COMMAND mode only)
   ======================================================================== */

void tui_handle_interactive_mulligan(TuiScreen* screen, struct gamestate* gstate,
                                     GameContext* ctx, config_t* cfg)
{ TuiIoContext tctx = { screen, gstate, cfg };
  UiIO io = tui_io_create(&tctx);

  tui_add_message(screen, "%s",
                  LOCALIZED_STRING(
                    "Mulligan: 'mull <indices>' (e.g. 'mull 1 3') or 'pass'",
                    "Mulligan: 'mull <indices>' (ex: 'mull 1 3') ou 'pass'",
                    "Mulligan: 'mull <indices>' (ej: 'mull 1 3') o 'pass'"));

  int done = 0;
  while(!done)
  { tui_draw_all(screen, gstate, cfg);
    char input[MAX_COMMAND_LEN];
    if(!io.read_line(&io, "> ", input, sizeof(input))) continue;
    done = game_process_mulligan_command(input, gstate, ctx, cfg, &io);
  }
}

void tui_handle_interactive_discard_to_7(TuiScreen* screen, struct gamestate* gstate,
                                         GameContext* ctx, config_t* cfg)
{ if(gstate->hand[gstate->current_player].size <= 7) return;

  TuiIoContext tctx = { screen, gstate, cfg };
  UiIO io = tui_io_create(&tctx);
  int cards_to_discard = gstate->hand[gstate->current_player].size - 7;

  tui_add_message(screen, "%s %d %s: 'disc <indices>' (e.g. 'disc 2 5')",
                  LOCALIZED_STRING("Must discard", "Doit defausser", "Debe descartar"),
                  cards_to_discard,
                  LOCALIZED_STRING("card(s)", "carte(s)", "carta(s)"));

  int done = 0;
  while(!done)
  { tui_draw_all(screen, gstate, cfg);
    char input[MAX_COMMAND_LEN];
    if(!io.read_line(&io, "> ", input, sizeof(input))) continue;
    done = game_process_discard_command(input, gstate, cards_to_discard, ctx, cfg, &io);
  }
}

/* ========================================================================
   Per-Turn Orchestrator
   ======================================================================== */

bool tui_play_turn_with_humans(TuiScreen* screen, struct gamestate* gstate,
                               StrategySet* strategies, GameContext* ctx,
                               config_t* cfg, PlayerConfig* pconfig)
{ begin_of_turn(gstate, ctx);

  PlayerID attacker = gstate->current_player;
  if(pconfig->player_types[attacker] == INTERACTIVE_PLAYER)
  { if(!tui_handle_interactive_attack(screen, gstate, attacker, ctx, cfg))
      return false;
    /* attack_phase() (AI path, below) sets these at its end; the human path
       above goes straight through game_commands.c and never touches them,
       so mirror it here too -- otherwise turn_phase stays ATTACK for the
       whole turn in a Human-vs-Human game, and the shortcuts-panel hint
       text (tui_shortcuts_text(), keyed off turn_phase) would stay
       attack-flavored during the human defender's turn too. */
    gstate->turn_phase = DEFENSE;
    gstate->player_to_move = 1 - attacker;
  }
  else
    attack_phase(gstate, strategies, ctx);

  if(gstate->combat_zone[gstate->current_player].size > 0)
  { PlayerID defender = 1 - attacker;
    if(pconfig->player_types[defender] == INTERACTIVE_PLAYER)
    { if(!tui_handle_interactive_defense(screen, gstate, defender, ctx, cfg))
        return false;
    }
    else
      defense_phase(gstate, strategies, ctx);

    if(pconfig->player_types[attacker] == INTERACTIVE_PLAYER ||
       pconfig->player_types[defender] == INTERACTIVE_PLAYER)
    { CombatDetails details;
      resolve_combat_with_details(gstate, &details, ctx);
      tui_show_combat_details(screen, gstate, &details, cfg);
      // No press-any-key pause here: combat/damage/energy now persist in the
      // Game Messages box, so the turn can flow straight into end-of-turn
      // without losing anything the player needed to read.
    }
    else
      resolve_combat(gstate, ctx);
  }

  // attack_phase()/its human mirror above unconditionally pointed
  // player_to_move at the defender, whether or not any champions were
  // actually played and regardless of how combat resolved. From here on
  // (luna collection, discard-to-7) it's the attacker's own housekeeping
  // again -- point Active/Waiting back at them, or the defender would
  // wrongly keep showing as Active with nothing left to decide.
  gstate->player_to_move = attacker;

  if(gstate->someone_has_zero_energy) return true;

  collect_1_luna(gstate);
  if(pconfig->player_types[gstate->current_player] == INTERACTIVE_PLAYER)
    tui_handle_interactive_discard_to_7(screen, gstate, ctx, cfg);
  else
    discard_to_7_cards(gstate, strategies, ctx);
  change_current_player(gstate);

  return true;
}
