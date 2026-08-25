# Local (No-GPU) Training Plan — Hardware-Grounded Notes

Captured 2026-08-25 from a scoping discussion, before `A10`/`A11` implementation
starts. Supersedes the **"Expected Training Requirements"** section of
`nn_mcts_overview.md` (which assumes an RTX 4090 / KataGo-scale regime) for
*this* project's actual hardware — that section still has reference value for
what large-scale training looks like, but the numbers below are what this
machine can actually do. See `about.md`'s "Design sources" for how this file
fits alongside the others.

## Hardware (verified via `lscpu`/`free`, not assumed)

ASRock DeskMini, i7-11700: 8 cores / 16 threads, boosts to 4.9GHz, **AVX-512
including VNNI** (int8 dot-product acceleration — PyTorch/oneDNN use this
automatically on Linux, no code changes needed), 30GB usable RAM. No discrete
GPU; the UHD 750 iGPU has no CUDA/ROCm path worth the setup cost at this
scale — train pure CPU, ignore the iGPU entirely.

## Deployed model: 50MB is a ceiling, not a target

Goal is a model transferable to PC/tablet/phone over WiFi in ~1 second,
transferred once (or on release, not per-session). 50MB in fp32 buys ~13M
parameters — far more than this game needs. Oracle's state/action space
(120-card fixed pool, ≤7-card hands, modest legal-action branching per turn)
doesn't need Go/Chess-scale depth; expect a workable policy/value net to land
in the low single-digit MB, well under budget. Don't design toward 50MB —
design for the game's actual complexity and let the ceiling be slack.

## Self-play corpus: disk is not the constraint

Verified via `df`/`lsblk`: root ext4 has 142G free now; `sda` (Kingston 480GB
SATA SSD) has ~227G unpartitioned; `nvme1n1` (WD 500GB NVMe) has ~365G
unpartitioned. 700GB+ of Linux-usable space is available between what's free
today and a quick partition/format of the unformatted headroom — several
orders of magnitude beyond any training corpus this game needs. Don't budget
"how many entries fit" — it's a non-question at this scale.

## The actual bottleneck: self-play generation throughput

Once corpus storage is a non-issue, the real cost is **generating** self-play
games, not storing or training on them:

- Small-net training (≤~1M params, tens-to-hundreds of thousands of
  examples) on this CPU: minutes, not hours, regardless of corpus size at
  this game's complexity.
- **A10's IS-MCTS simulations/rollouts per decision dominate wall-clock
  time** — orders of magnitude more expensive than the training step. Plan
  self-play capacity around *games/hour at a chosen simulation budget*, not
  around a target entry count.
- Game generation is embarrassingly parallel (each game is independent) —
  spread across all 8 cores/16 threads.

## Recommended recipe: single-pass distillation, not full AlphaZero loop

Don't necessarily run the full from-scratch AlphaZero cycle (network
bootstraps from random, self-plays, retrains, repeats over many generations)
— that's the expensive path, and this game's complexity doesn't require it
since `A10` (plain rollout IS-MCTS) already exists as a strong, correct
search-based teacher by the time `A11` starts.

Cheaper and likely sufficient for a "nudge above A10" rather than
learning from zero: run `A10` self-play (optionally vs. the weaker roster
agents for opponent diversity), log
`(information-set state, MCTS visit-count policy, game outcome)` per
decision, and train `A11`'s net to imitate that directly — the standard
"policy/value net learns from tree search" distillation step from
AlphaZero, without the iterative self-improvement loop. Full iterative
self-play (`A11` vs itself, retrain, repeat) remains an option later if a
single distillation pass undershoots the target rating (~97, "AlphaOracle
Prime" per the roster), but start with the cheap version.

## Information-set encoding: only what the deciding player can see

The logged/encoded state must be the **information set visible to the
deciding player** (own hand, discard, board, opponent's known cards/energy)
— not full hidden game state — or the net learns from information it won't
have at inference. This is a correctness requirement on `A10`'s
determinization/state representation, which `A11` inherits directly.

## Reshuffle narrows the information set — encode composition and order separately

Raised during scoping and worth designing for from the start, since it
affects `A10`'s determinization correctness (not just `A11`'s NN encoding —
see note below):

Per `card_actions.c`'s `shuffle_discard_and_form_deck()`, deck/discard are
**per-player**; when a player's deck empties, *that player's own discard*
reshuffles into a new deck for them (`doc/game_rules_doc.md` line 265). Before
any reshuffle, a player's remaining unseen deck is uniformly unknown — both
composition and order. **After a reshuffle, the composition of that deck is
exactly known** (it's precisely that player's prior discard pile, which was
visible), while only the **draw order** remains hidden. Treating
"not-in-hand, not-yet-played" cards as one uniform unknown pool — as a naive
determinization would — throws away real information once a reshuffle has
happened.

Concrete implications:

- **`A10` determinization**: after a player's reshuffle, sample permutations
  of that player's *known* reshuffled-deck composition rather than sampling
  card identities from the full remaining unseen pool. This is a tighter,
  more accurate determinization — a correctness improvement to `A10` itself,
  independent of whether `A11` ever gets built. Worth a pointer note in the
  `A10` folder so it isn't lost before `A11` starts (see below).
- **Opponent-hand inference**: any card the opponent draws *after* their own
  reshuffle comes from a pool of known composition — each such draw is a
  hypergeometric draw from a known multiset, giving a tighter posterior on
  plausible hand contents than draws from a still-partially-unknown pool.
  This is a real, usable signal, not just a modeling nicety.
- **`A11` state encoding**: represent, per player, a "known remaining
  composition since last reshuffle" feature (counts by card type in that
  player's known-but-unordered deck) separately from "fully unknown pool"
  (cards never seen at all — before either player's first reshuffle, or
  cards outside both players' decks entirely, if any exist). Track a
  cards-drawn-since-reshuffle counter to keep narrowing the known pool as it
  gets consumed. Collapsing these into one undifferentiated "unknown" feature
  would discard information the net could otherwise learn to exploit.

A short cross-reference pointing here was added to the `A10` folder
(`about.md`) so the determinization fix lands when `A10` is actually built,
rather than being rediscovered later during `A11` work.
