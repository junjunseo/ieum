"""Replay the language examples with explicit values, temporary I/O and rejection checks."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from workflow_support import ROOT, default_ieum, executable


def replay(ieum):
    results = []

    def check(name, source, expected, *, options=(), stdin="", exit_code=0, absent=()):
        process = subprocess.run(
            [str(ieum), str(source), "--run", "app.main", *map(str, options)],
            input=stdin, capture_output=True, encoding="utf-8", timeout=30,
        )
        output = process.stdout + process.stderr
        ok = (process.returncode == exit_code
              and all(text in output for text in expected)
              and all(text not in output for text in absent))
        results.append({"case": name, "passed": ok, "exit_code": process.returncode,
                        "stdout": process.stdout, "stderr": process.stderr})
        return ok

    check("values", ROOT / "examples/values.ieum",
          ["value data.total: int = 17", "value app.main.result: int = 8",
           "value app.main.safe: bool = false"])
    check("control-flow", ROOT / "examples/control_flow.ieum",
          ["return app.main: int = 175", "sum: int = 55", "fact: int = 120", "oddSum: int = 25"])
    check("for", ROOT / "examples/for_loop.ieum",
          ["return app.main: int = 55", "oddSum: int = 25"])
    check("collections", ROOT / "examples/collections.ieum",
          ["return app.main: int = 60", "count: 3"])
    modules = ROOT / "examples/multifile"
    check("multifile", modules / "app.ieum",
          ["return app.main: int = 60", "data.Summary{total: 60, count: 3}", "\n60\n"],
          options=("--module-path", modules))
    with tempfile.TemporaryDirectory(prefix="ieum-language-demo-") as directory:
        directory = Path(directory).resolve()
        source = directory / "숫자.txt"
        target = directory / "결과.txt"
        source.write_bytes((ROOT / "test/fixtures/numbers.txt").read_bytes())
        check("file-sum", ROOT / "examples/collections_io.ieum",
              ["return app.main: int = 60", "\n60\n"], stdin=f"{source}\n{target}\n")
        results[-1]["passed"] &= target.exists() and target.read_bytes() == b"60"
        # A failed parse must not overwrite an existing output file.
        source.write_text("10\ninvalid\n30", encoding="utf-8")
        target.write_bytes(b"preserve-existing")
        check("file-rejection", ROOT / "examples/collections_io.ieum",
              ["invalid_integer", "call_stack"], stdin=f"{source}\n{target}\n", exit_code=1)
        results[-1]["passed"] &= target.read_bytes() == b"preserve-existing"

        # Combine modules, private access and observable I/O: rejection precedes all execution.
        (directory / "data.ieum").write_text(
            "module data {\nprivate let secret = 99\n}\n", encoding="utf-8")
        entry = directory / "app.ieum"
        entry.write_text(
            'module app depends data, std_io {\nfn main() -> int {\n'
            'call write_text(read_line(), "changed")\nreturn data.secret\n}\n}\n', encoding="utf-8")
        check("private-prevents-io", entry, ["private_access"],
              options=("--module-path", directory), stdin=f"{target}\n", exit_code=1,
              absent=("enter app.main",))
        results[-1]["passed"] &= target.read_bytes() == b"preserve-existing"
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ieum", default=default_ieum())
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--output", type=Path, default=ROOT / "build/demo/language.json")
    args = parser.parse_args()
    if not 1 <= args.repeat <= 100:
        parser.error("--repeat must be between 1 and 100")
    ieum = executable(args.ieum)
    results = []
    for round_number in range(1, args.repeat + 1):
        for result in replay(ieum):
            result["round"] = round_number
            results.append(result)
            print(f"{round_number}: {result['case']}: {'PASS' if result['passed'] else 'FAIL'}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return int(not all(result["passed"] for result in results))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Language demo error: {error}", file=sys.stderr)
        sys.exit(2)
