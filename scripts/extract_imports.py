"""Extract static intra-package imports from pinned ZIPs without executing code.

Download archives separately; this command is intentionally network-free.
All AST import statements are included, even conditional/type-only/deferred imports.
No implicit package-initialization edges, dynamic imports or external edges.
"""
import argparse
import ast
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys
import zipfile

from workflow_support import ROOT, digest

SOURCES = ROOT / "evaluation/real/sources.json"


def json_text(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + "\n"


def extract_modules(files, package):
    """files maps package-relative POSIX Python paths to source text."""
    modules = {}
    for path, source in sorted(files.items()):
        parts = list(PurePosixPath(path).with_suffix("").parts)
        is_package = parts[-1] == "__init__"
        if is_package:
            parts.pop()
        name = ".".join([package, *parts])
        if name in modules:
            raise ValueError(f"Ambiguous module path: {name}")
        modules[name] = {"module": name, "file": path, "is_package": is_package,
                         "ieum": name.replace(".", "__"),
                         "source_sha256": hashlib.sha256(source.encode("utf-8")).hexdigest(),
                         "imports": [], "dependencies": []}
    identifiers = [item["ieum"] for item in modules.values()]
    if len(set(identifiers)) != len(identifiers):
        raise ValueError("Module names collide after Ieum identifier conversion")
    for name, record in modules.items():
        dependencies = set()
        tree = ast.parse(files[record["file"]], filename=record["file"])
        for node in sorted((n for n in ast.walk(tree) if isinstance(n, (ast.Import, ast.ImportFrom))),
                           key=lambda n: (n.lineno, n.col_offset)):
            candidates = []
            if isinstance(node, ast.Import):
                candidates = [alias.name for alias in node.names]
            else:
                base = node.module or ""
                if node.level:
                    context = name.split(".") if record["is_package"] else name.split(".")[:-1]
                    if node.level > len(context):
                        raise ValueError(f"Relative import beyond package: {name}:{node.lineno}")
                    base = ".".join(context[:len(context) - node.level + 1] + ([base] if base else []))
                # A named submodule wins over its re-exporting package. Otherwise
                # a symbol import refers to the module containing that symbol.
                for alias in node.names:
                    child = f"{base}.{alias.name}"
                    candidates.append(child if child in modules else base)
            targets = sorted({target for target in candidates if target in modules})
            dependencies.update(targets)
            record["imports"].append({"line": node.lineno, "statement": ast.unparse(node), "targets": targets})
        record["dependencies"] = sorted(dependencies)
    return sorted(modules.values(), key=lambda m: m["module"])


def extract_archive(archive, spec):
    if digest(archive) != spec["archive_sha256"]:
        raise ValueError(f"Archive hash mismatch: {spec['id']}")
    with zipfile.ZipFile(archive) as zipped:
        members = zipped.namelist()
        roots = {name.split("/")[0] for name in members}
        if len(roots) != 1:
            raise ValueError("Archive must have one top-level directory")
        prefix = roots.pop() + "/"
        package_prefix = prefix + spec["package_path"] + "/"
        files = {name[len(package_prefix):]: zipped.read(name).decode("utf-8-sig").replace("\r\n", "\n")
                 for name in members if name.startswith(package_prefix) and name.endswith(".py")}
        if not files:
            raise ValueError(f"No Python modules in {spec['package_path']}")
        licenses = {path: zipped.read(prefix + path).decode("utf-8").replace("\r\n", "\n")
                    for path in spec["license_files"]}
    return {"schema_version": 1, "source": spec,
            "policy": "all-static-imports-v1; internal-explicit-edges; no-package-init-side-effects",
            "modules": extract_modules(files, spec["id"]), "licenses": licenses}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archives", type=Path, default=ROOT / "build/upstream")
    parser.add_argument("--output", type=Path, default=ROOT / "evaluation/real/snapshots")
    parser.add_argument("--check", action="store_true", help="Compare snapshots without writing")
    args = parser.parse_args()
    specs = json.loads(SOURCES.read_text(encoding="utf-8"))["sources"]
    snapshots = [(s["id"], json_text(extract_archive(args.archives / (s["id"] + ".zip"), s))) for s in specs]
    for name, content in snapshots:
        path = args.output / f"{name}.json"
        if args.check:
            if path.read_text(encoding="utf-8") != content:
                raise ValueError(f"Snapshot differs: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        print(f"{'Verified' if args.check else 'Extracted'} {name}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, SyntaxError, zipfile.BadZipFile) as error:
        print(f"Extraction error: {error}", file=sys.stderr)
        sys.exit(2)
