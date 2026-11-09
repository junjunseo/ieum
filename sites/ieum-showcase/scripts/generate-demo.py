"""Regenerate the website's demo from the real IEUM executable and Graphviz."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

SITE = Path(__file__).resolve().parents[1]
REPO = SITE.parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--ieum", type=Path, default=REPO / "build" / "ieum.exe")
parser.add_argument("--dot", type=Path, required=True)
args = parser.parse_args()
output = SITE / "public" / "demo"
records = []

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

for name, expected_code in (("five_valid", 0), ("five_violation", 1)):
    run = subprocess.run([str(args.ieum.resolve()), f"{name}.ieum", "--emit-dot", f"{name}.dot"], cwd=output, capture_output=True, encoding="utf-8")
    log = run.stdout + run.stderr
    if run.returncode != expected_code:
        raise RuntimeError(f"{name}: unexpected exit code {run.returncode}: {log}")
    if expected_code == 1 and ("위반 1건" not in log or "'data'가 상위 계층 'auth'" not in log):
        raise RuntimeError(f"{name}: unexpected diagnostic: {log}")
    if expected_code == 0 and "구조 검사 통과" not in log:
        raise RuntimeError(f"{name}: missing success diagnostic")
    (output / f"{name}.log").write_text(log, encoding="utf-8")
    subprocess.run([str(args.dot.resolve()), "-Tsvg", "-Nfontsize=16", "-Efontsize=9", "-Gpad=0.25", "-Gnodesep=0.35", "-Granksep=0.55", f"{name}.dot", "-o", f"{name}.svg"], cwd=output, check=True)
    svg = ET.parse(output / f"{name}.svg")
    ns = {"s": "http://www.w3.org/2000/svg"}
    nodes = svg.findall(".//s:g[@class='node']", ns)
    if len(nodes) != 5:
        raise RuntimeError(f"{name}: expected five graph nodes")
    records.append({"example": name, "exit_code": run.returncode, "nodes": len(nodes), "sha256": {ext: digest(output / f"{name}.{ext}") for ext in ("ieum", "dot", "svg", "log")}})

manifest = {"generator": "IEUM --emit-dot + Graphviz", "ieum_sha256": digest(args.ieum), "graphviz": subprocess.run([str(args.dot.resolve()), "-V"], capture_output=True, text=True, check=True).stderr.strip(), "examples": records}
(output / "provenance.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("Generated and verified two examples: valid=0, violation=1; five modules each.")
