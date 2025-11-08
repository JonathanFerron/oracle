// game_context.c
// Game context implementation

#include "game_context.h"
#include "mtwister.h"
#include <stdlib.h>

GameContext* create_game_context(uint32_t seed, config_t* cfg) // TODO: i believe that the seed parameter could just be pulled from the cfg struct, provided it is set properly before calling this function.
{ GameContext* ctx = (GameContext*)malloc(sizeof(GameContext));
  if(ctx == NULL) return NULL;

  ctx->rng = seedRand(seed);
  ctx->config = cfg;

  return ctx;
} // create_game_context

void destroy_game_context(GameContext* ctx)
{ if(ctx != NULL)
    free(ctx);
} // destroy_game_context
