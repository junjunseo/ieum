"""Verify benchmark correctness and fail-closed integration demo behavior."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from benchmark_runtime import measurement
from workflow_support import executable

BENCHMARK = None
IEUM = None


class LanguageQaTests(unittest.TestCase):
    def test_runtime_scenarios_have_correct_results_and_samples(self):
        for scenario in ("loop", "recursion", "collections"):
            with self.subTest(scenario=scenario):
                result = measurement(BENCHMARK, scenario, 7, 2)
                self.assertEqual(len(result["samples_ms"]), 2)

    def test_runtime_rejects_invalid_or_unbounded_work(self):
        for args in (("unknown", "1", "1"), ("loop", "0", "1"), ("loop", "10001", "1"),
                     ("loop", "1", "0"), ("loop", "1", "1001"), ("recursion", "513", "1"),
                     ("loop", "-1", "1"), ("loop", "1", "1", "extra")):
            with self.subTest(args=args):
                result = subprocess.run([str(BENCHMARK), *args], capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)

    def test_language_demo_validates_all_examples_and_io_rejections(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "results.json"
            result = subprocess.run([sys.executable, "-B", str(ROOT / "scripts/language_demo.py"),
                                     "--ieum", str(IEUM), "--output", str(report)],
                                    capture_output=True, timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            cases = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(len(cases), 8)
            self.assertTrue(all(case["passed"] for case in cases))

    def test_demo_refuses_missing_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "results.json"
            result = subprocess.run([sys.executable, "-B", str(ROOT / "scripts/language_demo.py"),
                                     "--ieum", str(Path(directory) / "missing"), "--output", str(report)],
                                    capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertFalse(report.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--ieum", required=True)
    parser.add_argument("--runtime-benchmark", required=True)
    args, rest = parser.parse_known_args()
    IEUM, BENCHMARK = executable(args.ieum), executable(args.runtime_benchmark)
    unittest.main(argv=[sys.argv[0], *rest])
