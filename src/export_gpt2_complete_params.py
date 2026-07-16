#!/usr/bin/env python3
"""Export all parameters required by the C++ GPT-2 block prover.

The C++ circuit uses q_weight = round(weight / 2^-10).  FC biases are
exported in accumulator units, because the implementation fuses bias addition
with the following requantization: accumulator = q_x*q_w + q_bias_acc.
"""

import argparse
import struct
from pathlib import Path

import numpy as np
from transformers import GPT2LMHeadModel


WEIGHT_SCALE = 2.0 ** -10
ACTIVATION_SCALE = 10.0 * (2.0 ** -10)  # search(0.01) in the C++ code


def write_matrix(path: Path, value: np.ndarray) -> None:
    value = np.ascontiguousarray(value.astype("<i4"))
    with path.open("wb") as stream:
        stream.write(struct.pack("<ii", *value.shape))
        stream.write(value.tobytes())


def write_bias(path: Path, value: np.ndarray) -> None:
    value = np.ascontiguousarray(value.astype("<i4"))
    with path.open("wb") as stream:
        stream.write(struct.pack("<i", value.size))
        stream.write(value.tobytes())


def write_layer_norm(path: Path, weight: np.ndarray, bias: np.ndarray) -> None:
    q_weight = np.rint(weight / WEIGHT_SCALE).astype("<i4")
    q_bias = np.rint(bias / WEIGHT_SCALE).astype("<i4")
    with path.open("wb") as stream:
        # width, weight_c, weight_e, bias_c, bias_e
        stream.write(struct.pack("<iiiii", weight.size, 1, -10, 1, -10))
        stream.write(q_weight.tobytes())
        stream.write(q_bias.tobytes())


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", default="gpt2")
    parser.add_argument("--output", type=Path, default=Path("data/gpt2_int"))
    parser.add_argument(
        "--mlp-hidden-size",
        type=int,
        default=3072,
        help="MLP width used by the zkGPT circuit (the current demo uses 3072).",
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    model = GPT2LMHeadModel.from_pretrained(args.model).eval()
    fc_id = 0
    ln_id = 0
    for block in model.transformer.h:
        # Conv1D stores [in, out]; the prover consumes [out, in].
        for linear_index, linear in enumerate((
                block.attn.c_attn, block.attn.c_proj,
                block.mlp.c_fc, block.mlp.c_proj)):
            weight = linear.weight.detach().cpu().numpy().T
            bias = linear.bias.detach().cpu().numpy()
            if linear_index == 2:
                weight = weight[:args.mlp_hidden_size, :]
                bias = bias[:args.mlp_hidden_size]
            elif linear_index == 3:
                weight = weight[:, :args.mlp_hidden_size]
            write_matrix(args.output / f"fc_{fc_id}.bin",
                         np.rint(weight / WEIGHT_SCALE))
            write_bias(args.output / f"fc_{fc_id}_bias.bin",
                       np.rint(bias / (ACTIVATION_SCALE * WEIGHT_SCALE)))
            fc_id += 1

        for norm in (block.ln_1, block.ln_2):
            write_layer_norm(
                args.output / f"ln_{ln_id}.bin",
                norm.weight.detach().cpu().numpy(),
                norm.bias.detach().cpu().numpy(),
            )
            ln_id += 1

    print(f"exported {fc_id} FC layers and {ln_id} LayerNorms to {args.output}")


if __name__ == "__main__":
    main()
