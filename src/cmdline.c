/* ============================================================
   cmdline.c - Command line parsing implementation
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "main.h"

/* Print usage information */
void print_usage(const char* prog)
{ printf("Usage: %s [OPTIONS]\n\n", prog);
  printf("Options:\n");
  printf("  -h,  -he, --help              Show this help message\n");
  printf("  -v,  -vb, --verbose           Enable verbose output\n");
  printf("  -V,  -vr, --version           Show version information\n");
  printf("  -n,  -ns, --numsim N          Set number of simulations to N\n");
  printf("  -i,  -in, --input FILE        Use FILE as input configuration\n");
  printf("  -o,  -ou, --output FILE       Output to FILE instead of stdout\n\n");
  printf("Game Modes:\n");
  printf("  -a,  -sa, --stda.auto         Standalone automated mode\n");
  printf("  -s,  -ss, --stda.sim          Standalone simulation mode (ncurses)\n");
  printf("  -l,  -sl, --stda.cli          Standalone command line interface\n");
  printf("  -t,  -st, --stda.tui          Standalone text UI (ncurses)\n");
  printf("  -g,  -sg, --stda.gui          Standalone graphical UI\n");
  printf("  -S,  -sv, --server            Server mode\n");
  printf("  -C,  -cs, --client.sim        Client simulation mode (ncurses)\n");
  printf("  -L,  -cl, --client.cli        Client command line interface\n");
  printf("  -T,  -ct, --client.tui        Client text UI mode\n");
  printf("  -G,  -cg, --client.gui        Client graphical UI mode\n");
  printf("  -A,  -ai, --ai AGENT          AI agent client mode\n");
}

/* Print version information */
void print_version(void)
{ printf("Oracle: Les Champions d'Arcadie v%d.%02d%s\n",
         VERSION_YEAR, VERSION_MONTH, VERSION_SUFFIX);
}

/* Parse command line options */
int parse_options(int argc, char** argv, config_t* cfg)
{ int opt;
  int option_index = 0;

  static struct option long_options[] =
  { /* Single letter options */
    {"h",          no_argument,       0, 'h'},
    {"v",          no_argument,       0, 'v'},
    {"V",          no_argument,       0, 'V'},
    {"n",          required_argument, 0, 'n'},
    {"i",          required_argument, 0, 'i'},
    {"o",          required_argument, 0, 'o'},
    {"a",          no_argument,       0, 'a'},
    {"s",          no_argument,       0, 's'},
    {"l",          no_argument,       0, 'l'},
    {"t",          no_argument,       0, 't'},
    {"g",          no_argument,       0, 'g'},
    {"S",          no_argument,       0, 'S'},
    {"C",          no_argument,       0, 'C'},
    {"L",          no_argument,       0, 'L'},
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
    {"sl",         no_argument,       0, 'l'},
    {"st",         no_argument,       0, 't'},
    {"sg",         no_argument,       0, 'g'},
    {"sv",         no_argument,       0, 'S'},
    {"cs",         no_argument,       0, 'C'},
    {"cl",         no_argument,       0, 'L'},
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
    {"stda.cli",   no_argument,       0, 'l'},
    {"stda.tui",   no_argument,       0, 't'},
    {"stda.gui",   no_argument,       0, 'g'},
    {"server",     no_argument,       0, 'S'},
    {"client.sim", no_argument,       0, 'C'},
    {"client.cli", no_argument,       0, 'L'},
    {"client.tui", no_argument,       0, 'T'},
    {"client.gui", no_argument,       0, 'G'},
    {0, 0, 0, 0}
  };

  /* Initialize config with defaults */
  memset(cfg, 0, sizeof(config_t));
  cfg->verbose = false;
  cfg->numsim = 1000;  /* Default number of simulations */

  while((opt = getopt_long_only(argc, argv,
                                "hvVn:i:o:asltgSCLTGA:", long_options, &option_index)) != -1)
  { switch(opt)
    { case 'h':
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
        if(cfg->numsim <= 0)
        { fprintf(stderr, "Error: numsim must be positive\n");
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
      case 'l':
        cfg->mode = MODE_STDA_CLI;
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
      case 'L':
        cfg->mode = MODE_CLIENT_CLI;
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

  if(cfg->mode == MODE_NONE)
  { fprintf(stderr, "Error: no game mode specified\n");
    print_usage(argv[0]);
    return 1;
  }

  return EXIT_SUCCESS;
} // parse_options
