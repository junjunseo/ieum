"""Render checker-produced DOT into SVG and a self-contained offline gallery."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from time import perf_counter
import xml.etree.ElementTree as ET

from workflow_support import MANIFEST, ROOT, default_ieum, digest, executable, load_cases, run_case, text_digest

SVG = "http://www.w3.org/2000/svg"
ET.register_namespace("", SVG)


def annotate_svg(text, title):
    root = ET.fromstring(text)
    if root.tag != f"{{{SVG}}}svg":
        raise ValueError("Graphviz did not produce SVG")
    root.set("role", "img")
    root.set("aria-label", title)
    for group in root.iter(f"{{{SVG}}}g"):
        if group.get("class") != "edge":
            continue
        labels = [t.text for t in group.findall(f"{{{SVG}}}text")]
        group.set("data-kind", "layer" if "above" in labels else "dependency")
        red = any(e.get("stroke") == "#dc2626" for e in group.iter())
        group.set("data-violation", "true" if red else "false")
    return ET.tostring(root, encoding="unicode") + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ieum", default=default_ieum())
    parser.add_argument("--dot", default="dot", help="Graphviz dot executable (only needed to regenerate)")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/graphs")
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    args = parser.parse_args()
    ieum, dot = executable(args.ieum), executable(args.dot)
    version = subprocess.run([dot, "-V"], capture_output=True, text=True, check=True, timeout=10)
    cases = [c for c in load_cases(args.manifest) if "graph" in c]
    if not cases:
        raise ValueError("No graph cases in manifest")
    rendered, provenance = [], []
    (ROOT / "build").mkdir(exist_ok=True)
    # Stage and validate every graph before replacing the published demo assets.
    with tempfile.TemporaryDirectory(prefix="graphs-", dir=ROOT / "build") as temp:
        staged = Path(temp)
        for case in cases:
            name = case["id"]
            dot_path = staged / f"{name}.dot"
            extra = ["--emit-dot", str(dot_path)]
            if case["graph"]["group"] == "demo":
                extra += ["--run", "ui.main"]
            result = run_case(ieum, case, extra)
            if not result["exact_match"] or not dot_path.is_file() or "graph_export=failed" in result["stderr"]:
                raise ValueError(f"Unexpected checker result or export failure: {name}")
            render_start = perf_counter()
            process = subprocess.run([dot, "-Tsvg", "-Gpad=0.35", "-Granksep=0.7",
                                      "-Nfontsize=16", "-Nmargin=0.20,0.13", str(dot_path)],
                                     capture_output=True, encoding="utf-8", check=True, timeout=30)
            render_ms = (perf_counter() - render_start) * 1000
            svg = annotate_svg(process.stdout, case["graph"]["title"])
            svg_root = ET.fromstring(svg)
            nodes = [e for e in svg_root.iter() if e.get("class") == "node"]
            edges = [e for e in svg_root.iter() if e.get("class") == "edge"]
            if "size" in case:
                if len(nodes) != case["size"]["modules"] or len(edges) != case["size"]["dependencies"] + case["size"]["layers"]:
                    raise ValueError(f"SVG structure count mismatch: {name}")
            (staged / f"{name}.svg").write_text(svg, encoding="utf-8")
            summary = re.search(r"modules=(\d+), layers=(\d+)", result["stdout"])
            rendered.append({
                "id": name, **case["graph"], "svg": svg,
                "source": (ROOT / case["source"]).read_text(encoding="utf-8"),
                "sourcePath": case["source"], "modules": int(summary[1]), "layers": int(summary[2]),
                "violations": sum(case["expected_kinds"].values()),
                "diagnostics": [line.strip() for line in result["stdout"].splitlines() if line.strip().startswith("[")],
                "trace": result["stdout"].split("── 실행 Trace ──\n")[-1].strip()
                         if "── 실행 Trace ──" in result["stdout"] else "",
            })
            provenance.append({"id": name, "source": case["source"], "source_sha256": result["source_sha256"],
                               "dot_sha256": text_digest(dot_path), "svg_sha256": text_digest(staged / f"{name}.svg"),
                               "render_ms_including_dot_startup": render_ms,
                               "view_box": svg_root.get("viewBox"), "nodes": len(nodes), "edges": len(edges),
                               "highlighted_dependencies": sum(e.get("data-kind") == "dependency" and e.get("data-violation") == "true" for e in edges)})
        payload = json.dumps(rendered, ensure_ascii=False).replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
        template = (ROOT / "scripts/graph_gallery.html").read_text(encoding="utf-8")
        if template.count("/*GRAPH_DATA*/[]") != 1:
            raise ValueError("Gallery template requires exactly one graph data marker")
        guide = Path(os.path.relpath(ROOT / "docs/DEMO.md", args.output)).as_posix()
        (staged / "index.html").write_text(template.replace("/*GRAPH_DATA*/[]", payload).replace('href="../DEMO.md"', f'href="{guide}"'), encoding="utf-8")
        (staged / "provenance.json").write_text(json.dumps({
            "graphviz": (version.stderr + version.stdout).strip(),
            "ieum_sha256": digest(ieum), "manifest_sha256": text_digest(args.manifest), "graphs": provenance,
        }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        args.output.mkdir(parents=True, exist_ok=True)
        for path in staged.iterdir():
            (args.output / path.name).write_bytes(path.read_bytes())
    print(f"Rendered {len(rendered)} graphs: {args.output / 'index.html'}")
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, ET.ParseError) as error:
        print(f"Graph rendering error: {error}", file=sys.stderr)
        sys.exit(2)
