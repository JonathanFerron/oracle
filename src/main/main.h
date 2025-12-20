/* ============================================================
   main.h - Header file with shared declarations
   ============================================================ */

#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>
#include "version.h"
#include "game_types.h"

/* Utility functions */
void cleanup_config(config_t* cfg);

/* Run mode functions */
int run_mode_stda_sim(config_t* cfg);
int run_mode_stda_tui(config_t* cfg);
int run_mode_stda_gui(config_t* cfg);
int run_mode_server(config_t* cfg);
int run_mode_client_sim(config_t* cfg);
int run_mode_client_cli(config_t* cfg);
int run_mode_client_tui(config_t* cfg);
int run_mode_client_gui(config_t* cfg);
int run_mode_client_ai(config_t* cfg);

#endif /* MAIN_H */
