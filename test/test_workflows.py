"""Check evaluation failure handling and published graph provenance without Graphviz."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from evaluate import summarize
from workflow_support import MANIFEST, default_ieum, executable, load_cases, run_case, text_digest

IEUM = default_ieum()


class WorkflowTests(unittest.TestCase):
    def result_for(self, output, code=1):
        case = next(c for c in load_cases() if c["id"] == "cyclic_dependency")
        with patch("workflow_support.subprocess.run", return_value=subprocess.CompletedProcess([], code, output, "")):
            return run_case("unused", case)

    def test_parse_error_is_not_a_true_positive(self):
        result = self.result_for("Parser error: expected module\n")
        summary = summarize([result])
        self.assertFalse(result["exact_match"])
        self.assertEqual(summary["true_positive"], 0)
        self.assertEqual(summary["unclassified"], 1)
        self.assertIsNone(summary["accuracy_on_classified"])

    def test_semantic_error_is_not_a_structural_detection(self):
        result = self.result_for("✓ 구조 검사 통과: 위반 없음\n✗ 의미 검사 실패: 위반 1건\n  [미정의 함수] missing\n")
        self.assertFalse(result["classified"])

    def test_incomplete_diagnostics_are_unclassified(self):
        result = self.result_for("✗ 구조 검사 실패: 위반 2건\n  [순환 의존] a -> b -> a\n")
        self.assertFalse(result["classified"])

    def test_wrong_rule_is_not_an_exact_match(self):
        result = self.result_for("✗ 구조 검사 실패: 위반 1건\n  [계층 위반] data -> ui\n")
        self.assertTrue(result["classified"])
        self.assertFalse(result["exact_match"])
        self.assertEqual(summarize([result])["true_positive"], 1)

    def test_timeout_is_unclassified(self):
        with patch("workflow_support.subprocess.run", side_effect=subprocess.TimeoutExpired("ieum", 30)):
            result = run_case("unused", load_cases()[0])
        self.assertFalse(result["classified"])
        self.assertIsNone(result["actual_exit"])

    def test_manifest_rejects_inconsistent_expectations(self):
        case = dict(load_cases()[0], expected_exit=1)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "manifest.json"
            path.write_text(json.dumps({"schema_version": 1, "cases": [case]}), encoding="utf-8")
            with self.assertRaises(ValueError):
                load_cases(path)

    def test_classification_confusion_matrix(self):
        results = [{"classified": True, "expected_exit": expected, "actual_exit": actual,
                    "exact_match": expected == actual} for expected, actual in [(0, 0), (0, 1), (1, 0), (1, 1)]]
        summary = summarize(results)
        self.assertEqual([summary[k] for k in ("true_positive", "true_negative", "false_positive", "false_negative")], [1, 1, 1, 1])
        self.assertEqual(summary["false_positive_rate"], 0.5)
        self.assertEqual(summary["false_negative_rate"], 0.5)

    def test_published_graphs_match_sources_and_checker(self):
        provenance = json.loads((ROOT / "docs/graphs/provenance.json").read_text(encoding="utf-8"))
        records = {r["id"]: r for r in provenance["graphs"]}
        self.assertEqual(provenance["manifest_sha256"], text_digest(MANIFEST))
        with tempfile.TemporaryDirectory() as temp:
            for case in (c for c in load_cases() if "graph" in c):
                with self.subTest(case=case["id"]):
                    name = case["id"]
                    dot = Path(temp) / f"{name}.dot"
                    result = run_case(IEUM, case, ("--emit-dot", str(dot)))
                    self.assertTrue(result["exact_match"])
                    self.assertEqual(result["source_sha256"], records[name]["source_sha256"])
                    self.assertEqual(text_digest(dot), records[name]["dot_sha256"])
                    self.assertEqual(text_digest(ROOT / f"docs/graphs/{name}.dot"), records[name]["dot_sha256"])
                    svg_path = ROOT / f"docs/graphs/{name}.svg"
                    self.assertEqual(text_digest(svg_path), records[name]["svg_sha256"])
                    svg = ET.parse(svg_path).getroot()
                    self.assertEqual(svg.get("aria-label"), case["graph"]["title"])
                    edges = [e for e in svg.iter() if e.get("class") == "edge"]
                    self.assertTrue(edges)
                    self.assertTrue(all(e.get("data-kind") in ("dependency", "layer") for e in edges))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--ieum", default=default_ieum())
    args, rest = parser.parse_known_args()
    IEUM = executable(args.ieum)
    unittest.main(argv=[sys.argv[0], *rest])
