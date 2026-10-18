#!/usr/bin/env python3
"""
# 寻找共同mask的方法不一致，更符合直觉
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
import time
import resource
from collections import defaultdict
from typing import Any, DefaultDict, Dict, Iterable, List, Tuple, Literal

import numpy as np


UINT64_ONE = np.uint64(1)


def parse_tile_size(value: str | int, K: int) -> int:
    text = str(value).strip().lower()
    if text in {"all", "full", "全部", "k"}:
        return int(K)
    return int(text)


def now() -> float:
    return time.perf_counter()


def max_rss_mb() -> float:
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    # Linux reports KiB, macOS reports bytes. The project server is Linux.
    if os.uname().sysname.lower() == "darwin":
        return rss / (1024.0 * 1024.0)
    return rss / 1024.0


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
        default="32",
        help=(
            "Number of MSM rows covered by one local mask. "
            "Use integer, or 'all'/'全部' to cover all K MSM rows."
        ),
    )
    parser.add_argument(
        "--base-tile-size",
        type=int,
        default=32,
        help="Smallest fallback tile size used by --solver multiscale.",
    )
    parser.add_argument(
        "--split-factor",
        type=int,
        default=2,
        help="Recursive split factor used by multiscale. Usually 2.",
    )
    parser.add_argument(
        "--solver",
        choices=["single", "multiscale"],
        default="multiscale",
        help="single reproduces one-scale splitting; multiscale recursively falls back to smaller tiles.",
    )
    parser.add_argument(
        "--multiscale-decision",
        choices=["compare", "gated", "hybrid"],
        default="hybrid",
        help=(
            "compare=solve parent and children then choose cheaper; "
            "gated=threshold decides parent vs children without comparison; "
            "hybrid=cheap reject/early accept, compare uncertain cases."
        ),
    )
    parser.add_argument(
        "--gate-accept-saving-ratio",
        type=float,
        default=0.10,
        help="Accept parent tile early when its full saving ratio reaches this value in gated/hybrid mode.",
    )
    parser.add_argument(
        "--gate-reject-saving-ratio",
        type=float,
        default=0.02,
        help="Reject parent cheaply and split to children when cheap parent no-split saving is below this value.",
    )
    parser.add_argument(
        "--build-engine",
        choices=["dense", "sparse", "auto"],
        default="auto",
        help=(
            "dense=old bucket-by-bucket packbits; sparse=build masks from nonzero digits only; "
            "auto uses sparse for large tiles, large q, or low nonzero density."
        ),
    )
    parser.add_argument(
        "--sparse-density-threshold",
        type=float,
        default=0.50,
        help="For auto build: use sparse if nonzero density of the digit block is below this threshold.",
    )
    parser.add_argument(
        "--profile",
        action="store_true",
        help="Print per-window build/solve timing and memory usage.",
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
        "--selection-policy",
        choices=["popcount", "group_r", "lookahead"],
        default="lookahead",
        help=(
            "How to choose a split submask among candidates. "
            "popcount reproduces the old tie-break; group_r prefers the common core; "
            "lookahead estimates residual cost after applying a candidate."
        ),
    )
    parser.add_argument(
        "--lookahead-depth",
        type=int,
        default=2,
        help=(
            "Lookahead depth used when --selection-policy=lookahead. "
            "Depth 2 means choose one submask while allowing one additional residual split in the score."
        ),
    )
    parser.add_argument(
        "--lookahead-top-candidates",
        type=int,
        default=32,
        help="Only evaluate this many top immediate-gain candidates under lookahead to control runtime.",
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




def add_tile_mask_counts_sparse_all_buckets(
    window_bucket_counts: DefaultDict[int, DefaultDict[int, Dict[int, int]]],
    signed_digits: np.ndarray,
    tile_size: int,
) -> Dict[int, int]:
    """Build all bucket/tile masks from nonzero digits only.

    For every nonzero signed_digits[k, col] = bucket, this function sets bit
    (k % tile_size) in the mask belonging to (bucket, k//tile_size, col).
    Finally it aggregates equal masks into
        window_bucket_counts[bucket][tile_id][mask] += count.

    This avoids the old dense pattern:
        for bucket: eq = signed_digits == bucket; packbits(eq)
    which scans a mostly-zero K x point_chunk matrix once per bucket.
    """
    rows, cols = np.nonzero(signed_digits)
    if rows.size == 0:
        return {}

    vals = signed_digits[rows, cols].astype(np.int64, copy=False)
    unique_vals, unique_counts = np.unique(vals, return_counts=True)
    histogram = {int(v): int(c) for v, c in zip(unique_vals.tolist(), unique_counts.tolist())}

    tile_ids = (rows // tile_size).astype(np.int64, copy=False)
    bit_ids = (rows - tile_ids * tile_size).astype(np.int64, copy=False)
    cols_i64 = cols.astype(np.int64, copy=False)

    if tile_size <= 64:
        # Fast vectorized path: one mask fits in uint64.
        bit_masks = np.uint64(1) << bit_ids.astype(np.uint64, copy=False)
        order = np.lexsort((cols_i64, tile_ids, vals))
        vals_s = vals[order]
        tile_s = tile_ids[order]
        col_s = cols_i64[order]
        bm_s = bit_masks[order]

        group_start = np.empty(vals_s.size, dtype=bool)
        group_start[0] = True
        group_start[1:] = (
            (vals_s[1:] != vals_s[:-1]) |
            (tile_s[1:] != tile_s[:-1]) |
            (col_s[1:] != col_s[:-1])
        )
        starts = np.flatnonzero(group_start)
        g_vals = vals_s[starts]
        g_tiles = tile_s[starts]
        g_masks = np.bitwise_or.reduceat(bm_s, starts)

        # Aggregate equal (bucket, tile, mask) across point columns.
        order2 = np.lexsort((g_masks, g_tiles, g_vals))
        v2 = g_vals[order2]
        t2 = g_tiles[order2]
        m2 = g_masks[order2]
        final_start = np.empty(v2.size, dtype=bool)
        final_start[0] = True
        final_start[1:] = (
            (v2[1:] != v2[:-1]) |
            (t2[1:] != t2[:-1]) |
            (m2[1:] != m2[:-1])
        )
        fstarts = np.flatnonzero(final_start)
        counts = np.diff(np.append(fstarts, v2.size))
        for idx, cnt in zip(fstarts.tolist(), counts.tolist()):
            bucket = int(v2[idx])
            tile_id = int(t2[idx])
            mask = int(m2[idx])
            tile_counts = window_bucket_counts[bucket][tile_id]
            tile_counts[mask] = tile_counts.get(mask, 0) + int(cnt)
        return histogram

    # General path for tile_size > 64. Masks are Python big integers.
    order = np.lexsort((cols_i64, tile_ids, vals))
    vals_s = vals[order]
    tile_s = tile_ids[order]
    col_s = cols_i64[order]
    bit_s = bit_ids[order]

    group_start = np.empty(vals_s.size, dtype=bool)
    group_start[0] = True
    group_start[1:] = (
        (vals_s[1:] != vals_s[:-1]) |
        (tile_s[1:] != tile_s[:-1]) |
        (col_s[1:] != col_s[:-1])
    )
    starts = np.flatnonzero(group_start)
    ends = np.append(starts[1:], vals_s.size)

    tmp: Dict[Tuple[int, int, int], int] = {}
    for st, en in zip(starts.tolist(), ends.tolist()):
        mask = 0
        for b in bit_s[st:en].tolist():
            mask |= 1 << int(b)
        if mask == 0:
            continue
        key = (int(vals_s[st]), int(tile_s[st]), mask)
        tmp[key] = tmp.get(key, 0) + 1

    for (bucket, tile_id, mask), cnt in tmp.items():
        tile_counts = window_bucket_counts[bucket][tile_id]
        tile_counts[mask] = tile_counts.get(mask, 0) + int(cnt)
    return histogram


def should_use_sparse_build(
    build_engine: str,
    q: int,
    tile_size: int,
    nonzero_count: int,
    total_count: int,
    sparse_density_threshold: float,
) -> bool:
    if build_engine == "sparse":
        return True
    if build_engine == "dense":
        return False
    density = nonzero_count / total_count if total_count > 0 else 0.0
    return (tile_size > 64) or (q >= 1024) or (density <= sparse_density_threshold)

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



def apply_submask_to_residual(residual_items: List[Tuple[int, int]], submask: int) -> Dict[int, int]:
    """Remove submask from every residual mask that contains it."""
    next_residual: Dict[int, int] = {}
    submask_complement = ~submask
    for residual_mask, count in residual_items:
        if (residual_mask & submask) == submask:
            new_mask = residual_mask & submask_complement
            if new_mask != 0:
                next_residual[new_mask] = next_residual.get(new_mask, 0) + count
        else:
            next_residual[residual_mask] = next_residual.get(residual_mask, 0) + count
    return next_residual


def candidate_stats(candidate_mask: int, popcount: int, residual_items: List[Tuple[int, int]]) -> Tuple[int, int, int]:
    """Return (group_r, gain, selected_cost) for one candidate submask."""
    group_r = 0
    for residual_mask, count in residual_items:
        if (residual_mask & candidate_mask) == candidate_mask:
            group_r += count
    gain = (group_r - 1) * (popcount - 1)
    selected_cost = (group_r - 1) + popcount
    return group_r, gain, selected_cost


def sorted_candidate_infos(
    residual: Dict[int, int],
    top_masks_per_bucket: int,
    min_gain: int,
    min_submask_popcount: int,
) -> List[Tuple[int, int, int, int, int]]:
    """
    Build candidate list as tuples:
      (candidate_mask, popcount, group_r, gain, selected_cost)
    sorted by strong immediate potential. This is used both by greedy and lookahead.
    """
    candidates = build_candidate_submasks(
        residual,
        top_masks_per_bucket=top_masks_per_bucket,
        min_submask_popcount=min_submask_popcount,
        include_exact_masks=False,
    )
    residual_items = list(residual.items())
    infos: List[Tuple[int, int, int, int, int]] = []
    for candidate_mask, popcount in candidates.items():
        group_r, gain, selected_cost = candidate_stats(candidate_mask, popcount, residual_items)
        if gain >= min_gain:
            infos.append((candidate_mask, popcount, group_r, gain, selected_cost))
    # Primary sorting only controls pruning order for lookahead.
    infos.sort(key=lambda x: (x[3], x[2], x[1]), reverse=True)
    return infos


def estimate_residual_cost_with_lookahead(
    residual: Dict[int, int],
    top_masks_per_bucket: int,
    rounds_left: int,
    min_gain: int,
    min_submask_popcount: int,
    lookahead_depth: int,
    lookahead_top_candidates: int,
) -> int:
    """
    Estimate the best residual cost with limited recursive lookahead.

    The fallback is always no-split exact-mask grouping. Therefore the estimate
    cannot be worse than the current no-split residual cost.
    """
    if not residual:
        return 0
    no_split_cost = compute_no_split_group_cost(residual, min_gain)[0]
    if rounds_left <= 0 or lookahead_depth <= 0:
        return no_split_cost

    infos = sorted_candidate_infos(
        residual,
        top_masks_per_bucket=top_masks_per_bucket,
        min_gain=min_gain,
        min_submask_popcount=min_submask_popcount,
    )
    if not infos:
        return no_split_cost

    best_cost = no_split_cost
    residual_items = list(residual.items())
    for candidate_mask, popcount, group_r, gain, selected_cost in infos[:lookahead_top_candidates]:
        next_residual = apply_submask_to_residual(residual_items, candidate_mask)
        tail_cost = estimate_residual_cost_with_lookahead(
            next_residual,
            top_masks_per_bucket=top_masks_per_bucket,
            rounds_left=rounds_left - 1,
            min_gain=min_gain,
            min_submask_popcount=min_submask_popcount,
            lookahead_depth=lookahead_depth - 1,
            lookahead_top_candidates=lookahead_top_candidates,
        )
        best_cost = min(best_cost, selected_cost + tail_cost)
    return int(best_cost)


def choose_best_candidate(
    residual: Dict[int, int],
    top_masks_per_bucket: int,
    rounds_left: int,
    min_gain: int,
    min_submask_popcount: int,
    selection_policy: str,
    lookahead_depth: int,
    lookahead_top_candidates: int,
) -> Tuple[int, int, int, int, int, int]:
    """
    Choose one submask.

    Returns (mask, popcount, group_r, gain, selected_cost, rejected_count).
    rejected_count is the number of generated acceptable candidates not selected.
    """
    infos = sorted_candidate_infos(
        residual,
        top_masks_per_bucket=top_masks_per_bucket,
        min_gain=min_gain,
        min_submask_popcount=min_submask_popcount,
    )
    if not infos:
        return 0, 0, 0, -1, 0, 0

    if selection_policy == "popcount":
        # Old behavior: gain first, then larger popcount, then larger group_r.
        chosen = max(infos, key=lambda x: (x[3], x[1], x[2]))
        return (*chosen, len(infos) - 1)

    if selection_policy == "group_r":
        # Your common-core preference: gain first, then more points sharing it,
        # then larger popcount. This chooses 11100000 over 11100010 when gains tie.
        chosen = max(infos, key=lambda x: (x[3], x[2], x[1]))
        return (*chosen, len(infos) - 1)

    # Limited lookahead: choose the candidate that minimizes
    # selected_cost + estimated residual cost after more splitting.
    residual_items = list(residual.items())
    best = None
    best_score = None
    for info in infos[:lookahead_top_candidates]:
        candidate_mask, popcount, group_r, gain, selected_cost = info
        next_residual = apply_submask_to_residual(residual_items, candidate_mask)
        tail_cost = estimate_residual_cost_with_lookahead(
            next_residual,
            top_masks_per_bucket=top_masks_per_bucket,
            rounds_left=rounds_left - 1,
            min_gain=min_gain,
            min_submask_popcount=min_submask_popcount,
            lookahead_depth=max(0, lookahead_depth - 1),
            lookahead_top_candidates=lookahead_top_candidates,
        )
        score = selected_cost + tail_cost
        # Minimize final estimated cost; ties prefer common-core, then wider mask.
        tie_key = (-gain, -group_r, -popcount)
        current_key = (score, *tie_key)
        if best_score is None or current_key < best_score:
            best_score = current_key
            best = info
    assert best is not None
    return (*best, len(infos) - 1)


def split_bucket_masks(
    mask_counts: Dict[int, int],
    top_masks_per_bucket: int,
    max_rounds: int,
    min_gain: int,
    min_submask_popcount: int,
    selection_policy: str = "lookahead",
    lookahead_depth: int = 2,
    lookahead_top_candidates: int = 32,
) -> Dict[str, float | int]:
    """
    Residual splitting for one fixed (window, signed bucket, K-tile).

    It keeps the same monotonic fallback as the fixed version:
      C_group <= C_no_split <= C_naive.

    New selection policies:
      popcount  : old tie-break, gain -> popcount -> group_r.
      group_r   : common-core tie-break, gain -> group_r -> popcount.
      lookahead : choose the submask minimizing selected_cost + estimated residual cost.
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

    for round_idx in range(max_rounds):
        if not residual:
            if split_prefix_cost < best_total_cost:
                best_total_cost = split_prefix_cost
                best_residual_cost = 0
            break

        best_mask, best_u, best_r, best_gain, selected_cost, rejected = choose_best_candidate(
            residual,
            top_masks_per_bucket=top_masks_per_bucket,
            rounds_left=max_rounds - round_idx,
            min_gain=min_gain,
            min_submask_popcount=min_submask_popcount,
            selection_policy=selection_policy,
            lookahead_depth=lookahead_depth,
            lookahead_top_candidates=lookahead_top_candidates,
        )

        if best_gain < min_gain or best_mask == 0:
            break

        accepted_split_submask_count += 1
        rejected_candidate_count += rejected
        split_submask_popcount_sum += best_u
        split_group_r_sum += best_r
        max_gain_seen = max(max_gain_seen, best_gain)
        max_group_r = max(max_group_r, best_r)
        split_prefix_cost += selected_cost

        residual_items = list(residual.items())
        residual = apply_submask_to_residual(residual_items, best_mask)

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



def add_bucket_stats(dst: Dict[str, int], src: Dict[str, float | int]) -> None:
    for key in [
        "naive_cost", "no_split_cost", "grouped_cost", "split_plan_cost",
        "used_split_plan", "accepted_submask_count", "accepted_split_submask_count",
        "rejected_candidate_count", "submask_popcount_sum", "group_r_sum",
        "residual_cost", "multiscale_parent_chosen_count", "multiscale_child_chosen_count",
    ]:
        dst[key] = int(dst.get(key, 0)) + int(src.get(key, 0))
    dst["max_gain"] = max(int(dst.get("max_gain", 0)), int(src.get("max_gain", 0)))
    dst["max_group_r"] = max(int(dst.get("max_group_r", 0)), int(src.get("max_group_r", 0)))


def empty_bucket_stats() -> Dict[str, int]:
    return {
        "naive_cost": 0,
        "no_split_cost": 0,
        "grouped_cost": 0,
        "split_plan_cost": 0,
        "used_split_plan": 0,
        "accepted_submask_count": 0,
        "accepted_split_submask_count": 0,
        "rejected_candidate_count": 0,
        "submask_popcount_sum": 0,
        "group_r_sum": 0,
        "max_gain": 0,
        "max_group_r": 0,
        "residual_cost": 0,
        "multiscale_parent_chosen_count": 0,
        "multiscale_child_chosen_count": 0,
    }


def project_mask_counts_to_children(mask_counts: Dict[int, int], tile_size: int, child_tile_size: int) -> Dict[int, Dict[int, int]]:
    if tile_size % child_tile_size != 0:
        raise ValueError("tile_size must be a multiple of child_tile_size")
    base_mask = (1 << child_tile_size) - 1
    children: Dict[int, Dict[int, int]] = defaultdict(dict)
    for mask, count in mask_counts.items():
        for child_id, shift in enumerate(range(0, tile_size, child_tile_size)):
            submask = (mask >> shift) & base_mask
            if submask:
                d = children[child_id]
                d[submask] = d.get(submask, 0) + int(count)
    return children


def cheap_parent_no_split_saving_ratio(mask_counts: Dict[int, int], min_gain: int) -> Tuple[float, int, int]:
    naive = compute_mask_cost(mask_counts)
    if naive <= 0:
        return 0.0, 0, 0
    no_split = compute_no_split_group_cost(mask_counts, min_gain)[0]
    return (naive - no_split) / naive, int(naive), int(no_split)


def split_bucket_masks_multiscale(
    mask_counts: Dict[int, int],
    tile_size: int,
    base_tile_size: int,
    split_factor: int,
    top_masks_per_bucket: int,
    max_rounds: int,
    min_gain: int,
    min_submask_popcount: int,
    selection_policy: str,
    lookahead_depth: int,
    lookahead_top_candidates: int,
    multiscale_decision: str,
    gate_accept_saving_ratio: float,
    gate_reject_saving_ratio: float,
) -> Dict[str, float | int]:
    if tile_size <= base_tile_size:
        base = split_bucket_masks(
            mask_counts,
            top_masks_per_bucket=top_masks_per_bucket,
            max_rounds=max_rounds,
            min_gain=min_gain,
            min_submask_popcount=min_submask_popcount,
            selection_policy=selection_policy,
            lookahead_depth=lookahead_depth,
            lookahead_top_candidates=lookahead_top_candidates,
        )
        base["multiscale_parent_chosen_count"] = 1
        base["multiscale_child_chosen_count"] = 0
        return base

    child_tile_size = tile_size // split_factor

    def solve_children() -> Dict[str, int]:
        acc = empty_bucket_stats()
        child_counts = project_mask_counts_to_children(mask_counts, tile_size, child_tile_size)
        for child_mask_counts in child_counts.values():
            child_stats = split_bucket_masks_multiscale(
                child_mask_counts,
                tile_size=child_tile_size,
                base_tile_size=base_tile_size,
                split_factor=split_factor,
                top_masks_per_bucket=top_masks_per_bucket,
                max_rounds=max_rounds,
                min_gain=min_gain,
                min_submask_popcount=min_submask_popcount,
                selection_policy=selection_policy,
                lookahead_depth=lookahead_depth,
                lookahead_top_candidates=lookahead_top_candidates,
                multiscale_decision=multiscale_decision,
                gate_accept_saving_ratio=gate_accept_saving_ratio,
                gate_reject_saving_ratio=gate_reject_saving_ratio,
            )
            add_bucket_stats(acc, child_stats)
        acc["multiscale_child_chosen_count"] += 1
        return acc

    cheap_ratio, cheap_naive, _cheap_no_split = cheap_parent_no_split_saving_ratio(mask_counts, min_gain)

    if multiscale_decision == "gated":
        if cheap_ratio >= gate_accept_saving_ratio:
            parent = split_bucket_masks(
                mask_counts,
                top_masks_per_bucket=top_masks_per_bucket,
                max_rounds=max_rounds,
                min_gain=min_gain,
                min_submask_popcount=min_submask_popcount,
                selection_policy=selection_policy,
                lookahead_depth=lookahead_depth,
                lookahead_top_candidates=lookahead_top_candidates,
            )
            parent["multiscale_parent_chosen_count"] = 1
            parent["multiscale_child_chosen_count"] = 0
            return parent
        return solve_children()

    if multiscale_decision == "hybrid" and cheap_ratio < gate_reject_saving_ratio:
        return solve_children()

    parent = split_bucket_masks(
        mask_counts,
        top_masks_per_bucket=top_masks_per_bucket,
        max_rounds=max_rounds,
        min_gain=min_gain,
        min_submask_popcount=min_submask_popcount,
        selection_policy=selection_policy,
        lookahead_depth=lookahead_depth,
        lookahead_top_candidates=lookahead_top_candidates,
    )
    parent_naive = int(parent["naive_cost"])
    parent_ratio = ((parent_naive - int(parent["grouped_cost"])) / parent_naive) if parent_naive > 0 else 0.0
    if multiscale_decision == "hybrid" and parent_ratio >= gate_accept_saving_ratio:
        parent["multiscale_parent_chosen_count"] = 1
        parent["multiscale_child_chosen_count"] = 0
        return parent

    children = solve_children()
    if int(children["grouped_cost"]) <= int(parent["grouped_cost"]):
        return children
    parent["multiscale_parent_chosen_count"] = 1
    parent["multiscale_child_chosen_count"] = 0
    return parent


def split_bucket_masks_dispatch(
    mask_counts: Dict[int, int],
    tile_size: int,
    base_tile_size: int,
    split_factor: int,
    solver: str,
    top_masks_per_bucket: int,
    max_rounds: int,
    min_gain: int,
    min_submask_popcount: int,
    selection_policy: str,
    lookahead_depth: int,
    lookahead_top_candidates: int,
    multiscale_decision: str,
    gate_accept_saving_ratio: float,
    gate_reject_saving_ratio: float,
) -> Dict[str, float | int]:
    if solver == "single":
        out = split_bucket_masks(
            mask_counts,
            top_masks_per_bucket=top_masks_per_bucket,
            max_rounds=max_rounds,
            min_gain=min_gain,
            min_submask_popcount=min_submask_popcount,
            selection_policy=selection_policy,
            lookahead_depth=lookahead_depth,
            lookahead_top_candidates=lookahead_top_candidates,
        )
        out["multiscale_parent_chosen_count"] = 1
        out["multiscale_child_chosen_count"] = 0
        return out
    return split_bucket_masks_multiscale(
        mask_counts,
        tile_size=tile_size,
        base_tile_size=base_tile_size,
        split_factor=split_factor,
        top_masks_per_bucket=top_masks_per_bucket,
        max_rounds=max_rounds,
        min_gain=min_gain,
        min_submask_popcount=min_submask_popcount,
        selection_policy=selection_policy,
        lookahead_depth=lookahead_depth,
        lookahead_top_candidates=lookahead_top_candidates,
        multiscale_decision=multiscale_decision,
        gate_accept_saving_ratio=gate_accept_saving_ratio,
        gate_reject_saving_ratio=gate_reject_saving_ratio,
    )

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
    selection_policy: str,
    lookahead_depth: int,
    lookahead_top_candidates: int,
    out_dir: str,
    max_abs_bitlen: int,
    base_tile_size: int = 32,
    split_factor: int = 2,
    solver: str = "single",
    multiscale_decision: str = "compare",
    gate_accept_saving_ratio: float = 0.10,
    gate_reject_saving_ratio: float = 0.02,
    build_engine: str = "auto",
    sparse_density_threshold: float = 0.50,
    profile: bool = False,
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
    per_window_build_seconds = [0.0] * h
    per_window_solve_seconds = [0.0] * h
    build_seconds_total = 0.0
    solve_seconds_total = 0.0
    multiscale_parent_chosen_count = 0
    multiscale_child_chosen_count = 0
    sparse_chunk_count = 0
    dense_chunk_count = 0

    t_c0 = now()
    for window_j in range(h):
        t_build0 = now()
        shift = window_j * c
        window_bucket_counts: DefaultDict[int, DefaultDict[int, Dict[int, int]]] = defaultdict(lambda: defaultdict(dict))

        for point_start in range(0, n, point_chunk):
            point_end = min(n, point_start + point_chunk)
            block = scalars[:, point_start:point_end, :]

            abs_lo, abs_hi, neg_mask = decode_abs_block(block)
            digits = extract_window_digits(abs_lo, abs_hi, shift, qmask)
            signed_digits = build_signed_digits(digits, neg_mask, digit_dtype)
            nonzero_count = int(np.count_nonzero(signed_digits))
            use_sparse = should_use_sparse_build(
                build_engine=build_engine,
                q=q,
                tile_size=tile_size,
                nonzero_count=nonzero_count,
                total_count=signed_digits.size,
                sparse_density_threshold=sparse_density_threshold,
            )
            if use_sparse:
                sparse_chunk_count += 1
                sparse_hist = add_tile_mask_counts_sparse_all_buckets(
                    window_bucket_counts, signed_digits, tile_size
                )
                for bucket, bucket_digit_count in sparse_hist.items():
                    digit_histogram[bucket] += bucket_digit_count
                    total_nonzero_digits += bucket_digit_count
            else:
                dense_chunk_count += 1
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

        build_seconds = now() - t_build0
        build_seconds_total += build_seconds
        per_window_build_seconds[window_j] = build_seconds
        t_solve0 = now()
        window_naive = 0
        window_group = 0
        for bucket, tile_mask_counts in window_bucket_counts.items():
            for _tile_id, mask_counts in tile_mask_counts.items():
                bucket_stats = split_bucket_masks_dispatch(
                    mask_counts,
                    tile_size=tile_size,
                    base_tile_size=base_tile_size,
                    split_factor=split_factor,
                    solver=solver,
                    top_masks_per_bucket=top_masks_per_bucket,
                    max_rounds=max_rounds,
                    min_gain=min_gain,
                    min_submask_popcount=min_submask_popcount,
                    selection_policy=selection_policy,
                    lookahead_depth=lookahead_depth,
                    lookahead_top_candidates=lookahead_top_candidates,
                    multiscale_decision=multiscale_decision,
                    gate_accept_saving_ratio=gate_accept_saving_ratio,
                    gate_reject_saving_ratio=gate_reject_saving_ratio,
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
                multiscale_parent_chosen_count += int(bucket_stats.get("multiscale_parent_chosen_count", 0))
                multiscale_child_chosen_count += int(bucket_stats.get("multiscale_child_chosen_count", 0))

                per_digit_naive[bucket] += bucket_naive
                per_digit_group[bucket] += bucket_group

        solve_seconds = now() - t_solve0
        solve_seconds_total += solve_seconds
        per_window_solve_seconds[window_j] = solve_seconds
        per_window_C_naive[window_j] = window_naive
        per_window_C_group[window_j] = window_group
        if profile:
            print(
                f"[c={c} window={window_j}/{h-1}] build={build_seconds:.3f}s solve={solve_seconds:.3f}s "
                f"C_naive={window_naive} C_group={window_group} rss={max_rss_mb():.1f}MB",
                flush=True,
            )

    total_seconds = now() - t_c0
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

    tile_label = "all" if tile_size == K else str(tile_size)
    json_path = os.path.abspath(os.path.join(out_dir, f"tilemask_{solver}_{build_engine}_t{tile_label}_base{base_tile_size}_c{c}.json"))
    result: Dict[str, object] = {
        "input_path": os.path.abspath(input_path),
        "input_shape": list(input_shape),
        "logical_shape": [K, n, 4],
        "analysis_mode": "tilemask_residual_split_traditional_pippenger_sparse_multiscale",
        "solver": solver,
        "build_engine": build_engine,
        "sparse_density_threshold": sparse_density_threshold,
        "K": K,
        "n": n,
        "c": c,
        "h": h,
        "q": q,
        "point_chunk": point_chunk,
        "tile_size": tile_size,
        "base_tile_size": base_tile_size,
        "split_factor": split_factor,
        "multiscale_decision": multiscale_decision,
        "gate_accept_saving_ratio": gate_accept_saving_ratio,
        "gate_reject_saving_ratio": gate_reject_saving_ratio,
        "top_masks_per_bucket": top_masks_per_bucket,
        "max_rounds": max_rounds,
        "min_gain": min_gain,
        "min_submask_popcount": min_submask_popcount,
        "selection_policy": selection_policy,
        "lookahead_depth": lookahead_depth,
        "lookahead_top_candidates": lookahead_top_candidates,
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
        "multiscale_parent_chosen_count": int(multiscale_parent_chosen_count),
        "multiscale_child_chosen_count": int(multiscale_child_chosen_count),
        "sparse_chunk_count": int(sparse_chunk_count),
        "dense_chunk_count": int(dense_chunk_count),
        "per_window_C_naive": [int(x) for x in per_window_C_naive],
        "per_window_C_group": [int(x) for x in per_window_C_group],
        "per_window_saving_ratio": per_window_saving_ratio,
        "per_digit_saving_ratio": per_digit_saving_ratio,
        "zero_count": int(zero_count),
        "zero_rate": zero_rate,
        "digit_histogram": bucket_hist_to_dict(digit_histogram),
        "timing": {
            "build_seconds": build_seconds_total,
            "solve_seconds": solve_seconds_total,
            "total_seconds": total_seconds,
            "per_window_build_seconds": per_window_build_seconds,
            "per_window_solve_seconds": per_window_solve_seconds,
            "max_rss_mb": max_rss_mb(),
        },
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
        "solver",
        "build_engine",
        "tile_size",
        "base_tile_size",
        "multiscale_parent_chosen_count",
        "multiscale_child_chosen_count",
        "sparse_chunk_count",
        "dense_chunk_count",
        "build_seconds",
        "solve_seconds",
        "total_seconds",
        "max_rss_mb",
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
    args.tile_size = parse_tile_size(args.tile_size, args.K)
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
    if args.base_tile_size <= 0 or args.base_tile_size > args.tile_size:
        raise ValueError("--base-tile-size must be in [1, tile_size]")
    if args.tile_size % args.base_tile_size != 0:
        raise ValueError("--tile-size must be a multiple of --base-tile-size")
    if args.split_factor < 2:
        raise ValueError("--split-factor must be >= 2")
    x = args.tile_size
    while x > args.base_tile_size:
        if x % args.split_factor != 0:
            raise ValueError("tile_size must reach base_tile_size by repeated division by split_factor")
        x //= args.split_factor
    if args.top_masks_per_bucket <= 0:
        raise ValueError("--top-masks-per-bucket must be positive")
    if args.max_rounds < 0:
        raise ValueError("--max-rounds must be non-negative")
    if args.min_gain < 0:
        raise ValueError("--min-gain must be non-negative")
    if args.min_submask_popcount <= 0:
        raise ValueError("--min-submask-popcount must be positive")
    if args.lookahead_depth < 0:
        raise ValueError("--lookahead-depth must be non-negative")
    if args.lookahead_top_candidates <= 0:
        raise ValueError("--lookahead-top-candidates must be positive")
    if not (0.0 <= args.gate_reject_saving_ratio <= args.gate_accept_saving_ratio <= 1.0):
        raise ValueError("require 0 <= gate_reject_saving_ratio <= gate_accept_saving_ratio <= 1")
    if not (0.0 <= args.sparse_density_threshold <= 1.0):
        raise ValueError("--sparse-density-threshold must be in [0,1]")

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
            selection_policy=args.selection_policy,
            lookahead_depth=args.lookahead_depth,
            lookahead_top_candidates=args.lookahead_top_candidates,
            out_dir=args.out_dir,
            max_abs_bitlen=max_abs_bitlen,
            base_tile_size=args.base_tile_size,
            split_factor=args.split_factor,
            solver=args.solver,
            multiscale_decision=args.multiscale_decision,
            gate_accept_saving_ratio=args.gate_accept_saving_ratio,
            gate_reject_saving_ratio=args.gate_reject_saving_ratio,
            build_engine=args.build_engine,
            sparse_density_threshold=args.sparse_density_threshold,
            profile=args.profile,
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
                "solver": result["solver"],
                "build_engine": result["build_engine"],
                "tile_size": result["tile_size"],
                "base_tile_size": result["base_tile_size"],
                "multiscale_parent_chosen_count": result["multiscale_parent_chosen_count"],
                "multiscale_child_chosen_count": result["multiscale_child_chosen_count"],
                "sparse_chunk_count": result["sparse_chunk_count"],
                "dense_chunk_count": result["dense_chunk_count"],
                "build_seconds": result["timing"]["build_seconds"],
                "solve_seconds": result["timing"]["solve_seconds"],
                "total_seconds": result["timing"]["total_seconds"],
                "max_rss_mb": result["timing"]["max_rss_mb"],
                "json_path": result["json_path"],
            }
        )

    tile_label = "all" if args.tile_size == args.K else str(args.tile_size)
    csv_path = os.path.abspath(
        os.path.join(args.out_dir, f"tilemask_{args.solver}_{args.build_engine}_summary_c{args.c_start}_to_c{args.c_end}_t{tile_label}_base{args.base_tile_size}.csv")
    )
    write_summary_csv(summary_rows, csv_path)

    print(f"input_shape        : {input_shape}")
    print(f"logical_shape      : ({args.K}, {args.n}, 4)")
    print("analysis_mode      : tilemask_residual_split_traditional_pippenger_sparse_multiscale")
    print(f"solver             : {args.solver}")
    print(f"build_engine       : {args.build_engine}")
    print(f"sparse_density_threshold: {args.sparse_density_threshold}")
    print(f"multiscale_decision: {args.multiscale_decision}")
    print(f"gate_accept_saving_ratio: {args.gate_accept_saving_ratio}")
    print(f"gate_reject_saving_ratio: {args.gate_reject_saving_ratio}")
    print(f"c_range            : [{args.c_start}, {args.c_end}]")
    print(f"point_chunk        : {args.point_chunk}")
    print(f"tile_size          : {args.tile_size}")
    print(f"base_tile_size     : {args.base_tile_size}")
    print(f"split_factor       : {args.split_factor}")
    print(f"top_masks_per_bucket: {args.top_masks_per_bucket}")
    print(f"max_rounds         : {args.max_rounds}")
    print(f"min_gain           : {args.min_gain}")
    print(f"min_submask_popcount: {args.min_submask_popcount}")
    print(f"selection_policy   : {args.selection_policy}")
    print(f"lookahead_depth    : {args.lookahead_depth}")
    print(f"lookahead_top_candidates: {args.lookahead_top_candidates}")
    print(f"max_abs_bit_length : {max_abs_bitlen}")
    print(f"out_dir            : {os.path.abspath(args.out_dir)}")
    print(f"summary_csv        : {csv_path}")
    print_markdown_summary(summary_rows)


if __name__ == "__main__":
    main()
