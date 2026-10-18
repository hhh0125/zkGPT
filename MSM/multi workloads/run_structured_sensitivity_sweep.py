"""
Run a small structured-vs-IID sensitivity sweep for MSM mask reuse.

The purpose is not to claim that a synthetic matrix is real.  It is to measure
how much exact-mask reuse depends on structural assumptions that are absent from
marginal-only IID generation.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, List


def parse_csv_list(text: str) -> List[str]:
    return [x.strip() for x in text.split(",") if x.strip()]


def parse_int_list(text: str) -> List[int]:
    return [int(x.strip()) for x in text.split(",") if x.strip()]


def parse_float_list(text: str) -> List[float]:
    return [float(x.strip()) for x in text.split(",") if x.strip()]


def parse_args() -> argparse.Namespace:
    here = Path(__file__).resolve().parent
    p = argparse.ArgumentParser(
        description="Structured scalar sensitivity sweep for tile-mask MSM reuse."
    )
    p.add_argument("--summary", required=True)
    p.add_argument("--out-root", required=True)
    p.add_argument("--workloads", default="CNN,NanoGPT,SHA256,zkEmail,Aggregation,Sinsemilla,zkEVM-tx")
    p.add_argument("--K", type=int, default=512)
    p.add_argument("--n", type=int, default=512)
    p.add_argument("--scalar-format", default="bn254_centered")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--c-start", type=int, default=8)
    p.add_argument("--c-end", type=int, default=12)
    p.add_argument("--base-tile-size", type=int, default=32)
    p.add_argument("--point-chunk", type=int, default=256)
    p.add_argument("--min-gain", type=int, default=1)
    p.add_argument("--max-rounds", type=int, default=0)
    p.add_argument("--column-group-sizes", default="1,2,4,8")
    p.add_argument("--structured-ratios", default="0,0.25,0.5,0.75")
    p.add_argument("--template-noises", default="0.05")
    p.add_argument("--row-phase-sizes", default="64,128")
    p.add_argument("--template-count", type=int, default=64)
    p.add_argument("--random-template-ratio", type=float, default=0.15)
    p.add_argument("--generator", default=str(here / "generate_structured_profiled_scalars.py"))
    p.add_argument("--analyzer", default=str(here / "tilemask_multiscale_optimized_scalar_formats.py"))
    p.add_argument("--keep-scalars", action="store_true")
    p.add_argument("--dry-run", action="store_true")
    return p.parse_args()


def run_command(command: List[str]) -> None:
    print()
    print("$ " + " ".join(command), flush=True)
    subprocess.run(command, check=True)


def read_best_summary(summary_csv: Path) -> Dict[str, str]:
    with summary_csv.open("r", encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise ValueError(f"Empty summary CSV: {summary_csv}")
    return max(rows, key=lambda r: float(r["saving_ratio"]))


def remove_scalar(path: Path) -> None:
    path.unlink(missing_ok=True)
    Path(str(path) + ".meta.json").unlink(missing_ok=True)


def safe_name(text: str) -> str:
    return (
        text.replace("/", "_")
        .replace("\\", "_")
        .replace(" ", "_")
        .replace(".", "p")
        .replace("-", "_")
    )


def main() -> None:
    args = parse_args()
    out_root = Path(args.out_root)
    out_root.mkdir(parents=True, exist_ok=True)

    workloads = parse_csv_list(args.workloads)
    group_sizes = parse_int_list(args.column_group_sizes)
    structured_ratios = parse_float_list(args.structured_ratios)
    template_noises = parse_float_list(args.template_noises)
    row_phase_sizes = parse_int_list(args.row_phase_sizes)

    summary_rows: List[Dict[str, object]] = []
    run_id = 0
    for workload_idx, workload in enumerate(workloads):
        for group_size in group_sizes:
            for structured_ratio in structured_ratios:
                for template_noise in template_noises:
                    for row_phase_size in row_phase_sizes:
                        if structured_ratio == 0.0 and (
                            group_size != group_sizes[0] or row_phase_size != row_phase_sizes[0]
                        ):
                            continue
                        run_id += 1
                        seed = args.seed + workload_idx * 10_000 + run_id
                        case = (
                            f"{safe_name(workload)}_g{group_size}_sr{structured_ratio:.2f}"
                            f"_noise{template_noise:.2f}_phase{row_phase_size}_seed{seed}"
                        )
                        scalar_path = out_root / f"{case}.npy"
                        result_dir = out_root / f"results_{case}"

                        generator_command = [
                            sys.executable,
                            args.generator,
                            "--summary",
                            args.summary,
                            "--workload",
                            workload,
                            "--K",
                            str(args.K),
                            "--n",
                            str(args.n),
                            "--output",
                            str(scalar_path),
                            "--scalar-format",
                            args.scalar_format,
                            "--seed",
                            str(seed),
                            "--structured-column-ratio",
                            str(structured_ratio),
                            "--column-group-size",
                            str(group_size),
                            "--template-count",
                            str(args.template_count),
                            "--template-noise",
                            str(template_noise),
                            "--row-phase-size",
                            str(row_phase_size),
                            "--random-template-ratio",
                            str(args.random_template_ratio),
                            "--overwrite",
                        ]
                        analyzer_command = [
                            sys.executable,
                            args.analyzer,
                            "--input",
                            str(scalar_path),
                            "--scalar-format",
                            args.scalar_format,
                            "--K",
                            str(args.K),
                            "--n",
                            str(args.n),
                            "--c-start",
                            str(args.c_start),
                            "--c-end",
                            str(args.c_end),
                            "--tile-size",
                            "all",
                            "--base-tile-size",
                            str(args.base_tile_size),
                            "--solver",
                            "multiscale",
                            "--multiscale-decision",
                            "compare_nosplit_refine",
                            "--build-engine",
                            "sparse",
                            "--max-rounds",
                            str(args.max_rounds),
                            "--min-gain",
                            str(args.min_gain),
                            "--point-chunk",
                            str(args.point_chunk),
                            "--out-dir",
                            str(result_dir),
                        ]

                        if args.dry_run:
                            print(" ".join(generator_command))
                            print(" ".join(analyzer_command))
                            continue

                        try:
                            run_command(generator_command)
                            run_command(analyzer_command)
                            tile_label = "all"
                            summary_csv = result_dir / (
                                f"tilemask_multiscale_sparse_summary_c{args.c_start}_to_c{args.c_end}"
                                f"_t{tile_label}_base{args.base_tile_size}.csv"
                            )
                            best = read_best_summary(summary_csv)
                            meta = json.loads(Path(str(scalar_path) + ".meta.json").read_text(encoding="utf-8"))
                            summary_rows.append(
                                {
                                    "workload": workload,
                                    "case": case,
                                    "K": args.K,
                                    "n": args.n,
                                    "structured_column_ratio": structured_ratio,
                                    "column_group_size": group_size,
                                    "template_noise": template_noise,
                                    "row_phase_size": row_phase_size,
                                    "random_template_ratio": args.random_template_ratio,
                                    "seed": seed,
                                    "best_c": best["c"],
                                    "best_saving_ratio": best["saving_ratio"],
                                    "best_saving_percent": float(best["saving_ratio"]) * 100.0,
                                    "C_naive": best["C_naive"],
                                    "C_group": best["C_group"],
                                    "accepted_submask_count": best["accepted_submask_count"],
                                    "average_submask_popcount": best["average_submask_popcount"],
                                    "average_group_r": best["average_group_r"],
                                    "max_gain": best["max_gain"],
                                    "max_group_r": best["max_group_r"],
                                    "zero_rate": best["zero_rate"],
                                    "realized_zero_ratio": meta["realized"]["zero_ratio"],
                                    "realized_nonzero_nonone_ratio": meta["realized"]["nonzero_nonone_ratio"],
                                    "result_summary_csv": str(summary_csv.resolve()),
                                }
                            )
                        finally:
                            if not args.keep_scalars:
                                remove_scalar(scalar_path)

    if args.dry_run:
        return

    out_csv = out_root / "structured_sensitivity_summary.csv"
    if summary_rows:
        with out_csv.open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(summary_rows[0].keys()))
            writer.writeheader()
            writer.writerows(summary_rows)
    print()
    print(f"Wrote summary: {out_csv.resolve()}")


if __name__ == "__main__":
    main()
