#!/usr/bin/env python3
"""
Multi-scale / exact tile-mask residual splitting analyzer for batch fixed-base MSM.

This is a drop-in replacement for the previous lookahead script.

Modes
-----
1) --solver single
   Original single-scale behavior: run split on the current tile_size only.

2) --solver multiscale
   Practical version B. For each large tile, recursively compare:
       current large-tile plan
       vs. sum of child-tile plans
   and choose the cheaper one. If base_tile_size=32, then tile=64/128/.../K
   is guaranteed no worse than the corresponding tile=32 decomposition, because
   the whole tile can always fall back to children until 32.

3) --solver exact
   Exact version A, but only for small masks. It enumerates all submasks of the
   current residual masks and uses memoized dynamic programming to find the best
   residual-splitting plan under this cost model. This is exponential and is
   intentionally guarded by --exact-max-tile-size, --exact-max-distinct-masks and
   --exact-max-popcount-sum. It is suitable for debugging/small samples, not for
   K=16384 full masks.

4) --solver both
   Run multiscale and exact when exact is allowed; otherwise run multiscale and
   report exact as skipped. The chosen cost is min(multiscale, exact) when exact runs.

Cost model
----------
For a submask shared by r points and containing u set bits:
    raw cost     = r * u
    grouped cost = (r - 1) + u
    gain         = (r - 1) * (u - 1)

The script counts theoretical bucket-update point additions. It does not perform
elliptic-curve arithmetic.
"""

from __future__ import annotations

import argparse
import csv
import heapq
import json
import os
import resource
import sys
import time
from collections import defaultdict
from functools import lru_cache
from typing import DefaultDict, Dict, Iterable, List, Tuple, Any

import numpy as np

UINT64_ONE = np.uint64(1)

STAT_KEYS_SUM = [
    "naive_cost",
    "no_split_cost",
    "grouped_cost",
    "split_plan_cost",
    "used_split_plan",
    "accepted_submask_count",
    "accepted_split_submask_count",
    "rejected_candidate_count",
    "submask_popcount_sum",
    "group_r_sum",
    "residual_cost",
    "exact_skipped_count",
    "multiscale_parent_chosen_count",
    "multiscale_child_chosen_count",
]
STAT_KEYS_MAX = ["max_gain", "max_group_r"]


def parse_tile_size(value: str, K: int) -> int:
    value = str(value).strip().lower()
    if value in {"all", "full", "全部", "k"}:
        return K
    return int(value)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Analyze MSM tile-mask reuse with single, multiscale, and exact solvers."
    )
    p.add_argument("--input", required=True, help="Path to Vi.npy")
    p.add_argument("--K", type=int, default=1 << 14, help="Number of MSMs")
    p.add_argument("--n", type=int, default=1 << 14, help="Points per MSM")
    p.add_argument("--c-start", type=int, default=6)
    p.add_argument("--c-end", type=int, default=12)
    p.add_argument("--point-chunk", type=int, default=512)
    p.add_argument(
        "--tile-size",
        default="32",
        help="Tile size. Use integer or 'all'/'全部' to mean K.",
    )
    p.add_argument(
        "--base-tile-size",
        type=int,
        default=32,
        help="Smallest fallback tile size for --solver multiscale.",
    )
    p.add_argument(
        "--split-factor",
        type=int,
        default=2,
        help="Recursive split factor for multiscale fallback. Usually 2.",
    )
    p.add_argument("--top-masks-per-bucket", type=int, default=128)
    p.add_argument("--max-rounds", type=int, default=16)
    p.add_argument("--min-gain", type=int, default=32)
    p.add_argument("--min-submask-popcount", type=int, default=2)
    p.add_argument(
        "--selection-policy",
        choices=["popcount", "group_r", "lookahead"],
        default="lookahead",
    )
    p.add_argument("--lookahead-depth", type=int, default=2)
    p.add_argument("--lookahead-top-candidates", type=int, default=32)
    p.add_argument(
        "--solver",
        choices=["single", "multiscale", "exact", "both"],
        default="multiscale",
        help="single=old behavior, multiscale=practical B, exact=strict A for small masks, both=multiscale plus exact when allowed.",
    )
    p.add_argument(
        "--exact-max-tile-size",
        type=int,
        default=18,
        help="Exact solver is skipped if tile_size is larger than this.",
    )
    p.add_argument(
        "--exact-max-distinct-masks",
        type=int,
        default=40,
        help="Exact solver is skipped if residual has more distinct masks than this.",
    )
    p.add_argument(
        "--exact-max-popcount-sum",
        type=int,
        default=160,
        help="Exact solver is skipped if sum(popcount(mask)) over distinct masks exceeds this.",
    )
    p.add_argument(
        "--exact-max-candidates",
        type=int,
        default=20000,
        help="Exact solver is skipped if initial all-submask candidate count exceeds this.",
    )
    p.add_argument(
        "--exact-time-limit-sec",
        type=float,
        default=5.0,
        help="Soft time limit per bucket/tile for exact solver. 0 disables time limit.",
    )
    p.add_argument(
        "--out-dir",
        default="multiscale_split_results",
        help="Directory for JSON/CSV outputs.",
    )
    p.add_argument(
        "--profile",
        action="store_true",
        help="Print timing per c/window and write detailed timing fields.",
    )
    p.add_argument(
        "--multiscale-decision",
        choices=["compare", "gated", "hybrid"],
        default="compare",
        help=(
            "Decision rule for --solver multiscale/both. "
            "compare=always solve parent and children then choose lower cost; "
            "gated=use a cheap threshold to choose parent or split without comparing; "
            "hybrid=cheap reject + parent early accept + compare only in uncertain cases."
        ),
    )
    p.add_argument(
        "--gate-accept-saving-ratio",
        type=float,
        default=0.10,
        help=(
            "For gated/hybrid multiscale: if the parent tile saving ratio is at least this value, "
            "accept the parent tile and do not recurse into children."
        ),
    )
    p.add_argument(
        "--gate-reject-saving-ratio",
        type=float,
        default=0.02,
        help=(
            "For gated/hybrid multiscale: if the cheap parent no-split saving ratio is below this value, "
            "skip expensive parent splitting and recurse into children."
        ),
    )
    return p.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    args.tile_size = parse_tile_size(args.tile_size, args.K)
    if args.K <= 0 or args.n <= 0:
        raise ValueError("--K and --n must be positive")
    if args.c_start <= 0 or args.c_end <= 0 or args.c_start > args.c_end:
        raise ValueError("invalid c range")
    if args.point_chunk <= 0:
        raise ValueError("--point-chunk must be positive")
    if args.tile_size <= 0 or args.tile_size > args.K:
        raise ValueError("--tile-size must be in [1,K]")
    if args.base_tile_size <= 0 or args.base_tile_size > args.tile_size:
        raise ValueError("--base-tile-size must be in [1,tile_size]")
    if args.tile_size % args.base_tile_size != 0:
        raise ValueError("--tile-size must be a multiple of --base-tile-size")
    if args.split_factor < 2:
        raise ValueError("--split-factor must be >= 2")
    if args.tile_size != args.base_tile_size:
        x = args.tile_size
        while x > args.base_tile_size:
            if x % args.split_factor != 0:
                raise ValueError(
                    "for recursive multiscale splitting, tile_size/base_tile_size must be reachable by repeated split_factor divisions"
                )
            x //= args.split_factor
    if args.top_masks_per_bucket <= 0:
        raise ValueError("--top-masks-per-bucket must be positive")
    if args.max_rounds < 0:
        raise ValueError("--max-rounds must be non-negative")
    if args.min_gain < 0:
        raise ValueError("--min-gain must be non-negative")
    if args.min_submask_popcount <= 0:
        raise ValueError("--min-submask-popcount must be positive")
    if args.lookahead_depth < 0 or args.lookahead_top_candidates <= 0:
        raise ValueError("invalid lookahead parameters")
    if not (0.0 <= args.gate_reject_saving_ratio <= 1.0):
        raise ValueError("--gate-reject-saving-ratio must be in [0,1]")
    if not (0.0 <= args.gate_accept_saving_ratio <= 1.0):
        raise ValueError("--gate-accept-saving-ratio must be in [0,1]")
    if args.gate_reject_saving_ratio > args.gate_accept_saving_ratio:
        raise ValueError("--gate-reject-saving-ratio must be <= --gate-accept-saving-ratio")


