#!/usr/bin/env python3
"""
# 拆分了小mask

# python3 20fullmask_tile_split_fixed.py   --input vi_0402.npy   --K 16384   --n 16384   
# --c-start 8    --c-end 12   --point-chunk 512 --tile-size 32   --min-gain 1   --max-rounds 16   
# --min-submask-popcount 2   --top-masks-per-bucket 128   --out-dir full_split_reults


Analyze theoretical bucket-update savings for traditional batch fixed-base
Pippenger MSM using tile-mask residual splitting.

This script studies K MSMs of equal length n that all share the same points P[i].
It does not perform any elliptic-curve arithmetic. It only counts theoretical
point-add costs for bucket accumulation.

For each window width c in [c_start, c_end], the script:
1. scans all windows j of the traditional radix-2^c Pippenger decomposition,
2. builds full K-bit masks for every nonzero signed bucket,
3. builds tile-local masks so no-split mode matches the original tile method,
4. greedily extracts reusable common submasks from those tile-local masks,
5. writes one JSON result for that c,
6. appends one row to the summary CSV.

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
import csv
import heapq
import json
import os
from collections import defaultdict
from typing import DefaultDict, Dict, Iterable, List, Tuple

import numpy as np


UINT64_ONE = np.uint64(1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Analyze traditional Pippenger tile-mask residual splitting for "
            "all window widths in a given range."
        )
    )
    parser.add_argument("--input", required=True, help="Path to Vi.npy")
    parser.add_argument("--K", type=int, default=1 << 14, help="Number of MSMs")
    parser.add_argument("--n", type=int, default=1 << 14, help="Points per MSM")
    parser.add_argument("--c-start", type=int, default=6, help="First window width to test")
    parser.add_argument("--c-end", type=int, default=12, help="Last window width to test")
    parser.add_argument(
        "--point-chunk",
        type=int,
        default=512,
        help=(
            "Temporary point-column chunk size used only to limit memory. "
            "Masks are built inside each K-tile."
        ),
    )
    parser.add_argument(
        "--tile-size",
        type=int,
        default=32,
        help=(
            "Number of MSM rows covered by one local mask. "
            "When max_rounds=0, this recovers the original no-split tile-mask method."
        ),
    )
    parser.add_argument(
        "--top-masks-per-bucket",
        type=int,
        default=128,
        help="Use only the top-N residual masks per bucket/tile to generate candidates",
    )
    parser.add_argument(
        "--max-rounds",
        type=int,
        default=16,
        help="Maximum greedy splitting rounds for one (window, bucket)",
    )
    parser.add_argument(
        "--min-gain",
        type=int,
        default=32,
        help="Accept a candidate only when gain=(r-1)*(u-1) >= min_gain",
    )
    parser.add_argument(
        "--min-submask-popcount",
        type=int,
        default=2,
        help="Discard candidate submasks with popcount below this threshold",
    )
    parser.add_argument(
        "--out-dir",
        default="fullmask_split_results",
        help="Directory that will receive per-c JSON files and the summary CSV",
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


def add_tile_mask_counts_for_bucket(
    bucket_tile_mask_counts: DefaultDict[int, Dict[int, int]],
    signed_digits: np.ndarray,
    bucket: int,
    tile_size: int,
    mask_nbytes: int,
) -> None:
    """
    For one fixed window and one fixed signed digit bucket, build each point
    column's local tile mask, compress identical masks inside the chunk, and
    add them into bucket_tile_mask_counts[tile_id][mask_int].

    This is the key correction versus the old full-K-mask code:
    masks are local to a tile, so max_rounds=0 exactly falls back to the
    original no-split tile-mask grouping method.
    """

    K = signed_digits.shape[0]
    eq = signed_digits == bucket

    for tile_id, tile_start in enumerate(range(0, K, tile_size)):
        tile_end = min(K, tile_start + tile_size)
        eq_tile = eq[tile_start:tile_end, :]
        active_cols = np.flatnonzero(np.any(eq_tile, axis=0))
        if active_cols.size == 0:
            continue

        packed = np.packbits(eq_tile[:, active_cols], axis=0, bitorder="little").T
        packed = np.ascontiguousarray(packed)

        key_dtype = np.dtype((np.void, mask_nbytes))
        key_view = packed.view(key_dtype).reshape(-1)
        unique_keys, counts = np.unique(key_view, return_counts=True)
        unique_bytes = unique_keys.view(np.uint8).reshape(-1, mask_nbytes)

        tile_counts = bucket_tile_mask_counts[tile_id]
        for idx, count in enumerate(counts.tolist()):
            mask_int = int.from_bytes(unique_bytes[idx].tobytes(), "little")
            tile_counts[mask_int] = tile_counts.get(mask_int, 0) + int(count)


def compute_mask_cost(mask_counts: Dict[int, int]) -> int:
    """Raw point-update cost: every point is added to every set bit of its mask."""
    return sum(count * mask.bit_count() for mask, count in mask_counts.items())


def full_group_cost_for_mask(count: int, mask: int, min_gain: int) -> Tuple[int, bool, int]:
    """
    Cost of the original no-split grouping for one exact mask group.

    Raw cost is r*u. Grouped cost is (r-1)+u. The saving is
    (r*u)-((r-1)+u) = (r-1)*(u-1). We accept the exact-mask group only when
    this saving reaches min_gain, matching the original mask-group method.
    """
    u = mask.bit_count()
    raw_cost = count * u
    gain = (count - 1) * (u - 1)
    if u > 0 and gain >= min_gain:
        return (count - 1) + u, True, gain
    return raw_cost, False, gain


def compute_no_split_group_cost(mask_counts: Dict[int, int], min_gain: int) -> Tuple[int, int, int, int, int, int]:
    """
    Original tile-local no-split method over exact masks.

    Returns:
      cost, accepted_group_count, popcount_sum, group_r_sum, max_gain, max_group_r
    """
    cost = 0
    accepted = 0
    popcount_sum = 0
    group_r_sum = 0
    max_gain = 0
    max_group_r = 0

    for mask, count in mask_counts.items():
        group_cost, is_accepted, gain = full_group_cost_for_mask(count, mask, min_gain)
        cost += group_cost
        max_gain = max(max_gain, gain)
        if is_accepted:
            accepted += 1
            popcount_sum += mask.bit_count()
            group_r_sum += count
            max_group_r = max(max_group_r, count)

    return cost, accepted, popcount_sum, group_r_sum, max_gain, max_group_r


def build_candidate_submasks(
    residual: Dict[int, int],
    top_masks_per_bucket: int,
    min_submask_popcount: int,
    include_exact_masks: bool = False,
) -> Dict[int, int]:
    """
    Candidate submasks come from pairwise intersections among top residual masks.
    By default exact masks themselves are excluded, because the no-split baseline
    already handles exact-mask grouping. This makes splitting focus on additional
    smaller/common masks.
    Returns {candidate_mask: popcount(candidate_mask)}.
    """

    top_items = heapq.nlargest(top_masks_per_bucket, residual.items(), key=lambda item: item[1])
    top_masks = [mask for mask, _count in top_items]
    candidates: Dict[int, int] = {}

    if include_exact_masks:
        for mask in top_masks:
            popcount = mask.bit_count()
            if popcount >= min_submask_popcount:
                candidates[mask] = popcount

    for idx_a in range(len(top_masks)):
        mask_a = top_masks[idx_a]
        for idx_b in range(idx_a + 1, len(top_masks)):
            candidate = mask_a & top_masks[idx_b]
            if candidate == 0:
                continue
            if candidate in candidates:
                continue
            popcount = candidate.bit_count()
            if popcount >= min_submask_popcount:
                candidates[candidate] = popcount

    return candidates


def split_bucket_masks(
    mask_counts: Dict[int, int],
    top_masks_per_bucket: int,
    max_rounds: int,
    min_gain: int,
    min_submask_popcount: int,
) -> Dict[str, float | int]:
    """
    Greedy residual splitting for one fixed (window, signed bucket, K-tile).

    Correctness/monotonicity rule:
      C_split <= C_no_split <= C_naive.

    We guarantee this by:
      1. computing the original no-split exact-mask grouping as baseline;
      2. running greedy submask splitting as an alternative plan;
      3. after each split, charging the remaining residual with the same
         no-split exact-mask grouping, not with raw point updates;
      4. returning min(split_plan, no_split_baseline).

    This matches the intended semantics: not splitting falls back to the first
    method, while splitting can only add extra opportunities on residual masks.
    """

    residual = dict(mask_counts)
    naive_cost = compute_mask_cost(mask_counts)
    (
        no_split_cost,
        no_split_group_count,
        no_split_popcount_sum,
        no_split_group_r_sum,
        no_split_max_gain,
        no_split_max_group_r,
    ) = compute_no_split_group_cost(mask_counts, min_gain)

    # If max_rounds=0, exactly recover the original no-split tile-mask method.
    if max_rounds == 0:
        return {
            "naive_cost": int(naive_cost),
            "no_split_cost": int(no_split_cost),
            "grouped_cost": int(no_split_cost),
            "split_plan_cost": int(no_split_cost),
            "used_split_plan": 0,
            "accepted_submask_count": int(no_split_group_count),
            "accepted_split_submask_count": 0,
            "rejected_candidate_count": 0,
            "submask_popcount_sum": int(no_split_popcount_sum),
            "group_r_sum": int(no_split_group_r_sum),
            "max_gain": int(no_split_max_gain),
            "max_group_r": int(no_split_max_group_r),
            "residual_cost": 0,
        }

    split_prefix_cost = 0
    accepted_split_submask_count = 0
    rejected_candidate_count = 0
    split_submask_popcount_sum = 0
    split_group_r_sum = 0
    max_gain_seen = no_split_max_gain
    max_group_r = 0

    best_total_cost = no_split_cost
    best_residual_cost = no_split_cost
    best_stats = {
        "accepted_split_submask_count": 0,
        "split_submask_popcount_sum": 0,
        "split_group_r_sum": 0,
        "max_gain_seen": no_split_max_gain,
        "max_group_r": no_split_max_group_r,
        "rejected_candidate_count": 0,
        "residual_group_count": no_split_group_count,
        "residual_popcount_sum": no_split_popcount_sum,
        "residual_group_r_sum": no_split_group_r_sum,
        "residual_max_group_r": no_split_max_group_r,
    }

    for _round_idx in range(max_rounds):
        if not residual:
            best_total_cost = min(best_total_cost, split_prefix_cost)
            best_residual_cost = 0
            break

        candidates = build_candidate_submasks(
            residual,
            top_masks_per_bucket=top_masks_per_bucket,
            min_submask_popcount=min_submask_popcount,
            include_exact_masks=False,
        )
        if not candidates:
            break

        residual_items = list(residual.items())
        best_mask = 0
        best_u = 0
        best_r = 0
        best_gain = -1

        for candidate_mask, popcount in candidates.items():
            group_r = 0
            for residual_mask, count in residual_items:
                if (residual_mask & candidate_mask) == candidate_mask:
                    group_r += count

            gain = (group_r - 1) * (popcount - 1)
            if (
                gain > best_gain
                or (gain == best_gain and popcount > best_u)
                or (gain == best_gain and popcount == best_u and group_r > best_r)
            ):
                best_mask = candidate_mask
                best_u = popcount
                best_r = group_r
                best_gain = gain

        if best_gain < min_gain:
            rejected_candidate_count += len(candidates)
            break

        accepted_split_submask_count += 1
        rejected_candidate_count += len(candidates) - 1
        split_submask_popcount_sum += best_u
        split_group_r_sum += best_r
        max_gain_seen = max(max_gain_seen, best_gain)
        max_group_r = max(max_group_r, best_r)
        split_prefix_cost += (best_r - 1) + best_u

        next_residual: Dict[int, int] = {}
        best_mask_complement = ~best_mask
        for residual_mask, count in residual_items:
            if (residual_mask & best_mask) == best_mask:
                new_mask = residual_mask & best_mask_complement
                if new_mask != 0:
                    next_residual[new_mask] = next_residual.get(new_mask, 0) + count
            else:
                next_residual[residual_mask] = next_residual.get(residual_mask, 0) + count
        residual = next_residual

        # Fallback for the residual must be the no-split method, not raw updates.
        (
            residual_group_cost,
            residual_group_count,
            residual_popcount_sum,
            residual_group_r_sum,
            residual_max_gain,
            residual_max_group_r,
        ) = compute_no_split_group_cost(residual, min_gain)
        candidate_total_cost = split_prefix_cost + residual_group_cost
        if candidate_total_cost < best_total_cost:
            best_total_cost = candidate_total_cost
            best_residual_cost = residual_group_cost
            best_stats = {
                "accepted_split_submask_count": accepted_split_submask_count,
                "split_submask_popcount_sum": split_submask_popcount_sum,
                "split_group_r_sum": split_group_r_sum,
                "max_gain_seen": max(max_gain_seen, residual_max_gain),
                "max_group_r": max(max_group_r, residual_max_group_r),
                "rejected_candidate_count": rejected_candidate_count,
                "residual_group_count": residual_group_count,
                "residual_popcount_sum": residual_popcount_sum,
                "residual_group_r_sum": residual_group_r_sum,
                "residual_max_group_r": residual_max_group_r,
            }

    used_split_plan = int(best_total_cost < no_split_cost)
    if used_split_plan:
        accepted_submask_count = (
            best_stats["accepted_split_submask_count"] + best_stats["residual_group_count"]
        )
        submask_popcount_sum = (
            best_stats["split_submask_popcount_sum"] + best_stats["residual_popcount_sum"]
        )
        group_r_sum = (
            best_stats["split_group_r_sum"] + best_stats["residual_group_r_sum"]
        )
        max_gain_seen = best_stats["max_gain_seen"]
        max_group_r = best_stats["max_group_r"]
        rejected_candidate_count = best_stats["rejected_candidate_count"]
        residual_cost = best_residual_cost
    else:
        accepted_submask_count = no_split_group_count
        submask_popcount_sum = no_split_popcount_sum
        group_r_sum = no_split_group_r_sum
        max_gain_seen = no_split_max_gain
        max_group_r = no_split_max_group_r
        residual_cost = 0

    return {
        "naive_cost": int(naive_cost),
        "no_split_cost": int(no_split_cost),
        "grouped_cost": int(best_total_cost),
        "split_plan_cost": int(best_total_cost),
        "used_split_plan": int(used_split_plan),
        "accepted_submask_count": int(accepted_submask_count),
        "accepted_split_submask_count": int(best_stats["accepted_split_submask_count"] if used_split_plan else 0),
        "rejected_candidate_count": int(rejected_candidate_count),
        "submask_popcount_sum": int(submask_popcount_sum),
        "group_r_sum": int(group_r_sum),
        "max_gain": int(max_gain_seen),
        "max_group_r": int(max_group_r),
        "residual_cost": int(residual_cost),
    }

def analyze_for_c(
    scalars: np.ndarray,
    input_path: str,
    input_shape: Tuple[int, ...],
    K: int,
    n: int,
    c: int,
    point_chunk: int,
    tile_size: int,
    top_masks_per_bucket: int,
    max_rounds: int,
    min_gain: int,
    min_submask_popcount: int,
    out_dir: str,
    max_abs_bitlen: int,
) -> Dict[str, object]:
    q = 1 << c
    qmask = q - 1
    h = (max_abs_bitlen + c - 1) // c if max_abs_bitlen > 0 else 0
    mask_nbytes = (tile_size + 7) // 8
    digit_dtype = choose_digit_dtype(q - 1)

    total_nonzero_digits = 0
    C_naive = 0
    C_group = 0
    C_no_split = 0
    split_improvement = 0
    used_split_plan_count = 0
    accepted_split_submask_count = 0
    accepted_submask_count = 0
    rejected_candidate_count = 0
    submask_popcount_sum = 0
    group_r_sum = 0
    max_gain_seen = 0
    max_group_r = 0
    residual_cost_total = 0

    digit_histogram: DefaultDict[int, int] = defaultdict(int)
    per_digit_naive: DefaultDict[int, int] = defaultdict(int)
    per_digit_group: DefaultDict[int, int] = defaultdict(int)
    per_window_C_naive = [0] * h
    per_window_C_group = [0] * h

    for window_j in range(h):
        shift = window_j * c
        window_bucket_counts: DefaultDict[int, DefaultDict[int, Dict[int, int]]] = defaultdict(lambda: defaultdict(dict))

        for point_start in range(0, n, point_chunk):
            point_end = min(n, point_start + point_chunk)
            block = scalars[:, point_start:point_end, :]

            abs_lo, abs_hi, neg_mask = decode_abs_block(block)
            digits = extract_window_digits(abs_lo, abs_hi, shift, qmask)
            signed_digits = build_signed_digits(digits, neg_mask, digit_dtype)

            for bucket, bucket_digit_count in iter_present_nonzero_buckets(signed_digits, q):
                digit_histogram[bucket] += bucket_digit_count
                total_nonzero_digits += bucket_digit_count
                add_tile_mask_counts_for_bucket(
                    window_bucket_counts[bucket],
                    signed_digits,
                    bucket,
                    tile_size,
                    mask_nbytes,
                )

        window_naive = 0
        window_group = 0
        for bucket, tile_mask_counts in window_bucket_counts.items():
            for _tile_id, mask_counts in tile_mask_counts.items():
                bucket_stats = split_bucket_masks(
                    mask_counts,
                    top_masks_per_bucket=top_masks_per_bucket,
                    max_rounds=max_rounds,
                    min_gain=min_gain,
                    min_submask_popcount=min_submask_popcount,
                )

                bucket_naive = int(bucket_stats["naive_cost"])
                bucket_no_split = int(bucket_stats["no_split_cost"])
                bucket_group = int(bucket_stats["grouped_cost"])

                window_naive += bucket_naive
                window_group += bucket_group
                C_naive += bucket_naive
                C_no_split += bucket_no_split
                C_group += bucket_group
                split_improvement += bucket_no_split - bucket_group

                accepted_submask_count += int(bucket_stats["accepted_submask_count"])
                accepted_split_submask_count += int(bucket_stats["accepted_split_submask_count"])
                used_split_plan_count += int(bucket_stats["used_split_plan"])
                rejected_candidate_count += int(bucket_stats["rejected_candidate_count"])
                submask_popcount_sum += int(bucket_stats["submask_popcount_sum"])
                group_r_sum += int(bucket_stats["group_r_sum"])
                max_gain_seen = max(max_gain_seen, int(bucket_stats["max_gain"]))
                max_group_r = max(max_group_r, int(bucket_stats["max_group_r"]))
                residual_cost_total += int(bucket_stats["residual_cost"])

                per_digit_naive[bucket] += bucket_naive
                per_digit_group[bucket] += bucket_group

        per_window_C_naive[window_j] = window_naive
        per_window_C_group[window_j] = window_group

    total_positions = K * n * h
    zero_count = total_positions - total_nonzero_digits
    zero_rate = (zero_count / total_positions) if total_positions > 0 else 0.0
    saving_ratio = ((C_naive - C_group) / C_naive) if C_naive > 0 else 0.0
    reuse_factor = (C_naive / C_group) if C_group > 0 else None
    average_submask_popcount = (
        submask_popcount_sum / accepted_submask_count if accepted_submask_count > 0 else 0.0
    )
    average_group_r = group_r_sum / accepted_submask_count if accepted_submask_count > 0 else 0.0

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

    json_path = os.path.abspath(os.path.join(out_dir, f"tilemask_split_t{tile_size}_c{c}.json"))
    result: Dict[str, object] = {
        "input_path": os.path.abspath(input_path),
        "input_shape": list(input_shape),
        "logical_shape": [K, n, 4],
        "analysis_mode": "tilemask_residual_split_traditional_pippenger",
        "K": K,
        "n": n,
        "c": c,
        "h": h,
        "q": q,
        "point_chunk": point_chunk,
        "tile_size": tile_size,
        "top_masks_per_bucket": top_masks_per_bucket,
        "max_rounds": max_rounds,
        "min_gain": min_gain,
        "min_submask_popcount": min_submask_popcount,
        "mask_nbytes": mask_nbytes,
        "max_abs_bit_length": max_abs_bitlen,
        "total_nonzero_digits": int(total_nonzero_digits),
        "C_naive": int(C_naive),
        "C_no_split": int(C_no_split),
        "C_group": int(C_group),
        "split_improvement_over_no_split": int(split_improvement),
        "used_split_plan_count": int(used_split_plan_count),
        "saving_ratio": saving_ratio,
        "reuse_factor": reuse_factor,
        "accepted_submask_count": int(accepted_submask_count),
        "accepted_split_submask_count": int(accepted_split_submask_count),
        "rejected_candidate_count": int(rejected_candidate_count),
        "average_submask_popcount": average_submask_popcount,
        "average_group_r": average_group_r,
        "max_gain": int(max_gain_seen),
        "max_group_r": int(max_group_r),
        "residual_cost": int(residual_cost_total),
        "per_window_C_naive": [int(x) for x in per_window_C_naive],
        "per_window_C_group": [int(x) for x in per_window_C_group],
        "per_window_saving_ratio": per_window_saving_ratio,
        "per_digit_saving_ratio": per_digit_saving_ratio,
        "zero_count": int(zero_count),
        "zero_rate": zero_rate,
        "digit_histogram": bucket_hist_to_dict(digit_histogram),
        "json_path": json_path,
    }

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    return result


def write_summary_csv(summary_rows: List[Dict[str, object]], csv_path: str) -> None:
    fieldnames = [
        "c",
        "h",
        "q",
        "max_abs_bit_length",
        "total_nonzero_digits",
        "C_naive",
        "C_no_split",
        "C_group",
        "split_improvement_over_no_split",
        "used_split_plan_count",
        "saving_ratio",
        "reuse_factor",
        "accepted_submask_count",
        "accepted_split_submask_count",
        "average_submask_popcount",
        "average_group_r",
        "max_gain",
        "max_group_r",
        "zero_rate",
        "json_path",
    ]
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in summary_rows:
            writer.writerow({name: row[name] for name in fieldnames})


def print_markdown_summary(summary_rows: List[Dict[str, object]]) -> None:
    print(
        "| c | h | q | C_naive | C_no_split | C_group | split_gain | saving_ratio | "
        "reuse_factor | accepted_split_submask_count | max_gain |"
    )
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for row in summary_rows:
        reuse_factor = row["reuse_factor"]
        reuse_factor_str = "null" if reuse_factor is None else f"{float(reuse_factor):.8f}"
        print(
            f"| {row['c']} | {row['h']} | {row['q']} | {row['C_naive']} | "
            f"{row['C_no_split']} | {row['C_group']} | "
            f"{row['split_improvement_over_no_split']} | "
            f"{float(row['saving_ratio']):.8f} | {reuse_factor_str} | "
            f"{row['accepted_split_submask_count']} | {row['max_gain']} |"
        )


def main() -> None:
    args = parse_args()
    if args.K <= 0 or args.n <= 0:
        raise ValueError("--K and --n must be positive")
    if args.c_start <= 0 or args.c_end <= 0:
        raise ValueError("--c-start and --c-end must be positive")
    if args.c_start > args.c_end:
        raise ValueError("--c-start must be <= --c-end")
    if args.point_chunk <= 0:
        raise ValueError("--point-chunk must be positive")
    if args.tile_size <= 0:
        raise ValueError("--tile-size must be positive")
    if args.tile_size > args.K:
        raise ValueError("--tile-size must be <= K")
    if args.top_masks_per_bucket <= 0:
        raise ValueError("--top-masks-per-bucket must be positive")
    if args.max_rounds < 0:
        raise ValueError("--max-rounds must be non-negative")
    if args.min_gain < 0:
        raise ValueError("--min-gain must be non-negative")
    if args.min_submask_popcount <= 0:
        raise ValueError("--min-submask-popcount must be positive")

    os.makedirs(args.out_dir, exist_ok=True)

    scalars, input_shape = load_scalar_memmap(args.input, args.K, args.n)
    max_abs_bitlen = compute_max_abs_bitlen(scalars, args.point_chunk)

    summary_rows: List[Dict[str, object]] = []
    for c in range(args.c_start, args.c_end + 1):
        result = analyze_for_c(
            scalars=scalars,
            input_path=args.input,
            input_shape=input_shape,
            K=args.K,
            n=args.n,
            c=c,
            point_chunk=args.point_chunk,
            tile_size=args.tile_size,
            top_masks_per_bucket=args.top_masks_per_bucket,
            max_rounds=args.max_rounds,
            min_gain=args.min_gain,
            min_submask_popcount=args.min_submask_popcount,
            out_dir=args.out_dir,
            max_abs_bitlen=max_abs_bitlen,
        )
        summary_rows.append(
            {
                "c": result["c"],
                "h": result["h"],
                "q": result["q"],
                "max_abs_bit_length": result["max_abs_bit_length"],
                "total_nonzero_digits": result["total_nonzero_digits"],
                "C_naive": result["C_naive"],
                "C_no_split": result["C_no_split"],
                "C_group": result["C_group"],
                "split_improvement_over_no_split": result["split_improvement_over_no_split"],
                "used_split_plan_count": result["used_split_plan_count"],
                "saving_ratio": result["saving_ratio"],
                "reuse_factor": result["reuse_factor"],
                "accepted_submask_count": result["accepted_submask_count"],
                "accepted_split_submask_count": result["accepted_split_submask_count"],
                "average_submask_popcount": result["average_submask_popcount"],
                "average_group_r": result["average_group_r"],
                "max_gain": result["max_gain"],
                "max_group_r": result["max_group_r"],
                "zero_rate": result["zero_rate"],
                "json_path": result["json_path"],
            }
        )

    csv_path = os.path.abspath(
        os.path.join(args.out_dir, f"tilemask_split_summary_c{args.c_start}_to_c{args.c_end}_t{args.tile_size}.csv")
    )
    write_summary_csv(summary_rows, csv_path)

    print(f"input_shape        : {input_shape}")
    print(f"logical_shape      : ({args.K}, {args.n}, 4)")
    print("analysis_mode      : tilemask_residual_split_traditional_pippenger")
    print(f"c_range            : [{args.c_start}, {args.c_end}]")
    print(f"point_chunk        : {args.point_chunk}")
    print(f"tile_size          : {args.tile_size}")
    print(f"top_masks_per_bucket: {args.top_masks_per_bucket}")
    print(f"max_rounds         : {args.max_rounds}")
    print(f"min_gain           : {args.min_gain}")
    print(f"min_submask_popcount: {args.min_submask_popcount}")
    print(f"max_abs_bit_length : {max_abs_bitlen}")
    print(f"out_dir            : {os.path.abspath(args.out_dir)}")
    print(f"summary_csv        : {csv_path}")
    print_markdown_summary(summary_rows)


if __name__ == "__main__":
    main()
