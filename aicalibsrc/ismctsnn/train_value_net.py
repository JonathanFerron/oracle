#!/usr/bin/env python3
"""A11 IS-MCTS+NN ("AlphaOracle Prime") Stage 1 value-net trainer.

Trains a small CPU MLP to predict game outcome (0.0/0.5/1.0, gen_corpus.c's
own mc_outcome_for() convention) from the 537-float information-set state
vector (src/ai_strat/ai_strat_ismctsnn_state.h) logged by gen_corpus.c /
run_selfplay.sh. See "Confirmed plan" step 1-2 in
ideas/A11 ai agent is-mcts + nn (alphaoracle prime)/about.md.

No normalization is applied here beyond what ismctsnn_encode_state() itself
already does -- the encoder header is explicit that values are "intentionally
already normalized/relativized" so the same C function serves training data
and future C inference with nothing to keep in sync. The one nod to leftover
scale mismatch (0-1 ratios sitting next to raw counts like deck_remaining,
roughly 0-34) is a BatchNorm1d as the network's own first layer -- a pure
architecture choice, doesn't touch the corpus format.

Data split is by WHOLE SHARD, not by random row: shards are grouped by
matchup (mirror/vs_a7/vs_a3, from run_selfplay.sh's own filename convention
"<label>_<matchup>_seed<N>.bin") and the highest-seeded --holdout-shards-
per-matchup shard(s) per matchup become validation, the rest train. Splitting
by shard rather than row avoids leaking correlated decisions from the same
game across train/val.

Wall-clock budget matches the two-pass generation protocol
(local_training_plan.md): --max-seconds defaults to a 1-hour pilot ceiling,
--max-epochs is a generous fallback that should never actually bind given
local_training_plan.md's own finding that training a net this size is
"minutes, not hours" regardless of corpus size.

Usage:
    .venv/bin/python train_value_net.py corpus --label pilot
    .venv/bin/python train_value_net.py corpus --label full --max-seconds 43200
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
RECORD_DIM = STATE_DIM + 1  # + outcome
MATCHUPS = ("mirror", "vs_a7", "vs_a3")
HIDDEN = (256, 128, 64)
SHARD_RE = re.compile(r"^(?P<label>.+)_(?P<matchup>mirror|vs_a7|vs_a3)_seed(?P<seed>\d+)\.bin$")


def discover_shards(corpus_dir, label):
    """Groups every '<label>_<matchup>_seed<N>.bin' file in corpus_dir by
    matchup, sorted by seed ascending."""
    groups = {m: [] for m in MATCHUPS}
    for p in sorted(Path(corpus_dir).glob(f"{label}_*_seed*.bin")):
        m = SHARD_RE.match(p.name)
        if not m or m.group("label") != label:
            continue
        groups[m.group("matchup")].append((int(m.group("seed")), p))
    for matchup in groups:
        groups[matchup].sort(key=lambda t: t[0])
    return groups


def split_train_val(groups, holdout_per_matchup):
    """Per matchup: the highest-seeded holdout_per_matchup shard(s) become
    validation, the rest train. Never holds out a matchup's only shard (so a
    thin pilot corpus doesn't end up with zero training data for a matchup)."""
    train, val = {}, {}
    for matchup, shards in groups.items():
        n_holdout = min(holdout_per_matchup, max(0, len(shards) - 1))
        val_shards = shards[len(shards) - n_holdout:] if n_holdout else []
        train_shards = shards[:len(shards) - n_holdout]
        train[matchup] = [p for _, p in train_shards]
        val[matchup] = [p for _, p in val_shards]
    return train, val


def load_records(paths):
    if not paths:
        return np.empty((0, STATE_DIM), dtype=np.float32), np.empty((0,), dtype=np.float32)
    arrays = []
    for p in paths:
        arr = np.fromfile(p, dtype=np.float32)
        assert arr.size % RECORD_DIM == 0, f"{p}: size not a multiple of {RECORD_DIM} floats"
        arrays.append(arr.reshape(-1, RECORD_DIM))
    data = np.concatenate(arrays, axis=0)
    return data[:, :STATE_DIM].copy(), data[:, STATE_DIM].copy()


