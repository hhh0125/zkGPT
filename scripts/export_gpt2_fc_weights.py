#!/usr/bin/env python3
"""Export GPT-2 FC weights for zkGPT.

The C++ prover stores fully-connected weights as int32 matrices in
[out_dim, in_dim] row-major order. HuggingFace GPT-2 stores Conv1D weights as
[in_dim, out_dim], so this script transposes them and quantizes with the same
default weight scale used by neuralNetwork.cpp: 2^-10.
"""

import argparse
import os
import struct
from typing import Iterable, Tuple


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--model",
        default="gpt2",
        help="HuggingFace model name or local checkpoint directory.",
    )
    parser.add_argument(
        "--output-dir",
        default="data/gpt2_int",
        help="Directory for fc_0.bin ... fc_47.bin.",
    )
    parser.add_argument(
        "--layers",
        type=int,
        default=12,
        help="Number of transformer blocks to export.",
    )
    parser.add_argument(
        "--weight-scale",
        type=float,
        default=1024.0,
        help="Integer quantization multiplier. 1024 corresponds to scale 2^-10.",
    )
    parser.add_argument(
        "--mlp-hidden-size",
        type=int,
        default=2304,
        help=(
            "MLP hidden size expected by the current zkGPT demo. Standard GPT-2 "
            "small is 3072; the current demo uses 2304, so MLP weights are "
            "sliced by default to make the existing circuit run."
        ),
    )
    parser.add_argument(
        "--strict-shapes",
        action="store_true",
        help="Reject shape slicing. Use this with --mlp-hidden-size 3072 for standard GPT-2 small.",
    )
    return parser.parse_args()


def crop_cols(weight, cols: int, name: str, strict: bool):
    if weight.shape[1] == cols:
        return weight
    if weight.shape[1] < cols:
        raise ValueError(f"{name}: has only {weight.shape[1]} output cols, need {cols}")
    if strict:
        raise ValueError(f"{name}: has {weight.shape[1]} output cols, expected {cols}")
    print(f"{name}: slicing output cols {weight.shape[1]} -> {cols}")
    return weight[:, :cols]


def crop_rows(weight, rows: int, name: str, strict: bool):
    if weight.shape[0] == rows:
        return weight
    if weight.shape[0] < rows:
        raise ValueError(f"{name}: has only {weight.shape[0]} input rows, need {rows}")
    if strict:
        raise ValueError(f"{name}: has {weight.shape[0]} input rows, expected {rows}")
    print(f"{name}: slicing input rows {weight.shape[0]} -> {rows}")
    return weight[:rows, :]


def fc_items(state_dict, layers: int, mlp_hidden_size: int, strict: bool) -> Iterable[Tuple[str, object]]:
    for layer in range(layers):
        prefix = f"transformer.h.{layer}"

        c_attn = state_dict[f"{prefix}.attn.c_attn.weight"]
        c_proj = state_dict[f"{prefix}.attn.c_proj.weight"]
        c_fc = crop_cols(state_dict[f"{prefix}.mlp.c_fc.weight"], mlp_hidden_size, f"{prefix}.mlp.c_fc.weight", strict)
        mlp_proj = crop_rows(state_dict[f"{prefix}.mlp.c_proj.weight"], mlp_hidden_size, f"{prefix}.mlp.c_proj.weight", strict)

        yield f"{prefix}.attn.c_attn.weight", c_attn
        yield f"{prefix}.attn.c_proj.weight", c_proj
        yield f"{prefix}.mlp.c_fc.weight", c_fc
        yield f"{prefix}.mlp.c_proj.weight", mlp_proj


def main() -> None:
    args = parse_args()

    try:
        import torch
        from transformers import AutoModelForCausalLM
    except ImportError as exc:
        raise SystemExit(
            "Missing Python dependency. Install torch and transformers, then rerun this script."
        ) from exc

    os.makedirs(args.output_dir, exist_ok=True)
    model = AutoModelForCausalLM.from_pretrained(args.model)
    state_dict = model.state_dict()

    for fc_id, (name, weight) in enumerate(
        fc_items(state_dict, args.layers, args.mlp_hidden_size, args.strict_shapes)
    ):
        # HuggingFace GPT-2 Conv1D is [in_dim, out_dim]; zkGPT needs [out_dim, in_dim].
        w = weight.detach().cpu().float().T.contiguous()
        q = torch.round(w * args.weight_scale).to(torch.int32).contiguous()

        rows, cols = q.shape
        path = os.path.join(args.output_dir, f"fc_{fc_id}.bin")
        with open(path, "wb") as out:
            out.write(struct.pack("<ii", rows, cols))
            out.write(q.numpy().astype("<i4", copy=False).tobytes())

        print(
            f"fc_{fc_id}.bin {rows}x{cols} from {name} "
            f"range=[{int(q.min())}, {int(q.max())}]"
        )


if __name__ == "__main__":
    main()
