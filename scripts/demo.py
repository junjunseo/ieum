"""Replay normal -> rejected -> fixed, checking that rejection prevents execution."""
import argparse
import json
from pathlib import Path
import sys

from workflow_support import ROOT, default_ieum, executable, load_cases, run_case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ieum", default=default_ieum())
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--output", type=Path, default=ROOT / "build/demo/results.json")
    args = parser.parse_args()
    if not 1 <= args.repeat <= 100:
        parser.error("--repeat must be between 1 and 100")
    ieum = executable(args.ieum)
    cases = {c["id"]: c for c in load_cases()}
    results = []
    for round_number in range(1, args.repeat + 1):
        for name in ("demo_valid", "demo_violation", "demo_fixed"):
            result = run_case(ieum, cases[name], ("--run", "ui.main"))
            output = result["stdout"]
            execution_ok = (
                "── 실행 Trace ──" not in output and "enter " not in output
                if name == "demo_violation" else
                "functions_executed=3, calls_executed=2" in output
                and "enter ui.main" in output and "exit ui.main" in output
            )
            result.update(round=round_number, execution_ok=execution_ok)
            results.append(result)
            print(f"Round {round_number}: {name}")
            print(output)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    passed = all(r["exact_match"] and r["execution_ok"] for r in results)
    print(f"Demo: {'PASS' if passed else 'FAIL'} ({args.repeat} rounds)")
    return int(not passed)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"Demo error: {error}", file=sys.stderr)
        sys.exit(2)
