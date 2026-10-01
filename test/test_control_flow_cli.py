"""Exercise the actual CLI return values, resource limits and failure exit codes."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
IEUM = ROOT / "build" / "ieum"


class ControlFlowCliTests(unittest.TestCase):
    def invoke(self, source, *options):
        return subprocess.run([str(IEUM), str(source), *options], capture_output=True,
                              text=True, encoding="utf-8", timeout=20)

    def temporary_source(self, source, *options):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "control.ieum"
            path.write_text(source, encoding="utf-8")
            result = self.invoke(path, *options)
            return result

    def test_working_example(self):
        result = self.invoke(ROOT / "examples/control_flow.ieum", "--run", "app.main")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for value in ("return app.main: int = 175", "value app.main.sum: int = 55",
                      "value app.main.fact: int = 120", "value app.main.oddSum: int = 25"):
            self.assertIn(value, result.stdout)

    def test_invalid_limit_options(self):
        source = ROOT / "examples/control_flow.ieum"
        for flag in ("--max-steps", "--max-call-depth"):
            for value in ("0", "-1", "+1", "1.5", "abc", "", "184467440737095516160"):
                with self.subTest(flag=flag, value=value):
                    self.assertEqual(self.invoke(source, "--run", "app.main", flag, value).returncode, 2)
            with self.subTest(flag=flag, invalid="duplicate"):
                self.assertEqual(self.invoke(source, "--run", "app.main", flag, "10", flag, "20").returncode, 2)
            with self.subTest(flag=flag, invalid="missing value"):
                self.assertEqual(self.invoke(source, "--run", "app.main", flag).returncode, 2)
            with self.subTest(flag=flag, invalid="without run"):
                self.assertEqual(self.invoke(source, flag, "10").returncode, 2)

    def test_limit_order_and_graph_are_compatible(self):
        with tempfile.TemporaryDirectory() as directory:
            graph = Path(directory) / "out.dot"
            result = self.invoke(ROOT / "examples/control_flow.ieum", "--max-steps", "10000",
                                 "--run", "app.main", "--emit-dot", str(graph), "--max-call-depth", "20")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('"app" -> "math"', graph.read_text(encoding="utf-8"))

    def test_infinite_loop_stops_with_location(self):
        result = self.temporary_source("module app {\nfn main() {\nwhile true {}\n}\n}\n",
                                       "--run", "app.main", "--max-steps", "40")
        self.assertEqual(result.returncode, 1)
        self.assertIn("step_limit", result.stdout)
        self.assertIn("control.ieum:3:", result.stdout)
        self.assertNotIn("functions_executed=", result.stdout)

    def test_infinite_recursion_stops_with_location(self):
        result = self.temporary_source("module app {\nfn main() {\ncall main()\n}\n}\n",
                                       "--run", "app.main", "--max-call-depth", "8")
        self.assertEqual(result.returncode, 1)
        self.assertIn("call_depth_limit", result.stdout)
        self.assertIn("control.ieum:3:", result.stdout)

    def test_validation_does_not_execute(self):
        result = self.temporary_source("module app {\nfn main() {\nwhile true {}\n}\n}\n")
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("step_limit", result.stdout)
        self.assertNotIn("enter app.main", result.stdout)

    def test_missing_return_stops_before_execution(self):
        result = self.temporary_source("module app {\nfn main() -> int {}\n}\n", "--run", "app.main")
        self.assertEqual(result.returncode, 1)
        self.assertIn("missing_return", result.stdout)
        self.assertNotIn("enter app.main", result.stdout)

    def test_legacy_unit_trace_unchanged(self):
        result = self.invoke(ROOT / "examples/execution.ieum", "--run", "service.main")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("functions_executed=3, calls_executed=2", result.stdout)
        self.assertNotIn("return service.main", result.stdout)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ieum", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    IEUM = options.ieum.resolve()
    unittest.main(argv=[__file__, *remaining])
