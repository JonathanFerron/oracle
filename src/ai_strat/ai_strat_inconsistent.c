// ai_strat_inconsistent.c
// Inconsistent gap-filler strategy ("Weighted Mixture" / The Inconsistent)
// -- see doc/ai_agents.md's gap-2 section.
//
// The gap-2 cluster's one deliberate exception to "deterministic": at
// every individual attack/defense decision (not once per game, not once
// per turn), rolls a weighted 3-sided die and delegates that single
// decision to whichever of A2 Combo Threshold / A4 Balanced Rules /
// A3 Borealis's own (unmodified) attack_strategy()/defense_strategy() it
// lands on. This is what gives it the intended personality -- "makes a
// really good move once in a while, so-so decisions the rest of the
// time" -- rather than just picking one fixed identity for the whole
// game. Each call reads the delegate's own compiled defaults directly
// (combo_threshold_attack_strategy() etc.); this agent never calls any
// contributor's *_set_params() override hook, so it can't corrupt the
// real AI_STRATEGY_COMBO_THRESHOLD/BALANCED/BOREALIS agents' own behavior
// in a round-robin that plays all of them (same reasoning as every other
// gap-filler agent's design note).
//
// Weights are bounded to [20%, 60%] each (Jonathan's call) so no single
// contributor dominates enough to make the other two irrelevant -- a
// mixture that's effectively "always Borealis" wouldn't read as
// "inconsistent" anymore. This is also this cluster's designated free
// variable: The Auditor/Journeyman/Impersonator were built and measured
// first (landing at 39/40/44), and these weights were tuned afterward to
// hit whichever target rating was left uncovered.

#include "ai_strat_inconsistent.h"
#include "ai_strat_combo_threshold.h"
#include "ai_strat_balanced_rules.h"
#include "ai_strat_borealis.h"
#include "../util/mtwister.h"

// Retargeted from the cluster's original 43 to 46 once The Impersonator's
// own measured rating (44, not its 46 target) was known -- 44/46/48
// (Impersonator/Inconsistent/A15) evenly bisects the remaining gap up to
// A15/Borealis, rather than clustering a fourth agent down near
// Auditor/Journeyman's already-close 39/40. See doc/ai_agents.md's gap-2
// section and doc/changelog.md's 2026-09-11 entry for the retargeting
// discussion. Measured exactly on target (46, --rating.games=10000) on
// the first weight combination tried -- no further search needed, and
// none of the three weights has headroom left to push higher on Borealis
// anyway (60% is this cluster's own bound).
#define INCONSISTENT_WEIGHT_COMBO    0.20f
#define INCONSISTENT_WEIGHT_BALANCED 0.20f
#define INCONSISTENT_WEIGHT_BOREALIS 0.60f

typedef enum
{ DELEGATE_COMBO,
  DELEGATE_BALANCED,
  DELEGATE_BOREALIS
} Delegate;

// One fresh roll per call -- this is what makes the re-roll per-decision
// rather than per-turn or per-game (see this file's header comment).
static Delegate roll_delegate(GameContext* ctx)
{ double roll = genRand(&ctx->rng);

  if(roll < INCONSISTENT_WEIGHT_COMBO) return DELEGATE_COMBO;
  if(roll < INCONSISTENT_WEIGHT_COMBO + INCONSISTENT_WEIGHT_BALANCED)
    return DELEGATE_BALANCED;
  return DELEGATE_BOREALIS;
} // roll_delegate

void inconsistent_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ switch(roll_delegate(ctx))
  { case DELEGATE_COMBO:
      combo_threshold_attack_strategy(gstate, ctx);
      return;
    case DELEGATE_BALANCED:
      balanced_rules_attack_strategy(gstate, ctx);
      return;
    case DELEGATE_BOREALIS:
      borealis_attack_strategy(gstate, ctx);
      return;
  }
} // inconsistent_attack_strategy

void inconsistent_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ switch(roll_delegate(ctx))
  { case DELEGATE_COMBO:
      combo_threshold_defense_strategy(gstate, ctx);
      return;
    case DELEGATE_BALANCED:
      balanced_rules_defense_strategy(gstate, ctx);
      return;
    case DELEGATE_BOREALIS:
      borealis_defense_strategy(gstate, ctx);
      return;
  }
} // inconsistent_defense_strategy
