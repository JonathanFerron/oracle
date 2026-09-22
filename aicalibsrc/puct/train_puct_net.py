#!/usr/bin/env python3
"""A14 AlphaOracle Prime Plus I Stage 3 two-head (value + policy) net trainer.

Trains a small CPU MLP with a shared trunk (BatchNorm1d(537) -> 256 -> 128 ->
64, same architecture as A11's own value net) and TWO heads off that shared
64-wide output: a value head (1, sigmoid, same 0.0/0.5/1.0 scale as A11's) and
a policy head (PUCT_POLICY_DIM=218 raw logits, no activation here -- the
composed-and-masked softmax is computed in this script's own
compose_policy_scores(), the exact torch mirror of
ai_strat_puct_policy.c's puct_move_score()/puct_compose_priors(), and again
in ai_strat_puct_net.c's C forward pass -- three independent implementations
of the same formula that must all agree; see export_puct_weights.py's own
numeric verification for the export-time check).

Corpus format (aicalibsrc/puct/gen_policy_corpus.c): each record is 1692
raw float32s as of 2026-09-22 -- 537 (ISMCTSNNStateVector) + 1 (outcome) +
1 (num_moves) + 1 (total_visits, added 2026-09-22) + 128*9 (per-move type,
count, play[3], target[3], visit_fraction). Pre-2026-09-22 shards (1691
floats, no total_visits) still load -- see load_records()'s own comment.
The teacher is A11 (ismctsnn), not A14 itself -- see gen_policy_corpus.c's
own header for why. Data split is by WHOLE SHARD, not by random row, same
reasoning and the same --val-seeds/--max-train-records tooling as A11's
train_value_net.py (avoids leaking correlated decisions from the same game
across train/val; lets a bigger future corpus stay comparable to a smaller
run's own reported metrics).

Policy loss is soft-target cross-entropy against the visit_fraction
distribution (AlphaZero's own -pi^T log(p) formulation). Two DIFFERENT
things are both called "temperature" here, deliberately kept distinct in
naming: PUCTParams.policy_temperature (calibrated in Stage 5) reshapes the
NET's own predicted logits at INFERENCE time and is untouched by this
script; --policy-target-temperature below (added 2026-09-22, A16 Session 1
item 1.3) reshapes the recorded visit-fraction TARGET at TRAINING time
(pi_i ~ visit_fraction_i^(1/tau)), default 1.0 (identity, this trainer's
original behaviour, corpus data unchanged either way).

Usage:
    .venv/bin/python train_puct_net.py corpus --label full
    .venv/bin/python train_puct_net.py corpus --label full --policy-weight 1.0 --dropout 0.4 \\
        --weight-decay 1e-3 --lr 3e-4
"""

import argparse
import json
import os
import re
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn

STATE_DIM = 537
POLICY_DIM = 218  # PUCT_POLICY_DIM, ai_strat_puct_policy.h
MAX_MOVES = 128  # MOVE_GEN_MAX_MOVES
FLOATS_PER_MOVE = 9  # type, count, play[3], target[3], visit_fraction
# total_visits (added 2026-09-22, A16 Session 1 item 1.3) sits between
# num_moves and the move block -- see gen_policy_corpus.c's own header
# comment. OLD_RECORD_DIM is what every shard generated before that date
# used (no total_visits column); load_records() detects each file's own
# width from its size and loads either, so pre-2026-09-22 shards keep
# working without regeneration.
OLD_RECORD_DIM = STATE_DIM + 1 + 1 + MAX_MOVES * FLOATS_PER_MOVE  # 1691
RECORD_DIM = OLD_RECORD_DIM + 1  # 1692
HIDDEN = (256, 128, 64)
# Same curated pool as A11's own gen_corpus.c/train_value_net.py -- vs_a4/vs_a6
# are supported by gen_policy_corpus.c but weren't generated for the shipped
# corpus (run_selfplay.sh's default matchups_csv is mirror,vs_a7,vs_a3).
MATCHUPS = ("mirror", "vs_a7", "vs_a3", "vs_a4", "vs_a6")
SHARD_RE = re.compile(
    r"^(?P<label>.+)_(?P<matchup>mirror|vs_a7|vs_a3|vs_a4|vs_a6)_seed(?P<seed>\d+)\.bin$")

