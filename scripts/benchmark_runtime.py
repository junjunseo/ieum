"""Record execution-only measurements; never compare these times with structural checks."""
import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys

from workflow_support import ROOT, digest, executable, text_digest

CASES = (("loop", 100), ("loop", 1000), ("loop", 10000),
         ("recursion", 20), ("recursion", 200),
         ("collections", 100), ("collections", 1000))


def measurement(benchmark, scenario, size, iterations):
    process = subprocess.run([str(benchmark), scenario, str(size), str(iterations)],
                             capture_output=True, encoding="utf-8", check=True, timeout=120)
    fields = dict(line.split("=", 1) for line in process.stdout.splitlines() if "=" in line)
    result = {key: int(fields[key]) for key in ("size", "iterations", "warmups", "expected", "steps")}
    result.update({key: fields[key] for key in ("scenario", "compiler", "ndebug")})
    result.update({key: float(fields[key]) for key in ("min_ms", "median_ms", "p95_ms", "max_ms")})
    samples = result["samples_ms"] = [float(value) for value in fields["samples_ms"].split(",")]
    expected = size * (size + 1) // 2 if scenario == "loop" else size if scenario == "recursion" else 60 * size
    if (result["scenario"] != scenario or result["size"] != size or result["iterations"] != iterations
            or result["warmups"] != 1 or result["expected"] != expected or result["steps"] <= 0
            or len(samples) != iterations or any(not math.isfinite(v) or v < 0 for v in samples)):
        raise ValueError("Invalid runtime benchmark metadata or samples")
    summaries = {"min_ms": min(samples), "median_ms": statistics.median(samples),
                 "p95_ms": sorted(samples)[math.ceil(iterations * 0.95) - 1], "max_ms": max(samples)}
    if any(not math.isclose(result[key], value, abs_tol=0.000002) for key, value in summaries.items()):
        raise ValueError("Runtime summary disagrees with raw samples")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", required=True)
    parser.add_argument("--iterations", type=int, default=11)
    parser.add_argument("--build-label", required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build/evaluation/runtime.json")
    args = parser.parse_args()
    if not 1 <= args.iterations <= 1000:
        parser.error("--iterations must be between 1 and 1000")
    benchmark = executable(args.benchmark)
    results = [measurement(benchmark, scenario, size, args.iterations) for scenario, size in CASES]
    report = {
        "created_at": datetime.now(timezone.utc).isoformat(),
        "scope": "Interpreter::run only; includes machine/frames, module initialization, trace/result allocation; excludes lexer/parser/checker/semantic, source I/O, CLI startup/printing, result validation/destruction.",
        "environment": {"os": platform.platform(), "processor": platform.processor(),
                        "logical_cpus": os.cpu_count(), "python": platform.python_version()},
        "build_label": args.build_label, "benchmark_sha256": digest(benchmark),
        "implementation_sha256": {str(p.relative_to(ROOT)).replace("\\", "/"): text_digest(p)
                                  for p in [ROOT / "benchmark/benchmarkRuntime.cpp", *sorted((ROOT / "src").glob("*.h"))]},
        "cases": results,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for result in results:
        print(f"{result['scenario']}/{result['size']}: median={result['median_ms']:.6f} ms")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print(f"Runtime benchmark error: {error}", file=sys.stderr)
        sys.exit(2)
