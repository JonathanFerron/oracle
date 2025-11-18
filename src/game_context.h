#ifndef GAME_CONTEXT_H
#define GAME_CONTEXT_H

#include "mtwister.h"
#include "game_types.h"

// Forward declaration
//typedef struct config config_t;

typedef struct
{ MTRand rng;
  config_t* config; // For runtime settings (numsim, modes, etc.)
  // Future: network_context, ui_context, etc.
} GameContext;

// Context management functions
GameContext* create_game_context(config_t* cfg);
void destroy_game_context(GameContext* ctx);

#endif // GAME_CONTEXT_H
