#!/usr/bin/env python3
"""
根据 workload 统计分布生成独立随机 scalar，并运行 tile-mask 分析。

每个 scalar a[k, i] 独立随机抽样，不复制完整 scalar 列。
生成器固定使用：

    --column-group-size 1

输出示例：

    SHA256_random.npy
    results_SHA256_random/

    CNN_random.npy
    results_CNN_random/

建议先使用 K=n=4096。
K=n=16384 时，一个四 limb scalar 文件约为 8 GiB。
"""

from __future__ import annotations

import argparse
import csv
import shlex
import subprocess
import sys
from pathlib import Path


def parse_csv_list(text: str) -> list[str]:
    """解析逗号分隔的字符串列表。"""
    return [item.strip() for item in text.split(",") if item.strip()]


def parse_args() -> argparse.Namespace:
    here = Path(__file__).resolve().parent

    parser = argparse.ArgumentParser(
        description=(
            "Generate independently sampled workload-shaped scalar matrices "
            "and run tile-mask analysis."
        )
    )

    parser.add_argument(
        "--summary",
        required=True,
        help="Path to vanilla3_workloads_advice_width_summary.csv",
    )
    parser.add_argument(
        "--generator",
        default=str(here / "generate_profiled_scalars.py"),
        help="Path to generate_profiled_scalars.py",
    )
    parser.add_argument(
        "--analyzer",
        default=str(here / "tilemask_multiscale_optimized_scalar_formats.py"),
        help="Path to the tile-mask analyzer",
    )
    parser.add_argument(
        "--out-root",
        required=True,
        help="Root directory for generated scalar files and analysis results",
    )

    parser.add_argument(
        "--K",
        type=int,
        default=4096,
        help="Number of MSMs",
    )
    parser.add_argument(
        "--n",
        type=int,
        default=4096,
        help="Number of points in each MSM",
    )
    parser.add_argument(
        "--workloads",
        default="all",
        help=(
            "Comma-separated workload names, or 'all'. "
            "Example: SHA256,CNN,NanoGPT"
        ),
    )

    parser.add_argument(
        "--scalar-format",
        default="bn254_centered",
        choices=[
            "bn254_centered",
            "int128_twos_complement",
            "int256_twos_complement",
            "signed_magnitude_256",
        ],
        help="Scalar encoding format",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=1,
        help=(
            "Base random seed. Each workload receives a different seed "
            "derived from this value."
        ),
    )

    parser.add_argument(
        "--c-start",
        type=int,
        default=8,
        help="First Pippenger window width",
    )
    parser.add_argument(
        "--c-end",
        type=int,
        default=16,
        help="Last Pippenger window width",
    )
    parser.add_argument(
        "--base-tile-size",
        type=int,
        default=32,
        help="Smallest tile size in multiscale analysis",
    )
    parser.add_argument(
        "--point-chunk",
        type=int,
        default=512,
        help="Point-column chunk size",
    )
    parser.add_argument(
        "--min-gain",
        type=int,
        default=1,
        help="Minimum exact-mask grouping gain",
    )
    parser.add_argument(
        "--max-rounds",
        type=int,
        default=0,
        help=(
            "Residual splitting rounds. Use 0 for exact-mask no-split analysis."
        ),
    )

    parser.add_argument(
        "--keep-scalars",
        action="store_true",
        help="Keep generated .npy scalar files after analysis",
    )
    parser.add_argument(
        "--no-profile",
        action="store_true",
        help="Do not print per-window analyzer profiling output",
    )

    return parser.parse_args()


def load_all_workloads(summary_path: Path) -> list[str]:
    """从 summary CSV 中读取所有 workload 名称。"""
    workloads: list[str] = []

    with summary_path.open(
        "r",
        encoding="utf-8-sig",
        newline="",
    ) as file:
        reader = csv.DictReader(file)

        if reader.fieldnames is None or "workload" not in reader.fieldnames:
            raise ValueError(
                f"Summary CSV does not contain a 'workload' column: "
                f"{summary_path}"
            )

        for row in reader:
            workload = str(row.get("workload", "")).strip()
            if workload:
                workloads.append(workload)

    if not workloads:
        raise ValueError(
            f"No workload entries found in summary CSV: {summary_path}"
        )

    return workloads


def sanitize_filename(name: str) -> str:
    """把 workload 名称转换为安全文件名。"""
    safe = name.strip()

    for character in ["/", "\\", " ", ":", ";", "\t"]:
        safe = safe.replace(character, "_")

    return safe


def run_command(command: list[str]) -> None:
    """打印并执行一个子进程命令。"""
    print()
    print("$ " + shlex.join(command), flush=True)
    subprocess.run(command, check=True)


