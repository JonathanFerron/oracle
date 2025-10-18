/*
 * Oracle: Les Champions d'Arcadie
 * 
 * File structure:
 * - main.c: Main entry point and run mode functions
 * - cmdline.c: Command line parsing (print_usage, parse_options, print_version)
 * - oracle.h: Shared declarations
 */

/* ============================================================
 * version.h - Version information
 * ============================================================ */

#ifndef VERSION_H
#define VERSION_H

/* Version information - year.month format with optional letter suffix */
#define VERSION_YEAR 2025
#define VERSION_MONTH 10
#define VERSION_SUFFIX ""  /* Empty string or "b", "c", etc. for multiple releases */

#endif /* VERSION_H */

/* ============================================================
 * oracle.h - Header file with shared declarations
 * ============================================================ */

#ifndef ORACLE_H
#define ORACLE_H

#include <stdbool.h>
#include "version.h"

/* Game mode enumeration */
typedef enum {
    MODE_NONE = 0,
    MODE_STDA_AUTO,    /* Standalone automated */
    MODE_STDA_SIM,     /* Standalone simulation */
    MODE_STDA_TUI,     /* Standalone text UI */
    MODE_STDA_GUI,     /* Standalone graphical UI */
    MODE_SERVER,       /* Server mode */
    MODE_CLIENT_SIM,   /* Client simulation */
    MODE_CLIENT_TUI,   /* Client text UI */
    MODE_CLIENT_GUI,   /* Client graphical UI */
    MODE_CLIENT_AI     /* AI agent client */
} game_mode_t;

/* Configuration structure */
typedef struct {
    game_mode_t mode;
    bool verbose;
    int numsim;
    char *input_file;
    char *output_file;
    char *ai_agent;
} config_t;

/* Command line parsing functions (cmdline.c) */
void print_usage(const char *prog);
void print_version(void);
int parse_options(int argc, char **argv, config_t *cfg);

/* Utility functions (main.c) */
void cleanup_config(config_t *cfg);

/* Run mode functions (main.c) */
int run_mode_stda_auto(config_t *cfg);
int run_mode_stda_sim(config_t *cfg);
int run_mode_stda_tui(config_t *cfg);
int run_mode_stda_gui(config_t *cfg);
int run_mode_server(config_t *cfg);
int run_mode_client_sim(config_t *cfg);
int run_mode_client_tui(config_t *cfg);
int run_mode_client_gui(config_t *cfg);
int run_mode_client_ai(config_t *cfg);

#endif /* ORACLE_H */

/* ============================================================
 * main.c - Main entry point and run mode implementations
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include "oracle.h"

/* Cleanup configuration */
void cleanup_config(config_t *cfg) {
    if (cfg->input_file) free(cfg->input_file);
    if (cfg->output_file) free(cfg->output_file);
    if (cfg->ai_agent) free(cfg->ai_agent);
}

/* Main entry point */
int main(int argc, char **argv) {
    config_t cfg;
    int ret;
    
    /* Parse command line options */
    ret = parse_options(argc, argv, &cfg);
    if (ret != 0) {
        cleanup_config(&cfg);
        return (ret < 0) ? 0 : ret;
    }
    
    /* Redirect output if specified */
    if (cfg.output_file) {
        if (!freopen(cfg.output_file, "w", stdout)) {
            perror("Failed to redirect output");
            cleanup_config(&cfg);
            return 1;
        }
        /* Also redirect stderr to same file */
        if (!freopen(cfg.output_file, "a", stderr)) {
            perror("Failed to redirect stderr");
            cleanup_config(&cfg);
            return 1;
        }
    }
    
    /* Launch appropriate game mode */
    switch (cfg.mode) {
    case MODE_STDA_AUTO:
        ret = run_mode_stda_auto(&cfg);
        break;
    case MODE_STDA_SIM:
        ret = run_mode_stda_sim(&cfg);
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
        ret = 1;
    }
    
    cleanup_config(&cfg);
    return ret;
}

/* Run mode implementations */

int run_mode_stda_auto(config_t *cfg) {
    printf("Running standalone automated mode...\n");
    if (cfg->verbose) printf("Number of simulations: %d\n", cfg->numsim);
    /* TODO: Implement automated game logic */
    return 0;
}

int run_mode_stda_sim(config_t *cfg) {
    printf("Running standalone simulation mode...\n");
    /* TODO: Implement simulation interface */
    return 0;
}

int run_mode_stda_tui(config_t *cfg) {
    printf("Running standalone TUI mode (ncurses)...\n");
    /* TODO: Initialize ncurses and game loop */
    return 0;
}

int run_mode_stda_gui(config_t *cfg) {
    printf("Running standalone GUI mode...\n");
    /* TODO: Initialize GUI toolkit and game loop */
    return 0;
}

int run_mode_server(config_t *cfg) {
    printf("Running server mode...\n");
    if (cfg->verbose) printf("Server ready for client connections\n");
    /* TODO: Initialize network server */
    return 0;
}

int run_mode_client_sim(config_t *cfg) {
    printf("Running client simulation mode...\n");
    /* TODO: Connect to server and run simulation client */
    return 0;
}

int run_mode_client_tui(config_t *cfg) {
    printf("Running client TUI mode...\n");
    /* TODO: Connect to server and initialize ncurses client */
    return 0;
}

int run_mode_client_gui(config_t *cfg) {
    printf("Running client GUI mode...\n");
    /* TODO: Connect to server and initialize GUI client */
    return 0;
}

int run_mode_client_ai(config_t *cfg) {
    printf("Running AI agent client mode...\n");
    if (cfg->verbose) printf("AI agent: %s\n", cfg->ai_agent);
    /* TODO: Connect to server and run AI agent (e.g., IS-MCTS) */
    return 0;
}

