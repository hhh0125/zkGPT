#!/usr/bin/env python3
"""
Analyze theoretical savings from mask-grouped bucket accumulation for
BGMW / Pippenger variant with a global K-bit mask.

Mathematical model:
    Q[i, j] = q^j * P[i]
    S_k = sum_{i, j} d[k, i, j] * Q[i, j]

where d[k, i, j] is the signed window digit extracted from scalar A[k, i].

This script does not perform any elliptic-curve arithmetic. It only counts
theoretical online bucket-update costs. The precomputation cost of Q[i, j]
is intentionally excluded from the statistics.

Why grouping can cross windows here:
Traditional Pippenger keeps window weight q^j outside the point, so grouping
is only valid inside the same window. In BGMW, Q[i, j] already absorbs q^j
into the precomputed point. Therefore the online phase can flatten all (i, j)
positions, and the grouping key becomes:
    (signed_digit, global_mask)
instead of:
    (window_j, signed_digit, global_mask)

Input representation:
Each scalar is stored in 4 uint64 limbs, but only the first two limbs are
meaningful. They encode a signed __int128 in two's-complement form:

    low = limb[0]
    high = limb[1]
    u = low + (high << 64)
    if u >= 2^127:
        v = u - 2^128
    else:
        v = u

Do not interpret all 4 limbs as a signed 256-bit integer.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from collections import defaultdict
from typing import Dict, Iterable, List, Optional, Tuple

import numpy as np


UINT64_ONE = np.uint64(1)
BYTE_POPCOUNT = np.array([bin(i).count("1") for i in range(256)], dtype=np.uint8)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Analyze mask-grouped bucket accumulation for the BGMW / "
            "Pippenger variant using a global K-bit mask."
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
            "Masks always span all K MSM rows."
        ),
    )
    parser.add_argument(
        "--use-hash-key",
        action="store_true",
        help=(
            "Use a 128-bit BLAKE2b digest instead of exact packed-mask bytes as "
            "the dictionary key. This reduces key size but introduces collision risk."
        ),
    )
    parser.add_argument(
        "--hash-collision-check",
        action="store_true",
        help=(
            "When --use-hash-key is enabled, store the first exact mask for each "
            "hashed key and raise an error if a collision is detected."
        ),
    )
    parser.add_argument(
        "--out",
        default="mask_grouped_reuse_stats.json",
        help="Output JSON path",
    )
    parser.add_argument(
        "--ktile",
        type=int,
        default=None,
        help="Deprecated and ignored. Analysis always uses the full K MSMs.",
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
    """
    Signed-magnitude digit extraction:
        sign = sign(A[k, i])
        raw_digit = (abs(A[k, i]) >> (j * c)) & ((1 << c) - 1)
        signed_digit = sign * raw_digit

    raw_digit == 0 must stay 0.
    """

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
    _, n, _ = scalars.shape
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


def normalize_key_bytes(mask_bytes: bytes, use_hash_key: bool) -> bytes:
    if not use_hash_key:
        return mask_bytes
    return hashlib.blake2b(mask_bytes, digest_size=16).digest()


def add_mask_groups_for_bucket(
    groups: Dict[Tuple[int, bytes], List[int]],
    signed_digits: np.ndarray,
    bucket: int,
    mask_nbytes: int,
    use_hash_key: bool,
    collision_guard: Optional[Dict[Tuple[int, bytes], bytes]],
) -> None:
    """
    For one fixed signed digit bucket on one [K, point_chunk] block:
      1. build the global K-bit mask for every point column,
      2. deduplicate masks inside the chunk with np.unique,
      3. merge them into the target dictionary.

    With K = 16384, one exact mask is 2048 bytes. If the number of unique
    masks becomes very large, the exact-key dictionary can consume substantial
    memory. --use-hash-key trades memory for collision risk.
    """

    eq = signed_digits == bucket
    u_counts = eq.sum(axis=0, dtype=np.int32)
    active_cols = np.flatnonzero(u_counts)
    if active_cols.size == 0:
        return

    packed = np.packbits(eq[:, active_cols], axis=0, bitorder="little").T
    packed = np.ascontiguousarray(packed)

    key_dtype = np.dtype((np.void, mask_nbytes))
    key_view = packed.view(key_dtype).reshape(-1)
    unique_keys, counts = np.unique(key_view, return_counts=True)

    unique_bytes = unique_keys.view(np.uint8).reshape(-1, mask_nbytes)
    popcounts = BYTE_POPCOUNT[unique_bytes].sum(axis=1, dtype=np.int32)

    for idx in range(len(counts)):
        exact_bytes = bytes(unique_bytes[idx])
        key_tail = normalize_key_bytes(exact_bytes, use_hash_key)
        key = (bucket, key_tail)

        if collision_guard is not None:
            prev_exact = collision_guard.get(key)
            if prev_exact is None:
                collision_guard[key] = exact_bytes
            elif prev_exact != exact_bytes:
                raise RuntimeError(
                    "Hash collision detected for key "
                    f"(bucket={bucket}, digest={key_tail.hex()})"
                )

        r = int(counts[idx])
        u = int(popcounts[idx])
        prev = groups.get(key)
        if prev is None:
            groups[key] = [r, u]
        else:
            prev[0] += r


def finalize_global_groups(
    groups: Dict[Tuple[int, bytes], List[int]],
    per_digit_naive: Dict[int, int],
    per_digit_group: Dict[int, int],
    mask_popcount_histogram: np.ndarray,
    group_size_histogram: np.ndarray,
) -> Tuple[int, int, int, int, int]:
    C_naive = 0
    C_group = 0
    group_count = 0
    group_size_sum = 0
    max_group_size = 0

    for (bucket, _mask_key), (r, u) in groups.items():
        naive_cost = r * u
        grouped_cost = (r - 1) + u

        C_naive += naive_cost
        C_group += grouped_cost
        group_count += 1
        group_size_sum += r
        max_group_size = max(max_group_size, r)

        mask_popcount_histogram[u] += 1
        group_size_histogram[r] += 1
        per_digit_naive[bucket] += naive_cost
        per_digit_group[bucket] += grouped_cost

    return C_naive, C_group, group_count, group_size_sum, max_group_size


def finalize_diagnostic_window_groups(
    window_groups: Dict[Tuple[int, bytes], List[int]]
) -> Tuple[int, int]:
    """
    Diagnostic-only per-window result. This does not affect the true flattened
    BGMW global result.
    """

    C_naive = 0
    C_group_no_cross_window = 0
    for (_bucket, _mask_key), (r, u) in window_groups.items():
        C_naive += r * u
        C_group_no_cross_window += (r - 1) + u
    return C_naive, C_group_no_cross_window


def main() -> None:
    args = parse_args()
    if args.c <= 0 or args.c > 63:
        raise ValueError("--c must be in [1, 63]")
    if args.K <= 0 or args.n <= 0:
        raise ValueError("--K and --n must be positive")
    if args.point_chunk <= 0:
        raise ValueError("--point-chunk must be positive")
    if args.hash_collision_check and not args.use_hash_key:
        raise ValueError("--hash-collision-check requires --use-hash-key")

    scalars, input_shape = load_scalar_memmap(args.input, args.K, args.n)
    max_abs_bitlen = compute_max_abs_bitlen(scalars, args.point_chunk)

    q = 1 << args.c
    qmask = q - 1
    h = (max_abs_bitlen + args.c - 1) // args.c if max_abs_bitlen > 0 else 0
    mask_nbytes = (args.K + 7) // 8
    digit_dtype = choose_digit_dtype(q - 1)
    total_flattened_positions = args.n * h
    total_digit_positions = args.K * total_flattened_positions

    total_nonzero_digits = 0
    digit_histogram: Dict[int, int] = defaultdict(int)
    per_digit_naive: Dict[int, int] = defaultdict(int)
    per_digit_group: Dict[int, int] = defaultdict(int)

    mask_popcount_histogram = np.zeros(args.K + 1, dtype=np.int64)
    group_size_histogram = np.zeros(total_flattened_positions + 1, dtype=np.int64)
    per_window_C_naive = [0] * h
    per_window_C_group_no_cross_window = [0] * h

    global_groups: Dict[Tuple[int, bytes], List[int]] = {}
    collision_guard: Optional[Dict[Tuple[int, bytes], bytes]] = None
    if args.hash_collision_check:
        collision_guard = {}

    for window_j in range(h):
        shift = window_j * args.c

        # Diagnostic-only per-window grouping. The true BGMW result is
        # accumulated into global_groups across all windows.
        window_groups: Dict[Tuple[int, bytes], List[int]] = {}

        for point_start in range(0, args.n, args.point_chunk):
            point_end = min(args.n, point_start + args.point_chunk)
            block = scalars[:, point_start:point_end, :]

            abs_lo, abs_hi, neg_mask = decode_abs_block(block)
            digits = extract_window_digits(abs_lo, abs_hi, shift, qmask)
            signed_digits = build_signed_digits(digits, neg_mask, digit_dtype)

            for bucket, bucket_digit_count in iter_present_nonzero_buckets(signed_digits, q):
                digit_histogram[bucket] += bucket_digit_count
                total_nonzero_digits += bucket_digit_count

                add_mask_groups_for_bucket(
                    global_groups,
                    signed_digits,
                    bucket,
                    mask_nbytes,
                    args.use_hash_key,
                    collision_guard,
                )
                add_mask_groups_for_bucket(
                    window_groups,
                    signed_digits,
                    bucket,
                    mask_nbytes,
                    args.use_hash_key,
                    collision_guard=None,
                )

        (
            per_window_C_naive[window_j],
            per_window_C_group_no_cross_window[window_j],
        ) = finalize_diagnostic_window_groups(window_groups)

        del window_groups

    (
        C_naive,
        C_group,
        group_count,
        group_size_sum,
        max_group_size,
    ) = finalize_global_groups(
        global_groups,
        per_digit_naive,
        per_digit_group,
        mask_popcount_histogram,
        group_size_histogram,
    )

    saving_ratio = ((C_naive - C_group) / C_naive) if C_naive > 0 else 0.0
    reuse_factor = (C_naive / C_group) if C_group > 0 else None
    average_group_size = (group_size_sum / group_count) if group_count > 0 else 0.0

    per_window_saving_ratio_no_cross_window = []
    for naive, grouped in zip(per_window_C_naive, per_window_C_group_no_cross_window):
        if naive > 0:
            per_window_saving_ratio_no_cross_window.append((naive - grouped) / naive)
        else:
            per_window_saving_ratio_no_cross_window.append(0.0)

    per_digit_saving_ratio: Dict[str, float] = {}
    for digit in sorted(set(per_digit_naive) | set(per_digit_group)):
        naive = per_digit_naive[digit]
        grouped = per_digit_group[digit]
        if naive > 0:
            per_digit_saving_ratio[str(digit)] = (naive - grouped) / naive
        else:
            per_digit_saving_ratio[str(digit)] = 0.0

    zero_count = total_digit_positions - total_nonzero_digits
    zero_rate = (zero_count / total_digit_positions) if total_digit_positions > 0 else 0.0
    digit_histogram[0] = zero_count

    result = {
        "input_path": os.path.abspath(args.input),
        "input_shape": list(input_shape),
        "logical_shape": [args.K, args.n, 4],
        "analysis_mode": "global_mask_bgmw_flattened",
        "K": args.K,
        "n": args.n,
        "c": args.c,
        "h": h,
        "q": q,
        "point_chunk": args.point_chunk,
        "deprecated_ktile": args.ktile,
        "use_hash_key": args.use_hash_key,
        "hash_collision_check": args.hash_collision_check,
        "mask_nbytes": mask_nbytes,
        "max_abs_bit_length": max_abs_bitlen,
        "total_flattened_positions": total_flattened_positions,
        "total_digit_positions": total_digit_positions,
        "total_nonzero_digits": int(total_nonzero_digits),
        "C_naive": int(C_naive),
        "C_group": int(C_group),
        "saving_ratio": saving_ratio,
        "reuse_factor": reuse_factor,
        "group_count": int(group_count),
        "average_group_size": average_group_size,
        "max_group_size": int(max_group_size),
        "mask_popcount_histogram": sparse_hist_to_dict(mask_popcount_histogram),
        "group_size_histogram": sparse_hist_to_dict(group_size_histogram),
        "digit_histogram": bucket_hist_to_dict(digit_histogram),
        "zero_count": int(zero_count),
        "zero_rate": zero_rate,
        "per_window_C_naive": [int(x) for x in per_window_C_naive],
        "per_window_C_group_no_cross_window": [
            int(x) for x in per_window_C_group_no_cross_window
        ],
        "per_window_saving_ratio_no_cross_window": per_window_saving_ratio_no_cross_window,
        "per_digit_saving_ratio": per_digit_saving_ratio,
    }

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"input_path           : {os.path.abspath(args.input)}")
    print(f"input_shape          : {input_shape}")
    print(f"logical_shape        : ({args.K}, {args.n}, 4)")
    print("analysis_mode        : global_mask_bgmw_flattened")
    print(f"K                    : {args.K}")
    print(f"n                    : {args.n}")
    print(f"c                    : {args.c}")
    print(f"h                    : {h}")
    print(f"q                    : {q}")
    print(f"flattened_positions  : {total_flattened_positions}")
    print(f"point_chunk          : {args.point_chunk}")
    print(f"mask_nbytes          : {mask_nbytes}")
    print(f"max_abs_bit_length   : {max_abs_bitlen}")
    print(f"total_nonzero_digits : {int(total_nonzero_digits)}")
    print(f"C_naive              : {int(C_naive)}")
    print(f"C_group              : {int(C_group)}")
    print(f"group_count          : {int(group_count)}")
    print(f"average_group_size   : {average_group_size:.8f}")
    print(f"max_group_size       : {int(max_group_size)}")
    print(f"saving_ratio         : {saving_ratio:.8f}")
    print(
        "reuse_factor         : "
        + ("null" if reuse_factor is None else f"{reuse_factor:.8f}")
    )
    print(f"json_saved_to        : {os.path.abspath(args.out)}")


if __name__ == "__main__":
    main()
