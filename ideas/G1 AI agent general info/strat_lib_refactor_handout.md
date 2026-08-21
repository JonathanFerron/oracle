# Refactoring Task: Extract Shared AI Heuristics into `strat_lib.c/h`

## Goal

Create a new shared library module (`src/strat_lib.c` / `src/strat_lib.h`) that holds
power-heuristic-based decision logic currently embedded directly in game/mode code.
This logic is AI strategy logic, not game rule logic, and should be reusable across
multiple future AI strategies (random, balanced, value-based, Borealis, heuristic, etc.).

Follow the project's existing conventions:
- ≤35 lines per function (firm limit 100)
- ≤400 lines per file (soft limit 500, firm limit 1000)
- snake_case naming, existing brace/indent style (see other `strat_*.c` files)
- Manual implementation preferred over macro magic
- Follow the refactoring methodology in `doc/REFACTORING.md` (Part 1: new code,
  Part 2: structural updates, Part 3: pattern replacements, Part 4: cleanup)

---

## Part 1: New Files to Create

### `src/strat_lib.h`

Declare parameterized, reusable heuristic functions. Suggested initial API:

```c
#ifndef STRAT_LIB_H
#define STRAT_LIB_H

#include "game_types.h"
#include "game_context.h"

// Mulligan heuristics
void power_based_mulligan(struct gamestate* gstate, PlayerID player,
                           float power_threshold, uint8_t max_cards,
                           GameContext* ctx);

// Discard-to-N heuristics
void power_based_discard_to_n(struct gamestate* gstate, PlayerID player,
                               uint8_t target_hand_size, GameContext* ctx);

// Cash exchange selection heuristics
uint8_t select_lowest_power_champion(Hand* hand);

#endif // STRAT_LIB_H
```

Leave room to add `cost_efficiency_*` and `select_lowest_efficiency_champion()`
variants later — stub them out only if trivial, otherwise just leave a comment
placeholder so the file doesn't grow unnecessarily in this pass.

### `src/strat_lib.c`

Move logic (not just copy — see Part 4 for removal) from:

1. **`apply_mulligan()`** in `src/stda_auto.c` → becomes `power_based_mulligan()`
   - Generalize: accept `player`, `power_threshold`, and `max_cards` as parameters
     instead of hardcoding `PLAYER_B` and `2`.
   - Preserve exact existing behavior when called with
     `AVERAGE_POWER_FOR_MULLIGAN` and `max_cards = 2`.

2. **`discard_to_7_cards()`** in `src/card_actions.c` → becomes
   `power_based_discard_to_n()`
   - Generalize: accept `target_hand_size` parameter instead of hardcoded `7`.

3. **`select_champion_for_cash_exchange()`** in `src/card_actions.c` → becomes
   `select_lowest_power_champion()`
   - Signature stays essentially the same (takes `Hand*`, returns `uint8_t`).

Each function must include its own brief doc comment describing the heuristic
and noting it is shared/reusable across strategies.

---

## Part 2: Structural Updates to Existing Files

### `src/strat_random.h`

Add declarations:

```c
void random_mulligan_strategy(struct gamestate* gstate, PlayerID player,
                               GameContext* ctx);
void random_discard_to_7_strategy(struct gamestate* gstate, PlayerID player,
                                   GameContext* ctx);
uint8_t random_select_champion_for_exchange(Hand* hand);
```

### `src/strat_random.c`

Add thin wrapper implementations that call into `strat_lib`:

```c
#include "strat_lib.h"

void random_mulligan_strategy(struct gamestate* gstate, PlayerID player,
                               GameContext* ctx)
{ power_based_mulligan(gstate, player, AVERAGE_POWER_FOR_MULLIGAN, 2, ctx);
}

void random_discard_to_7_strategy(struct gamestate* gstate, PlayerID player,
                                   GameContext* ctx)
{ power_based_discard_to_n(gstate, player, 7, ctx);
}

uint8_t random_select_champion_for_exchange(Hand* hand)
{ return select_lowest_power_champion(hand);
}
```

### `src/card_actions.c` / `src/card_actions.h`

- Remove `select_champion_for_cash_exchange()` (moved to `strat_lib.c`).
- Remove `discard_to_7_cards()` (moved to `strat_lib.c`).
- Update `play_cash_card()` in `card_actions.c`: it currently calls
  `select_champion_for_cash_exchange()` directly. This needs a strategy
  function pointer or parameter passed in — see **Part 3, Pattern 3** below for
  how to thread this through without breaking the call chain.

### `src/stda_auto.c` / `src/stda_auto.h`

- Remove `apply_mulligan()` (moved to `strat_lib.c` + `strat_random.c` wrapper).
- Update `play_stda_auto_game()` to call `random_mulligan_strategy()` instead
  of `apply_mulligan()` (temporary — see Part 3, Pattern 1, for the proper
  long-term StrategySet wiring).

### `src/turn_logic.c`

