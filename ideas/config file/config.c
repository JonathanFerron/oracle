/*
   config.c - Simple INI-style configuration file parser

   Supports:
   - Comments with # or ;
   - Sections with [section]
   - Key-value pairs: key = value
   - Whitespace trimming
   - Boolean values: true/false, yes/no, 1/0
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "oracle.h"

#define MAX_LINE_LEN 256

/* Trim whitespace from both ends of a string */
static char* trim(char* str)
{ char* end;

  /* Trim leading space */
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0) return str;

  /* Trim trailing space */
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  end[1] = '\0';
  return str;
}

/* Parse a boolean value from string */
static bool parse_bool(const char* value)
{ if(strcasecmp(value, "true") == 0 ||
     strcasecmp(value, "yes") == 0 ||
     strcasecmp(value, "1") == 0)
    return true;
  return false;
}

/* Parse game mode from string */
static game_mode_t parse_mode(const char* value)
{ if(strcasecmp(value, "stda.auto") == 0) return MODE_STDA_AUTO;
  if(strcasecmp(value, "stda.sim") == 0) return MODE_STDA_SIM;
  if(strcasecmp(value, "stda.tui") == 0) return MODE_STDA_TUI;
  if(strcasecmp(value, "stda.gui") == 0) return MODE_STDA_GUI;
  if(strcasecmp(value, "server") == 0) return MODE_SERVER;
  if(strcasecmp(value, "client.sim") == 0) return MODE_CLIENT_SIM;
  if(strcasecmp(value, "client.tui") == 0) return MODE_CLIENT_TUI;
  if(strcasecmp(value, "client.gui") == 0) return MODE_CLIENT_GUI;
  if(strcasecmp(value, "ai") == 0) return MODE_CLIENT_AI;
  return MODE_NONE;
}

/* Read and parse configuration file */
int read_config_file(const char* filename, config_t* cfg)
{ FILE *fp;
  char line[MAX_LINE_LEN];
  char section[64] = "";
  char* key, *value, *ptr;
  int line_num = 0;

  fp = fopen(filename, "r");
  if(!fp)
  { fprintf(stderr, "Error: cannot open config file '%s'\n", filename);
    return 1;
  }

  while(fgets(line, sizeof(line), fp))
  { line_num++;

    /* Remove newline */
    ptr = strchr(line, '\n');
    if(ptr) *ptr = '\0';

    /* Trim whitespace */
    ptr = trim(line);

    /* Skip empty lines and comments */
    if(*ptr == '\0' || *ptr == '#' || *ptr == ';')
      continue;

    /* Section header */
    if(*ptr == '[')
    { ptr++;
      char* end = strchr(ptr, ']');
      if(!end)
      { fprintf(stderr, "Warning: malformed section at line %d\n",
                line_num);
        continue;
      }
      *end = '\0';
      strncpy(section, trim(ptr), sizeof(section) - 1);
      section[sizeof(section) - 1] = '\0';
      continue;
    }

    /* Key-value pair */
    key = ptr;
    value = strchr(ptr, '=');
    if(!value)
    { fprintf(stderr, "Warning: no '=' found at line %d\n", line_num);
      continue;
    }

    *value = '\0';
    value++;

    key = trim(key);
    value = trim(value);

    /* Remove quotes from value if present */
    if(*value == '"' || *value == '\'')
    { value++;
      char* end = value + strlen(value) - 1;
      if(*end == '"' || *end == '\'') *end = '\0';
    }

    /* Parse based on section and key */
    if(strcasecmp(section, "general") == 0)
    { if(strcasecmp(key, "verbose") == 0)
        cfg->verbose = parse_bool(value);
      else if(strcasecmp(key, "numsim") == 0)
      { cfg->numsim = atoi(value);
        if(cfg->numsim <= 0)
        { fprintf(stderr, "Warning: invalid numsim at line %d\n",
                  line_num);
          cfg->numsim = 1000;
        }
      }
    }
    else if(strcasecmp(section, "mode") == 0)
    { if(strcasecmp(key, "type") == 0)
      { cfg->mode = parse_mode(value);
        if(cfg->mode == MODE_NONE)
        { fprintf(stderr, "Warning: invalid mode at line %d\n",
                  line_num);
        }
      }
    }
    else if(strcasecmp(section, "ai") == 0)
    { if(strcasecmp(key, "agent") == 0)
      { if(cfg->ai_agent) free(cfg->ai_agent);
        cfg->ai_agent = strdup(value);
      }
    }
    else if(strcasecmp(section, "output") == 0)
    { if(strcasecmp(key, "file") == 0)
      { if(cfg->output_file) free(cfg->output_file);
        cfg->output_file = strdup(value);
      }
    }
  }

  fclose(fp);
  return 0;
}

/* Example usage and test function */
#ifdef CONFIG_TEST
int main(int argc, char** argv)
{ config_t cfg;

  if(argc < 2)
  { fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
    return 1;
  }

  /* Initialize config with defaults */
  memset(&cfg, 0, sizeof(config_t));
  cfg.verbose = false;
  cfg.numsim = 1000;

  /* Read config file */
  if(read_config_file(argv[1], &cfg) != 0)
    return 1;

  /* Print parsed values */
  printf("Configuration:\n");
  printf("  verbose: %s\n", cfg.verbose ? "true" : "false");
  printf("  numsim: %d\n", cfg.numsim);
  printf("  mode: %d\n", cfg.mode);
  if(cfg.ai_agent) printf("  ai_agent: %s\n", cfg.ai_agent);
  if(cfg.output_file) printf("  output_file: %s\n", cfg.output_file);

  cleanup_config(&cfg);
  return 0;
}
#endif