class ValueNet(nn.Module):
    def __init__(self, input_dim=STATE_DIM, hidden=HIDDEN, dropout=0.0):
        super().__init__()
        layers = [nn.BatchNorm1d(input_dim)]
        prev = input_dim
        for h in hidden:
            layers += [nn.Linear(prev, h), nn.ReLU()]
            if dropout > 0:
                layers.append(nn.Dropout(dropout))
            prev = h
        layers += [nn.Linear(prev, 1), nn.Sigmoid()]
        self.net = nn.Sequential(*layers)

    def forward(self, x):
        return self.net(x).squeeze(-1)


def predict(model, X_t, batch_size=8192):
    model.eval()
    if X_t.shape[0] == 0:
        return torch.empty(0)
    outs = []
    with torch.no_grad():
        for start in range(0, X_t.shape[0], batch_size):
            outs.append(model(X_t[start:start + batch_size]))
    return torch.cat(outs)


def iterate_batches(X_t, y_t, batch_size, generator):
    n = X_t.shape[0]
    perm = torch.randperm(n, generator=generator)
    for start in range(0, n, batch_size):
        idx = perm[start:start + batch_size]
        yield X_t[idx], y_t[idx]


def directional_accuracy(pred, actual):
    if pred.shape[0] == 0:
        return float("nan")
    return ((pred - 0.5).sign() == (actual - 0.5).sign()).float().mean().item()


def report_calibration(pred, actual, n_buckets=10):
    pred_np = pred.numpy()
    order = np.argsort(pred_np)
    n = len(order)
    print("Calibration (predicted decile -> mean actual outcome):")
    for b in range(n_buckets):
        lo, hi = b * n // n_buckets, (b + 1) * n // n_buckets
        idx = order[lo:hi]
        if len(idx) == 0:
            continue
        print(f"  decile {b + 1:2d}: pred_mean={pred_np[idx].mean():.3f} "
              f"actual_mean={actual[idx].mean():.3f}  n={len(idx)}")


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("corpus_dir", help="directory holding <label>_<matchup>_seed<N>.bin shards")
    ap.add_argument("--label", default="pilot")
    ap.add_argument("--max-seconds", type=float, default=3600)
    ap.add_argument("--max-epochs", type=int, default=500)
    ap.add_argument("--batch-size", type=int, default=4096)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--weight-decay", type=float, default=0.0, help="Adam L2 penalty")
    ap.add_argument("--dropout", type=float, default=0.0, help="dropout after each hidden ReLU")
    ap.add_argument("--patience", type=int, default=0,
                    help="stop after this many epochs with no val_mse improvement "
                         "(0 = disabled, only max-epochs/max-seconds apply)")
    ap.add_argument("--threads", type=int, default=max(1, int(os.cpu_count() * 0.75)))
    ap.add_argument("--holdout-shards-per-matchup", type=int, default=1)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--out-dir", default=None,
                    help="default: <corpus_dir>/../checkpoints")
    ap.add_argument("--run-name", default=None,
                    help="checkpoint/metadata filename prefix (default: --label) -- "
                         "set this when trying several hyperparameter variants against "
                         "the same --label corpus, so they don't overwrite each other")
    return ap.parse_args()


