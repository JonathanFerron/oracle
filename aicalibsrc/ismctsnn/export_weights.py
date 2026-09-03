#!/usr/bin/env python3
"""Exports a trained ValueNet checkpoint (train_value_net.py's .pt state_dict)
to the flat float32 weight file src/ai_strat/ai_strat_ismctsnn_net.c's
ismctsnn_net_load() expects.

Fuses the model's BatchNorm1d(537) into the first Linear layer (BN in eval
mode is an exact affine transform, foldable into the preceding layer's
weight columns + bias -- see this file's fuse_batchnorm()), so the C side
only ever does plain matmul + activation, four layers, nothing else. The
fusion is verified numerically here (a NumPy reimplementation of the fused
forward pass compared against the live PyTorch model on real corpus
records) before the file is written -- never trust the math un-checked.

Usage:
    .venv/bin/python export_weights.py checkpoints/pilot_value_net.pt \\
        corpus/pilot_vs_a3_seed3.bin  -o checkpoints/pilot_c_weights.bin
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import torch

from train_value_net import STATE_DIM, RECORD_DIM, HIDDEN, ValueNet


def fuse_batchnorm(bn_state, w1, b1):
    """bn_state: dict with 'weight'(gamma), 'bias'(beta), 'running_mean',
    'running_var' (each shape (STATE_DIM,)). w1: (H1, STATE_DIM), b1: (H1,).
    Returns (fused_w1, fused_b1) such that Linear(BN(x)) == fused_w1 @ x + fused_b1.

    BN(x)_j = gamma_j * (x_j - mean_j) / sqrt(var_j + eps) + beta_j
    z_i = sum_j w1[i,j] * BN(x)_j + b1[i]
        = sum_j (w1[i,j] * gamma_j/std_j) * x_j
          + b1[i] + sum_j w1[i,j] * (beta_j - gamma_j*mean_j/std_j)
    """
    eps = 1e-5
    gamma = bn_state["weight"].numpy()
    beta = bn_state["bias"].numpy()
    mean = bn_state["running_mean"].numpy()
    var = bn_state["running_var"].numpy()
    std = np.sqrt(var + eps)

    scale = gamma / std  # (STATE_DIM,)
    fused_w1 = w1 * scale[np.newaxis, :]  # scale each input COLUMN j
    adj = beta - gamma * mean / std  # (STATE_DIM,)
    fused_b1 = b1 + w1 @ adj
    return fused_w1.astype(np.float32), fused_b1.astype(np.float32)


def numpy_forward(x, fw1, fb1, w2, b2, w3, b3, w4, b4):
    """Mirrors ai_strat_ismctsnn_net.c's forward() exactly -- same layer
    order, same relu/sigmoid placement -- for the pre-export correctness
    check."""
    h1 = np.maximum(0.0, x @ fw1.T + fb1)
    h2 = np.maximum(0.0, h1 @ w2.T + b2)
    h3 = np.maximum(0.0, h2 @ w3.T + b3)
    z = h3 @ w4.T + b4
    return (1.0 / (1.0 + np.exp(-z))).squeeze(-1)  # (N,1) -> (N,), matches ValueNet.forward()


def load_sample_states(corpus_path, n=2000):
    arr = np.fromfile(corpus_path, dtype=np.float32)
    assert arr.size % RECORD_DIM == 0, f"{corpus_path}: not a multiple of {RECORD_DIM} floats"
    data = arr.reshape(-1, RECORD_DIM)
    idx = np.random.default_rng(0).choice(len(data), size=min(n, len(data)), replace=False)
    return data[idx, :STATE_DIM]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("checkpoint", help="train_value_net.py .pt state_dict")
    ap.add_argument("sample_corpus_shard", help="a corpus .bin shard to sanity-check against")
    ap.add_argument("-o", "--out", required=True, help="output flat float32 weight file")
    ap.add_argument("--tol", type=float, default=1e-4,
                    help="max abs diff allowed between fused NumPy and live PyTorch forward")
    args = ap.parse_args()

    # Dropout layers (if the checkpoint was trained with --dropout > 0) shift
    # every Sequential index after them, so the module must be reconstructed
    # with the SAME dropout the checkpoint was trained with -- read from the
    # sidecar JSON train_value_net.py always writes next to the .pt file,
    # rather than hardcoding net.<index> keys that only work for one shape.
    meta_path = Path(args.checkpoint).with_suffix(".json")
    dropout = 0.0
    if meta_path.exists():
        dropout = json.loads(meta_path.read_text()).get("dropout", 0.0)
    else:
        print(f"warning: no sidecar metadata at {meta_path}, assuming dropout=0.0",
              file=sys.stderr)

    model = ValueNet(input_dim=STATE_DIM, hidden=HIDDEN, dropout=dropout)
    state = torch.load(args.checkpoint, map_location="cpu")
    model.load_state_dict(state)
    model.eval()

    linear_layers = [m for m in model.net if isinstance(m, torch.nn.Linear)]
    bn_layer = next(m for m in model.net if isinstance(m, torch.nn.BatchNorm1d))
    assert len(linear_layers) == 4, f"expected 4 Linear layers, found {len(linear_layers)}"

    bn_state = {"weight": bn_layer.weight.detach(), "bias": bn_layer.bias.detach(),
                "running_mean": bn_layer.running_mean, "running_var": bn_layer.running_var}
    (w1, b1), (w2, b2), (w3, b3), (w4, b4) = (
        (layer.weight.detach().numpy(), layer.bias.detach().numpy()) for layer in linear_layers
    )

    fw1, fb1 = fuse_batchnorm(bn_state, w1, b1)

    X = load_sample_states(args.sample_corpus_shard)
    with torch.no_grad():
        torch_out = model(torch.from_numpy(X)).numpy()
    numpy_out = numpy_forward(X, fw1, fb1, w2, b2, w3, b3, w4, b4)

    max_diff = float(np.max(np.abs(torch_out - numpy_out)))
    print(f"Fusion check on {len(X)} sampled records: max abs diff = {max_diff:.8f}")
    if max_diff > args.tol:
        print(f"FAILED: max diff {max_diff} exceeds tolerance {args.tol} -- not writing output",
              file=sys.stderr)
        sys.exit(1)
    print("OK -- fused forward pass matches the live PyTorch model")

    with open(args.out, "wb") as f:
        for arr in (fw1, fb1, w2, b2, w3, b3, w4, b4):
            f.write(arr.astype(np.float32).tobytes())

    total_floats = sum(a.size for a in (fw1, fb1, w2, b2, w3, b3, w4, b4))
    print(f"Wrote {args.out}: {total_floats} floats ({total_floats * 4} bytes)")

    # Print a handful of encoded records + predictions so the C-side smoke
    # test (loading this same file) has something concrete to compare
    # against -- see the Stage 2 verification step.
    print()
    print("Reference predictions for the first 5 sampled records (for C-side cross-check):")
    for i in range(min(5, len(X))):
        print(f"  record {i}: predicted={float(numpy_out[i]):.6f}")


if __name__ == "__main__":
    main()
