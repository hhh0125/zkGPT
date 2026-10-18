#!/usr/bin/env python3
"""
Analyze theoretical bucket-update savings for traditional batch fixed-base
Pippenger MSM using tile-based common-submask grouping.

This script studies K MSMs of equal length n that all share the same points P[i].
It does not perform any elliptic-curve arithmetic. It only counts theoretical
point-add costs for bucket accumulation.

# 传统的pippenger算法
# 依旧分块，必须要每块内的mask相同
Model:
    S_k = sum_j q^j * T[k][j]
    T[k][j] = sum_i digit(A[k][i], j) * P[i]

For each fixed window j and signed digit bucket b, every point i induces a
K-bit residual mask:
    M_i = { k | digit(A[k, i], j) == b }

Instead of requiring exact equality of the full K-bit masks, this analyzer
splits the K MSM rows into tiles. For each tile t, it groups points by the
exact tile-local mask:
    local_mask(i, t) = M_i restricted to tile t

For one such local-mask group:
    r = number of points in the group
    u = popcount(local_mask)
    naive cost   = r * u
    grouped cost = (r - 1) + u
    gain         = (r - 1) * (u - 1)

The grouped cost is used only when gain >= min_gain. Since tiles are disjoint,
their costs can be summed independently without double counting.

Important representation note:
The input .npy stores each scalar in 4 uint64 limbs, but only the first two
limbs are meaningful. They represent a signed __int128 in two's complement:

    low = limb[0]
    high = limb[1]
    u = low + (high << 64)
    if u >= 2^127:
        v = u - 2^128
    else:
        v = u

Do not interpret all 4 limbs as a signed 256-bit integer, and do not convert
negative values into large positive residues modulo the scalar field order.
"""

from __future__ import annotations

import argparse
import json
import os
from collections import defaultdict
from typing import Dict, Iterable, List, Tuple

import numpy as np


UINT64_ONE = np.uint64(1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Analyze tile-based common-submask grouping for traditional "
            "batch fixed-base Pippenger MSM."
        )
    )
    parser.add_argument("--input", required=True, help="Path to Vi.npy")
    parser.add_argument("--K", type=int, default=1 << 14, help="Number of MSMs")
    parser.add_argument("--n", type=int, default=1 << 14, help="Points per MSM")
    parser.add_argument("--c", type=int, default=6, help="Window size in bits")
    parser.add_argument(
        "--point-chunk",
        type=int,
        default=512,
        help=(
            "Temporary point-column chunk size used only to limit memory. "
            "Masks still always span all K MSM rows."
        ),
    )
    parser.add_argument(
        "--out",
        default="mask_grouped_reuse_stats.json",
        help="Output JSON path",
    )
    parser.add_argument(
        "--tile-size",
        type=int,
        default=16,
        choices=(8, 16, 32, 64),
        help="Tile size used to partition the K-dimensional mask space",
    )
    parser.add_argument(
        "--min-gain",
        type=int,
        default=1,
        help="Accept grouping only when gain=(r-1)*(u-1) >= min_gain",
    )
    parser.add_argument(
        "--ktile",
        type=int,
        default=None,
        help="Deprecated and ignored. Use --tile-size instead.",
    )
    return parser.parse_args()


def load_scalar_memmap(path: str, K: int, n: int) -> Tuple[np.ndarray, Tuple[int, ...]]:
    arr = np.load(path, mmap_mode="r")
    original_shape = tuple(arr.shape)
    expected_scalars = K * n
    expected_flat = expected_scalars * 4

    if arr.ndim == 1:
        if arr.shape[0] != expected_flat:
            raise ValueError(
                f"Flat .npy length mismatch: got {arr.shape[0]}, expected {expected_flat}"
            )
        arr = arr.reshape(K, n, 4)
    elif arr.ndim == 2:
        if arr.shape == (expected_scalars, 4):
            arr = arr.reshape(K, n, 4)
        elif arr.shape == (K, n * 4):
            arr = arr.reshape(K, n, 4)
        else:
            raise ValueError(
                f"Unsupported 2D .npy shape {arr.shape}; expected {(expected_scalars, 4)}"
            )
    elif arr.ndim == 3:
        if arr.shape != (K, n, 4):
            raise ValueError(
                f"Unsupported 3D .npy shape {arr.shape}; expected {(K, n, 4)}"
            )
    else:
        raise ValueError(f"Unsupported .npy rank {arr.ndim}, shape={arr.shape}")

    if arr.dtype != np.uint64:
        arr = arr.astype(np.uint64, copy=False)
    return arr, original_shape


