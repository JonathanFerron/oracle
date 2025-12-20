// main.c
// Oracle: The Champions of Arcadia - Main entry point

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "cmdline.h"
#include "../core/game_constants.h"
#include "../core/game_state.h"
#include "../ai_strat/ai_strategy.h"
#include "../ai_strat/ai_strat_random.h"
#include "../util/mtwister.h"
#include "../util/prng_seed.h"
#include "../roles/stda/stda_auto.h"
#include "../roles/stda/stda_cli.h"

/* Main entry point */
int main(int argc, char** argv)
{ config_t cfg;  // config struct
  int ret;  // return value

  /* Parse command line options */
  ret = parse_options(argc, argv, &cfg);
  if(ret != EXIT_SUCCESS)
  { cleanup_config(&cfg);
    return (ret < EXIT_SUCCESS) ? EXIT_SUCCESS : ret;
  }

  /* Initialize PRNG with the configured seed */
  seedRand(validate_seed(cfg.prng_seed));

  /* Redirect output if requested */
  if(cfg.output_file)
  { if(!freopen(cfg.output_file, "w", stdout))
    { perror("Failed to redirect output");
      cleanup_config(&cfg);
      return EXIT_FAILURE;
    }
    /* Also redirect stderr to same file */
    if(!freopen(cfg.output_file, "a", stderr))
    { perror("Failed to redirect stderr");
      cleanup_config(&cfg);
      return EXIT_FAILURE;
    }
  }

  /* Launch appropriate game mode */
  switch(cfg.mode)
  { case MODE_STDA_AUTO:
      ret = run_mode_stda_auto(&cfg);
      break;
    case MODE_STDA_SIM:
      ret = run_mode_stda_sim(&cfg);
      break;
    case MODE_STDA_CLI:
      ret = run_mode_stda_cli(&cfg);
      break;
    case MODE_STDA_TUI:
      ret = run_mode_stda_tui(&cfg);
      break;
    case MODE_STDA_GUI:
      ret = run_mode_stda_gui(&cfg);
      break;
    case MODE_SERVER:
      ret = run_mode_server(&cfg);
      break;
    case MODE_CLIENT_SIM:
      ret = run_mode_client_sim(&cfg);
      break;
    case MODE_CLIENT_CLI:
      ret = run_mode_client_cli(&cfg);
      break;
    case MODE_CLIENT_TUI:
      ret = run_mode_client_tui(&cfg);
      break;
    case MODE_CLIENT_GUI:
      ret = run_mode_client_gui(&cfg);
      break;
    case MODE_CLIENT_AI:
      ret = run_mode_client_ai(&cfg);
      break;
    default:
      fprintf(stderr, "Error: invalid game mode\n");
      ret = EXIT_FAILURE;
  }

  cleanup_config(&cfg);
  return ret;

} // main

int run_mode_stda_sim(config_t* cfg)
{ printf("Standalone simulation (ncurses) mode not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_stda_tui(config_t* cfg)
{ printf("Standalone TUI mode (ncurses) not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_stda_gui(config_t* cfg)
{ printf("Standalone GUI mode not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_server(config_t* cfg)
{ printf("Server mode not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_client_sim(config_t* cfg)
{ printf("Client simulation (ncurses) mode not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_client_cli(config_t* cfg)
{ printf("Client command line interface mode not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_client_tui(config_t* cfg)
{ printf("Client TUI mode (ncurses) not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_client_gui(config_t* cfg)
{ printf("Client GUI mode not yet implemented...\n");
  return EXIT_SUCCESS;
}
int run_mode_client_ai(config_t* cfg)
{ printf("AI agent client mode not yet implemented...\n");
  printf("AI agent: %s\n", cfg->ai_agent);
  return EXIT_SUCCESS;
}

/* Cleanup configuration */
void cleanup_config(config_t* cfg)
{ if(cfg->input_file) free(cfg->input_file);
  if(cfg->output_file) free(cfg->output_file);
  if(cfg->ai_agent) free(cfg->ai_agent);
}
