#ifndef MAIN_H
#define MAIN_H

typedef struct
{ bool debug_enabled;
  MTRand MTwister_rand_struct;
  config_t* config;
  // Future: network_context, ui_context, etc.
} GameContext;

#endif
