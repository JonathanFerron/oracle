#!/usr/bin/env python3
"""Exports a trained PUCTNet checkpoint (train_puct_net.py's .pt state_dict)
to the flat float32 weight file src/ai_strat/ai_strat_puct_net.c's
puctnet_load() expects.

Fuses the model's BatchNorm1d(537) into the first trunk Linear layer (BN in
eval mode is an exact affine transform, foldable into the preceding layer's
weight columns + bias -- see fuse_batchnorm(), ported unchanged from
aicalibsrc/ismctsnn/export_weights.py), so the C side only ever does plain
matmul + activation. Unlike A11's exporter, this one does NOT need to worry
about dropout shifting Linear-layer indices for the HEADS -- PUCTNet keeps
value_head/policy_head as separate named nn.Linear attributes, not buried in
one nn.Sequential -- only the shared trunk's 3 hidden layers still need the
isinstance-filter approach A11's own exporter uses.

The fusion (and both heads' raw outputs) is verified numerically here (a
NumPy reimplementation of the fused forward pass compared against the live
PyTorch model on real corpus records) before the file is written -- never
trust the math un-checked, same discipline as A11's own exporter.

Output order (ai_strat_puct_net.c's PUCTNetWeights struct, back to back):
    W1,b1, W2,b2, W3,b3  (fused trunk)
    Wv,bv                (value head)
    Wp,bp                (policy head, raw logits -- no activation)

Usage:
    .venv/bin/python export_puct_weights.py checkpoints/full_puct_net.pt \\
        corpus/full_vs_a3_seed3.bin -o checkpoints/full_c_weights.bin
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import torch

from train_puct_net import STATE_DIM, RECORD_DIM, HIDDEN, POLICY_DIM, PUCTNet


def fuse_batchnorm(bn_state, w1, b1):
    """Identical algebra to aicalibsrc/ismctsnn/export_weights.py's own
    fuse_batchnorm() -- see that file's docstring for the derivation."""
    eps = 1e-5
    gamma = bn_state["weight"].numpy()
    beta = bn_state["bias"].numpy()
    mean = bn_state["running_mean"].numpy()
    var = bn_state["running_var"].numpy()
    std = np.sqrt(var + eps)

    scale = gamma / std
    fused_w1 = w1 * scale[np.newaxis, :]
    adj = beta - gamma * mean / std
    fused_b1 = b1 + w1 @ adj
    return fused_w1.astype(np.float32), fused_b1.astype(np.float32)


def numpy_forward(x, fw1, fb1, w2, b2, w3, b3, wv, bv, wp, bp):
    """Mirrors ai_strat_puct_net.c's puctnet_forward() exactly -- same layer
    order, same relu placement, value sigmoid / policy raw logits."""
    h1 = np.maximum(0.0, x @ fw1.T + fb1)
    h2 = np.maximum(0.0, h1 @ w2.T + b2)
    h3 = np.maximum(0.0, h2 @ w3.T + b3)
    value_z = h3 @ wv.T + bv
    value = (1.0 / (1.0 + np.exp(-value_z))).squeeze(-1)
    policy_logits = h3 @ wp.T + bp
    return value, policy_logits


def load_sample_states(corpus_path, n=2000):
    arr = np.fromfile(corpus_path, dtype=np.float32)
    assert arr.size % RECORD_DIM == 0, f"{corpus_path}: not a multiple of {RECORD_DIM} floats"
    data = arr.reshape(-1, RECORD_DIM)
    idx = np.random.default_rng(0).choice(len(data), size=min(n, len(data)), replace=False)
    return data[idx, :STATE_DIM]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("checkpoint", help="train_puct_net.py .pt state_dict")
    ap.add_argument("sample_corpus_shard", help="a corpus .bin shard to sanity-check against")
    ap.add_argument("-o", "--out", required=True, help="output flat float32 weight file")
    ap.add_argument("--tol", type=float, default=1e-4,
                    help="max abs diff allowed between fused NumPy and live PyTorch forward "
                         "(checked on both value and policy-logit outputs)")
    args = ap.parse_args()

    meta_path = Path(args.checkpoint).with_suffix(".json")
    dropout = 0.0
    if meta_path.exists():
        dropout = json.loads(meta_path.read_text()).get("dropout", 0.0)
    else:
        print(f"warning: no sidecar metadata at {meta_path}, assuming dropout=0.0",
              file=sys.stderr)

    model = PUCTNet(input_dim=STATE_DIM, hidden=HIDDEN, policy_dim=POLICY_DIM, dropout=dropout)
    state = torch.load(args.checkpoint, map_location="cpu")
    model.load_state_dict(state)
    model.eval()

    trunk_linears = [m for m in model.trunk if isinstance(m, torch.nn.Linear)]
    bn_layer = next(m for m in model.trunk if isinstance(m, torch.nn.BatchNorm1d))
    assert len(trunk_linears) == 3, f"expected 3 trunk Linear layers, found {len(trunk_linears)}"

    bn_state = {"weight": bn_layer.weight.detach(), "bias": bn_layer.bias.detach(),
                "running_mean": bn_layer.running_mean, "running_var": bn_layer.running_var}
    (w1, b1), (w2, b2), (w3, b3) = (
        (layer.weight.detach().numpy(), layer.bias.detach().numpy()) for layer in trunk_linears
    )
    wv = model.value_head.weight.detach().numpy()
    bv = model.value_head.bias.detach().numpy()
    wp = model.policy_head.weight.detach().numpy()
    bp = model.policy_head.bias.detach().numpy()

    fw1, fb1 = fuse_batchnorm(bn_state, w1, b1)

    X = load_sample_states(args.sample_corpus_shard)
    with torch.no_grad():
        torch_value, torch_policy = model(torch.from_numpy(X))
    torch_value = torch_value.numpy()
    torch_policy = torch_policy.numpy()
    numpy_value, numpy_policy = numpy_forward(X, fw1, fb1, w2, b2, w3, b3, wv, bv, wp, bp)

    value_diff = float(np.max(np.abs(torch_value - numpy_value)))
    policy_diff = float(np.max(np.abs(torch_policy - numpy_policy)))
    print(f"Fusion check on {len(X)} sampled records:")
    print(f"  value  max abs diff = {value_diff:.8f}")
    print(f"  policy max abs diff = {policy_diff:.8f}")
    if value_diff > args.tol or policy_diff > args.tol:
        print(f"FAILED: a max diff exceeds tolerance {args.tol} -- not writing output",
              file=sys.stderr)
        sys.exit(1)
    print("OK -- fused forward pass matches the live PyTorch model on both heads")

    with open(args.out, "wb") as f:
        for arr in (fw1, fb1, w2, b2, w3, b3, wv, bv, wp, bp):
            f.write(arr.astype(np.float32).tobytes())

    total_floats = sum(a.size for a in (fw1, fb1, w2, b2, w3, b3, wv, bv, wp, bp))
    print(f"Wrote {args.out}: {total_floats} floats ({total_floats * 4} bytes)")

    print()
    print("Reference predictions for the first 5 sampled records (for C-side cross-check):")
    for i in range(min(5, len(X))):
        top_move = int(np.argmax(numpy_policy[i]))
        print(f"  record {i}: value={float(numpy_value[i]):.6f}  "
              f"argmax_policy_logit_index={top_move} (raw logit={numpy_policy[i, top_move]:.4f})")


if __name__ == "__main__":
    main()