def now() -> float:
    return time.perf_counter()


def max_rss_mb() -> float:
    # Linux: ru_maxrss is KB. macOS: bytes. This environment is usually Linux.
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if sys.platform == "darwin":
        return rss / (1024.0 * 1024.0)
    return rss / 1024.0


def load_scalar_memmap(path: str, K: int, n: int) -> Tuple[np.ndarray, Tuple[int, ...]]:
    arr = np.load(path, mmap_mode="r")
    original_shape = tuple(arr.shape)
    expected_scalars = K * n
    expected_flat = expected_scalars * 4
    if arr.ndim == 1:
        if arr.shape[0] != expected_flat:
            raise ValueError(f"Flat .npy length mismatch: got {arr.shape[0]}, expected {expected_flat}")
        arr = arr.reshape(K, n, 4)
    elif arr.ndim == 2:
        if arr.shape == (expected_scalars, 4):
            arr = arr.reshape(K, n, 4)
        elif arr.shape == (K, n * 4):
            arr = arr.reshape(K, n, 4)
        else:
            raise ValueError(f"Unsupported 2D .npy shape {arr.shape}")
    elif arr.ndim == 3:
        if arr.shape != (K, n, 4):
            raise ValueError(f"Unsupported 3D .npy shape {arr.shape}; expected {(K,n,4)}")
    else:
        raise ValueError(f"Unsupported .npy rank {arr.ndim}, shape={arr.shape}")
    if arr.dtype != np.uint64:
        arr = arr.astype(np.uint64, copy=False)
    return arr, original_shape


