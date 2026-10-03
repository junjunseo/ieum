"""Multi-file resolution, access control and diagnostics through the real CLI."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
IEUM = ROOT / "build/ieum"

class ModulesCliTests(unittest.TestCase):
    def invoke(self, source, *options):
        return subprocess.run([str(IEUM), str(source), *map(str, options)], capture_output=True,
                              encoding="utf-8", timeout=20)

    def write(self, directory, name, source):
        path = Path(directory) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source, encoding="utf-8")
        return path

    def app(self, directory, body="return data.get()", dependencies="data"):
        return self.write(directory, "app.ieum", "module app depends " + dependencies + " {\nfn main() -> int {\n" + body + "\n}\n}\n")

    def assertError(self, result, code):
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(code, result.stdout + result.stderr)

    def test_three_file_example(self):
        path = ROOT / "examples/multifile"
        result = self.invoke(path / "app.ieum", "--module-path", path, "--run", "app.main")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("return app.main: int = 60", result.stdout)
        self.assertIn("data.Summary{total: 60, count: 3}", result.stdout)
        self.assertIn("\n60\n", result.stdout)
        self.assertIn("modules=6", result.stdout)  # shared builtins installed only once

    def test_no_implicit_surrounding_load(self):
        result = self.invoke(ROOT / "examples/multifile/app.ieum", "--run", "app.main")
        self.assertError(result, "service")
        self.assertNotIn("return app.main", result.stdout)

    def test_required_files_only_and_unicode_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            roots = Path(directory) / "모듈 😀"
            entry = self.app(directory)
            self.write(roots, "data.ieum", "module data {\nfn get() -> int {\nreturn 42\n}\n}\n")
            self.write(roots, "unused.ieum", "invalid syntax !")
            result = self.invoke(entry, "--module-path", roots, "--run", "app.main")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("return app.main: int = 42", result.stdout)

    def test_multiple_search_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / "a", Path(directory) / "b"
            entry = self.app(directory, "return service.get()", "service")
            self.write(a, "service.ieum", "module service depends data {\nfn get() -> int {\nreturn data.value\n}\n}\n")
            self.write(b, "data.ieum", "module data {\nlet value = 17\n}\n")
            result = self.invoke(entry, "--module-path", a, "--module-path", b, "--run", "app.main")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("return app.main: int = 17", result.stdout)

    def test_collision_is_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / "a", Path(directory) / "b"
            entry = self.app(directory)
            for root in (a, b):
                self.write(root, "data.ieum", "module data {}\n")
            one = self.invoke(entry, "--module-path", a, "--module-path", b)
            two = self.invoke(entry, "--module-path", b, "--module-path", a)
            self.assertError(one, "ambiguous_module_file")
            self.assertEqual(one.stdout + one.stderr, two.stdout + two.stderr)
            self.assertIn(str(a / "data.ieum"), one.stderr)
            self.assertIn(str(b / "data.ieum"), one.stderr)

    def test_normalized_duplicate_search_path(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory)
            (Path(directory) / "sub").mkdir()
            result = self.invoke(entry, "--module-path", directory, "--module-path", Path(directory) / "sub/..")
            self.assertError(result, "duplicate_path")

    def test_same_file_via_hardlink_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory)
            a, b = Path(directory) / "a", Path(directory) / "b"
            original = self.write(a, "data.ieum", "module data {}\n")
            b.mkdir()
            try:
                os.link(original, b / "data.ieum")
            except OSError as error:
                self.skipTest(f"Hardlinks unavailable: {error}")
            result = self.invoke(entry, "--module-path", a, "--module-path", b)
            self.assertError(result, "duplicate_path")

    def test_missing_and_invalid_search_roots(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory)
            for root in (Path(directory) / "missing", entry):
                with self.subTest(root=str(root)):
                    self.assertError(self.invoke(entry, "--module-path", root), "path_error")
            self.assertError(self.invoke(entry, "--module-path", directory), "module_not_found")
            self.assertEqual(self.invoke(entry, "--module-path").returncode, 2)

    def test_module_file_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory)
            self.write(directory, "data.ieum", "module wrong {}\n")
            self.assertError(self.invoke(entry, "--module-path", directory), "module_file_mismatch")

    def test_duplicate_module_declaration(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory, "return 0", "data, other")
            self.write(directory, "data.ieum", "module data {}\n")
            self.write(directory, "other.ieum", "module other {}\nmodule data {}\n")
            result = self.invoke(entry, "--module-path", directory)
            self.assertError(result, "other.ieum:2:1")
            self.assertIn("module data {}", result.stdout)

    def test_private_reference_source_context(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory, "return data.secret")
            self.write(directory, "data.ieum", "module data {\nprivate let secret = 8\n}\n")
            result = self.invoke(entry, "--module-path", directory, "--run", "app.main")
            self.assertError(result, "private_access")
            self.assertIn(str(entry) + ":3:8", result.stdout)
            self.assertIn("return data.secret", result.stdout)
            self.assertIn("^", result.stdout)

    def test_module_cycle_and_reverse_layer(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory, "return 0")
            data = self.write(directory, "data.ieum", "module data depends app\n")
            result = self.invoke(entry, "--module-path", directory)
            self.assertError(result, "순환 의존")
            self.assertIn(".ieum:1:1", result.stdout)
            data.write_text("module data {}\nlayer data above app\n", encoding="utf-8")
            self.assertError(self.invoke(entry, "--module-path", directory), "계층 위반")

    def test_imported_parse_error_context(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory)
            data = self.write(directory, "data.ieum", "module data {\nfn get() {\nlet n = ]\n}\n}\n")
            result = self.invoke(entry, "--module-path", directory)
            self.assertError(result, str(data) + ":3:9")
            self.assertIn("let n = ]", result.stderr)
            self.assertIn("^", result.stderr)

    def test_multifile_runtime_call_stack(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory, "return service.get()", "service")
            service = self.write(directory, "service.ieum", "module service depends data {\nfn get() -> int {\nreturn data.fail()\n}\n}\n")
            data = self.write(directory, "data.ieum", "module data {\nfn fail() -> int {\nreturn 1 / 0\n}\n}\n")
            result = self.invoke(entry, "--module-path", directory, "--run", "app.main")
            self.assertError(result, "division_by_zero")
            for text in (str(data) + ":3:", str(service) + ":3:", str(entry) + ":3:", "return 1 / 0", "call_stack:"):
                self.assertIn(text, result.stdout)
            stack = result.stdout.split("call_stack:")[1]
            self.assertLess(stack.index("data.fail"), stack.index("service.get"))
            self.assertLess(stack.index("service.get"), stack.index("app.main"))

    def test_multifile_limit_stack(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = self.app(directory)
            self.write(directory, "data.ieum", "module data {\nfn get() -> int {\nreturn get()\n}\n}\n")
            result = self.invoke(entry, "--module-path", directory, "--run", "app.main", "--max-call-depth", "4")
            self.assertError(result, "call_depth_limit")
            self.assertIn("call_stack:", result.stdout)

    def test_deterministic_graph_across_search_order(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / "a", Path(directory) / "b"
            entry = self.app(directory, "return data.a + other.b", "data, other")
            self.write(a, "data.ieum", "module data {\nlet a = 1\n}\n")
            self.write(b, "other.ieum", "module other {\nlet b = 2\n}\n")
            one, two = Path(directory) / "one.dot", Path(directory) / "two.dot"
            for roots, dot in (((a, b), one), ((b, a), two)):
                result = self.invoke(entry, "--module-path", roots[0], "--module-path", roots[1], "--emit-dot", dot, "--run", "app.main")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(one.read_bytes(), two.read_bytes())

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ieum", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    IEUM = options.ieum.resolve()
    unittest.main(argv=[__file__, *remaining])