/* ============================================================
 * cmdline.c - Command line parsing implementation
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "oracle.h"

/* Print usage information */
void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  -h,  -he, --help              Show this help message\n");
    printf("  -v,  -vb, --verbose           Enable verbose output\n");
    printf("  -V,  -vr, --version           Show version information\n");
    printf("  -n,  -ns, --numsim N          Set number of simulations to N\n");
    printf("  -i,  -in, --input FILE        Use FILE as input configuration\n");
    printf("  -o,  -ou, --output FILE       Output to FILE instead of stdout\n\n");
    printf("Game Modes:\n");
    printf("  -a,  -sa, --stda.auto         Standalone automated mode\n");
    printf("  -s,  -ss, --stda.sim          Standalone simulation mode\n");
    printf("  -t,  -st, --stda.tui          Standalone text UI (ncurses)\n");
    printf("  -g,  -sg, --stda.gui          Standalone graphical UI\n");
    printf("  -S,  -sv, --server            Server mode\n");
    printf("  -C,  -cs, --client.sim        Client simulation mode\n");
    printf("  -T,  -ct, --client.tui        Client text UI mode\n");
    printf("  -G,  -cg, --client.gui        Client graphical UI mode\n");
    printf("  -A,  -ai, --ai AGENT          AI agent client mode\n");
}

/* Print version information */
void print_version(void) {
    printf("Oracle: Les Champions d'Arcadie v%d.%02d%s\n",
           VERSION_YEAR, VERSION_MONTH, VERSION_SUFFIX);
}

/* Parse command line options */
int parse_options(int argc, char **argv, config_t *cfg) {
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        /* Single letter options */
        {"h",          no_argument,       0, 'h'},
        {"v",          no_argument,       0, 'v'},
        {"V",          no_argument,       0, 'V'},
        {"n",          required_argument, 0, 'n'},
        {"i",          required_argument, 0, 'i'},
        {"o",          required_argument, 0, 'o'},
        {"a",          no_argument,       0, 'a'},
        {"s",          no_argument,       0, 's'},
        {"t",          no_argument,       0, 't'},
        {"g",          no_argument,       0, 'g'},
        {"S",          no_argument,       0, 'S'},
        {"C",          no_argument,       0, 'C'},
        {"T",          no_argument,       0, 'T'},
        {"G",          no_argument,       0, 'G'},
        {"A",          required_argument, 0, 'A'},
        /* Two letter options */
        {"he",         no_argument,       0, 'h'},
        {"vb",         no_argument,       0, 'v'},
        {"vr",         no_argument,       0, 'V'},
        {"ns",         required_argument, 0, 'n'},
        {"in",         required_argument, 0, 'i'},
        {"ou",         required_argument, 0, 'o'},
        {"sa",         no_argument,       0, 'a'},
        {"ss",         no_argument,       0, 's'},
        {"st",         no_argument,       0, 't'},
        {"sg",         no_argument,       0, 'g'},
        {"sv",         no_argument,       0, 'S'},
        {"cs",         no_argument,       0, 'C'},
        {"ct",         no_argument,       0, 'T'},
        {"cg",         no_argument,       0, 'G'},
        {"ai",         required_argument, 0, 'A'},
        /* Long form options */
        {"help",       no_argument,       0, 'h'},
        {"verbose",    no_argument,       0, 'v'},
        {"version",    no_argument,       0, 'V'},
        {"numsim",     required_argument, 0, 'n'},
        {"input",      required_argument, 0, 'i'},
        {"output",     required_argument, 0, 'o'},
        {"stda.auto",  no_argument,       0, 'a'},
        {"stda.sim",   no_argument,       0, 's'},
        {"stda.tui",   no_argument,       0, 't'},
        {"stda.gui",   no_argument,       0, 'g'},
        {"server",     no_argument,       0, 'S'},
        {"client.sim", no_argument,       0, 'C'},
        {"client.tui", no_argument,       0, 'T'},
        {"client.gui", no_argument,       0, 'G'},
        {0, 0, 0, 0}
    };
    
    /* Initialize config with defaults */
    memset(cfg, 0, sizeof(config_t));
    cfg->verbose = false;
    cfg->numsim = 1000;  /* Default number of simulations */
    
    while ((opt = getopt_long_only(argc, argv, 
            "hvVn:i:o:astgSCTGA:", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            return -1;
        case 'v':
            cfg->verbose = true;
            break;
        case 'V':
            print_version();
            return -1;
        case 'n':
            cfg->numsim = atoi(optarg);
            if (cfg->numsim <= 0) {
                fprintf(stderr, "Error: numsim must be positive\n");
                return 1;
            }
            break;
        case 'i':
            cfg->input_file = strdup(optarg);
            break;
        case 'o':
            cfg->output_file = strdup(optarg);
            break;
        case 'a':
            cfg->mode = MODE_STDA_AUTO;
            break;
        case 's':
            cfg->mode = MODE_STDA_SIM;
            break;
        case 't':
            cfg->mode = MODE_STDA_TUI;
            break;
        case 'g':
            cfg->mode = MODE_STDA_GUI;
            break;
        case 'S':
            cfg->mode = MODE_SERVER;
            break;
        case 'C':
            cfg->mode = MODE_CLIENT_SIM;
            break;
        case 'T':
            cfg->mode = MODE_CLIENT_TUI;
            break;
        case 'G':
            cfg->mode = MODE_CLIENT_GUI;
            break;
        case 'A':
            cfg->mode = MODE_CLIENT_AI;
            cfg->ai_agent = strdup(optarg);
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (cfg->mode == MODE_NONE) {
        fprintf(stderr, "Error: no game mode specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}