"""
Generate workload-shaped synthetic scalar matrices with controllable structure.

This script is intentionally separate from generate_profiled_scalars.py.  The
original generator is an IID marginal-distribution baseline.  This generator
keeps the same value sampler, then adds engineering assumptions that are useful
for MSM mask-reuse sensitivity experiments:

  - repeated column templates,
  - row-phase/block repetition,
  - workload-specific structured-vs-random column mixtures,
  - optional per-column noise.

These outputs are simulations, not reconstructions of the real workloads.
"""

from __future__ import annotations

import argparse
import json
import os
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, Tuple

import numpy as np

from generate_profiled_scalars import (
    BN254_SCALAR_MODULUS,
    RealizedStats,
    build_width_segments,
    expected_profile,
    generate_encoded_values,
    load_profile,
    scalar_to_signed_limbs,
)


@dataclass(frozen=True)
class StructurePreset:
    structured_column_ratio: float
    column_group_size: int
    template_count: int
    row_phase_size: int
    template_noise: float
    random_template_ratio: float


PRESETS: Dict[str, StructurePreset] = {
    # Many small values and repeated constants; model stronger local reuse.
    "cnn": StructurePreset(0.75, 8, 64, 192, 0.04, 0.10),
    # Similar, but with weaker phase and more residual randomness.
    "nanogpt": StructurePreset(0.55, 4, 96, 128, 0.08, 0.18),
    # Round/bitwise-style workload: periodic phases are the main assumption.
    "sha256": StructurePreset(0.80, 8, 64, 64, 0.03, 0.08),
    # Mixed: only part of the columns are highly structured.
    "aggregation": StructurePreset(0.35, 4, 128, 256, 0.18, 0.45),
    # Sparse high-width activations; assume weak reuse.
    "sinsemilla": StructurePreset(0.12, 2, 128, 256, 0.35, 0.70),
    # Sparse bursts by phase/opcode, but limited global reuse.
    "zkevm-tx": StructurePreset(0.22, 4, 96, 128, 0.25, 0.55),
    # Mixed parsing/packing-style local repetition plus random full-width residue.
    "zkemail": StructurePreset(0.55, 4, 96, 128, 0.10, 0.28),
}


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate structured synthetic scalar matrices for MSM mask-reuse experiments."
    )
    p.add_argument("--summary", required=True)
    p.add_argument("--workload", required=True)
    p.add_argument("--K", type=int, required=True)
    p.add_argument("--n", type=int, required=True)
    p.add_argument("--output", required=True)
    p.add_argument(
        "--scalar-format",
        choices=["bn254_centered", "int256_twos_complement", "signed_magnitude_256"],
        default="bn254_centered",
    )
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--negative-ratio", type=float, default=0.5)
    p.add_argument("--overwrite", action="store_true")

    p.add_argument(
        "--preset",
        default="auto",
        help="auto or one of: " + ",".join(sorted(PRESETS)),
    )
    p.add_argument("--structured-column-ratio", type=float, default=None)
    p.add_argument("--column-group-size", type=int, default=None)
    p.add_argument("--template-count", type=int, default=None)
    p.add_argument("--row-phase-size", type=int, default=None)
    p.add_argument("--template-noise", type=float, default=None)
    p.add_argument("--random-template-ratio", type=float, default=None)
    return p.parse_args()


def choose_preset(args: argparse.Namespace, workload_name: str) -> StructurePreset:
    key = workload_name.strip().lower()
    if args.preset != "auto":
        key = args.preset.strip().lower()
    if key not in PRESETS:
        raise ValueError(f"Unknown preset {args.preset!r}; available: auto,{','.join(sorted(PRESETS))}")
    base = PRESETS[key]
    return StructurePreset(
        structured_column_ratio=base.structured_column_ratio
        if args.structured_column_ratio is None
        else args.structured_column_ratio,
        column_group_size=base.column_group_size if args.column_group_size is None else args.column_group_size,
        template_count=base.template_count if args.template_count is None else args.template_count,
        row_phase_size=base.row_phase_size if args.row_phase_size is None else args.row_phase_size,
        template_noise=base.template_noise if args.template_noise is None else args.template_noise,
        random_template_ratio=base.random_template_ratio
        if args.random_template_ratio is None
        else args.random_template_ratio,
    )


