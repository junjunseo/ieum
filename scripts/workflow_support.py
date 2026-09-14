"""Shared, standard-library-only helpers for the demo and seed evaluation."""
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "evaluation/manifest.json"
KINDS = {
    "duplicate_module": "중복 모듈",
    "undefined_layer": "미선언 계층 모듈",
    "self_layer": "자기 계층",
    "undefined_dependency": "암묵적 의존",
    "cycle": "순환 의존",
    "layer_violation": "계층 위반",
}


def executable(value):
    path = Path(value).expanduser()
    if path.is_file():
        return str(path.resolve())
    found = shutil.which(str(value))
    if found:
        return found
    raise ValueError(f"Executable not found: {value}")


def default_ieum():
    return ROOT / "build" / ("ieum.exe" if os.name == "nt" else "ieum")


def load_cases(path=MANIFEST):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not data.get("cases"):
        raise ValueError("Manifest requires schema_version=1 and nonempty cases")
    seen = set()
    for case in data["cases"]:
        name = case["id"]
        if not re.fullmatch(r"[a-z0-9_-]+", name) or name in seen:
            raise ValueError(f"Invalid or duplicate case ID: {name}")
        seen.add(name)
        source = (ROOT / case["source"]).resolve()
        if not source.is_relative_to(ROOT) or not source.is_file():
            raise ValueError(f"Invalid case source: {case['source']}")
        expected = case["expected_kinds"]
        if not isinstance(expected, dict) or any(
            k not in KINDS or type(v) is not int or v < 1 for k, v in expected.items()
        ):
            raise ValueError(f"Invalid expected kinds: {name}")
        if type(case["expected_exit"]) is not int or case["expected_exit"] != int(bool(expected)):
            raise ValueError(f"Exit code and expected kinds disagree: {name}")
    return data["cases"]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def text_digest(path):
    # Text checkout line endings differ between Windows and Unix.
    text = Path(path).read_text(encoding="utf-8-sig")
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def run_case(ieum, case, extra=()):
    source = ROOT / case["source"]
    command = [str(ieum), str(source), *extra]
    try:
        result = subprocess.run(command, capture_output=True, encoding="utf-8",
                                errors="replace", timeout=30)
        stdout, stderr, code = result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        stdout, stderr, code = "", "Process timed out after 30 seconds", None
    labels = Counter(re.findall(r"^\s+\[([^\]]+)\]", stdout, re.MULTILINE))
    expected_labels = Counter({KINDS[k]: v for k, v in case["expected_kinds"].items()})
    failure = re.search(r"✗ 구조 검사 실패: 위반 (\d+)건", stdout)
    passed = "✓ 구조 검사 통과: 위반 없음" in stdout
    # Parse/semantic failures, crashes and file errors must not count as detections.
    structural_result = (
        code == 0 and passed and not labels and not failure
    ) or (
        code == 1 and failure is not None and not passed
        and int(failure[1]) > 0 and int(failure[1]) == sum(labels.values())
        and set(labels).issubset(KINDS.values())
    )
    exact = bool(structural_result and code == case["expected_exit"] and labels == expected_labels)
    return {
        "id": case["id"], "source": case["source"], "source_sha256": text_digest(source),
        "expected_exit": case["expected_exit"], "actual_exit": code,
        "expected_kinds": case["expected_kinds"], "actual_labels": dict(labels),
        "classified": bool(structural_result), "exact_match": exact,
        "stdout": stdout, "stderr": stderr,
    }