# Policy logit layout, mirroring ai_strat_puct_policy.h exactly.
PLAY_OFFSET = 0
TARGET_OFFSET = 105
TYPE_OFFSET = 210
TYPE_COUNT = 5
COUNT_OFFSET = 215
COUNT_SLOTS = 3


def discover_shards(corpus_dir, labels):
    """Groups every '<label>_<matchup>_seed<N>.bin' file in corpus_dir by
    matchup, sorted by seed ascending, for any label in `labels` (a set)."""
    groups = {m: [] for m in MATCHUPS}
    for p in sorted(Path(corpus_dir).glob("*_*_seed*.bin")):
        m = SHARD_RE.match(p.name)
        if not m or m.group("label") not in labels:
            continue
        groups[m.group("matchup")].append((int(m.group("seed")), p))
    for matchup in groups:
        groups[matchup].sort(key=lambda t: t[0])
    return groups


def split_train_val(groups, holdout_per_matchup):
    """Per matchup: the highest-seeded holdout_per_matchup shard(s) become
    validation, the rest train. Never holds out a matchup's only shard."""
    train, val = {}, {}
    for matchup, shards in groups.items():
        n_holdout = min(holdout_per_matchup, max(0, len(shards) - 1))
        val_shards = shards[len(shards) - n_holdout:] if n_holdout else []
        train_shards = shards[:len(shards) - n_holdout]
        train[matchup] = [p for _, p in train_shards]
        val[matchup] = [p for _, p in val_shards]
    return train, val


def parse_val_seeds(spec):
    """Parses --val-seeds "mirror=10,vs_a7=11,vs_a3=12" into {matchup: seed}."""
    if not spec:
        return None
    out = {}
    for part in spec.split(","):
        matchup, seed = part.split("=")
        matchup = matchup.strip()
        if matchup not in MATCHUPS:
            raise SystemExit(f"--val-seeds: unknown matchup '{matchup}' (want one of {MATCHUPS})")
        out[matchup] = int(seed)
    return out


def split_train_val_explicit(groups, val_seeds):
    """Per matchup, the shard whose seed == val_seeds[matchup] becomes
    validation, every other shard for that matchup trains. A matchup with
    no entry in val_seeds sends all its shards to training."""
    train, val = {}, {}
    for matchup, shards in groups.items():
        if not shards:
            train[matchup], val[matchup] = [], []
            continue
        if matchup not in val_seeds:
            train[matchup] = [p for _, p in shards]
            val[matchup] = []
            continue
        want = val_seeds[matchup]
        val_shards = [p for seed, p in shards if seed == want]
        if not val_shards:
            raise SystemExit(f"--val-seeds: no {matchup} shard with seed={want} in this corpus")
        train[matchup] = [p for seed, p in shards if seed != want]
        val[matchup] = val_shards
    return train, val


def load_records(paths):
    """Returns (state[N,537], outcome[N], num_moves[N] int64, total_visits[N]
    float32, moves[N,128,9]). Detects each file's own record width from its
    byte size -- RECORD_DIM (1692, with total_visits) for shards generated
    2026-09-22 or later, OLD_RECORD_DIM (1691, without it) for anything
    older -- rather than assuming one width for the whole corpus, so
    pre-2026-09-22 shards keep loading without regeneration. The two
    widths are coprime, so a file's size being divisible by both would
    need ~2.86 million records in one shard, far past anything this
    project generates -- detection stays unambiguous in practice.
    Old-format records report total_visits as NaN (never recorded, not a
    real zero) in the column gen_policy_corpus.c now writes it to."""
    if not paths:
        return (np.empty((0, STATE_DIM), dtype=np.float32), np.empty((0,), dtype=np.float32),
                np.empty((0,), dtype=np.int64), np.empty((0,), dtype=np.float32),
                np.empty((0, MAX_MOVES, FLOATS_PER_MOVE), dtype=np.float32))
    rows = []
    for p in paths:
        arr = np.fromfile(p, dtype=np.float32)
        if arr.size % RECORD_DIM == 0:
            rows.append(arr.reshape(-1, RECORD_DIM))
        elif arr.size % OLD_RECORD_DIM == 0:
            old = arr.reshape(-1, OLD_RECORD_DIM)
            nan_col = np.full((old.shape[0], 1), np.nan, dtype=np.float32)
            rows.append(np.concatenate(
                [old[:, :STATE_DIM + 2], nan_col, old[:, STATE_DIM + 2:]], axis=1))
        else:
            raise AssertionError(f"{p}: size not a multiple of RECORD_DIM ({RECORD_DIM}) or "
                                 f"the pre-2026-09-22 OLD_RECORD_DIM ({OLD_RECORD_DIM})")
    data = np.concatenate(rows, axis=0)
    state = data[:, :STATE_DIM].copy()
    outcome = data[:, STATE_DIM].copy()
    num_moves = data[:, STATE_DIM + 1].astype(np.int64).copy()
    total_visits = data[:, STATE_DIM + 2].copy()
    moves = data[:, STATE_DIM + 3:].reshape(-1, MAX_MOVES, FLOATS_PER_MOVE).copy()
    return state, outcome, num_moves, total_visits, moves