def decode_abs_block(block: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
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
        return max(current_max, ((max_hi << 64) | max_lo).bit_length())
    return max(current_max, int(abs_lo.max()).bit_length())


def compute_max_abs_bitlen(scalars: np.ndarray, point_chunk: int) -> int:
    K, n, _ = scalars.shape
    max_abs_bitlen = 0
    for point_start in range(0, n, point_chunk):
        point_end = min(n, point_start + point_chunk)
        abs_lo, abs_hi, _ = decode_abs_block(scalars[:, point_start:point_end, :])
        max_abs_bitlen = update_chunk_max_bitlen(abs_lo, abs_hi, max_abs_bitlen)
    return max_abs_bitlen


def extract_window_digits(abs_lo: np.ndarray, abs_hi: np.ndarray, shift: int, qmask: int) -> np.ndarray:
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


def iter_present_nonzero_buckets(signed_digits: np.ndarray, q: int) -> Iterable[Tuple[int, int]]:
    offset = q - 1
    if q <= 4096:
        flat = signed_digits.astype(np.int32, copy=False).reshape(-1) + offset
        counts = np.bincount(flat, minlength=2 * q - 1)
        for idx in np.flatnonzero(counts):
            if idx != offset:
                yield idx - offset, int(counts[idx])
        return
    values, counts = np.unique(signed_digits, return_counts=True)
    for value, count in zip(values.tolist(), counts.tolist()):
        if value != 0:
            yield int(value), int(count)


def bucket_hist_to_dict(bucket_stats: Dict[int, int]) -> Dict[str, int]:
    return {str(k): int(v) for k, v in sorted(bucket_stats.items()) if v != 0}


def add_tile_mask_counts_for_bucket(
    bucket_tile_mask_counts: DefaultDict[int, Dict[int, int]],
    signed_digits: np.ndarray,
    bucket: int,
    tile_size: int,
    mask_nbytes: int,
) -> None:
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
    return sum(int(count) * int(mask).bit_count() for mask, count in mask_counts.items())


def full_group_cost_for_mask(count: int, mask: int, min_gain: int) -> Tuple[int, bool, int]:
    u = mask.bit_count()
    raw_cost = count * u
    gain = (count - 1) * (u - 1)
    if u > 0 and gain >= min_gain:
        return (count - 1) + u, True, gain
    return raw_cost, False, gain


def compute_no_split_group_cost(mask_counts: Dict[int, int], min_gain: int) -> Tuple[int, int, int, int, int, int]:
    cost = accepted = popcount_sum = group_r_sum = max_gain = max_group_r = 0
    for mask, count in mask_counts.items():
        group_cost, ok, gain = full_group_cost_for_mask(int(count), int(mask), min_gain)
        cost += group_cost
        max_gain = max(max_gain, gain)
        if ok:
            accepted += 1
            popcount_sum += int(mask).bit_count()
            group_r_sum += int(count)
            max_group_r = max(max_group_r, int(count))
    return cost, accepted, popcount_sum, group_r_sum, max_gain, max_group_r


def empty_stats(method: str = "empty") -> Dict[str, Any]:
    s: Dict[str, Any] = {k: 0 for k in STAT_KEYS_SUM + STAT_KEYS_MAX}
    s.update({
        "solver_method": method,
        "chosen_scale": 0,
        "exact_status": "not_run",
        "exact_candidate_count": 0,
        "solve_seconds": 0.0,
    })
    return s


def no_split_stats(mask_counts: Dict[int, int], min_gain: int, method: str = "no_split") -> Dict[str, Any]:
    naive_cost = compute_mask_cost(mask_counts)
    cost, acc, pop_sum, r_sum, max_gain, max_r = compute_no_split_group_cost(mask_counts, min_gain)
    return {
        "naive_cost": int(naive_cost),
        "no_split_cost": int(cost),
        "grouped_cost": int(cost),
        "split_plan_cost": int(cost),
        "used_split_plan": 0,
        "accepted_submask_count": int(acc),
        "accepted_split_submask_count": 0,
        "rejected_candidate_count": 0,
        "submask_popcount_sum": int(pop_sum),
        "group_r_sum": int(r_sum),
        "max_gain": int(max_gain),
        "max_group_r": int(max_r),
        "residual_cost": 0,
        "exact_skipped_count": 0,
        "multiscale_parent_chosen_count": 0,
        "multiscale_child_chosen_count": 0,
        "solver_method": method,
        "chosen_scale": 0,
        "exact_status": "not_run",
        "exact_candidate_count": 0,
        "solve_seconds": 0.0,
    }


def add_stats(dst: Dict[str, Any], src: Dict[str, Any]) -> None:
    for k in STAT_KEYS_SUM:
        dst[k] = int(dst.get(k, 0)) + int(src.get(k, 0))
    for k in STAT_KEYS_MAX:
        dst[k] = max(int(dst.get(k, 0)), int(src.get(k, 0)))
    dst["solve_seconds"] = float(dst.get("solve_seconds", 0.0)) + float(src.get("solve_seconds", 0.0))


def build_candidate_submasks(
    residual: Dict[int, int],
    top_masks_per_bucket: int,
    min_submask_popcount: int,
    include_exact_masks: bool = False,
) -> Dict[int, int]:
    top_items = heapq.nlargest(top_masks_per_bucket, residual.items(), key=lambda item: item[1])
    top_masks = [int(mask) for mask, _count in top_items]
    candidates: Dict[int, int] = {}
    if include_exact_masks:
        for mask in top_masks:
            u = mask.bit_count()
            if u >= min_submask_popcount:
                candidates[mask] = u
    for i, a in enumerate(top_masks):
        for b in top_masks[i + 1:]:
            cand = a & b
            if cand:
                u = cand.bit_count()
                if u >= min_submask_popcount:
                    candidates.setdefault(cand, u)
    return candidates


def apply_submask_to_residual(residual_items: List[Tuple[int, int]], submask: int) -> Dict[int, int]:
    out: Dict[int, int] = {}
    comp = ~submask
    for residual_mask, count in residual_items:
        if (residual_mask & submask) == submask:
            new_mask = residual_mask & comp
            if new_mask:
                out[new_mask] = out.get(new_mask, 0) + count
        else:
            out[residual_mask] = out.get(residual_mask, 0) + count
    return out


def candidate_stats(candidate_mask: int, popcount: int, residual_items: List[Tuple[int, int]]) -> Tuple[int, int, int]:
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
    candidates = build_candidate_submasks(
        residual,
        top_masks_per_bucket=top_masks_per_bucket,
        min_submask_popcount=min_submask_popcount,
        include_exact_masks=False,
    )
    residual_items = list(residual.items())
    infos: List[Tuple[int, int, int, int, int]] = []
    for cand, u in candidates.items():
        r, gain, selected_cost = candidate_stats(cand, u, residual_items)
        if gain >= min_gain:
            infos.append((cand, u, r, gain, selected_cost))
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
    if not residual:
        return 0
    no_split_cost = compute_no_split_group_cost(residual, min_gain)[0]
    if rounds_left <= 0 or lookahead_depth <= 0:
        return no_split_cost
    infos = sorted_candidate_infos(residual, top_masks_per_bucket, min_gain, min_submask_popcount)
    if not infos:
        return no_split_cost
    best = no_split_cost
    residual_items = list(residual.items())
    for cand, u, r, gain, selected_cost in infos[:lookahead_top_candidates]:
        nxt = apply_submask_to_residual(residual_items, cand)
        best = min(best, selected_cost + estimate_residual_cost_with_lookahead(
            nxt, top_masks_per_bucket, rounds_left - 1, min_gain, min_submask_popcount,
            lookahead_depth - 1, lookahead_top_candidates
        ))
    return int(best)


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
    infos = sorted_candidate_infos(residual, top_masks_per_bucket, min_gain, min_submask_popcount)
    if not infos:
        return 0, 0, 0, -1, 0, 0
    if selection_policy == "popcount":
        chosen = max(infos, key=lambda x: (x[3], x[1], x[2]))
        return (*chosen, len(infos) - 1)
    if selection_policy == "group_r":
        chosen = max(infos, key=lambda x: (x[3], x[2], x[1]))
        return (*chosen, len(infos) - 1)
    residual_items = list(residual.items())
    best = None
    best_key = None
    for info in infos[:lookahead_top_candidates]:
        cand, u, r, gain, selected_cost = info
        nxt = apply_submask_to_residual(residual_items, cand)
        tail = estimate_residual_cost_with_lookahead(
            nxt, top_masks_per_bucket, rounds_left - 1, min_gain, min_submask_popcount,
            max(0, lookahead_depth - 1), lookahead_top_candidates
        )
        key = (selected_cost + tail, -gain, -r, -u)
        if best_key is None or key < best_key:
            best_key = key
            best = info
    assert best is not None
    return (*best, len(infos) - 1)


def split_bucket_masks_heuristic(
    mask_counts: Dict[int, int],
    top_masks_per_bucket: int,
    max_rounds: int,
    min_gain: int,
    min_submask_popcount: int,
    selection_policy: str = "lookahead",
    lookahead_depth: int = 2,
    lookahead_top_candidates: int = 32,
) -> Dict[str, Any]:
    t0 = now()
    residual = dict(mask_counts)
    naive_cost = compute_mask_cost(mask_counts)
    no_split_cost, no_split_group_count, no_split_popcount_sum, no_split_group_r_sum, no_split_max_gain, no_split_max_group_r = compute_no_split_group_cost(mask_counts, min_gain)
    if max_rounds == 0:
        s = no_split_stats(mask_counts, min_gain, method="single_no_split")
        s["solve_seconds"] = now() - t0
        return s

    split_prefix_cost = 0
    accepted_split = rejected = split_pop_sum = split_r_sum = 0
    max_gain_seen = no_split_max_gain
    split_max_group_r = 0
    best_total = no_split_cost
    best_residual_cost = no_split_cost
    best_stats = {
        "accepted_split": 0,
        "split_pop_sum": 0,
        "split_r_sum": 0,
        "max_gain_seen": no_split_max_gain,
        "max_group_r": no_split_max_group_r,
        "rejected": 0,
        "residual_group_count": no_split_group_count,
        "residual_pop_sum": no_split_popcount_sum,
        "residual_r_sum": no_split_group_r_sum,
        "residual_max_group_r": no_split_max_group_r,
    }

    for round_idx in range(max_rounds):
        if not residual:
            if split_prefix_cost < best_total:
                best_total = split_prefix_cost
                best_residual_cost = 0
            break
        best_mask, best_u, best_r, best_gain, selected_cost, rej = choose_best_candidate(
            residual, top_masks_per_bucket, max_rounds - round_idx, min_gain, min_submask_popcount,
            selection_policy, lookahead_depth, lookahead_top_candidates
        )
        if best_gain < min_gain or best_mask == 0:
            break
        accepted_split += 1
        rejected += rej
        split_pop_sum += best_u
        split_r_sum += best_r
        max_gain_seen = max(max_gain_seen, best_gain)
        split_max_group_r = max(split_max_group_r, best_r)
        split_prefix_cost += selected_cost
        residual = apply_submask_to_residual(list(residual.items()), best_mask)
        rg_cost, rg_count, rg_pop, rg_r_sum, rg_max_gain, rg_max_r = compute_no_split_group_cost(residual, min_gain)
        total = split_prefix_cost + rg_cost
        if total < best_total:
            best_total = total
            best_residual_cost = rg_cost
            best_stats = {
                "accepted_split": accepted_split,
                "split_pop_sum": split_pop_sum,
                "split_r_sum": split_r_sum,
                "max_gain_seen": max(max_gain_seen, rg_max_gain),
                "max_group_r": max(split_max_group_r, rg_max_r),
                "rejected": rejected,
                "residual_group_count": rg_count,
                "residual_pop_sum": rg_pop,
                "residual_r_sum": rg_r_sum,
                "residual_max_group_r": rg_max_r,
            }

    used = int(best_total < no_split_cost)
    if used:
        accepted_submask_count = best_stats["accepted_split"] + best_stats["residual_group_count"]
        pop_sum = best_stats["split_pop_sum"] + best_stats["residual_pop_sum"]
        r_sum = best_stats["split_r_sum"] + best_stats["residual_r_sum"]
        max_gain_seen = best_stats["max_gain_seen"]
        max_group_r = best_stats["max_group_r"]
        accepted_split_submask_count = best_stats["accepted_split"]
        rejected_candidate_count = best_stats["rejected"]
        residual_cost = best_residual_cost
    else:
        accepted_submask_count = no_split_group_count
        pop_sum = no_split_popcount_sum
        r_sum = no_split_group_r_sum
        max_group_r = no_split_max_group_r
        accepted_split_submask_count = 0
        rejected_candidate_count = rejected
        residual_cost = 0

    return {
        "naive_cost": int(naive_cost),
        "no_split_cost": int(no_split_cost),
        "grouped_cost": int(best_total),
        "split_plan_cost": int(best_total),
        "used_split_plan": int(used),
        "accepted_submask_count": int(accepted_submask_count),
        "accepted_split_submask_count": int(accepted_split_submask_count),
        "rejected_candidate_count": int(rejected_candidate_count),
        "submask_popcount_sum": int(pop_sum),
        "group_r_sum": int(r_sum),
        "max_gain": int(max_gain_seen),
        "max_group_r": int(max_group_r),
        "residual_cost": int(residual_cost),
        "exact_skipped_count": 0,
        "multiscale_parent_chosen_count": 0,
        "multiscale_child_chosen_count": 0,
        "solver_method": "single_heuristic",
        "chosen_scale": 0,
        "exact_status": "not_run",
        "exact_candidate_count": 0,
        "solve_seconds": now() - t0,
    }


def project_mask_counts_to_children(mask_counts: Dict[int, int], tile_size: int, child_tile_size: int) -> Dict[int, Dict[int, int]]:
    if tile_size % child_tile_size != 0:
        raise ValueError("tile_size must be a multiple of child_tile_size")
    base_mask = (1 << child_tile_size) - 1
    children: Dict[int, Dict[int, int]] = defaultdict(dict)
    for mask, count in mask_counts.items():
        m = int(mask)
        for child_id, shift in enumerate(range(0, tile_size, child_tile_size)):
            sub = (m >> shift) & base_mask
            if sub:
                d = children[child_id]
                d[sub] = d.get(sub, 0) + int(count)
    return children


def cheap_parent_no_split_saving_ratio(mask_counts: Dict[int, int], min_gain: int) -> Tuple[float, int, int]:
    """Return (saving_ratio, naive_cost, no_split_cost) using exact-mask grouping only.

    This is much cheaper than lookahead splitting and is used as a gate for
    large tiles. It is only a heuristic signal: a low value does not prove that
    a split plan is bad, and a high value does not prove that children cannot be
    better.
    """
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
    multiscale_decision: str = "compare",
    gate_accept_saving_ratio: float = 0.10,
    gate_reject_saving_ratio: float = 0.02,
) -> Dict[str, Any]:
    """Recursive multiscale solver.

    compare (default):
        Solve the current parent tile and all child tiles, then choose the lower
        grouped_cost. This is slower but preserves the base-tile fallback guarantee.

    gated:
        Use a threshold to choose parent vs. children without comparing both:
          - compute cheap exact-mask saving ratio on the parent;
          - if it exceeds gate_accept_saving_ratio, solve and accept parent;
          - otherwise split into children.
        This is faster, but it does NOT guarantee no worse than base_tile_size.

    hybrid:
        A safer speed/quality compromise:
          - if cheap parent signal is below gate_reject_saving_ratio, skip parent
            splitting and split immediately;
          - otherwise solve parent;
          - if full parent saving ratio exceeds gate_accept_saving_ratio, accept
            parent immediately;
          - otherwise fall back to compare mode for uncertain cases.
        This saves many child recursions while retaining comparison in the gray zone.
    """

    if tile_size <= base_tile_size:
        parent = split_bucket_masks_heuristic(
            mask_counts, top_masks_per_bucket, max_rounds, min_gain, min_submask_popcount,
            selection_policy, lookahead_depth, lookahead_top_candidates
        )
        parent["chosen_scale"] = tile_size
        parent["solver_method"] = "multiscale_base"
        parent["multiscale_parent_chosen_count"] = 1
        return parent

    child_tile_size = tile_size // split_factor

    def solve_children() -> Dict[str, Any]:
        child_stats = empty_stats("multiscale_children")
        child_counts = project_mask_counts_to_children(mask_counts, tile_size, child_tile_size)
        for _child_id, child_mask_counts in child_counts.items():
            cs = split_bucket_masks_multiscale(
                child_mask_counts, child_tile_size, base_tile_size, split_factor,
                top_masks_per_bucket, max_rounds, min_gain, min_submask_popcount,
                selection_policy, lookahead_depth, lookahead_top_candidates,
                multiscale_decision, gate_accept_saving_ratio, gate_reject_saving_ratio,
            )
            add_stats(child_stats, cs)
        child_stats["chosen_scale"] = child_tile_size
        child_stats["multiscale_child_chosen_count"] = int(child_stats.get("multiscale_child_chosen_count", 0)) + 1
        return child_stats

    cheap_ratio, cheap_naive, _cheap_no_split = cheap_parent_no_split_saving_ratio(mask_counts, min_gain)

    # Fast hard-gate mode: no parent/child comparison.
    if multiscale_decision == "gated":
        if cheap_naive == 0:
            return no_split_stats(mask_counts, min_gain, method="multiscale_empty")
        if cheap_ratio >= gate_accept_saving_ratio:
            parent = split_bucket_masks_heuristic(
                mask_counts, top_masks_per_bucket, max_rounds, min_gain, min_submask_popcount,
                selection_policy, lookahead_depth, lookahead_top_candidates
            )
            parent["chosen_scale"] = tile_size
            parent["solver_method"] = "multiscale_gated_parent"
            parent["multiscale_parent_chosen_count"] = 1
            parent["gate_parent_cheap_saving_ratio"] = cheap_ratio
            return parent
        child_stats = solve_children()
        child_stats["solver_method"] = "multiscale_gated_child"
        child_stats["gate_parent_cheap_saving_ratio"] = cheap_ratio
        return child_stats

    # Hybrid mode: cheap reject, parent early accept, otherwise compare.
    if multiscale_decision == "hybrid" and cheap_ratio < gate_reject_saving_ratio:
        child_stats = solve_children()
        child_stats["solver_method"] = "multiscale_hybrid_cheap_reject_child"
        child_stats["gate_parent_cheap_saving_ratio"] = cheap_ratio
        return child_stats

    parent = split_bucket_masks_heuristic(
        mask_counts, top_masks_per_bucket, max_rounds, min_gain, min_submask_popcount,
        selection_policy, lookahead_depth, lookahead_top_candidates
    )
    parent["chosen_scale"] = tile_size
    parent["gate_parent_cheap_saving_ratio"] = cheap_ratio

    parent_naive = int(parent.get("naive_cost", 0))
    parent_ratio = ((parent_naive - int(parent["grouped_cost"])) / parent_naive) if parent_naive > 0 else 0.0
    parent["gate_parent_full_saving_ratio"] = parent_ratio

    if multiscale_decision == "hybrid" and parent_ratio >= gate_accept_saving_ratio:
        parent["solver_method"] = "multiscale_hybrid_parent_early_accept"
        parent["multiscale_parent_chosen_count"] = 1
        return parent

    child_stats = solve_children()

    if int(child_stats["naive_cost"]) == 0:
        parent["solver_method"] = "multiscale_parent"
        parent["multiscale_parent_chosen_count"] = 1
        return parent

    # Compare mode, or hybrid uncertain zone.
    if int(child_stats["grouped_cost"]) <= int(parent["grouped_cost"]):
        child_stats["solver_method"] = "multiscale_child" if multiscale_decision == "compare" else "multiscale_hybrid_child_after_compare"
        return child_stats

    parent["solver_method"] = "multiscale_parent" if multiscale_decision == "compare" else "multiscale_hybrid_parent_after_compare"
    parent["multiscale_parent_chosen_count"] = 1
    return parent


