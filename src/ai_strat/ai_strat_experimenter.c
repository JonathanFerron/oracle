// ai_strat_experimenter.c
// Experimenter gap-filler strategy ("Weighted Mixture II" / The
// Experimenter) -- see doc/ai_agents.md's gap-3 section.
//
// The gap-3 cluster's one deliberate exception to "deterministic", same
// mechanism as gap-2's The Inconsistent (ai_strat_inconsistent.c): at
// every individual attack/defense decision (re-rolled independently, not
// once per game or per turn), rolls a weighted 3-sided die and delegates
// that single decision to whichever of A3 Borealis / A6 Tactical / A5
// Heuristic's own (unmodified) attack_strategy()/defense_strategy() it
// lands on. Jonathan's own framing: via experimentation, this agent is
// starting to get the hang of Heuristic's more advanced approach, but
// it's more intuition than mastered skill -- hence the heavier weight on
// `heuristic` than on `borealis`/`tactical`, unlike The Inconsistent's
// own heaviest weight landing on its single strongest contributor.
//
// Weights bounded to [20%, 60%] each (Jonathan's call, same bound as The
// Inconsistent) so no single contributor can dominate enough to erase
// the "experimenting" character. Never calls any contributor's
// *_set_params() override hook, so it can't corrupt the real
// AI_STRATEGY_BOREALIS/TACTICAL/HEURISTIC agents' own behavior in a
// round-robin that plays all of them (same reasoning as every other
// gap-filler agent's design note).

#include "ai_strat_experimenter.h"
#include "ai_strat_borealis.h"
#include "ai_strat_tactical.h"
#include "ai_strat_heuristic.h"
#include "../util/mtwister.h"

// Retargeted from the cluster's original 56 to 57 once The Sparring
// Partner/Opportunist/Adept's own measured ratings (54/54/60, not their
// 54/56/58 targets) were known -- 54->57->60 evenly bisects the actual
// remaining gap, rather than sitting close to the already-covered 54.
// See doc/ai_agents.md's gap-3 section and doc/changelog.md's 2026-09-11
// entry for the retargeting discussion.
//
// First tried at 22%/33%/45%: measured 58, within tolerance of 57 but
// not exact. Jonathan asked how much of a nudge from heuristic toward
// borealis (tactical held fixed) would land exactly on 57 -- a 5-point
// shift (22->27 / 45->40) hit it exactly, stable at both 6,000 and
// 340,000-game samples. Shipped at this exact value rather than the
// first, merely-in-tolerance one.
#define EXPERIMENTER_WEIGHT_BOREALIS  0.27f
#define EXPERIMENTER_WEIGHT_TACTICAL  0.33f
#define EXPERIMENTER_WEIGHT_HEURISTIC 0.40f

typedef enum
{ DELEGATE_BOREALIS,
  DELEGATE_TACTICAL,
  DELEGATE_HEURISTIC
} Delegate;

// One fresh roll per call -- per-decision, not per-turn or per-game (see
// this file's header comment).
static Delegate roll_delegate(GameContext* ctx)
{ double roll = genRand(&ctx->rng);

  if(roll < EXPERIMENTER_WEIGHT_BOREALIS) return DELEGATE_BOREALIS;
  if(roll < EXPERIMENTER_WEIGHT_BOREALIS + EXPERIMENTER_WEIGHT_TACTICAL)
    return DELEGATE_TACTICAL;
  return DELEGATE_HEURISTIC;
} // roll_delegate

void experimenter_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ switch(roll_delegate(ctx))
  { case DELEGATE_BOREALIS:
      borealis_attack_strategy(gstate, ctx);
      return;
    case DELEGATE_TACTICAL:
      tactical_attack_strategy(gstate, ctx);
      return;
    case DELEGATE_HEURISTIC:
      heuristic_attack_strategy(gstate, ctx);
      return;
  }
} // experimenter_attack_strategy

void experimenter_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ switch(roll_delegate(ctx))
  { case DELEGATE_BOREALIS:
      borealis_defense_strategy(gstate, ctx);
      return;
    case DELEGATE_TACTICAL:
      tactical_defense_strategy(gstate, ctx);
      return;
    case DELEGATE_HEURISTIC:
      heuristic_defense_strategy(gstate, ctx);
      return;
  }
} // experimenter_defense_strategy
