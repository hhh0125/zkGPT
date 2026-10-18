#!/usr/bin/env python3
"""
Generate synthetic scalar matrices whose marginal distribution approximately matches
one row of vanilla3_workloads_advice_width_summary.csv.

Output layout
-------------
The .npy output has shape (K, n, 4), dtype=uint64, little-endian limbs.
Supported encodings:

  bn254_centered (default)
      Canonical BN254 scalar-field residues. A signed integer x is encoded as x mod r.
      Positive x is stored directly; negative -x is stored as r-x. This is the most
      realistic format for workloads whose large "negative" values are field residues.

  int256_twos_complement
      Signed 256-bit two's-complement integer in four little-endian uint64 limbs.

  signed_magnitude_256
      Magnitude in bits 0..254 and sign in bit 255.

The summary CSV contains only marginal statistics, not cross-row/cross-column
correlations. Therefore --column-group-size=1 creates an IID matrix. Setting it to a
value greater than 1 makes every adjacent group of columns share the same K-row scalar
pattern; this preserves the marginal distribution while injecting controllable exact
mask reuse for tile-mask experiments.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import time
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

import numpy as np


BN254_SCALAR_MODULUS = int(
    "30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001", 16
)
UINT64_MAX = np.iinfo(np.uint64).max


def int_to_limbs4(value: int) -> np.ndarray:
    return np.array([(value >> (64 * i)) & ((1 << 64) - 1) for i in range(4)], dtype=np.uint64)


BN254_R_LIMBS = int_to_limbs4(BN254_SCALAR_MODULUS)
BN254_HALF = BN254_SCALAR_MODULUS // 2
BN254_HALF_LIMBS = int_to_limbs4(BN254_HALF)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Generate synthetic scalar matrices from workload summary statistics.")
    p.add_argument("--summary", required=True, help="Path to vanilla3_workloads_advice_width_summary.csv")
    p.add_argument("--workload", required=True, help="Workload name, e.g. SHA256, CNN, NanoGPT")
    p.add_argument("--K", type=int, required=True, help="Number of MSM rows")
    p.add_argument("--n", type=int, required=True, help="Number of point columns")
    p.add_argument("--output", required=True, help="Output .npy path")
    p.add_argument(
        "--scalar-format",
        choices=["bn254_centered", "int256_twos_complement", "signed_magnitude_256"],
        default="bn254_centered",
        help="Four-limb encoding written to the output file",
    )
    p.add_argument("--seed", type=int, default=1)
    p.add_argument(
        "--chunk-scalars",
        type=int,
        default=1_000_000,
        help="IID generation chunk size. One million scalars correspond to 32 MB of output limbs.",
    )
    p.add_argument(
        "--negative-ratio",
        type=float,
        default=0.5,
        help="Negative-sign probability for generated nonzero/nonone values not forced to the CSV top-1 value.",
    )
    p.add_argument(
        "--column-group-size",
        type=int,
        default=1,
        help=(
            "1 creates IID columns. Values >1 replicate one K-row pattern across each adjacent column group, "
            "injecting exact cross-column mask reuse while keeping the marginal distribution."
        ),
    )
    p.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite an existing output file.",
    )
    return p.parse_args()


def load_profile(path: str, workload: str) -> Dict[str, str]:
    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    matches = [row for row in rows if row["workload"].strip().lower() == workload.strip().lower()]
    if not matches:
        available = ", ".join(row["workload"] for row in rows)
        raise ValueError(f"Unknown workload {workload!r}. Available: {available}")
    return matches[0]


def f(row: Dict[str, str], key: str) -> float:
    return float(row[key])


def i(row: Dict[str, str], key: str) -> int:
    return int(row[key])


def build_width_segments(row: Dict[str, str]) -> List[Tuple[float, float, np.ndarray, np.ndarray]]:
    """Build piecewise inverse-CDF segments that preserve the <=32/64/128 masses.

    Each segment is (prob_start, prob_end, anchor_probabilities, anchor_widths).
    Empty bins are omitted. Quantile anchors p50/p90/p99 are inserted into the
    appropriate bit-width bin, so the generated marginal distribution approximates
    both the cumulative width ratios and the reported quantiles.
    """
    max_width = int(row["signed_abs_bit_width_max"])
    r32 = f(row, "ratio_nonzero_small_width_le_32")
    r64 = f(row, "ratio_nonzero_small_width_le_64")
    r128 = f(row, "ratio_nonzero_small_width_le_128")
    boundaries = [0.0, r32, r64, r128, 1.0]
    width_ranges = [
        (2, min(32, max_width)),
        (33, min(64, max_width)),
        (65, min(128, max_width)),
        (129, max_width),
    ]
    quantiles = [
        (0.50, int(row["signed_abs_bit_width_p50"])),
        (0.90, int(row["signed_abs_bit_width_p90"])),
        (0.99, int(row["signed_abs_bit_width_p99"])),
        (1.00, max_width),
    ]

    segments: List[Tuple[float, float, np.ndarray, np.ndarray]] = []
    for seg_idx, (lo_w, hi_w) in enumerate(width_ranges):
        p0, p1 = boundaries[seg_idx], boundaries[seg_idx + 1]
        if p1 <= p0 + 1e-15 or hi_w < lo_w:
            continue
        anchors: List[Tuple[float, float]] = [(p0, float(lo_w)), (p1, float(hi_w))]
        for qp, qw in quantiles:
            # Include an anchor only when its probability and reported width both
            # belong to this segment. This avoids inventing mass in empty width gaps.
            in_probability = (qp > p0 + 1e-15) and (qp <= p1 + 1e-15)
            in_width = lo_w <= qw <= hi_w
            if in_probability and in_width:
                anchors.append((qp, float(qw)))
        anchors.sort(key=lambda x: x[0])

        probs: List[float] = []
        widths: List[float] = []
        for ap, aw in anchors:
            if probs and math.isclose(ap, probs[-1], abs_tol=1e-15):
                widths[-1] = max(widths[-1], aw)
            else:
                probs.append(ap)
                widths.append(aw)
        width_arr = np.maximum.accumulate(np.asarray(widths, dtype=np.float64))
        width_arr = np.clip(width_arr, lo_w, hi_w)
        segments.append((p0, p1, np.asarray(probs, dtype=np.float64), width_arr))
    return segments


def sample_bit_widths(
    count: int,
    segments: List[Tuple[float, float, np.ndarray, np.ndarray]],
    rng: np.random.Generator,
) -> np.ndarray:
    if count <= 0:
        return np.empty(0, dtype=np.uint16)
    u = rng.random(count)
    sampled = np.empty(count, dtype=np.int16)
    assigned = np.zeros(count, dtype=bool)
    for seg_idx, (p0, p1, probs, widths) in enumerate(segments):
        if seg_idx == len(segments) - 1:
            mask = (u >= p0) & (u <= p1)
        else:
            mask = (u >= p0) & (u < p1)
        if not np.any(mask):
            continue
        sampled[mask] = np.rint(np.interp(u[mask], probs, widths)).astype(np.int16)
        assigned[mask] = True
    if not np.all(assigned):
        # Numerical boundary fallback; use the nearest final segment width.
        sampled[~assigned] = int(segments[-1][3][-1])
    return np.clip(sampled, 2, 253).astype(np.uint16)


def compare_limbs_gt(values: np.ndarray, constant: np.ndarray) -> np.ndarray:
    """Unsigned little-endian 256-bit comparison: values > constant."""
    gt = np.zeros(values.shape[0], dtype=bool)
    eq = np.ones(values.shape[0], dtype=bool)
    for limb in range(3, -1, -1):
        v = values[:, limb]
        c = constant[limb]
        gt |= eq & (v > c)
        eq &= v == c
    return gt


def random_raw_u64(rng: np.random.Generator, shape: Tuple[int, ...]) -> np.ndarray:
    return rng.bit_generator.random_raw(int(np.prod(shape))).reshape(shape).astype(np.uint64, copy=False)


def generate_magnitudes(widths: np.ndarray, rng: np.random.Generator, limit_to_bn254_half: bool) -> np.ndarray:
    """Generate positive magnitudes with exact requested bit widths."""
    count = widths.size
    out = np.zeros((count, 4), dtype=np.uint64)
    if count == 0:
        return out

    for bit_width in np.unique(widths).tolist():
        bit_width = int(bit_width)
        idx = np.flatnonzero(widths == bit_width)
        limb_count = (bit_width + 63) // 64
        top_bits = bit_width - 64 * (limb_count - 1)
        raw = random_raw_u64(rng, (idx.size, limb_count))
        if top_bits < 64:
            mask = np.uint64((1 << top_bits) - 1)
            high_bit = np.uint64(1 << (top_bits - 1))
            raw[:, limb_count - 1] &= mask
            raw[:, limb_count - 1] |= high_bit
        else:
            raw[:, limb_count - 1] |= np.uint64(1 << 63)
        out[idx, :limb_count] = raw

    if limit_to_bn254_half:
        too_large = compare_limbs_gt(out, BN254_HALF_LIMBS)
        # Only 253-bit samples can exceed r/2. Rejection keeps the requested width.
        while np.any(too_large):
            idx = np.flatnonzero(too_large)
            replacement = generate_magnitudes(widths[idx], rng, limit_to_bn254_half=False)
            out[idx] = replacement
            too_large = compare_limbs_gt(out, BN254_HALF_LIMBS)
    return out


def twos_complement_256(magnitudes: np.ndarray) -> np.ndarray:
    out = ~magnitudes
    carry = np.ones(out.shape[0], dtype=np.uint64)
    for limb in range(4):
        old = out[:, limb].copy()
        out[:, limb] = old + carry
        carry = ((carry == 1) & (out[:, limb] == 0)).astype(np.uint64)
    return out


def constant_minus_values(constant: np.ndarray, values: np.ndarray) -> np.ndarray:
    """Compute constant-values for unsigned four-limb values, constant >= values."""
    out = np.empty_like(values)
    borrow = np.zeros(values.shape[0], dtype=np.uint64)
    for limb in range(4):
        c = np.full(values.shape[0], constant[limb], dtype=np.uint64)
        v = values[:, limb]
        tmp = c - v
        borrow1 = c < v
        result = tmp - borrow
        borrow2 = tmp < borrow
        out[:, limb] = result
        borrow = (borrow1 | borrow2).astype(np.uint64)
    return out


def encode_signed(magnitudes: np.ndarray, negative: np.ndarray, scalar_format: str) -> np.ndarray:
    encoded = magnitudes.copy()
    if not np.any(negative):
        return encoded
    neg_mag = magnitudes[negative]
    if scalar_format == "bn254_centered":
        encoded[negative] = constant_minus_values(BN254_R_LIMBS, neg_mag)
    elif scalar_format == "int256_twos_complement":
        encoded[negative] = twos_complement_256(neg_mag)
    elif scalar_format == "signed_magnitude_256":
        encoded[negative, 3] |= np.uint64(1 << 63)
    else:
        raise ValueError(f"Unsupported scalar format: {scalar_format}")
    return encoded


def scalar_to_signed_limbs(value: int, scalar_format: str) -> np.ndarray:
    negative = value < 0
    magnitude = abs(value)
    if scalar_format == "bn254_centered" and magnitude > BN254_HALF:
        raise ValueError(f"Value {value} is outside centered BN254 scalar range")
    mag = int_to_limbs4(magnitude).reshape(1, 4)
    return encode_signed(mag, np.array([negative]), scalar_format)[0]


class RealizedStats:
    def __init__(self, top1_value: int) -> None:
        self.total = 0
        self.zero = 0
        self.one = 0
        self.nonzero_nonone = 0
        self.top1 = 0
        self.top1_value = top1_value
        self.width_hist = np.zeros(254, dtype=np.int64)

    def add(self, signed_values_kind: np.ndarray, widths_nonzero_nonone: np.ndarray, top1_count: int, multiplier: int = 1) -> None:
        # kind: 0=zero, 1=one, 2=nonzero_nonone
        self.total += int(signed_values_kind.size) * multiplier
        self.zero += int(np.count_nonzero(signed_values_kind == 0)) * multiplier
        self.one += int(np.count_nonzero(signed_values_kind == 1)) * multiplier
        nn = int(np.count_nonzero(signed_values_kind == 2))
        self.nonzero_nonone += nn * multiplier
        self.top1 += int(top1_count) * multiplier
        if widths_nonzero_nonone.size:
            hist = np.bincount(widths_nonzero_nonone.astype(np.int64), minlength=254)[:254]
            self.width_hist += hist.astype(np.int64) * multiplier

    def quantile(self, q: float) -> int:
        if self.nonzero_nonone <= 0:
            return 0
        target = int(math.ceil(q * self.nonzero_nonone))
        cdf = np.cumsum(self.width_hist)
        return int(np.searchsorted(cdf, target, side="left"))

    def as_dict(self) -> Dict[str, object]:
        nn = max(1, self.nonzero_nonone)
        return {
            "cells_total": self.total,
            "zero": self.zero,
            "one": self.one,
            "zero_ratio": self.zero / self.total if self.total else 0.0,
            "one_ratio": self.one / self.total if self.total else 0.0,
            "nonzero_nonone": self.nonzero_nonone,
            "nonzero_nonone_ratio": self.nonzero_nonone / self.total if self.total else 0.0,
            "ratio_nonzero_small_width_le_32": int(self.width_hist[:33].sum()) / nn,
            "ratio_nonzero_small_width_le_64": int(self.width_hist[:65].sum()) / nn,
            "ratio_nonzero_small_width_le_128": int(self.width_hist[:129].sum()) / nn,
            "signed_abs_bit_width_p50": self.quantile(0.50),
            "signed_abs_bit_width_p90": self.quantile(0.90),
            "signed_abs_bit_width_p99": self.quantile(0.99),
            "signed_abs_bit_width_max": int(np.max(np.flatnonzero(self.width_hist))) if self.width_hist.any() else 0,
            "global_top1_signed_value": self.top1_value,
            "global_top1_count": self.top1,
            "global_top1_ratio_nonzero_nonone": self.top1 / nn,
        }


def generate_encoded_values(
    count: int,
    row: Dict[str, str],
    scalar_format: str,
    negative_ratio: float,
    width_segments: List[Tuple[float, float, np.ndarray, np.ndarray]],
    rng: np.random.Generator,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, int]:
    """Return encoded limbs, category kind, widths for nonzero/nonone entries, top1 count."""
    total_source = i(row, "cells_total")
    p_zero = i(row, "zero") / total_source
    p_one = i(row, "one") / total_source
    top1_ratio = f(row, "global_top1_ratio_nonzero_nonone")
    top1_value = int(row["global_top1_signed_value"])

    u = rng.random(count)
    kind = np.full(count, 2, dtype=np.uint8)
    kind[u < p_zero] = 0
    kind[(u >= p_zero) & (u < p_zero + p_one)] = 1

    encoded = np.zeros((count, 4), dtype=np.uint64)
    one_idx = np.flatnonzero(kind == 1)
    if one_idx.size:
        encoded[one_idx, 0] = 1

    nn_idx = np.flatnonzero(kind == 2)
    widths = np.empty(nn_idx.size, dtype=np.uint16)
    top_count = 0
    if nn_idx.size:
        top_flags = rng.random(nn_idx.size) < top1_ratio
        top_count = int(np.count_nonzero(top_flags))
        random_flags = ~top_flags

        # Random part from the approximate width CDF.
        random_widths = sample_bit_widths(int(np.count_nonzero(random_flags)), width_segments, rng)
        widths[random_flags] = random_widths
        magnitudes = np.zeros((nn_idx.size, 4), dtype=np.uint64)
        if random_widths.size:
            magnitudes[random_flags] = generate_magnitudes(
                random_widths,
                rng,
                limit_to_bn254_half=(scalar_format == "bn254_centered"),
            )

        negative = np.zeros(nn_idx.size, dtype=bool)
        if np.any(random_flags):
            negative[random_flags] = rng.random(int(np.count_nonzero(random_flags))) < negative_ratio

        if top_count:
            top_mag = abs(top1_value)
            widths[top_flags] = max(1, top_mag.bit_length())
            magnitudes[top_flags] = int_to_limbs4(top_mag)
            negative[top_flags] = top1_value < 0

        encoded[nn_idx] = encode_signed(magnitudes, negative, scalar_format)

    return encoded, kind, widths, top_count


def expected_profile(row: Dict[str, str]) -> Dict[str, object]:
    return {
        "workload": row["workload"],
        "cells_total_source": i(row, "cells_total"),
        "zero_ratio": i(row, "zero") / i(row, "cells_total"),
        "one_ratio": i(row, "one") / i(row, "cells_total"),
        "nonzero_nonone_ratio": f(row, "nonzero_nonone_ratio"),
        "ratio_nonzero_small_width_le_32": f(row, "ratio_nonzero_small_width_le_32"),
        "ratio_nonzero_small_width_le_64": f(row, "ratio_nonzero_small_width_le_64"),
        "ratio_nonzero_small_width_le_128": f(row, "ratio_nonzero_small_width_le_128"),
        "signed_abs_bit_width_p50": i(row, "signed_abs_bit_width_p50"),
        "signed_abs_bit_width_p90": i(row, "signed_abs_bit_width_p90"),
        "signed_abs_bit_width_p99": i(row, "signed_abs_bit_width_p99"),
        "signed_abs_bit_width_max": i(row, "signed_abs_bit_width_max"),
        "global_top1_signed_value": i(row, "global_top1_signed_value"),
        "global_top1_ratio_nonzero_nonone": f(row, "global_top1_ratio_nonzero_nonone"),
    }


def main() -> None:
    args = parse_args()
    if args.K <= 0 or args.n <= 0:
        raise ValueError("--K and --n must be positive")
    if args.chunk_scalars <= 0:
        raise ValueError("--chunk-scalars must be positive")
    if not 0.0 <= args.negative_ratio <= 1.0:
        raise ValueError("--negative-ratio must be in [0,1]")
    if args.column_group_size <= 0:
        raise ValueError("--column-group-size must be positive")

    output = Path(args.output)
    if output.exists() and not args.overwrite:
        raise FileExistsError(f"Output already exists: {output}. Use --overwrite to replace it.")
    output.parent.mkdir(parents=True, exist_ok=True)

    row = load_profile(args.summary, args.workload)
    width_segments = build_width_segments(row)
    rng = np.random.default_rng(args.seed)
    stats = RealizedStats(int(row["global_top1_signed_value"]))

    print(f"workload            : {row['workload']}")
    print(f"shape               : ({args.K}, {args.n}, 4)")
    print(f"scalar_format       : {args.scalar_format}")
    print(f"column_group_size   : {args.column_group_size}")
    print(f"output_size_GiB     : {args.K * args.n * 4 * 8 / (1024**3):.3f}")
    print("width_segments      : " + str([(p0, p1, list(zip(ps.tolist(), ws.tolist()))) for p0, p1, ps, ws in width_segments]))

    t0 = time.perf_counter()
    arr = np.lib.format.open_memmap(output, mode="w+", dtype=np.uint64, shape=(args.K, args.n, 4))

    if args.column_group_size == 1:
        flat = arr.reshape(-1, 4)
        total = args.K * args.n
        for start in range(0, total, args.chunk_scalars):
            end = min(total, start + args.chunk_scalars)
            encoded, kind, widths, top_count = generate_encoded_values(
                end - start,
                row,
                args.scalar_format,
                args.negative_ratio,
                width_segments,
                rng,
            )
            flat[start:end] = encoded
            stats.add(kind, widths, top_count)
            if start == 0 or end == total or (start // args.chunk_scalars) % 20 == 0:
                print(f"generated           : {end}/{total} ({100.0 * end / total:.2f}%)", flush=True)
    else:
        # Every group of columns receives one K-row pattern. The final short group is
        # replicated only across its actual width, preserving the target cell count.
        g = args.column_group_size
        for col_start in range(0, args.n, g):
            col_end = min(args.n, col_start + g)
            width = col_end - col_start
            encoded, kind, widths, top_count = generate_encoded_values(
                args.K,
                row,
                args.scalar_format,
                args.negative_ratio,
                width_segments,
                rng,
            )
            arr[:, col_start:col_end, :] = encoded[:, None, :]
            stats.add(kind, widths, top_count, multiplier=width)
            if col_start == 0 or col_end == args.n or (col_start // g) % 256 == 0:
                print(f"generated columns   : {col_end}/{args.n} ({100.0 * col_end / args.n:.2f}%)", flush=True)

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
        "column_group_size": args.column_group_size,
        "expected": expected_profile(row),
        "realized": stats.as_dict(),
        "generation_seconds": elapsed,
        "output_path": str(output.resolve()),
        "output_size_bytes": output.stat().st_size,
        "note": (
            "The CSV contains marginal statistics only. IID generation does not reconstruct unknown cross-row/column correlations. "
            "column_group_size>1 deliberately injects exact repeated column patterns for mask-reuse testing."
        ),
    }
    meta_path = output.with_suffix(output.suffix + ".meta.json")
    with open(meta_path, "w", encoding="utf-8") as fobj:
        json.dump(metadata, fobj, ensure_ascii=False, indent=2)

    print(f"generation_seconds  : {elapsed:.3f}")
    print(f"output              : {output.resolve()}")
    print(f"metadata            : {meta_path.resolve()}")
    print(json.dumps(metadata["realized"], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
