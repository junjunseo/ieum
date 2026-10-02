"""End-to-end collections and UTF-8 I/O, using only temporary output files."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
IEUM = ROOT / "build" / "ieum"

class CollectionsCliTests(unittest.TestCase):
    def invoke(self, source, input_text="", *options):
        return subprocess.run([str(IEUM), str(source), "--run", "app.main", *options],
                              input=input_text, capture_output=True, text=True, encoding="utf-8", timeout=20)

    def source(self, directory, body, declarations="", dependencies="std_io, std_text, std_list"):
        path = Path(directory) / "program.ieum"
        path.write_text("module app depends " + dependencies + " {\n" + declarations +
                        "fn main() {\n" + body + "\n}\n}\n", encoding="utf-8")
        return path

    def test_collections_example(self):
        result = self.invoke(ROOT / "examples/collections.ieum")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for expected in ("return app.main: int = 60", "value app.main.values: list<int> = [10, 20, 30]",
                         "value app.main.copy: list<int> = [99, 20, 30]", "app.Summary{total: 60, count: 3}"):
            self.assertIn(expected, result.stdout)

    def test_fixture_read_split_parse_sum_write(self):
        with tempfile.TemporaryDirectory() as directory:
            input_path = Path(directory) / "숫자.txt"
            output_path = Path(directory) / "결과.txt"
            input_path.write_bytes((ROOT / "test/fixtures/numbers.txt").read_bytes())
            result = self.invoke(ROOT / "examples/collections_io.ieum", f"{input_path}\n{output_path}\n")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(output_path.read_text(encoding="utf-8"), "60")
            self.assertIn("return app.main: int = 60", result.stdout)
            self.assertIn("app.Summary{total: 60, count: 3}", result.stdout)
            self.assertIn("\n60\n", result.stdout)

    def test_utf8_roundtrip_and_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "출력.txt"
            target.write_text("old longer contents", encoding="utf-8")
            source = self.source(directory, 'let path = read_line()\ncall write_text(path, "안녕 이음\\n😀")\ncall print(read_text(path))')
            result = self.invoke(source, str(target) + "\n")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(target.read_bytes(), "안녕 이음\n😀".encode("utf-8"))
            self.assertIn("안녕 이음\n😀", result.stdout)

    def test_read_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            invalid = Path(directory) / "invalid.txt"
            invalid.write_bytes(b"\xc0\xaf")
            source = self.source(directory, "let text = read_text(read_line())")
            for path, code in ((Path(directory) / "missing.txt", "io_error"), (invalid, "invalid_utf8"),
                               (Path(directory), "io_error")):
                with self.subTest(path=str(path)):
                    result = self.invoke(source, str(path) + "\n")
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertIn(code, result.stdout)
                    self.assertIn("program.ieum:3:", result.stdout)

    def test_empty_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "empty.txt"
            path.write_bytes(b"")
            source = self.source(directory, "call print(read_text(read_line()))")
            result = self.invoke(source, str(path) + "\n")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_write_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory, 'call write_text(read_line(), "data")')
            for path in (Path(directory), Path(directory) / "missing-parent" / "file.txt"):
                with self.subTest(path=str(path)):
                    result = self.invoke(source, str(path) + "\n")
                    self.assertEqual(result.returncode, 1)
                    self.assertIn("io_error", result.stdout)

    def test_bad_integer_does_not_write_result(self):
        with tempfile.TemporaryDirectory() as directory:
            input_path = Path(directory) / "input.txt"
            output_path = Path(directory) / "output.txt"
            input_path.write_text("10\nnot-an-int\n", encoding="utf-8")
            result = self.invoke(ROOT / "examples/collections_io.ieum", f"{input_path}\n{output_path}\n")
            self.assertEqual(result.returncode, 1)
            self.assertIn("invalid_integer", result.stdout)
            self.assertFalse(output_path.exists())

    def test_type_failure_prevents_all_io(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "output.txt"
            source = self.source(directory, 'call write_text(read_line(), "data")\nlet wrong = [1, true]')
            result = self.invoke(source, str(target) + "\n")
            self.assertEqual(result.returncode, 1)
            self.assertIn("type_mismatch", result.stdout)
            self.assertFalse(target.exists())

    def test_stdin_eof(self):
        result = self.invoke(ROOT / "examples/collections_io.ieum")
        self.assertEqual(result.returncode, 1)
        self.assertIn("io_error", result.stdout)

    def test_unicode_source_and_graph_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "한글 😀.ieum"
            dot = Path(directory) / "그래프 😀.dot"
            source.write_text('module app {\nfn main() {\nlet xs = [1, 2]\n}\n}\n', encoding="utf-8")
            result = self.invoke(source, "", "--emit-dot", str(dot))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(str(dot), result.stdout)
            self.assertIn('"app"', dot.read_text(encoding="utf-8"))
            source.write_text('module app {\nfn main() {\nlet bad = [1][2]\n}\n}\n', encoding="utf-8")
            result = self.invoke(source)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn(str(source) + ":3:", result.stdout)
            self.assertIn("index_out_of_range", result.stdout)

    def test_stdlib_graph_and_layer(self):
        with tempfile.TemporaryDirectory() as directory:
            dot = Path(directory) / "graph.dot"
            result = self.invoke(ROOT / "examples/collections.ieum", "", "--emit-dot", str(dot))
            self.assertEqual(result.returncode, 0)
            graph = dot.read_text(encoding="utf-8")
            self.assertIn('"app" -> "std_list"', graph)
            self.assertNotIn("std_io", graph)
            self.assertNotIn("std_text", graph)
            source = self.source(directory, 'call print("must not print")', dependencies="std_io")
            with source.open("a", encoding="utf-8") as stream:
                stream.write("layer std_io above app\n")
            result = self.invoke(source)
            self.assertEqual(result.returncode, 1)
            self.assertNotIn("must not print", result.stdout)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ieum", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    IEUM = options.ieum.resolve()
    unittest.main(argv=[__file__, *remaining])