def validate_args(args: argparse.Namespace) -> None:
    if args.K <= 0:
        raise ValueError("--K must be positive")

    if args.n <= 0:
        raise ValueError("--n must be positive")

    if args.c_start <= 0 or args.c_end <= 0:
        raise ValueError("--c-start and --c-end must be positive")

    if args.c_start > args.c_end:
        raise ValueError("--c-start must be <= --c-end")

    if args.base_tile_size <= 0:
        raise ValueError("--base-tile-size must be positive")

    if args.K % args.base_tile_size != 0:
        raise ValueError(
            "--K must be divisible by --base-tile-size when "
            "--tile-size all is used"
        )

    if args.point_chunk <= 0:
        raise ValueError("--point-chunk must be positive")

    if args.min_gain < 0:
        raise ValueError("--min-gain must be non-negative")

    if args.max_rounds < 0:
        raise ValueError("--max-rounds must be non-negative")


def remove_generated_scalar(scalar_path: Path) -> None:
    """删除生成的 scalar 文件及其 metadata。"""
    metadata_path = Path(str(scalar_path) + ".meta.json")

    scalar_path.unlink(missing_ok=True)
    metadata_path.unlink(missing_ok=True)


def main() -> None:
    args = parse_args()
    validate_args(args)

    summary_path = Path(args.summary).expanduser().resolve()
    generator_path = Path(args.generator).expanduser().resolve()
    analyzer_path = Path(args.analyzer).expanduser().resolve()
    out_root = Path(args.out_root).expanduser().resolve()

    if not summary_path.is_file():
        raise FileNotFoundError(
            f"Summary CSV does not exist: {summary_path}"
        )

    if not generator_path.is_file():
        raise FileNotFoundError(
            f"Scalar generator does not exist: {generator_path}"
        )

    if not analyzer_path.is_file():
        raise FileNotFoundError(
            f"Tile-mask analyzer does not exist: {analyzer_path}"
        )

    out_root.mkdir(parents=True, exist_ok=True)

    available_workloads = load_all_workloads(summary_path)

    if args.workloads.strip().lower() == "all":
        workloads = available_workloads
    else:
        workloads = parse_csv_list(args.workloads)

        unknown_workloads = [
            workload
            for workload in workloads
            if workload not in available_workloads
        ]

        if unknown_workloads:
            raise ValueError(
                "The following workloads are not present in the summary CSV: "
                + ", ".join(unknown_workloads)
                + "\nAvailable workloads: "
                + ", ".join(available_workloads)
            )

    scalar_size_gib = (
        args.K
        * args.n
        * 4
        * 8
        / (1024 ** 3)
    )

    print(f"Summary CSV       : {summary_path}")
    print(f"Generator         : {generator_path}")
    print(f"Analyzer          : {analyzer_path}")
    print(f"Output root       : {out_root}")
    print(f"K                 : {args.K}")
    print(f"n                 : {args.n}")
    print(f"Workloads         : {', '.join(workloads)}")
    print(f"Scalar format     : {args.scalar_format}")
    print(f"Scalar generation : independent IID sampling")
    print(f"Column group size : 1")
    print(
        f"Each scalar file  : approximately "
        f"{scalar_size_gib:.3f} GiB"
    )

    total_cases = len(workloads)

    for workload_index, workload in enumerate(workloads):
        safe_workload = sanitize_filename(workload)

        case_name = f"{safe_workload}_random"
        scalar_path = out_root / f"{case_name}.npy"
        result_dir = out_root / f"results_{case_name}"

        # 每个 workload 使用不同 seed，但同一次运行仍然可复现。
        workload_seed = args.seed + workload_index

        print()
        print("=" * 78)
        print(
            f"[{workload_index + 1}/{total_cases}] "
            f"Processing workload: {workload}"
        )
        print(f"Random seed       : {workload_seed}")
        print(f"Scalar output     : {scalar_path}")
        print(f"Result directory  : {result_dir}")
        print("=" * 78)

        generator_command = [
            sys.executable,
            str(generator_path),
            "--summary",
            str(summary_path),
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
            str(workload_seed),

            # 关键设置：
            # 每个 point column 独立生成，不复制完整 scalar 列。
            "--column-group-size",
            "1",

            "--overwrite",
        ]

        run_command(generator_command)

        analyzer_command = [
            sys.executable,
            str(analyzer_path),
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

        if not args.no_profile:
            analyzer_command.append("--profile")

        try:
            run_command(analyzer_command)
        finally:
            if not args.keep_scalars:
                print()
                print(f"Removing generated scalar file: {scalar_path}")
                remove_generated_scalar(scalar_path)

    print()
    print("=" * 78)
    print("All workload cases completed.")
    print(f"Results are stored under: {out_root}")
    print("=" * 78)


if __name__ == "__main__":
    main()