// gui_config.c -- see gui_config.h.

#include <ctype.h>
#include <string.h>
#include <strings.h> // strcasecmp

#include "gui_config.h"

#define GUI_CONFIG_LINE_MAX 1024

void gui_config_defaults(GuiConfig* cfg)
{ cfg->font_path[0] = '\0';
  cfg->legacy_fractal = false;
} // gui_config_defaults

// Trims leading/trailing whitespace in place; returns the (possibly
// advanced) start of the trimmed string.
static char* trim(char* s)
{ while(isspace((unsigned char)*s))
    s++;
  size_t n = strlen(s);
  while(n > 0 && isspace((unsigned char)s[n - 1]))
    s[--n] = '\0';
  return s;
} // trim

static bool parse_bool(const char* v, bool* out)
{ if(!strcasecmp(v, "true") || !strcasecmp(v, "yes") || !strcasecmp(v, "on") || !strcmp(v, "1"))
  { *out = true;
    return true;
  }
  if(!strcasecmp(v, "false") || !strcasecmp(v, "no") || !strcasecmp(v, "off") || !strcmp(v, "0"))
  { *out = false;
    return true;
  }
  return false;
} // parse_bool

static void apply_gui_key(GuiConfig* cfg, const char* key, const char* value)
{ if(!strcmp(key, "font_path"))
  { if(strlen(value) >= sizeof(cfg->font_path))
      fprintf(stderr, "GUI config: font_path too long, ignored\n");
    else
      strcpy(cfg->font_path, value);
  }
  else if(!strcmp(key, "legacy_fractal"))
  { if(!parse_bool(value, &cfg->legacy_fractal))
      fprintf(stderr, "GUI config: legacy_fractal: expected true/false, got '%s'\n", value);
  }
  else
    fprintf(stderr, "GUI config: unknown [gui] key '%s'\n", key);
} // apply_gui_key

void gui_config_read(GuiConfig* cfg, FILE* in)
{ char raw[GUI_CONFIG_LINE_MAX];
  bool in_gui = false;

  while(fgets(raw, sizeof(raw), in))
  { char* line = trim(raw);
    if(!*line || *line == '#' || *line == ';')
      continue;

    if(*line == '[')
    { char* end = strchr(line, ']');
      if(end)
        *end = '\0';
      in_gui = end && !strcmp(trim(line + 1), "gui");
      continue;
    }

    char* eq = strchr(line, '=');
    if(!in_gui)
      continue;
    if(!eq)
    { fprintf(stderr, "GUI config: ignoring malformed line '%s'\n", line);
      continue;
    }
    *eq = '\0';
    apply_gui_key(cfg, trim(line), trim(eq + 1));
  }
} // gui_config_read

bool gui_config_load(GuiConfig* cfg, const char* path)
{ FILE* in = fopen(path, "r");
  if(!in)
    return false;
  gui_config_read(cfg, in);
  fclose(in);
  return true;
} // gui_config_load
