"""Fresh witness-free verifier per case, exact exit code and measured peak RSS."""
import csv
import json
from pathlib import Path
import platform
import subprocess
import sys
import time


def main():
    generator, verifier, output = sys.argv[1:]
    directory = Path(output)
    directory.mkdir(parents=True, exist_ok=True)
    generated = subprocess.run([generator, output], capture_output=True, text=True, timeout=180)
    (directory / "generator.log").write_text(generated.stdout + generated.stderr)
    if generated.returncode:
        print(generated.stdout + generated.stderr)
        return 1
    results = []
    for name, expected, category in csv.reader((directory / "manifest.tsv").open(), delimiter="\t"):
        rss = directory / (name + ".rss")
        started = time.perf_counter()
        process = subprocess.run(
            ["/usr/bin/time", "-f", "%M", "-o", str(rss), verifier,
             str(directory / (name + ".instance")), str(directory / (name + ".b2"))],
            capture_output=True, text=True, timeout=30)
        elapsed = (time.perf_counter() - started) * 1000
        # time emits an exit-status diagnostic before the final RSS on rejected cases.
        peak = int(rss.read_text().splitlines()[-1])
        passed = process.returncode == int(expected)
        results.append(dict(case=name, category=category, expected_exit=int(expected),
                            actual_exit=process.returncode, passed=passed, elapsed_ms=elapsed,
                            peak_rss_kib=peak, stdout=process.stdout, stderr=process.stderr))
        print(("PASS " if passed else "FAIL ") + name)
    data = dict(environment=dict(platform=platform.platform(), python=platform.python_version()),
                generator_exit=generated.returncode, cases=results,
                all_pass=all(item["passed"] for item in results))
    (directory / "results.json").write_text(json.dumps(data, indent=2))
    print(f"{sum(item['passed'] for item in results)}/{len(results)} independent verifier cases")
    return 0 if data["all_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