class ExactTimeout(Exception):
    pass


def iter_nonzero_submasks(mask: int) -> Iterable[int]:
    sub = mask
    while sub:
        yield sub
        sub = (sub - 1) & mask


def exact_initial_candidates(
    residual: Dict[int, int],
    min_gain: int,
    min_submask_popcount: int,
    max_candidates: int,
) -> Tuple[Dict[int, int], bool]:
    candidates: Dict[int, int] = {}
    for mask in residual:
        for sub in iter_nonzero_submasks(int(mask)):
            u = sub.bit_count()
            if u < min_submask_popcount:
                continue
            candidates.setdefault(sub, u)
            if len(candidates) > max_candidates:
                return candidates, False
    residual_items = list(residual.items())
    filtered: Dict[int, int] = {}
    for sub, u in candidates.items():
        r, gain, _ = candidate_stats(sub, u, residual_items)
        if gain >= min_gain:
            filtered[sub] = u
    return filtered, True


def normalize_residual(residual: Dict[int, int]) -> Tuple[Tuple[int, int], ...]:
    return tuple(sorted((int(m), int(c)) for m, c in residual.items() if m and c))


def split_bucket_masks_exact(
    mask_counts: Dict[int, int],
    tile_size: int,
    min_gain: int,
    min_submask_popcount: int,
    exact_max_tile_size: int,
    exact_max_distinct_masks: int,
    exact_max_popcount_sum: int,
    exact_max_candidates: int,
    exact_time_limit_sec: float,
) -> Dict[str, Any]:
    t0 = now()
    no_split = no_split_stats(mask_counts, min_gain, method="exact_fallback")
    if tile_size > exact_max_tile_size:
        no_split["exact_status"] = f"skipped_tile_size>{exact_max_tile_size}"
        no_split["exact_skipped_count"] = 1
        no_split["solve_seconds"] = now() - t0
        return no_split
    if len(mask_counts) > exact_max_distinct_masks:
        no_split["exact_status"] = f"skipped_distinct_masks>{exact_max_distinct_masks}"
        no_split["exact_skipped_count"] = 1
        no_split["solve_seconds"] = now() - t0
        return no_split
    pop_sum = sum(int(m).bit_count() for m in mask_counts)
    if pop_sum > exact_max_popcount_sum:
        no_split["exact_status"] = f"skipped_popcount_sum>{exact_max_popcount_sum}"
        no_split["exact_skipped_count"] = 1
        no_split["solve_seconds"] = now() - t0
        return no_split

    init_candidates, complete = exact_initial_candidates(
        mask_counts, min_gain, min_submask_popcount, exact_max_candidates
    )
    if not complete:
        no_split["exact_status"] = f"skipped_candidates>{exact_max_candidates}"
        no_split["exact_skipped_count"] = 1
        no_split["exact_candidate_count"] = len(init_candidates)
        no_split["solve_seconds"] = now() - t0
        return no_split
    candidate_masks = tuple(sorted(init_candidates.keys(), key=lambda x: (x.bit_count(), x), reverse=True))
    no_split["exact_candidate_count"] = len(candidate_masks)

    deadline = (t0 + exact_time_limit_sec) if exact_time_limit_sec > 0 else None

    @lru_cache(maxsize=None)
    def dp(state: Tuple[Tuple[int, int], ...]) -> Tuple[int, Tuple[int, ...]]:
        if deadline is not None and now() > deadline:
            raise ExactTimeout()
        residual = dict(state)
        fallback = compute_no_split_group_cost(residual, min_gain)[0]
        best_cost = fallback
        best_plan: Tuple[int, ...] = ()
        if not residual:
            return 0, ()
        residual_items = list(residual.items())
        # Use only candidates that are subsets of at least two/current count enough masks.
        for sub in candidate_masks:
            # sub must still occur in current residual.
            r, gain, selected_cost = candidate_stats(sub, sub.bit_count(), residual_items)
            if gain < min_gain:
                continue
            nxt = apply_submask_to_residual(residual_items, sub)
            nxt_state = normalize_residual(nxt)
            if nxt_state == state:
                continue
            tail, tail_plan = dp(nxt_state)
            total = selected_cost + tail
            if total < best_cost:
                best_cost = total
                best_plan = (sub,) + tail_plan
        return best_cost, best_plan

    try:
        best_cost, best_plan = dp(normalize_residual(mask_counts))
    except ExactTimeout:
        no_split["exact_status"] = "skipped_timeout"
        no_split["exact_skipped_count"] = 1
        no_split["solve_seconds"] = now() - t0
        return no_split

    # Reconstruct approximate stats from the exact plan, then no-split the residual.
    residual = dict(mask_counts)
    split_cost = split_count = split_pop_sum = split_r_sum = max_gain = max_r = 0
    rejected_count = 0
    for sub in best_plan:
        items = list(residual.items())
        r, gain, selected_cost = candidate_stats(sub, sub.bit_count(), items)
        if gain < min_gain:
            continue
        split_count += 1
        split_pop_sum += sub.bit_count()
        split_r_sum += r
        max_gain = max(max_gain, gain)
        max_r = max(max_r, r)
        split_cost += selected_cost
        residual = apply_submask_to_residual(items, sub)
    rg_cost, rg_count, rg_pop, rg_r_sum, rg_max_gain, rg_max_r = compute_no_split_group_cost(residual, min_gain)
    used = int(best_cost < int(no_split["no_split_cost"]))
    if used:
        result = {
            "naive_cost": int(no_split["naive_cost"]),
            "no_split_cost": int(no_split["no_split_cost"]),
            "grouped_cost": int(best_cost),
            "split_plan_cost": int(best_cost),
            "used_split_plan": 1,
            "accepted_submask_count": int(split_count + rg_count),
            "accepted_split_submask_count": int(split_count),
            "rejected_candidate_count": int(rejected_count),
            "submask_popcount_sum": int(split_pop_sum + rg_pop),
            "group_r_sum": int(split_r_sum + rg_r_sum),
            "max_gain": int(max(max_gain, rg_max_gain)),
            "max_group_r": int(max(max_r, rg_max_r)),
            "residual_cost": int(rg_cost),
            "exact_skipped_count": 0,
            "multiscale_parent_chosen_count": 0,
            "multiscale_child_chosen_count": 0,
            "solver_method": "exact",
            "chosen_scale": tile_size,
            "exact_status": "ok",
            "exact_candidate_count": len(candidate_masks),
            "solve_seconds": now() - t0,
        }
    else:
        result = dict(no_split)
        result["solver_method"] = "exact_no_better_than_no_split"
        result["exact_status"] = "ok"
        result["exact_candidate_count"] = len(candidate_masks)
        result["solve_seconds"] = now() - t0
    return result