def apply_policy_temperature(moves, num_moves, temperature):
    """Reshapes the visit_fraction target column (moves[...,8]) in place:
    pi_i ~ fraction_i^(1/temperature), renormalized over each record's own
    legal moves (masked by num_moves). temperature=1.0 is a no-op. Uses
    visit_fraction alone, not total_visits: visit_i = fraction_i *
    total_visits, so visit_i^(1/tau) = fraction_i^(1/tau) *
    total_visits^(1/tau), and the second factor is constant across one
    record's moves, cancelling under renormalization -- see
    gen_policy_corpus.c's own header comment. Works identically whether or
    not total_visits was actually recorded (old-format shards included)."""
    if temperature == 1.0:
        return moves
    move_range = torch.arange(moves.shape[1], device=moves.device).unsqueeze(0)
    legal_mask = (move_range < num_moves.unsqueeze(1)).float()
    frac = moves[:, :, 8]
    reshaped = torch.where(frac > 0, frac.clamp(min=1e-12).pow(1.0 / temperature),
                           torch.zeros_like(frac)) * legal_mask
    total = reshaped.sum(dim=1, keepdim=True).clamp(min=1e-12)
    moves[:, :, 8] = reshaped / total
    return moves


class PUCTNet(nn.Module):
    """Shared trunk (BatchNorm1d -> hidden Linear/ReLU[/Dropout] stack) with
    two separate head modules (not buried in one nn.Sequential like A11's
    single-head ValueNet) -- keeps export_puct_weights.py from needing the
    same dropout-shifts-Sequential-indices care A11's exporter needs for its
    own single head."""

    def __init__(self, input_dim=STATE_DIM, hidden=HIDDEN, policy_dim=POLICY_DIM, dropout=0.0):
        super().__init__()
        layers = [nn.BatchNorm1d(input_dim)]
        prev = input_dim
        for h in hidden:
            layers += [nn.Linear(prev, h), nn.ReLU()]
            if dropout > 0:
                layers.append(nn.Dropout(dropout))
            prev = h
        self.trunk = nn.Sequential(*layers)
        self.value_head = nn.Linear(prev, 1)
        self.policy_head = nn.Linear(prev, policy_dim)

    def forward(self, x):
        h = self.trunk(x)
        value = torch.sigmoid(self.value_head(h)).squeeze(-1)
        policy_logits = self.policy_head(h)
        return value, policy_logits


def compose_policy_scores(policy_logits, moves):
    """Vectorized torch mirror of puct_move_score() (ai_strat_puct_policy.c)
    over every one of the 128 move slots at once. `policy_logits`: (B,218).
    `moves`: (B,128,9) raw columns [type,count,play0,play1,play2,target0,
    target1,target2,visit_fraction]. Returns (B,128) raw (pre-softmax,
    pre-mask) scores -- masking to num_moves happens in the caller, since
    that's a property of the move LIST, not the composition formula itself
    (matching puct_compose_priors()'s own division of labour)."""
    B = moves.shape[0]
    play_logits = policy_logits[:, PLAY_OFFSET:PLAY_OFFSET + 105]
    target_logits = policy_logits[:, TARGET_OFFSET:TARGET_OFFSET + 105]
    type_logits = policy_logits[:, TYPE_OFFSET:TYPE_OFFSET + TYPE_COUNT]
    count_logits = policy_logits[:, COUNT_OFFSET:COUNT_OFFSET + COUNT_SLOTS]

    type_idx = moves[:, :, 0].long()
    type_score = torch.gather(type_logits, 1, type_idx)  # (B,128)

    count_idx = moves[:, :, 1].long()
    has_count = (count_idx > 0).float()
    count_idx_c = (count_idx - 1).clamp(min=0)
    count_score = torch.gather(count_logits, 1, count_idx_c) * has_count

    play_idx = moves[:, :, 2:5].long()  # (B,128,3)
    valid_play = (play_idx >= 0).float()
    play_idx_c = play_idx.clamp(min=0).reshape(B, -1)
    play_gathered = torch.gather(play_logits, 1, play_idx_c).reshape(B, MAX_MOVES, 3)
    play_score = (play_gathered * valid_play).sum(dim=2) / valid_play.sum(dim=2).clamp(min=1)

    target_idx = moves[:, :, 5:8].long()  # (B,128,3)
    valid_target = (target_idx >= 0).float()
    target_idx_c = target_idx.clamp(min=0).reshape(B, -1)
    target_gathered = torch.gather(target_logits, 1, target_idx_c).reshape(B, MAX_MOVES, 3)
    target_score = (target_gathered * valid_target).sum(dim=2) / valid_target.sum(dim=2).clamp(min=1)

    return type_score + count_score + play_score + target_score  # (B,128)