- Update `end_of_turn()`: currently calls `discard_to_7_cards()` directly.
  Same threading concern as `play_cash_card()` — see Part 3, Pattern 3.

### `src/cli_game.c`

- `handle_interactive_discard_to_7()` currently falls back to
  `discard_to_7_cards()` on input error. Update this call site once the
  function is renamed/relocated.

### `Makefile`

- Ensure `strat_lib.c` is picked up (should be automatic if using
  `$(shell find $(SRCDIR) ...)` source discovery — verify).

---

## Part 3: Patterns

### Pattern 1: Mulligan call site replacement

**Find:** `apply_mulligan(gstate, ctx);` (in `stda_auto.c`, `stda_cli.c`)
**Replace:** `random_mulligan_strategy(gstate, PLAYER_B, ctx);`
**Locations:**
- `stda_auto.c:play_stda_auto_game()`
- `stda_cli.c:run_mode_stda_cli()` (the non-interactive branch)

```diff
- apply_mulligan(gstate, ctx);
+ random_mulligan_strategy(gstate, PLAYER_B, ctx);
```

> Note: this hardcodes the Random AI's mulligan approach as the only current
> AI implementation. This is acceptable for now since Random is the only
> implemented AI strategy. When Balanced/Heuristic AI strategies are added,
> the correct strategy's mulligan function should be selected based on
> `pconfig->ai_strategies[player]` (see Pattern 2 below for the general
> long-term shape of this).

### Pattern 2: StrategySet extension (future-facing, do NOT implement fully yet)

Just leave a `// TODO:` comment in `strategy.h` noting that `StrategySet`
will eventually need `mulligan_strategy[2]`, `discard_strategy[2]`, and
`exchange_select[2]` function pointer arrays, analogous to
`attack_strategy[2]` / `defense_strategy[2]`, once more than one AI strategy
exists. Don't wire this up yet — it's premature given only Random AI exists,
but the comment should describe the shape so it's easy later.

### Pattern 3: Threading strategy choice through `play_cash_card()` and `end_of_turn()`

For now (single AI strategy = Random), simplest fix:

**`card_actions.c:play_cash_card()`**
```diff
- uint8_t champion_to_exchange = select_champion_for_cash_exchange(&gstate->hand[player]);
+ uint8_t champion_to_exchange = random_select_champion_for_exchange(&gstate->hand[player]);
```
Requires `#include "strat_random.h"` in `card_actions.c`.

**`turn_logic.c:end_of_turn()`**
```diff
- discard_to_7_cards(gstate, ctx);
+ random_discard_to_7_strategy(gstate, gstate->current_player, ctx);
```
Requires `#include "strat_random.h"` in `turn_logic.c`.

> This is a known temporary shortcut (hardcoding Random AI's heuristic as the
> only path) and should be flagged with a `// TODO:` comment referencing
> Pattern 2 above, to be revisited once multiple AI strategies exist and the
> active player's assigned strategy must be looked up instead of assuming
> Random.

### Pattern 4: `cli_game.c` fallback call site

**Find:** `discard_to_7_cards(gstate, ctx);` (in `handle_interactive_discard_to_7()`
error-handling fallback)
**Replace:** `random_discard_to_7_strategy(gstate, gstate->current_player, ctx);`

---

## Part 4: Cleanup

- Delete the now-unused original function bodies from `stda_auto.c` and
  `card_actions.c` (do not leave dead code behind).
- Remove now-unneeded includes if `stda_auto.c` / `card_actions.c` no longer
  reference anything that required them.
- Update `card_actions.h` to remove the two moved function declarations.
- Update `stda_auto.h` to remove `apply_mulligan()` declaration.
- Update `doc/oracle_design.md`:
  - Add `strat_lib.c/h` to the Module Organization file tree and module
    dependency list.
  - Update `card_actions.c` module description to remove references to the
    two heuristic functions (they no longer belong there).
  - Note in "Architectural Boundaries" table that cash card selection and
    discard-to-N heuristics now live in `strat_lib.c`, called via
    `strat_random.c` wrappers.
- Update `doc/oracle_todo.md`: check off / annotate the existing TODO items
  referencing these three functions as "moved to strat_lib.c" rather than
  deleting the history.

---

## Testing Checkpoints

1. Build cleanly with no new warnings (`-Wall -Wextra`).
2. Run `stda.auto` mode for a batch of simulations (e.g. `-a -n 1000`) and
   confirm win/loss/draw statistics are statistically consistent with
   pre-refactor behavior (same PRNG seed should give identical results).
3. Run `stda.cli` mode interactively:
   - Confirm mulligan phase behavior unchanged (Player B, AI path).
   - Confirm discard-to-7 AI fallback behavior unchanged.
   - Confirm cash card exchange still selects lowest-power champion.
4. Confirm no leftover references to the old function names anywhere in
   `src/` (`grep -rn "apply_mulligan\|select_champion_for_cash_exchange\|discard_to_7_cards" src/`).