def split_bucket_masks_dispatch(mask_counts: Dict[int, int], tile_size: int, args: argparse.Namespace) -> Dict[str, Any]:
    if args.solver == "single":
        s = split_bucket_masks_heuristic(
            mask_counts, args.top_masks_per_bucket, args.max_rounds, args.min_gain,
            args.min_submask_popcount, args.selection_policy, args.lookahead_depth,
            args.lookahead_top_candidates
        )
        s["chosen_scale"] = tile_size
        return s

    if args.solver == "multiscale":
        return split_bucket_masks_multiscale(
            mask_counts, tile_size, args.base_tile_size, args.split_factor,
            args.top_masks_per_bucket, args.max_rounds, args.min_gain,
            args.min_submask_popcount, args.selection_policy, args.lookahead_depth,
            args.lookahead_top_candidates,
            args.multiscale_decision, args.gate_accept_saving_ratio, args.gate_reject_saving_ratio,
        )

    if args.solver == "exact":
        return split_bucket_masks_exact(
            mask_counts, tile_size, args.min_gain, args.min_submask_popcount,
            args.exact_max_tile_size, args.exact_max_distinct_masks,
            args.exact_max_popcount_sum, args.exact_max_candidates,
            args.exact_time_limit_sec
        )

    # both: run multiscale, then exact when exact is allowed; choose lower cost.
    ms = split_bucket_masks_multiscale(
        mask_counts, tile_size, args.base_tile_size, args.split_factor,
        args.top_masks_per_bucket, args.max_rounds, args.min_gain,
        args.min_submask_popcount, args.selection_policy, args.lookahead_depth,
        args.lookahead_top_candidates,
        args.multiscale_decision, args.gate_accept_saving_ratio, args.gate_reject_saving_ratio,
    )
    ex = split_bucket_masks_exact(
        mask_counts, tile_size, args.min_gain, args.min_submask_popcount,
        args.exact_max_tile_size, args.exact_max_distinct_masks,
        args.exact_max_popcount_sum, args.exact_max_candidates,
        args.exact_time_limit_sec
    )
    # Always add exact solve time/skip info to chosen result for diagnostics.
    if int(ex["exact_skipped_count"]) == 0 and int(ex["grouped_cost"]) < int(ms["grouped_cost"]):
        ex["solve_seconds"] = float(ex.get("solve_seconds", 0.0)) + float(ms.get("solve_seconds", 0.0))
        ex["solver_method"] = "both_exact_chosen"
        return ex
    ms["exact_skipped_count"] = int(ms.get("exact_skipped_count", 0)) + int(ex.get("exact_skipped_count", 0))
    ms["exact_status"] = ex.get("exact_status", "not_run")
    ms["exact_candidate_count"] = ex.get("exact_candidate_count", 0)
    ms["solve_seconds"] = float(ms.get("solve_seconds", 0.0)) + float(ex.get("solve_seconds", 0.0))
    ms["solver_method"] = "both_multiscale_chosen"
    return ms


