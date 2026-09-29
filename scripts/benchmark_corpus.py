"""Measure in-process Lexer -> Parser -> Checker work; no CLI startup/I/O timing."""
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

from prepare_corpus import MANIFEST_REAL
from workflow_support import ROOT, digest, executable, load_cases, text_digest


def measurement(command, iterations):
    process = subprocess.run([str(v) for v in command], capture_output=True, encoding="utf-8", check=True, timeout=120)
    values = dict(line.split("=", 1) for line in process.stdout.splitlines() if "=" in line)
    result = {key: int(values[key]) for key in ("modules", "layers", "dependencies", "violations", "source_bytes", "iterations", "warmups")}
    result.update({key: float(values[key]) for key in ("min_ms", "median_ms", "p95_ms", "max_ms")})
    result.update({key: values[key] for key in ("scenario", "compiler", "ndebug")})
    result["samples_ms"] = samples = [float(v) for v in values["samples_ms"].split(",")]
    if result["iterations"] != iterations or len(samples) != iterations or result["warmups"] != 1 or any(
        not math.isfinite(v) or v < 0 for v in samples
    ):
        raise ValueError("Invalid benchmark sample count or value")
    expected = {"min_ms": min(samples), "median_ms": statistics.median(samples),
                "p95_ms": sorted(samples)[math.ceil(iterations * 0.95) - 1], "max_ms": max(samples)}
    if any(not math.isclose(result[k], v, abs_tol=0.000002) for k, v in expected.items()):
        raise ValueError("Benchmark summary disagrees with raw samples")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", default=ROOT / "build" / ("benchmarkChecker.exe" if os.name == "nt" else "benchmarkChecker"))
    parser.add_argument("--manifest", type=Path, default=MANIFEST_REAL)
    parser.add_argument("--iterations", type=int, default=11)
    parser.add_argument("--include-baseline", action="store_true")
    parser.add_argument("--build-label", default="unspecified; supply the build command/options for published measurements")
    parser.add_argument("--output", type=Path, default=ROOT / "build/evaluation/performance.json")
    args = parser.parse_args()
    if not 1 <= args.iterations <= 1000:
        raise ValueError("iterations must be between 1 and 1000")
    benchmark = executable(args.benchmark)
    results = []
    for case in load_cases(args.manifest):
        source = ROOT / case["source"]
        result = measurement([benchmark, "--source", source, args.iterations, sum(case["expected_kinds"].values())], args.iterations)
        if result["source_bytes"] != source.stat().st_size or any(result[key] != value for key, value in case["size"].items()):
            raise ValueError(f"Benchmark source metadata disagrees with manifest: {case['id']}")
        results.append({"id": case["id"], "category": case["category"], "group": case["group"],
                        "source": case["source"], "source_sha256": text_digest(source), "source_bytes_sha256": digest(source),
                        "topology": case["topology"], **result})
        print(f"{case['id']}: median={result['median_ms']:.6f} ms p95={result['p95_ms']:.6f} ms")
    baseline = [measurement([benchmark, count, args.iterations], args.iterations) for count in (50, 100, 200)] if args.include_baseline else []
    report = {"created_at": datetime.now(timezone.utc).isoformat(),
              "scope": "Lexer -> Parser -> Checker, AST/diagnostic destruction and count checks; input read/metadata parse outside timer; no semantic/runtime/DOT/SVG/startup.",
              "environment": {"os": platform.platform(), "processor": platform.processor(), "logical_cpus": os.cpu_count(), "python": platform.python_version()},
              "build_label": args.build_label, "benchmark_sha256": digest(benchmark),
              "implementation_sha256": {p: text_digest(ROOT / p) for p in ["benchmark/benchmarkChecker.cpp", "src/lexer.h", "src/parser.h", "src/checker.h", "src/ast.h", "src/token.h"]},
              "manifest_sha256": text_digest(args.manifest), "cases": results, "synthetic_baseline": baseline}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Report: {args.output}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print(f"Benchmark error: {error}", file=sys.stderr)
        sys.exit(2)
