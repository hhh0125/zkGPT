"""Generate canonical fixtures, then verify each in a fresh witness-free process."""
import json
from pathlib import Path
import subprocess
import sys


def main():
    generator, verifier, output = sys.argv[1:]
    directory = Path(output)
    generated = subprocess.run([generator, output], capture_output=True, text=True, timeout=180)
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "generator.log").write_text(generated.stdout + generated.stderr)
    if generated.returncode:
        print(generated.stdout + generated.stderr)
        return 1
    results = []
    for row in (directory / "manifest.tsv").read_text().splitlines():
        name, expected, category = row.split("\t")
        process = subprocess.run(
            [verifier, str(directory / (name + ".instance")), str(directory / (name + ".lasso"))],
            capture_output=True, text=True, timeout=20,
        )
        passed = process.returncode == int(expected)
        results.append(dict(case=name, category=category, expected_exit=int(expected),
                            actual_exit=process.returncode, passed=passed,
                            stdout=process.stdout, stderr=process.stderr))
        print(("PASS " if passed else "FAIL ") + name)
    (directory / "results.json").write_text(json.dumps(results, indent=2))
    print(f"{sum(item['passed'] for item in results)}/{len(results)} independent verifier cases")
    return 0 if all(item["passed"] for item in results) else 1


if __name__ == "__main__":
    sys.exit(main())