def analyze_for_c(
    scalars: np.ndarray,
    input_path: str,
    input_shape: Tuple[int, ...],
    c: int,
    args: argparse.Namespace,
    max_abs_bitlen: int,
) -> Dict[str, Any]:
    t_c0 = now()
    K, n = args.K, args.n
    q = 1 << c
    qmask = q - 1
    h = (max_abs_bitlen + c - 1) // c if max_abs_bitlen > 0 else 0
    mask_nbytes = (args.tile_size + 7) // 8
    digit_dtype = choose_digit_dtype(q - 1)

    totals = empty_stats("totals")
    total_nonzero_digits = 0
    digit_histogram: DefaultDict[int, int] = defaultdict(int)
    per_digit_naive: DefaultDict[int, int] = defaultdict(int)
    per_digit_group: DefaultDict[int, int] = defaultdict(int)
    per_window_C_naive = [0] * h
    per_window_C_group = [0] * h
    per_window_build_seconds = [0.0] * h
    per_window_solve_seconds = [0.0] * h
    build_seconds_total = 0.0
    solve_seconds_total = 0.0

    for window_j in range(h):
        t_build0 = now()
        shift = window_j * c
        window_bucket_counts: DefaultDict[int, DefaultDict[int, Dict[int, int]]] = defaultdict(lambda: defaultdict(dict))
        for point_start in range(0, n, args.point_chunk):
            point_end = min(n, point_start + args.point_chunk)
            block = scalars[:, point_start:point_end, :]
            abs_lo, abs_hi, neg_mask = decode_abs_block(block)
            digits = extract_window_digits(abs_lo, abs_hi, shift, qmask)
            signed_digits = build_signed_digits(digits, neg_mask, digit_dtype)
            for bucket, bucket_digit_count in iter_present_nonzero_buckets(signed_digits, q):
                digit_histogram[bucket] += bucket_digit_count
                total_nonzero_digits += bucket_digit_count
                add_tile_mask_counts_for_bucket(
                    window_bucket_counts[bucket], signed_digits, bucket,
                    args.tile_size, mask_nbytes
                )
        build_seconds = now() - t_build0
        build_seconds_total += build_seconds
        per_window_build_seconds[window_j] = build_seconds

        t_solve0 = now()
        window_naive = 0
        window_group = 0
        for bucket, tile_mask_counts in window_bucket_counts.items():
            for _tile_id, mask_counts in tile_mask_counts.items():
                bs = split_bucket_masks_dispatch(mask_counts, args.tile_size, args)
                add_stats(totals, bs)
                bucket_naive = int(bs["naive_cost"])
                bucket_group = int(bs["grouped_cost"])
                window_naive += bucket_naive
                window_group += bucket_group
                per_digit_naive[bucket] += bucket_naive
                per_digit_group[bucket] += bucket_group
        solve_seconds = now() - t_solve0
        solve_seconds_total += solve_seconds
        per_window_solve_seconds[window_j] = solve_seconds
        per_window_C_naive[window_j] = window_naive
        per_window_C_group[window_j] = window_group
        if args.profile:
            print(
                f"[c={c} window={window_j}/{h-1}] build={build_seconds:.3f}s solve={solve_seconds:.3f}s "
                f"C_naive={window_naive} C_group={window_group} rss={max_rss_mb():.1f}MB",
                flush=True,
            )

    C_naive = int(totals["naive_cost"])
    C_no_split = int(totals["no_split_cost"])
    C_group = int(totals["grouped_cost"])
    total_positions = K * n * h
    zero_count = total_positions - total_nonzero_digits
    zero_rate = zero_count / total_positions if total_positions > 0 else 0.0
    saving_ratio = (C_naive - C_group) / C_naive if C_naive > 0 else 0.0
    reuse_factor = C_naive / C_group if C_group > 0 else None
    accepted = int(totals["accepted_submask_count"])
    avg_pop = int(totals["submask_popcount_sum"]) / accepted if accepted > 0 else 0.0
    avg_r = int(totals["group_r_sum"]) / accepted if accepted > 0 else 0.0
    per_window_saving_ratio = [
        ((a - g) / a if a > 0 else 0.0) for a, g in zip(per_window_C_naive, per_window_C_group)
    ]
    per_digit_saving_ratio: Dict[str, float] = {}
    for digit in sorted(set(per_digit_naive) | set(per_digit_group)):
        a = per_digit_naive[digit]
        g = per_digit_group[digit]
        per_digit_saving_ratio[str(digit)] = (a - g) / a if a > 0 else 0.0
    digit_histogram[0] = int(zero_count)

    total_seconds = now() - t_c0
    tile_label = "all" if args.tile_size == K else str(args.tile_size)
    json_path = os.path.abspath(os.path.join(args.out_dir, f"tilemask_{args.solver}_t{tile_label}_base{args.base_tile_size}_c{c}.json"))
    result: Dict[str, Any] = {
        "input_path": os.path.abspath(input_path),
        "input_shape": list(input_shape),
        "logical_shape": [K, n, 4],
        "analysis_mode": "tilemask_multiscale_exact_traditional_pippenger",
        "solver": args.solver,
        "multiscale_decision": args.multiscale_decision,
        "gate_accept_saving_ratio": args.gate_accept_saving_ratio,
        "gate_reject_saving_ratio": args.gate_reject_saving_ratio,
        "K": K,
        "n": n,
        "c": c,
        "h": h,
        "q": q,
        "point_chunk": args.point_chunk,
        "tile_size": args.tile_size,
        "base_tile_size": args.base_tile_size,
        "split_factor": args.split_factor,
        "top_masks_per_bucket": args.top_masks_per_bucket,
        "max_rounds": args.max_rounds,
        "min_gain": args.min_gain,
        "min_submask_popcount": args.min_submask_popcount,
        "selection_policy": args.selection_policy,
        "lookahead_depth": args.lookahead_depth,
        "lookahead_top_candidates": args.lookahead_top_candidates,
        "exact_max_tile_size": args.exact_max_tile_size,
        "exact_max_distinct_masks": args.exact_max_distinct_masks,
        "exact_max_popcount_sum": args.exact_max_popcount_sum,
        "exact_max_candidates": args.exact_max_candidates,
        "exact_time_limit_sec": args.exact_time_limit_sec,
        "mask_nbytes": mask_nbytes,
        "max_abs_bit_length": max_abs_bitlen,
        "total_nonzero_digits": int(total_nonzero_digits),
        "C_naive": C_naive,
        "C_no_split": C_no_split,
        "C_group": C_group,
        "split_improvement_over_no_split": int(C_no_split - C_group),
        "used_split_plan_count": int(totals["used_split_plan"]),
        "saving_ratio": saving_ratio,
        "reuse_factor": reuse_factor,
        "accepted_submask_count": accepted,
        "accepted_split_submask_count": int(totals["accepted_split_submask_count"]),
        "rejected_candidate_count": int(totals["rejected_candidate_count"]),
        "average_submask_popcount": avg_pop,
        "average_group_r": avg_r,
        "max_gain": int(totals["max_gain"]),
        "max_group_r": int(totals["max_group_r"]),
        "residual_cost": int(totals["residual_cost"]),
        "exact_skipped_count": int(totals["exact_skipped_count"]),
        "multiscale_parent_chosen_count": int(totals["multiscale_parent_chosen_count"]),
        "multiscale_child_chosen_count": int(totals["multiscale_child_chosen_count"]),
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


def write_summary_csv(rows: List[Dict[str, Any]], csv_path: str) -> None:
    fieldnames = [
        "c", "h", "q", "max_abs_bit_length", "total_nonzero_digits",
        "C_naive", "C_no_split", "C_group", "split_improvement_over_no_split",
        "used_split_plan_count", "saving_ratio", "reuse_factor",
        "accepted_submask_count", "accepted_split_submask_count",
        "average_submask_popcount", "average_group_r", "max_gain", "max_group_r",
        "zero_rate", "solver", "tile_size", "base_tile_size",
        "exact_skipped_count", "multiscale_parent_chosen_count", "multiscale_child_chosen_count",
        "build_seconds", "solve_seconds", "total_seconds", "max_rss_mb", "json_path",
    ]
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for r in rows:
            timing = r.get("timing", {})
            out = {k: r.get(k, None) for k in fieldnames}
            out["build_seconds"] = timing.get("build_seconds", None)
            out["solve_seconds"] = timing.get("solve_seconds", None)
            out["total_seconds"] = timing.get("total_seconds", None)
            out["max_rss_mb"] = timing.get("max_rss_mb", None)
            w.writerow(out)


def print_markdown_summary(rows: List[Dict[str, Any]]) -> None:
    print("| c | h | q | C_naive | C_no_split | C_group | split_gain | saving_ratio | reuse_factor | exact_skip | parent_chosen | child_chosen | time(s) |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for r in rows:
        reuse = r["reuse_factor"]
        reuse_s = "null" if reuse is None else f"{float(reuse):.8f}"
        print(
            f"| {r['c']} | {r['h']} | {r['q']} | {r['C_naive']} | {r['C_no_split']} | {r['C_group']} | "
            f"{r['split_improvement_over_no_split']} | {float(r['saving_ratio']):.8f} | {reuse_s} | "
            f"{r['exact_skipped_count']} | {r['multiscale_parent_chosen_count']} | {r['multiscale_child_chosen_count']} | "
            f"{float(r['timing']['total_seconds']):.2f} |"
        )


def main() -> None:
    args = parse_args()
    validate_args(args)
    os.makedirs(args.out_dir, exist_ok=True)

    t0 = now()
    scalars, input_shape = load_scalar_memmap(args.input, args.K, args.n)
    load_seconds = now() - t0
    t1 = now()
    max_abs_bitlen = compute_max_abs_bitlen(scalars, args.point_chunk)
    bitlen_seconds = now() - t1

    rows: List[Dict[str, Any]] = []
    for c in range(args.c_start, args.c_end + 1):
        r = analyze_for_c(scalars, args.input, input_shape, c, args, max_abs_bitlen)
        rows.append(r)
        print(
            f"done c={c}: C_group={r['C_group']} saving={100*r['saving_ratio']:.3f}% "
            f"time={r['timing']['total_seconds']:.2f}s rss={r['timing']['max_rss_mb']:.1f}MB",
            flush=True,
        )

    tile_label = "all" if args.tile_size == args.K else str(args.tile_size)
    csv_path = os.path.abspath(os.path.join(
        args.out_dir,
        f"tilemask_{args.solver}_summary_c{args.c_start}_to_c{args.c_end}_t{tile_label}_base{args.base_tile_size}.csv",
    ))
    write_summary_csv(rows, csv_path)

    print(f"input_shape          : {input_shape}")
    print(f"logical_shape        : ({args.K}, {args.n}, 4)")
    print("analysis_mode        : tilemask_multiscale_exact_traditional_pippenger")
    print(f"solver               : {args.solver}")
    print(f"multiscale_decision  : {args.multiscale_decision}")
    print(f"gate_accept_saving_ratio: {args.gate_accept_saving_ratio}")
    print(f"gate_reject_saving_ratio: {args.gate_reject_saving_ratio}")
    print(f"c_range              : [{args.c_start}, {args.c_end}]")
    print(f"point_chunk          : {args.point_chunk}")
    print(f"tile_size            : {args.tile_size}")
    print(f"base_tile_size       : {args.base_tile_size}")
    print(f"split_factor         : {args.split_factor}")
    print(f"top_masks_per_bucket : {args.top_masks_per_bucket}")
    print(f"max_rounds           : {args.max_rounds}")
    print(f"min_gain             : {args.min_gain}")
    print(f"selection_policy     : {args.selection_policy}")
    print(f"lookahead_depth      : {args.lookahead_depth}")
    print(f"exact_max_tile_size  : {args.exact_max_tile_size}")
    print(f"max_abs_bit_length   : {max_abs_bitlen}")
    print(f"load_seconds         : {load_seconds:.3f}")
    print(f"bitlen_seconds       : {bitlen_seconds:.3f}")
    print(f"out_dir              : {os.path.abspath(args.out_dir)}")
    print(f"summary_csv          : {csv_path}")
    print_markdown_summary(rows)


if __name__ == "__main__":
    main()