def widths_to_rows(kind: np.ndarray, widths: np.ndarray) -> np.ndarray:
    out = np.zeros(kind.size, dtype=np.uint16)
    nn_idx = np.flatnonzero(kind == 2)
    if nn_idx.size != widths.size:
        raise ValueError("width vector does not match nonzero-nonone count")
    out[nn_idx] = widths
    return out


def top1_count(encoded: np.ndarray, top1_limbs: np.ndarray) -> int:
    return int(np.count_nonzero(np.all(encoded == top1_limbs.reshape(1, 4), axis=1)))


def add_exact_stats(stats: RealizedStats, encoded: np.ndarray, kind: np.ndarray, width_by_row: np.ndarray, top1_limbs: np.ndarray) -> None:
    stats.add(kind, width_by_row[kind == 2], top1_count(encoded, top1_limbs))


def sample_full_column(
    row: Dict[str, str],
    scalar_format: str,
    negative_ratio: float,
    width_segments,
    rng: np.random.Generator,
    K: int,
    row_phase_size: int,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    if row_phase_size > 0 and row_phase_size < K:
        phase_len = min(row_phase_size, K)
        encoded, kind, widths, _top = generate_encoded_values(
            phase_len, row, scalar_format, negative_ratio, width_segments, rng
        )
        width_by_row = widths_to_rows(kind, widths)
        reps = (K + phase_len - 1) // phase_len
        return (
            np.tile(encoded, (reps, 1))[:K].copy(),
            np.tile(kind, reps)[:K].copy(),
            np.tile(width_by_row, reps)[:K].copy(),
        )

    encoded, kind, widths, _top = generate_encoded_values(
        K, row, scalar_format, negative_ratio, width_segments, rng
    )
    return encoded, kind, widths_to_rows(kind, widths)


def apply_noise(
    encoded: np.ndarray,
    kind: np.ndarray,
    width_by_row: np.ndarray,
    row: Dict[str, str],
    scalar_format: str,
    negative_ratio: float,
    width_segments,
    rng: np.random.Generator,
    noise: float,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    if noise <= 0.0:
        return encoded, kind, width_by_row
    mask = rng.random(kind.size) < noise
    count = int(np.count_nonzero(mask))
    if count == 0:
        return encoded, kind, width_by_row
    repl_encoded, repl_kind, repl_widths, _top = generate_encoded_values(
        count, row, scalar_format, negative_ratio, width_segments, rng
    )
    out_encoded = encoded.copy()
    out_kind = kind.copy()
    out_width = width_by_row.copy()
    out_encoded[mask] = repl_encoded
    out_kind[mask] = repl_kind
    out_width[mask] = widths_to_rows(repl_kind, repl_widths)
    return out_encoded, out_kind, out_width


def main() -> None:
    args = parse_args()
    if args.K <= 0 or args.n <= 0:
        raise ValueError("--K and --n must be positive")
    if not 0.0 <= args.negative_ratio <= 1.0:
        raise ValueError("--negative-ratio must be in [0,1]")

    output = Path(args.output)
    if output.exists() and not args.overwrite:
        raise FileExistsError(f"Output already exists: {output}. Use --overwrite to replace it.")
    output.parent.mkdir(parents=True, exist_ok=True)

    row = load_profile(args.summary, args.workload)
    preset = choose_preset(args, row["workload"])
    if not 0.0 <= preset.structured_column_ratio <= 1.0:
        raise ValueError("structured_column_ratio must be in [0,1]")
    if preset.column_group_size <= 0 or preset.template_count <= 0:
        raise ValueError("column_group_size and template_count must be positive")
    if not 0.0 <= preset.template_noise <= 1.0:
        raise ValueError("template_noise must be in [0,1]")
    if not 0.0 <= preset.random_template_ratio <= 1.0:
        raise ValueError("random_template_ratio must be in [0,1]")

    rng = np.random.default_rng(args.seed)
    width_segments = build_width_segments(row)
    top1_value = int(row["global_top1_signed_value"])
    top1_limbs = scalar_to_signed_limbs(top1_value, args.scalar_format)
    stats = RealizedStats(top1_value)

    print(f"workload                  : {row['workload']}")
    print(f"shape                     : ({args.K}, {args.n}, 4)")
    print(f"scalar_format             : {args.scalar_format}")
    print(f"structure_preset          : {asdict(preset)}")
    print(f"output_size_GiB           : {args.K * args.n * 4 * 8 / (1024**3):.3f}")

    t0 = time.perf_counter()
    arr = np.lib.format.open_memmap(output, mode="w+", dtype=np.uint64, shape=(args.K, args.n, 4))

    template_count = min(preset.template_count, max(1, args.n))
    templates = []
    random_template_count = int(round(template_count * preset.random_template_ratio))
    for idx in range(template_count):
        phase = 0 if idx < random_template_count else preset.row_phase_size
        templates.append(
            sample_full_column(
                row, args.scalar_format, args.negative_ratio, width_segments, rng, args.K, phase
            )
        )

    g = preset.column_group_size
    structured_groups = 0
    random_groups = 0
    for col_start in range(0, args.n, g):
        col_end = min(args.n, col_start + g)
        width = col_end - col_start
        use_structured = rng.random() < preset.structured_column_ratio
        if use_structured:
            structured_groups += 1
            base_encoded, base_kind, base_width = templates[int(rng.integers(0, template_count))]
            encoded, kind, width_by_row = apply_noise(
                base_encoded,
                base_kind,
                base_width,
                row,
                args.scalar_format,
                args.negative_ratio,
                width_segments,
                rng,
                preset.template_noise,
            )
        else:
            random_groups += 1
            encoded, kind, width_by_row = sample_full_column(
                row, args.scalar_format, args.negative_ratio, width_segments, rng, args.K, 0
            )

        arr[:, col_start:col_end, :] = encoded[:, None, :]
        for _ in range(width):
            add_exact_stats(stats, encoded, kind, width_by_row, top1_limbs)
        if col_start == 0 or col_end == args.n or (col_start // g) % 256 == 0:
            print(f"generated columns         : {col_end}/{args.n} ({100.0 * col_end / args.n:.2f}%)", flush=True)

    arr.flush()
    elapsed = time.perf_counter() - t0
    metadata = {
        "generator": os.path.abspath(__file__),
        "summary_csv": os.path.abspath(args.summary),
        "workload": row["workload"],
        "shape": [args.K, args.n, 4],
        "dtype": "uint64",
        "limb_order": "little_endian",
        "scalar_format": args.scalar_format,
        "bn254_scalar_modulus": str(BN254_SCALAR_MODULUS) if args.scalar_format == "bn254_centered" else None,
        "seed": args.seed,
        "negative_ratio_for_non_top1": args.negative_ratio,
        "structure_preset": asdict(preset),
        "structured_groups": structured_groups,
        "random_groups": random_groups,
        "expected": expected_profile(row),
        "realized": stats.as_dict(),
        "generation_seconds": elapsed,
        "output_path": str(output.resolve()),
        "output_size_bytes": output.stat().st_size,
        "note": (
            "This is a structured simulation for sensitivity testing. It preserves the CSV marginal value "
            "profile approximately while injecting column-template and row-phase correlations. It is not a "
            "claim about the true hidden workload matrices."
        ),
    }
    meta_path = output.with_suffix(output.suffix + ".meta.json")
    with open(meta_path, "w", encoding="utf-8") as fobj:
        json.dump(metadata, fobj, ensure_ascii=False, indent=2)

    print(f"generation_seconds        : {elapsed:.3f}")
    print(f"output                    : {output.resolve()}")
    print(f"metadata                  : {meta_path.resolve()}")
    print(json.dumps(metadata["realized"], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
