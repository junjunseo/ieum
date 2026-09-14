"""Evaluate the hand-authored seed corpus; deliberately excludes timing."""
import argparse
from datetime import datetime, timezone
import json
import platform
import sys
from pathlib import Path

from workflow_support import MANIFEST, ROOT, default_ieum, digest, executable, load_cases, run_case, text_digest


def summarize(results):
    tp = tn = fp = fn = 0
    for result in results:
        if not result["classified"]:
            continue
        expected, actual = result["expected_exit"] == 1, result["actual_exit"] == 1
        tp += expected and actual
        tn += not expected and not actual
        fp += not expected and actual
        fn += expected and not actual
    total = len(results)
    classified = tp + tn + fp + fn
    ratio = lambda numerator, denominator: numerator / denominator if denominator else None
    return {
        "total": total, "classified": classified, "unclassified": total - classified,
        "exact_matches": sum(r["exact_match"] for r in results),
        "true_positive": tp, "true_negative": tn, "false_positive": fp, "false_negative": fn,
        "classification_coverage": ratio(classified, total),
        "accuracy_on_classified": ratio(tp + tn, classified),
        "false_positive_rate": ratio(fp, fp + tn),
        "false_negative_rate": ratio(fn, fn + tp),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ieum", default=default_ieum())
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--output", type=Path, default=ROOT / "build/evaluation/results.json")
    args = parser.parse_args()
    ieum = executable(args.ieum)
    results = [run_case(ieum, case) for case in load_cases(args.manifest)]
    summary = summarize(results)
    report = {
        "scope": "Hand-authored seed corpus; not independent real-project accuracy.",
        "created_at": datetime.now(timezone.utc).isoformat(),
        "environment": {"os": platform.platform(), "python": platform.python_version()},
        "executable_sha256": digest(ieum), "manifest_sha256": text_digest(args.manifest),
        "summary": summary, "cases": results,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for result in results:
        print(f"{'PASS' if result['exact_match'] else 'FAIL'} {result['id']}")
    print(json.dumps(summary, indent=2))
    print(f"Report: {args.output}")
    return int(summary["exact_matches"] != summary["total"])


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"Evaluation error: {error}", file=sys.stderr)
        sys.exit(2)
