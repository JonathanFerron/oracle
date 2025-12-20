// game_context.c
// Game context implementation

#include "game_context.h"
#include "mtwister.h"
#include <stdlib.h>
#include <stdio.h>

GameContext* create_game_context(config_t* cfg)
{     if (cfg == NULL) {
        fprintf(stderr, "Error: config required for GameContext\n");
        return NULL;
    }
    
  GameContext* ctx = (GameContext*)malloc(sizeof(GameContext));
  if(ctx == NULL) return NULL;

  ctx->rng = seedRand(cfg->prng_seed);
  ctx->config = cfg;

  return ctx;
} // create_game_context

void destroy_game_context(GameContext* ctx)
{ if(ctx != NULL)
    free(ctx);
} // destroy_game_context