def decode_abs_block(block: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Decode block[..., 0:2] as signed __int128 two's complement, but return the
    magnitude in two uint64 limbs plus the sign bit mask.

    Returns:
        abs_lo: uint64 array of low 64 bits of |v|
        abs_hi: uint64 array of high 64 bits of |v|
        neg:    bool array, True where the original scalar was negative
    """

    lo = np.asarray(block[..., 0], dtype=np.uint64)
    hi = np.asarray(block[..., 1], dtype=np.uint64)
    neg = (hi >> np.uint64(63)) != 0

    abs_lo = lo.copy()
    abs_hi = hi.copy()
    if np.any(neg):
        lo_neg = lo[neg]
        hi_neg = hi[neg]

        abs_lo_neg = (~lo_neg + UINT64_ONE).astype(np.uint64)
        carry = (abs_lo_neg == 0).astype(np.uint64)
        abs_hi_neg = (~hi_neg + carry).astype(np.uint64)

        abs_lo[neg] = abs_lo_neg
        abs_hi[neg] = abs_hi_neg

    return abs_lo, abs_hi, neg


def update_chunk_max_bitlen(abs_lo: np.ndarray, abs_hi: np.ndarray, current_max: int) -> int:
    if abs_hi.size == 0:
        return current_max

    max_hi = int(abs_hi.max())
    if max_hi > 0:
        mask = abs_hi == np.uint64(max_hi)
        max_lo = int(abs_lo[mask].max()) if np.any(mask) else 0
        candidate = (max_hi << 64) | max_lo
        return max(current_max, candidate.bit_length())

    max_lo = int(abs_lo.max())
    return max(current_max, max_lo.bit_length())


def extract_window_digits(abs_lo: np.ndarray, abs_hi: np.ndarray, shift: int, qmask: int) -> np.ndarray:
    """
    Extract one radix-2^c magnitude digit from |v| represented by (abs_lo, abs_hi).
    """

    qmask_u = np.uint64(qmask)
    if shift == 0:
        return abs_lo & qmask_u
    if shift < 64:
        return ((abs_lo >> np.uint64(shift)) | (abs_hi << np.uint64(64 - shift))) & qmask_u
    return (abs_hi >> np.uint64(shift - 64)) & qmask_u


def choose_digit_dtype(max_digit: int) -> np.dtype:
    if max_digit <= np.iinfo(np.int8).max:
        return np.int8
    if max_digit <= np.iinfo(np.int16).max:
        return np.int16
    if max_digit <= np.iinfo(np.int32).max:
        return np.int32
    return np.int64


def build_signed_digits(digits: np.ndarray, neg_mask: np.ndarray, dtype: np.dtype) -> np.ndarray:
    signed = digits.astype(dtype, copy=True)
    signed[neg_mask & (digits != 0)] *= -1
    return signed


def sparse_hist_to_dict(counts: np.ndarray) -> Dict[str, int]:
    result: Dict[str, int] = {}
    for idx in np.flatnonzero(counts):
        result[str(int(idx))] = int(counts[idx])
    return result


def bucket_hist_to_dict(bucket_stats: Dict[int, int]) -> Dict[str, int]:
    return {str(k): int(v) for k, v in sorted(bucket_stats.items()) if v != 0}


def compute_max_abs_bitlen(scalars: np.ndarray, point_chunk: int) -> int:
    K, n, _ = scalars.shape
    max_abs_bitlen = 0
    for point_start in range(0, n, point_chunk):
        point_end = min(n, point_start + point_chunk)
        block = scalars[:, point_start:point_end, :]
        abs_lo, abs_hi, _ = decode_abs_block(block)
        max_abs_bitlen = update_chunk_max_bitlen(abs_lo, abs_hi, max_abs_bitlen)
    return max_abs_bitlen


def iter_present_nonzero_buckets(
    signed_digits: np.ndarray, q: int
) -> Iterable[Tuple[int, int]]:
    """
    Yield (signed_digit, count) pairs for nonzero signed digits in the chunk.
    """

    offset = q - 1
    if q <= 4096:
        flat = signed_digits.astype(np.int32, copy=False).reshape(-1) + offset
        counts = np.bincount(flat, minlength=2 * q - 1)
        for idx in np.flatnonzero(counts):
            if idx == offset:
                continue
            yield idx - offset, int(counts[idx])
        return

    values, counts = np.unique(signed_digits, return_counts=True)
    for value, count in zip(values.tolist(), counts.tolist()):
        if value == 0:
            continue
        yield int(value), int(count)


def add_tile_mask_groups_for_bucket(
    window_groups: Dict[Tuple[int, int, int], int],
    signed_digits: np.ndarray,
    bucket: int,
    tile_ranges: List[Tuple[int, int]],
    bit_weights: Dict[int, np.ndarray],
) -> None:
    """
    For one fixed window and one fixed signed digit bucket:
      - restrict the K-row mask to each tile,
      - group identical nonzero tile-local masks,
      - update window_groups[(bucket, tile_idx, local_mask)] += r
    """

    eq = signed_digits == bucket
    active_cols = np.flatnonzero(np.any(eq, axis=0))
    if active_cols.size == 0:
        return

    eq_active = eq[:, active_cols]
    for tile_idx, (tile_start, tile_end) in enumerate(tile_ranges):
        tile_eq = eq_active[tile_start:tile_end, :]
        tile_len = tile_end - tile_start
        weights = bit_weights[tile_len]

        local_masks = (tile_eq.astype(np.uint64, copy=False) * weights[:, None]).sum(
            axis=0, dtype=np.uint64
        )
        nonzero_masks = local_masks[local_masks != 0]
        if nonzero_masks.size == 0:
            continue

        unique_masks, counts = np.unique(nonzero_masks, return_counts=True)
        for local_mask, count in zip(unique_masks.tolist(), counts.tolist()):
            key = (bucket, tile_idx, int(local_mask))
            window_groups[key] = window_groups.get(key, 0) + int(count)


def finalize_window_groups(
    window_groups: Dict[Tuple[int, int, int], int],
    window_j: int,
    min_gain: int,
    per_window_C_naive: List[int],
    per_window_C_group: List[int],
    per_digit_naive: Dict[int, int],
    per_digit_group: Dict[int, int],
    mask_popcount_histogram: np.ndarray,
    group_size_histogram: np.ndarray,
) -> Tuple[int, int, int, int, int, int]:
    """
    Consume one window dictionary and return:
      window_naive, window_group,
      accepted_group_count, rejected_group_count,
      accepted_group_size_sum, max_group_size
    """

    window_naive = 0
    window_group = 0
    accepted_group_count = 0
    rejected_group_count = 0
    accepted_group_size_sum = 0
    max_group_size = 0

    for (bucket, _tile_idx, local_mask), r in window_groups.items():
        u = int(local_mask).bit_count()
        naive_cost = r * u
        gain = (r - 1) * (u - 1)
        if gain >= min_gain:
            grouped_cost = (r - 1) + u
            accepted_group_count += 1
            accepted_group_size_sum += r
        else:
            grouped_cost = naive_cost
            rejected_group_count += 1

        window_naive += naive_cost
        window_group += grouped_cost
        max_group_size = max(max_group_size, r)

        mask_popcount_histogram[u] += 1
        group_size_histogram[r] += 1

        per_digit_naive[bucket] += naive_cost
        per_digit_group[bucket] += grouped_cost

    per_window_C_naive[window_j] = window_naive
    per_window_C_group[window_j] = window_group
    return (
        window_naive,
        window_group,
        accepted_group_count,
        rejected_group_count,
        accepted_group_size_sum,
        max_group_size,
    )


def main() -> None:
    args = parse_args()
    if args.c <= 0 or args.c > 63:
        raise ValueError("--c must be in [1, 63]")
    if args.K <= 0 or args.n <= 0:
        raise ValueError("--K and --n must be positive")
    if args.point_chunk <= 0:
        raise ValueError("--point-chunk must be positive")
    if args.min_gain < 0:
        raise ValueError("--min-gain must be non-negative")

    scalars, input_shape = load_scalar_memmap(args.input, args.K, args.n)
    max_abs_bitlen = compute_max_abs_bitlen(scalars, args.point_chunk)

    q = 1 << args.c
    qmask = q - 1
    h = (max_abs_bitlen + args.c - 1) // args.c if max_abs_bitlen > 0 else 0
    digit_dtype = choose_digit_dtype(q - 1)
    tile_ranges = [
        (tile_start, min(args.K, tile_start + args.tile_size))
        for tile_start in range(0, args.K, args.tile_size)
    ]
    bit_weights = {
        tile_end - tile_start: np.left_shift(
            np.uint64(1), np.arange(tile_end - tile_start, dtype=np.uint64)
        )
        for tile_start, tile_end in tile_ranges
    }

    total_nonzero_digits = 0
    C_naive = 0
    C_group = 0
    accepted_group_count = 0
    rejected_group_count = 0
    accepted_group_size_sum = 0
    max_group_size = 0

    digit_histogram: Dict[int, int] = defaultdict(int)
    per_digit_naive: Dict[int, int] = defaultdict(int)
    per_digit_group: Dict[int, int] = defaultdict(int)

    mask_popcount_histogram = np.zeros(args.tile_size + 1, dtype=np.int64)
    group_size_histogram = np.zeros(args.n + 1, dtype=np.int64)
    per_window_C_naive = [0] * h
    per_window_C_group = [0] * h

    for window_j in range(h):
        shift = window_j * args.c
        window_groups: Dict[Tuple[int, int, int], int] = {}

        for point_start in range(0, args.n, args.point_chunk):
            point_end = min(args.n, point_start + args.point_chunk)
            block = scalars[:, point_start:point_end, :]

            abs_lo, abs_hi, neg_mask = decode_abs_block(block)
            digits = extract_window_digits(abs_lo, abs_hi, shift, qmask)
            signed_digits = build_signed_digits(digits, neg_mask, digit_dtype)

            for bucket, bucket_digit_count in iter_present_nonzero_buckets(signed_digits, q):
                digit_histogram[bucket] += bucket_digit_count
                total_nonzero_digits += bucket_digit_count
                add_tile_mask_groups_for_bucket(
                    window_groups, signed_digits, bucket, tile_ranges, bit_weights
                )

        (
            window_naive,
            window_group,
            window_accepted_count,
            window_rejected_count,
            window_accepted_size_sum,
            window_max_r,
        ) = finalize_window_groups(
            window_groups,
            window_j,
            args.min_gain,
            per_window_C_naive,
            per_window_C_group,
            per_digit_naive,
            per_digit_group,
            mask_popcount_histogram,
            group_size_histogram,
        )

        C_naive += window_naive
        C_group += window_group
        accepted_group_count += window_accepted_count
        rejected_group_count += window_rejected_count
        accepted_group_size_sum += window_accepted_size_sum
        max_group_size = max(max_group_size, window_max_r)

        del window_groups

    total_positions = args.K * args.n * h
    zero_count = total_positions - total_nonzero_digits
    zero_rate = (zero_count / total_positions) if total_positions > 0 else 0.0
    saving_ratio = ((C_naive - C_group) / C_naive) if C_naive > 0 else 0.0
    reuse_factor = (C_naive / C_group) if C_group > 0 else None
    average_accepted_group_size = (
        accepted_group_size_sum / accepted_group_count if accepted_group_count > 0 else 0.0
    )

    per_window_saving_ratio = []
    for naive, grouped in zip(per_window_C_naive, per_window_C_group):
        if naive > 0:
            per_window_saving_ratio.append((naive - grouped) / naive)
        else:
            per_window_saving_ratio.append(0.0)

    per_digit_saving_ratio: Dict[str, float] = {}
    for digit in sorted(set(per_digit_naive) | set(per_digit_group)):
        naive = per_digit_naive[digit]
        grouped = per_digit_group[digit]
        if naive > 0:
            per_digit_saving_ratio[str(digit)] = (naive - grouped) / naive
        else:
            per_digit_saving_ratio[str(digit)] = 0.0

    digit_histogram[0] = zero_count

    result = {
        "input_path": os.path.abspath(args.input),
        "input_shape": list(input_shape),
        "logical_shape": [args.K, args.n, 4],
        "analysis_mode": "tile_based_common_submask_traditional_pippenger",
        "K": args.K,
        "n": args.n,
        "c": args.c,
        "h": h,
        "q": q,
        "point_chunk": args.point_chunk,
        "tile_size": args.tile_size,
        "tile_count": len(tile_ranges),
        "min_gain": args.min_gain,
        "deprecated_ktile": args.ktile,
        "max_abs_bit_length": max_abs_bitlen,
        "total_nonzero_digits": int(total_nonzero_digits),
        "C_naive": int(C_naive),
        "C_group": int(C_group),
        "saving_ratio": saving_ratio,
        "reuse_factor": reuse_factor,
        "accepted_group_count": int(accepted_group_count),
        "rejected_group_count": int(rejected_group_count),
        "average_accepted_group_size": average_accepted_group_size,
        "max_group_size": int(max_group_size),
        "mask_popcount_histogram": sparse_hist_to_dict(mask_popcount_histogram),
        "group_size_histogram": sparse_hist_to_dict(group_size_histogram),
        "per_window_C_naive": [int(x) for x in per_window_C_naive],
        "per_window_C_group": [int(x) for x in per_window_C_group],
        "per_window_saving_ratio": per_window_saving_ratio,
        "per_digit_saving_ratio": per_digit_saving_ratio,
        "zero_rate": zero_rate,
        "zero_count": int(zero_count),
        "digit_histogram": bucket_hist_to_dict(digit_histogram),
    }

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"input_shape          : {input_shape}")
    print(f"logical_shape        : ({args.K}, {args.n}, 4)")
    print("analysis_mode        : tile_based_common_submask_traditional_pippenger")
    print(f"K                    : {args.K}")
    print(f"n                    : {args.n}")
    print(f"c                    : {args.c}")
    print(f"h                    : {h}")
    print(f"q                    : {q}")
    print(f"point_chunk          : {args.point_chunk}")
    print(f"tile_size            : {args.tile_size}")
    print(f"tile_count           : {len(tile_ranges)}")
    print(f"min_gain             : {args.min_gain}")
    print(f"max_abs_bit_length   : {max_abs_bitlen}")
    print(f"total_nonzero_digits : {int(total_nonzero_digits)}")
    print(f"C_naive              : {int(C_naive)}")
    print(f"C_group              : {int(C_group)}")
    print(f"saving_ratio         : {saving_ratio:.8f}")
    print(
        "reuse_factor         : "
        + ("null" if reuse_factor is None else f"{reuse_factor:.8f}")
    )
    print(f"accepted_group_count : {int(accepted_group_count)}")
    print(f"rejected_group_count : {int(rejected_group_count)}")
    print(f"avg_acc_group_size   : {average_accepted_group_size:.8f}")
    print(f"max_group_size       : {int(max_group_size)}")
    print(f"zero_rate            : {zero_rate:.8f}")
    print(f"json_saved_to        : {os.path.abspath(args.out)}")


if __name__ == "__main__":
    main()