def policy_loss_and_metrics(policy_logits, moves, num_moves):
    """Soft-target cross-entropy against visit_fraction (moves[...,8]),
    masked to each record's own num_moves. Returns (mean_loss, top1_agree,
    mean_entropy) -- top1_agree is the fraction of records where the net's
    own argmax matches the visit distribution's argmax (an interpretable
    "does it pick the same best move" diagnostic alongside the loss)."""
    B = moves.shape[0]
    scores = compose_policy_scores(policy_logits, moves)
    move_range = torch.arange(MAX_MOVES, device=moves.device).unsqueeze(0)
    legal_mask = move_range < num_moves.unsqueeze(1)  # (B,128) bool
    masked_scores = scores.masked_fill(~legal_mask, float("-inf"))
    log_probs = torch.log_softmax(masked_scores, dim=1)
    log_probs = torch.nan_to_num(log_probs, neginf=0.0)  # a record with num_moves==0 never
    # occurs in practice (MOVE_PASS always legal) but guards div-by-zero softmax degenerately

    target = moves[:, :, 8]  # already sums to 1 over the legal slots, 0 elsewhere
    per_example = -(target * log_probs).sum(dim=1)
    loss = per_example.mean()

    with torch.no_grad():
        pred_top1 = masked_scores.argmax(dim=1)
        target_top1 = target.argmax(dim=1)
        top1_agree = (pred_top1 == target_top1).float().mean().item()
        probs = log_probs.exp()
        entropy = -(probs * torch.nan_to_num(log_probs, neginf=0.0)).sum(dim=1).mean().item()
    return loss, top1_agree, entropy


def policy_target_baselines(moves, num_moves):
    """Two data-only baselines for the policy target, no model involved --
    added 2026-09-22 (A16 Session 1, item 1.2) so val_p_loss reads as
    "fraction of available headroom captured" instead of a bare number.
    `uniform_ce`: cross-entropy of a UNIFORM distribution over the legal
    moves against the visit_fraction target -- equals mean(log(num_moves))
    since the target sums to 1 over legal slots, no model needed to define
    it. `target_entropy`: the target distribution's OWN entropy, the
    irreducible floor even a perfect net can't beat under this soft-target
    CE loss (its global minimum is exactly target_entropy). Their
    difference is the real learnable headroom -- see doc/ai_agents.md's
    A14 section (Finding 2, folded in from ideas/A16 .../about.md): on the
    shipped corpus this was tiny (~0.128 nats out of ~2.27), not because
    the head was broken but because 4000-iteration search rarely
    concentrates once legal-move count climbs past ~25.
    torch.xlogy(target, target) is 0 wherever target==0 by definition,
    sidestepping the log(0)=-inf/NaN case on every illegal or zero-visit
    slot without a separate mask."""
    if moves.shape[0] == 0:
        return float("nan"), float("nan")
    target = moves[:, :, 8]
    target_entropy = (-torch.xlogy(target, target).sum(dim=1)).mean().item()
    uniform_ce = torch.log(num_moves.float()).mean().item()
    return uniform_ce, target_entropy