def main():
    args = parse_args()
    torch.set_num_threads(args.threads)
    torch.manual_seed(args.seed)

    groups = discover_shards(args.corpus_dir, args.label)
    print(f"Shards found for label='{args.label}':")
    for m in MATCHUPS:
        print(f"  {m:8s}: {len(groups[m])} shard(s) -- "
              f"{[p.name for _, p in groups[m]]}")

    train_files, val_files = split_train_val(groups, args.holdout_shards_per_matchup)
    print("Validation holdout:")
    for m in MATCHUPS:
        print(f"  {m:8s}: {[p.name for p in val_files[m]]}")

    Xtr_parts, ytr_parts = [], []
    val_by_matchup = {}
    for m in MATCHUPS:
        X, y = load_records(train_files[m])
        Xtr_parts.append(X)
        ytr_parts.append(y)
        val_by_matchup[m] = load_records(val_files[m])
    Xtr = np.concatenate(Xtr_parts) if Xtr_parts else np.empty((0, STATE_DIM), dtype=np.float32)
    ytr = np.concatenate(ytr_parts) if ytr_parts else np.empty((0,), dtype=np.float32)
    Xval = np.concatenate([val_by_matchup[m][0] for m in MATCHUPS])
    yval = np.concatenate([val_by_matchup[m][1] for m in MATCHUPS])
    print(f"train records: {len(ytr)}   val records: {len(yval)}")
    if len(ytr) == 0:
        raise SystemExit("No training records found -- check --corpus-dir/--label")

    Xtr_t, ytr_t = torch.from_numpy(Xtr), torch.from_numpy(ytr)
    Xval_t, yval_t = torch.from_numpy(Xval), torch.from_numpy(yval)

    model = ValueNet(dropout=args.dropout)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    loss_fn = nn.MSELoss()

    baseline_pred = float(ytr_t.mean().item())
    baseline_mse = float(((yval_t - baseline_pred) ** 2).mean().item()) if len(yval) else float("nan")
    print(f"Baseline (predict train mean={baseline_pred:.4f}) val MSE: {baseline_mse:.6f}")

    gen = torch.Generator().manual_seed(args.seed)
    best_val, best_state, epoch = float("inf"), None, 0
    epochs_since_improvement = 0
    start = time.time()

    for epoch in range(1, args.max_epochs + 1):
        model.train()
        total_loss, n_seen = 0.0, 0
        for xb, yb in iterate_batches(Xtr_t, ytr_t, args.batch_size, gen):
            opt.zero_grad()
            pred = model(xb)
            loss = loss_fn(pred, yb)
            loss.backward()
            opt.step()
            total_loss += loss.item() * xb.shape[0]
            n_seen += xb.shape[0]
        train_loss = total_loss / n_seen

        val_pred = predict(model, Xval_t)
        val_loss = loss_fn(val_pred, yval_t).item() if len(yval) else float("nan")
        dir_acc = directional_accuracy(val_pred, yval_t)
        elapsed = time.time() - start
        print(f"epoch {epoch:4d}  train_mse={train_loss:.6f}  val_mse={val_loss:.6f}  "
              f"dir_acc={dir_acc:.3%}  elapsed={elapsed:.0f}s", flush=True)

        if val_loss < best_val:
            best_val = val_loss
            best_state = {k: v.clone() for k, v in model.state_dict().items()}
            epochs_since_improvement = 0
        else:
            epochs_since_improvement += 1

        if args.patience > 0 and epochs_since_improvement >= args.patience:
            print(f"Stopping: no val_mse improvement in {args.patience} epochs "
                  f"(best={best_val:.6f} at epoch {epoch - epochs_since_improvement})")
            break
        if elapsed > args.max_seconds:
            print(f"Stopping: wall-clock budget ({args.max_seconds:.0f}s) reached")
            break
    else:
        print(f"Stopping: max_epochs ({args.max_epochs}) reached")

    model.load_state_dict(best_state)
    elapsed = time.time() - start

    print()
    print(f"Best val MSE: {best_val:.6f}  (baseline: {baseline_mse:.6f})")

    print()
    print("Per-matchup validation breakdown:")
    for m in MATCHUPS:
        Xm, ym = val_by_matchup[m]
        if len(ym) == 0:
            print(f"  {m:8s}: no held-out shard")
            continue
        Xm_t, ym_t = torch.from_numpy(Xm), torch.from_numpy(ym)
        pm = predict(model, Xm_t)
        mse_m = loss_fn(pm, ym_t).item()
        dir_m = directional_accuracy(pm, ym_t)
        print(f"  {m:8s} n={len(ym):7d}  val_mse={mse_m:.6f}  dir_acc={dir_m:.3%}")

    print()
    final_val_pred = predict(model, Xval_t)
    report_calibration(final_val_pred, yval)

    run_name = args.run_name or args.label
    out_dir = Path(args.out_dir) if args.out_dir else Path(args.corpus_dir).parent / "checkpoints"
    out_dir.mkdir(parents=True, exist_ok=True)
    ckpt_path = out_dir / f"{run_name}_value_net.pt"
    meta_path = out_dir / f"{run_name}_value_net.json"
    torch.save(best_state, ckpt_path)
    meta = {
        "state_dim": STATE_DIM,
        "hidden": list(HIDDEN),
        "dropout": args.dropout,
        "weight_decay": args.weight_decay,
        "best_val_mse": best_val,
        "baseline_val_mse": baseline_mse,
        "train_records": int(len(ytr)),
        "val_records": int(len(yval)),
        "epochs_run": epoch,
        "elapsed_seconds": elapsed,
        "batch_size": args.batch_size,
        "lr": args.lr,
        "patience": args.patience,
        "seed": args.seed,
        "corpus_label": args.label,
        "run_name": run_name,
    }
    meta_path.write_text(json.dumps(meta, indent=2))
    print()
    print(f"Saved checkpoint: {ckpt_path}")
    print(f"Saved metadata:   {meta_path}")


if __name__ == "__main__":
    main()
