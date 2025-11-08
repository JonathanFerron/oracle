### 

# PRNG Seed location

## Current State: Seed Lives in Two Places

```c
// In game_types.h - config_t struct
typedef struct {
    // ... other fields ...
    uint32_t prng_seed;
    bool use_random_seed;
} config_t;

// In game_context.h - GameContext struct
typedef struct {
    MTRand rng;           // Contains the seed's result (initialized state)
    config_t* config;
} GameContext;
```

## The Problem

The seed is stored in `config_t` but **also used to initialize `GameContext.rng`**, creating redundancy:

```c
GameContext* ctx = create_game_context(cfg->prng_seed, cfg);
// Now the seed's effect exists in ctx->rng
// And the seed value exists in ctx->config->prng_seed
```

## Keep Seed in Config

**Rationale**: The seed is a *configuration setting* - it controls how the program runs.

```c
// Simplified signature
GameContext* create_game_context(config_t* cfg) {
    GameContext* ctx = malloc(sizeof(GameContext));
    ctx->rng = seedRand(cfg->prng_seed);
    ctx->config = cfg;
    return ctx;
}

// Usage becomes cleaner
GameContext* ctx = create_game_context(cfg);
```

**Pros**:

- Seed is with other runtime settings (verbose, language, numsim)
- One source of truth
- Simpler function signature

**Cons**:

- Config can't be NULL (but maybe it shouldn't be?)
- Testing must create a config struct

Move seed logic fully into config, simplify GameContext creation:

### Implementation Steps

#### Step 1: Update `create_game_context()`

```c
// game_context.h
GameContext* create_game_context(config_t* cfg);

// game_context.c
GameContext* create_game_context(config_t* cfg) {
    if (cfg == NULL) {
        fprintf(stderr, "Error: config required for GameContext\n");
        return NULL;
    }

    GameContext* ctx = malloc(sizeof(GameContext));
    if (ctx == NULL) return NULL;

    ctx->rng = seedRand(cfg->prng_seed);
    ctx->config = cfg;

    return ctx;
}
```

#### Step 2: Update Call Sites

```c
// stda_auto.c
GameContext* ctx = create_game_context(cfg);

// stda_cli.c
GameContext* ctx = create_game_context(cfg);
```

#### Step 3: For Testing (Create Helper)

```c
// In test_helpers.c
config_t* create_test_config(uint32_t seed) {
    config_t* cfg = calloc(1, sizeof(config_t));
    cfg->prng_seed = seed;
    cfg->use_random_seed = false;
    cfg->mode = MODE_STDA_AUTO;
    // ... set other required fields to defaults ...
    return cfg;
}

// In tests
config_t* test_cfg = create_test_config(DETERMINISTIC_SEED);
GameContext* ctx = create_game_context(test_cfg);
// ... run test ...
free(test_cfg);
```

## Why this is Best for Your Project

1. **Seed IS configuration** - it's set via command-line, affects reproducibility, needs logging
2. **Aligns with future modes** - All modes (CLI, TUI, GUI, server) will need config anyway
3. **Simpler signature** - Less parameter passing
4. **Consistency** - Config already has `language`, `verbose`, `numsim` - seed fits here

## The Bug You Actually Have

In `stda_cli.c` line 457 (from your current code):

```c
// WRONG: Creates context with cfg's seed but doesn't pass cfg
GameContext* ctx = create_game_context(cfg->prng_seed, NULL);
```

This means `ctx->config` is NULL, so any code trying to access `ctx->config->verbose` will crash.