def predict(model, X_t, batch_size=8192):
    model.eval()
    if X_t.shape[0] == 0:
        return torch.empty(0), torch.empty(0, POLICY_DIM)
    values, policies = [], []
    with torch.no_grad():
        for start in range(0, X_t.shape[0], batch_size):
            v, p = model(X_t[start:start + batch_size])
            values.append(v)
            policies.append(p)
    return torch.cat(values), torch.cat(policies)


def iterate_batches(n, batch_size, generator):
    perm = torch.randperm(n, generator=generator)
    for start in range(0, n, batch_size):
        yield perm[start:start + batch_size]


def directional_accuracy(pred, actual):
    if pred.shape[0] == 0:
        return float("nan")
    return ((pred - 0.5).sign() == (actual - 0.5).sign()).float().mean().item()


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("corpus_dir", help="directory holding <label>_<matchup>_seed<N>.bin shards")
    ap.add_argument("--label", default="full",
                    help="comma-separated list of labels to pool together")
    ap.add_argument("--val-seeds", default=None,
                    help="pin validation to specific seeds per matchup, e.g. "
                         "mirror=1,vs_a7=2,vs_a3=3")
    ap.add_argument("--max-train-records", type=int, default=None,
                    help="randomly subsample the training set (not validation), fixed by --seed")
    ap.add_argument("--policy-weight", type=float, default=1.0,
                    help="weight on the policy cross-entropy term added to value MSE")
    ap.add_argument("--policy-target-temperature", type=float, default=1.0,
                    help="reshapes the visit_fraction TARGET at training time, "
                         "pi_i ~ fraction_i^(1/tau); 1.0 (default) is the corpus's "
                         "own recorded distribution, unchanged -- NOT the same as "
                         "PUCTParams.policy_temperature (an inference-time dial on "
                         "the net's own predictions, untouched by this script)")
    ap.add_argument("--max-seconds", type=float, default=3600)
    ap.add_argument("--max-epochs", type=int, default=500)
    ap.add_argument("--batch-size", type=int, default=4096)
    ap.add_argument("--lr", type=float, default=3e-4)
    ap.add_argument("--weight-decay", type=float, default=1e-3, help="Adam L2 penalty")
    ap.add_argument("--dropout", type=float, default=0.4, help="dropout after each hidden ReLU")
    ap.add_argument("--patience", type=int, default=0,
                    help="stop after this many epochs with no val total-loss improvement "
                         "(0 = disabled)")
    ap.add_argument("--threads", type=int, default=max(1, int(os.cpu_count() * 0.75)))
    ap.add_argument("--holdout-shards-per-matchup", type=int, default=1)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--out-dir", default=None, help="default: <corpus_dir>/../checkpoints")
    ap.add_argument("--run-name", default=None, help="default: --label")
    return ap.parse_args()


