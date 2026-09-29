"""Independent extraction/oracle edge cases and executable corpus regressions."""
import argparse
from collections import deque
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from extract_imports import extract_archive, extract_modules
from prepare_corpus import MANIFEST_REAL, dag_depth, products, reference
from workflow_support import default_ieum, executable, load_cases, run_case, text_digest

IEUM = default_ieum()
BENCHMARK = ROOT / "build/benchmarkChecker.exe"


def case_graph(case):
    graph, layers = {}, set()
    for line in (ROOT / case["source"]).read_text(encoding="utf-8").splitlines():
        if line.startswith("module "):
            parts = line.split(" depends ")
            graph[parts[0].split()[1]] = set(parts[1].split(", ")) if len(parts) == 2 else set()
        if line.startswith("layer "):
            _, upper, _, lower = line.split()
            layers.add((upper, lower))
    return graph, layers


def shortest_path(graph, start, goal):
    queue, seen = deque([[start]]), {start}
    while queue:
        path = queue.popleft()
        for child in sorted(graph[path[-1]]):
            if child == goal:
                return [*path, child]
            if child not in seen:
                seen.add(child)
                queue.append([*path, child])
    raise AssertionError("No path for declared violation witness")


class RealCorpusTests(unittest.TestCase):
    def test_relative_aliases_and_package_reexports(self):
        modules = extract_modules({"__init__.py": "from . import util as u\nfrom .util import value",
                                   "util.py": "value = 1", "sub/__init__.py": "from ..util import value",
                                   "sub/client.py": "from .. import util\nfrom . import sibling\nimport pkg.util as u",
                                   "sub/sibling.py": ""}, "pkg")
        actual = {m["module"]: m["dependencies"] for m in modules}
        self.assertEqual(actual["pkg"], ["pkg.util"])
        self.assertEqual(actual["pkg.sub"], ["pkg.util"])
        self.assertEqual(actual["pkg.sub.client"], ["pkg.sub.sibling", "pkg.util"])

    def test_conditional_deferred_and_self_imports_are_preserved(self):
        source = '"from pkg import bogus"\nif TYPE_CHECKING:\n    from .b import Value\ndef f():\n    import pkg.a\n    import importlib\n    importlib.import_module("pkg.dynamic")\n'
        modules = extract_modules({"__init__.py": "", "a.py": source, "b.py": ""}, "pkg")
        record = next(m for m in modules if m["module"] == "pkg.a")
        self.assertEqual(record["dependencies"], ["pkg.a", "pkg.b"])
        self.assertEqual([r["line"] for r in record["imports"]], [3, 5, 6])

    def test_identifier_collisions_fail(self):
        with self.assertRaises(ValueError):
            extract_modules({"a__b.py": "", "a/b.py": ""}, "pkg")

    def test_relative_import_beyond_root_fails(self):
        with self.assertRaises(ValueError):
            extract_modules({"a.py": "from .. import outside"}, "pkg")

    def test_archive_hash_is_checked_before_parsing(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "source.zip"
            archive.write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                extract_archive(archive, {"id": "sample", "archive_sha256": "0" * 64})

    def test_diamond_is_not_a_cycle_and_depth_counts_nodes(self):
        graph = {"a": {"b", "c"}, "b": {"d"}, "c": {"d"}, "d": set()}
        self.assertEqual(reference(graph, [])[0], {})
        self.assertEqual(dag_depth(graph), 3)

    def test_oracle_transitive_layer_and_undefined_evidence(self):
        kinds, evidence = reference({"a": set(), "b": {"a"}, "c": {"b", "absent"}}, [("a", "c")])
        self.assertEqual(kinds, {"undefined_dependency": 1, "layer_violation": 1})
        self.assertEqual(evidence["layer_violation_pairs"], [["c", "a"]])
        self.assertEqual(evidence["undefined_edges"], [["c", "absent"]])

    def test_disjoint_simple_cycles_and_self_loop(self):
        graph = {"a": {"b"}, "b": {"a"}, "c": {"c"}}
        self.assertEqual(reference(graph, [])[0], {"cycle": 2})
        self.assertIsNone(dag_depth(graph))

    def test_complex_scc_requires_manual_expectations(self):
        with self.assertRaisesRegex(ValueError, "Complex cyclic"):
            reference({"a": {"b", "c"}, "b": {"a"}, "c": {"a"}}, [])

    def test_committed_cases_are_reproducible_and_review_pending(self):
        for relative, expected in products().items():
            self.assertEqual((ROOT / relative).read_text(encoding="utf-8"), expected, relative)
        cases = load_cases(MANIFEST_REAL)
        self.assertEqual(len(cases), 14)
        self.assertEqual(sum(c["category"] == "real_original" for c in cases), 2)
        self.assertTrue(all(c["review_status"] == "pending-human-review" for c in cases))

    def test_evaluation_reports_groups_scope_and_all_exact_results(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "results.json"
            process = subprocess.run([sys.executable, "-B", str(ROOT / "scripts/evaluate.py"), "--ieum", str(IEUM),
                                      "--manifest", str(MANIFEST_REAL), "--output", str(path)], capture_output=True, timeout=90)
            self.assertEqual(process.returncode, 0, process.stderr)
            report = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(report["summary"]["exact_matches"], 14)
            self.assertEqual(report["by_category"]["real_original"]["total"], 2)
            self.assertEqual(report["by_category"]["synthetic"]["total"], 4)
            self.assertEqual(report["review_status"], "pending-human-review")
            self.assertEqual(len(report["sources"]), 2)

    def test_saved_svg_edges_and_highlights_match_reference_witnesses(self):
        directory = ROOT / "docs/graphs/evaluation"
        provenance = json.loads((directory / "provenance.json").read_text(encoding="utf-8"))
        self.assertEqual(provenance["manifest_sha256"], text_digest(MANIFEST_REAL))
        records = {r["id"]: r for r in provenance["graphs"]}
        with tempfile.TemporaryDirectory() as temp:
            for case in (c for c in load_cases(MANIFEST_REAL) if "graph" in c):
                with self.subTest(case=case["id"]):
                    name = case["id"]
                    dot = Path(temp) / f"{name}.dot"
                    result = run_case(IEUM, case, ("--emit-dot", str(dot)))
                    self.assertTrue(result["exact_match"])
                    self.assertEqual(text_digest(dot), records[name]["dot_sha256"])
                    self.assertEqual(text_digest(directory / f"{name}.dot"), records[name]["dot_sha256"])
                    svg = directory / f"{name}.svg"
                    self.assertEqual(text_digest(svg), records[name]["svg_sha256"])
                    self.assertEqual(result["source_sha256"], records[name]["source_sha256"])
                    graph, layers = case_graph(case)
                    expected_edges = {(node, target) for node, targets in graph.items() for target in targets}
                    expected_red = set()
                    for comp in case["reference_evidence"]["cyclic_components"]:
                        expected_red.update((a, b) for a, b in expected_edges if a in comp and b in comp)
                    for lower, upper in case["reference_evidence"]["layer_violation_pairs"]:
                        path = shortest_path(graph, lower, upper)
                        expected_red.update(zip(path, path[1:]))
                    actual_edges, actual_layers, actual_red, nodes = set(), set(), set(), set()
                    for group in ET.parse(svg).getroot().iter():
                        if group.get("class") not in ("edge", "node"):
                            continue
                        title = group.find("{http://www.w3.org/2000/svg}title").text
                        if group.get("class") == "node":
                            nodes.add(title)
                            continue
                        edge = tuple(title.split("->"))
                        if group.get("data-kind") == "layer":
                            actual_layers.add(edge)
                        else:
                            actual_edges.add(edge)
                            if group.get("data-violation") == "true":
                                actual_red.add(edge)
                    self.assertEqual(nodes, set(graph))
                    self.assertEqual(actual_edges, expected_edges)
                    self.assertEqual(actual_layers, layers)
                    self.assertEqual(actual_red, expected_red)

    def test_benchmark_runner_checks_counts_samples_and_source_hashes(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "performance.json"
            process = subprocess.run([sys.executable, "-B", str(ROOT / "scripts/benchmark_corpus.py"), "--benchmark", str(BENCHMARK),
                                      "--iterations", "2", "--output", str(path)], capture_output=True, timeout=90)
            self.assertEqual(process.returncode, 0, process.stderr)
            results = json.loads(path.read_text(encoding="utf-8"))["cases"]
            self.assertEqual(len(results), 14)
            for result in results:
                self.assertEqual(len(result["samples_ms"]), 2)
                self.assertEqual(result["source_sha256"], text_digest(ROOT / result["source"]))

    def test_benchmark_rejects_bad_counts_missing_source_and_wrong_expectation(self):
        source = str(ROOT / "evaluation/real/corpus/itsdangerous_original.ieum")
        for args in [("--source", source, "0", "0"), ("--source", source, "-1", "0"),
                     ("--source", source, "1", "1"), ("--source", source, "1", "-1"),
                     ("--source", source + ".missing", "1", "0")]:
            with self.subTest(args=args):
                process = subprocess.run([str(BENCHMARK), *args], capture_output=True, timeout=10)
                self.assertEqual(process.returncode, 2)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--ieum", default=default_ieum())
    parser.add_argument("--benchmark", default=BENCHMARK)
    args, rest = parser.parse_known_args()
    IEUM, BENCHMARK = executable(args.ieum), executable(args.benchmark)
    unittest.main(argv=[sys.argv[0], *rest])
