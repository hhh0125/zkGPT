#!/usr/bin/env python3
"""
Batch-run analyze_mask_group0526.py over a parameter grid and collect results
into a single CSV file.

Default sweep:
  tile-size in {8, 16, 32, 64}
  min-gain  in {1, 2, 4, 8, 16}
  c         in {3, 4, 5, 6, 7, 8}

The analyzer's JSON top-level fields are preserved in the CSV as columns.
Nested dict/list values are JSON-encoded into a single cell so the CSV keeps
the same output content shape as the original per-run JSON, just flattened to
one row per run.
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, Iterable, List, Sequence


DEFAULT_TILE_SIZES = ( 32, 64)
DEFAULT_MIN_GAINS = (1, 2)
DEFAULT_CS = (9,10,11,12)


def parse_int_list(value: str) -> List[int]:
    parts = [part.strip() for part in value.split(",") if part.strip()]
    if not parts:
        raise argparse.ArgumentTypeError("expected a non-empty comma-separated integer list")
    try:
        return [int(part) for part in parts]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(str(exc)) from exc


def parse_args() -> argparse.Namespace:
    root_dir = Path(__file__).resolve().parent.parent
    default_script = root_dir / "analyze_mask_group0526.py"
    default_input = root_dir / "vi_0402.npy"
    default_run_dir = Path.cwd() / "sweep_analyze_mask_group0526_runs"
    default_csv = Path.cwd() / "sweep_analyze_mask_group0526_summary.csv"

    parser = argparse.ArgumentParser(
        description=(
            "Sweep analyze_mask_group0526.py over tile-size/min-gain/c and "
            "collect one CSV row per run."
        )
    )
    parser.add_argument("--script", default=str(default_script), help="Path to analyze_mask_group0526.py")
    parser.add_argument("--input", default=str(default_input), help="Path to vi_0402.npy")
    parser.add_argument("--python", default=sys.executable, help="Python executable used to launch the analyzer")
    parser.add_argument("--K", type=int, default=16384, help="Number of MSMs")
    parser.add_argument("--n", type=int, default=16384, help="Points per MSM")
    parser.add_argument("--point-chunk", type=int, default=512, help="point-chunk passed to analyzer")
    parser.add_argument(
        "--tile-sizes",
        type=parse_int_list,
        default=list(DEFAULT_TILE_SIZES),
        help="Comma-separated tile sizes, default: 8,16,32,64",
    )
    parser.add_argument(
        "--min-gains",
        type=parse_int_list,
        default=list(DEFAULT_MIN_GAINS),
        help="Comma-separated min-gain values, default: 1,2,4,8,16",
    )
    parser.add_argument(
        "--cs",
        type=parse_int_list,
        default=list(DEFAULT_CS),
        help="Comma-separated c values, default: 3,4,5,6,7,8",
    )
    parser.add_argument(
        "--run-dir",
        default=str(default_run_dir),
        help="Directory for per-run JSON/stdout/stderr artifacts",
    )
    parser.add_argument(
        "--csv",
        default=str(default_csv),
        help="Output summary CSV path",
    )
    parser.add_argument(
        "--timeout-sec",
        type=float,
        default=None,
        help="Optional timeout for each analyzer run",
    )
    parser.add_argument(
        "--stop-on-error",
        action="store_true",
        help="Stop immediately if any run fails",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned commands without executing them",
    )
    return parser.parse_args()


def json_cell(value: object) -> str:
    if value is None:
        return ""
    if isinstance(value, (str, int, float, bool)):
        return str(value)
    return json.dumps(value, ensure_ascii=False, sort_keys=True)


def ordered_columns(rows: Sequence[Dict[str, object]]) -> List[str]:
    metadata = [
        "_status",
        "_returncode",
        "_elapsed_sec",
        "_command",
        "_output_json",
        "_stdout_log",
        "_stderr_log",
    ]

    analyzer_order: List[str] = []
    seen = set(metadata)
    for row in rows:
        for key in row.keys():
            if key.startswith("_"):
                continue
            if key not in seen:
                analyzer_order.append(key)
                seen.add(key)

    return metadata + analyzer_order


def write_csv(rows: Sequence[Dict[str, object]], csv_path: Path) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        rows = []
    fieldnames = ordered_columns(rows)

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow({key: json_cell(row.get(key)) for key in fieldnames})


def build_command(
    python_exe: str,
    script_path: Path,
    input_path: Path,
    K: int,
    n: int,
    point_chunk: int,
    tile_size: int,
    min_gain: int,
    c: int,
    output_json: Path,
) -> List[str]:
    return [
        python_exe,
        str(script_path),
        "--input",
        str(input_path),
        "--K",
        str(K),
        "--n",
        str(n),
        "--c",
        str(c),
        "--point-chunk",
        str(point_chunk),
        "--tile-size",
        str(tile_size),
        "--min-gain",
        str(min_gain),
        "--out",
        str(output_json),
    ]


def run_one(
    command: List[str],
    output_json: Path,
    stdout_log: Path,
    stderr_log: Path,
    timeout_sec: float | None,
) -> Dict[str, object]:
    started = time.time()
    try:
        completed = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_sec,
            check=False,
        )
        elapsed = time.time() - started
        stdout_log.write_text(completed.stdout, encoding="utf-8")
        stderr_log.write_text(completed.stderr, encoding="utf-8")

        row: Dict[str, object] = {
            "_status": "ok" if completed.returncode == 0 else "failed",
            "_returncode": completed.returncode,
            "_elapsed_sec": elapsed,
            "_command": subprocess.list2cmdline(command),
            "_output_json": str(output_json),
            "_stdout_log": str(stdout_log),
            "_stderr_log": str(stderr_log),
        }

        if completed.returncode == 0 and output_json.exists():
            with output_json.open("r", encoding="utf-8") as f:
                payload = json.load(f)
            row.update(payload)

        return row
    except subprocess.TimeoutExpired as exc:
        elapsed = time.time() - started
        stdout = exc.stdout or ""
        stderr = exc.stderr or ""
        stdout_log.write_text(stdout, encoding="utf-8")
        stderr_log.write_text(stderr, encoding="utf-8")
        return {
            "_status": "timeout",
            "_returncode": "",
            "_elapsed_sec": elapsed,
            "_command": subprocess.list2cmdline(command),
            "_output_json": str(output_json),
            "_stdout_log": str(stdout_log),
            "_stderr_log": str(stderr_log),
        }


def main() -> None:
    args = parse_args()

    script_path = Path(args.script).resolve()
    input_path = Path(args.input).resolve()
    run_dir = Path(args.run_dir).resolve()
    csv_path = Path(args.csv).resolve()

    if not script_path.exists():
        raise FileNotFoundError(f"Analyzer script not found: {script_path}")
    if not input_path.exists():
        raise FileNotFoundError(f"Input .npy not found: {input_path}")

    run_dir.mkdir(parents=True, exist_ok=True)

    rows: List[Dict[str, object]] = []
    total_runs = len(args.tile_sizes) * len(args.min_gains) * len(args.cs)
    run_index = 0

    for tile_size in args.tile_sizes:
        for min_gain in args.min_gains:
            for c in args.cs:
                run_index += 1
                stem = f"tile{tile_size}_gain{min_gain}_c{c}"
                output_json = run_dir / f"{stem}.json"
                stdout_log = run_dir / f"{stem}.stdout.txt"
                stderr_log = run_dir / f"{stem}.stderr.txt"
                command = build_command(
                    args.python,
                    script_path,
                    input_path,
                    args.K,
                    args.n,
                    args.point_chunk,
                    tile_size,
                    min_gain,
                    c,
                    output_json,
                )

                print(f"[{run_index}/{total_runs}] {stem}")
                print("  " + subprocess.list2cmdline(command))

                if args.dry_run:
                    rows.append(
                        {
                            "_status": "dry_run",
                            "_returncode": "",
                            "_elapsed_sec": "",
                            "_command": subprocess.list2cmdline(command),
                            "_output_json": str(output_json),
                            "_stdout_log": str(stdout_log),
                            "_stderr_log": str(stderr_log),
                            "tile_size": tile_size,
                            "min_gain": min_gain,
                            "c": c,
                        }
                    )
                    continue

                row = run_one(command, output_json, stdout_log, stderr_log, args.timeout_sec)
                if "tile_size" not in row:
                    row["tile_size"] = tile_size
                if "min_gain" not in row:
                    row["min_gain"] = min_gain
                if "c" not in row:
                    row["c"] = c
                rows.append(row)

                write_csv(rows, csv_path)

                if args.stop_on_error and row["_status"] != "ok":
                    print(f"Stopping on error at {stem}. Partial CSV written to {csv_path}")
                    return

    write_csv(rows, csv_path)
    print(f"summary_csv : {csv_path}")
    print(f"run_dir     : {run_dir}")


if __name__ == "__main__":
    main()