def main():
    args = parse_args()
    torch.set_num_threads(args.threads)
    torch.manual_seed(args.seed)

    labels = {s.strip() for s in args.label.split(",") if s.strip()}
    groups = discover_shards(args.corpus_dir, labels)
    print(f"Shards found for label(s)={sorted(labels)}:")
    for m in MATCHUPS:
        print(f"  {m:8s}: {len(groups[m])} shard(s) -- {[p.name for _, p in groups[m]]}")

    val_seeds = parse_val_seeds(args.val_seeds)
    if val_seeds is not None:
        train_files, val_files = split_train_val_explicit(groups, val_seeds)
    else:
        train_files, val_files = split_train_val(groups, args.holdout_shards_per_matchup)
    print("Validation holdout:")
    for m in MATCHUPS:
        print(f"  {m:8s}: {[p.name for p in val_files[m]]}")

    Str_parts, otr_parts, ntr_parts, mtr_parts = [], [], [], []
    val_by_matchup = {}
    for m in MATCHUPS:
        s, o, n, _tv, mv = load_records(train_files[m])  # total_visits unused
        # on the train side -- see apply_policy_temperature()'s own comment
        # on why the temperature transform never needs it
        Str_parts.append(s)
        otr_parts.append(o)
        ntr_parts.append(n)
        mtr_parts.append(mv)
        val_by_matchup[m] = load_records(val_files[m])
    Str = np.concatenate(Str_parts) if Str_parts else np.empty((0, STATE_DIM), dtype=np.float32)
    otr = np.concatenate(otr_parts) if otr_parts else np.empty((0,), dtype=np.float32)
    ntr = np.concatenate(ntr_parts) if ntr_parts else np.empty((0,), dtype=np.int64)
    mtr = np.concatenate(mtr_parts) if mtr_parts else np.empty((0, MAX_MOVES, FLOATS_PER_MOVE), dtype=np.float32)
    Sval = np.concatenate([val_by_matchup[m][0] for m in MATCHUPS])
    oval = np.concatenate([val_by_matchup[m][1] for m in MATCHUPS])
    nval = np.concatenate([val_by_matchup[m][2] for m in MATCHUPS])
    # index 3 is total_visits (unused here, see apply_policy_temperature()'s
    # own comment); moves moved from index 3 to 4 when that field was added
    mval = np.concatenate([val_by_matchup[m][4] for m in MATCHUPS])
    print(f"train records: {len(otr)}   val records: {len(oval)}")
    if len(otr) == 0:
        raise SystemExit("No training records found -- check --corpus-dir/--label")

    if args.max_train_records is not None and args.max_train_records < len(otr):
        rng = np.random.default_rng(args.seed)
        idx = rng.choice(len(otr), size=args.max_train_records, replace=False)
        Str, otr, ntr, mtr = Str[idx], otr[idx], ntr[idx], mtr[idx]
        print(f"Subsampled training set to {len(otr)} records (--max-train-records, seed={args.seed})")

    Str_t, otr_t, ntr_t, mtr_t = (torch.from_numpy(Str), torch.from_numpy(otr),
                                   torch.from_numpy(ntr), torch.from_numpy(mtr))
    Sval_t, oval_t, nval_t, mval_t = (torch.from_numpy(Sval), torch.from_numpy(oval),
                                       torch.from_numpy(nval), torch.from_numpy(mval))
    mtr_t = apply_policy_temperature(mtr_t, ntr_t, args.policy_target_temperature)
    mval_t = apply_policy_temperature(mval_t, nval_t, args.policy_target_temperature)

    model = PUCTNet(dropout=args.dropout)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    value_loss_fn = nn.MSELoss()

    baseline_pred = float(otr_t.mean().item())
    baseline_mse = float(((oval_t - baseline_pred) ** 2).mean().item()) if len(oval) else float("nan")
    print(f"Baseline value (predict train mean={baseline_pred:.4f}) val MSE: {baseline_mse:.6f}")

    uniform_ce, target_entropy = policy_target_baselines(mval_t, nval_t)
    headroom = uniform_ce - target_entropy
    print(f"Baseline policy (uniform prior) val CE: {uniform_ce:.4f} nats   "
         f"target entropy (irreducible floor): {target_entropy:.4f} nats   "
         f"learnable headroom: {headroom:.4f} nats")

    gen = torch.Generator().manual_seed(args.seed)
    best_total, best_state, epoch = float("inf"), None, 0
    epochs_since_improvement = 0
    start = time.time()

    for epoch in range(1, args.max_epochs + 1):
        model.train()
        total_v, total_p, n_seen = 0.0, 0.0, 0
        for idx in iterate_batches(Str_t.shape[0], args.batch_size, gen):
            opt.zero_grad()
            v_pred, p_logits = model(Str_t[idx])
            v_loss = value_loss_fn(v_pred, otr_t[idx])
            p_loss, _, _ = policy_loss_and_metrics(p_logits, mtr_t[idx], ntr_t[idx])
            loss = v_loss + args.policy_weight * p_loss
            loss.backward()
            opt.step()
            total_v += v_loss.item() * idx.shape[0]
            total_p += p_loss.item() * idx.shape[0]
            n_seen += idx.shape[0]
        train_v_mse = total_v / n_seen
        train_p_loss = total_p / n_seen

        val_v_pred, val_p_logits = predict(model, Sval_t)
        val_v_mse = value_loss_fn(val_v_pred, oval_t).item() if len(oval) else float("nan")
        val_p_loss, val_top1, val_entropy = (
            policy_loss_and_metrics(val_p_logits, mval_t, nval_t) if len(oval)
            else (torch.tensor(float("nan")), float("nan"), float("nan")))
        val_p_loss = val_p_loss.item() if torch.is_tensor(val_p_loss) else val_p_loss
        dir_acc = directional_accuracy(val_v_pred, oval_t)
        val_total = val_v_mse + args.policy_weight * val_p_loss
        captured = (uniform_ce - val_p_loss) / headroom if headroom > 0 else float("nan")
        elapsed = time.time() - start
        print(f"epoch {epoch:4d}  train_v_mse={train_v_mse:.6f}  train_p_loss={train_p_loss:.6f}  "
              f"val_v_mse={val_v_mse:.6f}  val_p_loss={val_p_loss:.6f}  val_top1={val_top1:.3%}  "
              f"headroom_captured={captured:.1%}  "
              f"dir_acc={dir_acc:.3%}  elapsed={elapsed:.0f}s", flush=True)

        if val_total < best_total:
            best_total = val_total
            best_state = {k: v.clone() for k, v in model.state_dict().items()}
            epochs_since_improvement = 0
        else:
            epochs_since_improvement += 1

        if args.patience > 0 and epochs_since_improvement >= args.patience:
            print(f"Stopping: no val total-loss improvement in {args.patience} epochs "
                  f"(best={best_total:.6f} at epoch {epoch - epochs_since_improvement})")
            break
        if elapsed > args.max_seconds:
            print(f"Stopping: wall-clock budget ({args.max_seconds:.0f}s) reached")
            break
    else:
        print(f"Stopping: max_epochs ({args.max_epochs}) reached")

    model.load_state_dict(best_state)
    elapsed = time.time() - start

    print()
    print(f"Best val total loss: {best_total:.6f}")

    print()
    print("Per-matchup validation breakdown:")
    for m in MATCHUPS:
        Sm, om, nm, _tvm, mm = val_by_matchup[m]  # total_visits unused, see above
        if len(om) == 0:
            print(f"  {m:8s}: no held-out shard")
            continue
        Sm_t, om_t, nm_t, mm_t = (torch.from_numpy(Sm), torch.from_numpy(om),
                                   torch.from_numpy(nm), torch.from_numpy(mm))
        mm_t = apply_policy_temperature(mm_t, nm_t, args.policy_target_temperature)
        vm, pm = predict(model, Sm_t)
        v_mse_m = value_loss_fn(vm, om_t).item()
        p_loss_m, top1_m, _ = policy_loss_and_metrics(pm, mm_t, nm_t)
        dir_m = directional_accuracy(vm, om_t)
        uniform_ce_m, target_entropy_m = policy_target_baselines(mm_t, nm_t)
        headroom_m = uniform_ce_m - target_entropy_m
        captured_m = ((uniform_ce_m - p_loss_m.item()) / headroom_m
                     if headroom_m > 0 else float("nan"))
        print(f"  {m:8s} n={len(om):7d}  val_v_mse={v_mse_m:.6f}  val_p_loss={p_loss_m.item():.6f}  "
              f"val_top1={top1_m:.3%}  headroom_captured={captured_m:.1%} "
              f"(uniform_ce={uniform_ce_m:.4f} floor={target_entropy_m:.4f})  dir_acc={dir_m:.3%}")

    run_name = args.run_name or args.label
    out_dir = Path(args.out_dir) if args.out_dir else Path(args.corpus_dir).parent / "checkpoints"
    out_dir.mkdir(parents=True, exist_ok=True)
    ckpt_path = out_dir / f"{run_name}_puct_net.pt"
    meta_path = out_dir / f"{run_name}_puct_net.json"
    torch.save(best_state, ckpt_path)
    meta = {
        "state_dim": STATE_DIM,
        "policy_dim": POLICY_DIM,
        "hidden": list(HIDDEN),
        "dropout": args.dropout,
        "weight_decay": args.weight_decay,
        "policy_weight": args.policy_weight,
        "policy_target_temperature": args.policy_target_temperature,
        "best_val_total_loss": best_total,
        "baseline_val_mse": baseline_mse,
        "baseline_uniform_policy_ce": uniform_ce,
        "baseline_policy_target_entropy": target_entropy,
        "train_records": int(len(otr)),
        "val_records": int(len(oval)),
        "epochs_run": epoch,
        "elapsed_seconds": elapsed,
        "batch_size": args.batch_size,
        "lr": args.lr,
        "patience": args.patience,
        "seed": args.seed,
        "corpus_label": args.label,
        "val_seeds": val_seeds,
        "max_train_records": args.max_train_records,
        "run_name": run_name,
    }
    meta_path.write_text(json.dumps(meta, indent=2))
    print()
    print(f"Saved checkpoint: {ckpt_path}")
    print(f"Saved metadata:   {meta_path}")


if __name__ == "__main__":
    main()
