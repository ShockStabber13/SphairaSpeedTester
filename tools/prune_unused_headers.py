#!/usr/bin/env python3
"""Read actual GCC/CMake include dependencies; list or remove unneeded tester headers."""
import argparse
import json
import re
import shlex
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/Lite"
parser = argparse.ArgumentParser()
parser.add_argument("--write", action="store_true")
args = parser.parse_args()
compdb = BUILD / "compile_commands.json"
if not compdb.is_file():
    raise SystemExit("Missing compile_commands.json; configure and compile Lite first")
entries = json.loads(compdb.read_text())
deps = set()
compiled = 0
for entry in entries:
    src = Path(entry["file"]).resolve()
    if not src.is_relative_to(ROOT / "sphaira/source"):
        continue
    cmd = entry.get("arguments") or shlex.split(entry["command"])
    if "-MF" in cmd:
        dep_name = cmd[cmd.index("-MF") + 1]
    elif "-o" in cmd:
        dep_name = cmd[cmd.index("-o") + 1] + ".d"
    else:
        raise SystemExit("Cannot determine dependency file for " + str(src))
    dep = Path(dep_name)
    if not dep.is_absolute():
        dep = Path(entry["directory"]) / dep
    if not dep.is_file():
        raise SystemExit("Missing dependency file: " + str(dep))
    # GCC .d: makefile target, backslash continuation lines, escaped paths.
    contents = re.sub(r"\\\r?\n", " ", dep.read_text())
    colon = contents.find(": ")
    if colon < 0:
        raise SystemExit("Invalid dependency file: " + str(dep))
    for raw in shlex.split(contents[colon+2:]):
        candidate = Path(raw)
        if not candidate.is_absolute():
            candidate = Path(entry["directory"]) / candidate
        candidate = candidate.resolve()
        if candidate.is_relative_to(ROOT):
            deps.add(candidate.relative_to(ROOT).as_posix())
    compiled += 1

if compiled < 17:
    raise SystemExit(f"Expected >=17 original tester units, only found {compiled}")
tracked = subprocess.check_output(["git","ls-files","-z"], cwd=ROOT).decode().split("\0")
headers = [
    p for p in tracked
    if (p.startswith("sphaira/include/") or p.startswith("sphaira/source/"))
    and p.endswith((".hpp",".h"))
]
# Protected originals remain for integration regardless of compiler tracking.
protected = {
    "sphaira/include/app.hpp",
    "sphaira/include/speed_test.hpp",
    "sphaira/include/threaded_file_transfer.hpp",
    "sphaira/include/yati/source/http.hpp",
    "sphaira/include/ui/progress_box.hpp",
    "sphaira/include/ui/option_box.hpp",
    "sphaira/include/ui/menus/main_menu.hpp",
    "sphaira/include/utils/devoptab_common.hpp",
}
unused = sorted(p for p in headers if p not in deps and p not in protected)
print("COMPILED_UNIT_COUNT:", compiled, flush=True)
print("REACHABLE_HEADER_COUNT:", len(headers)-len(unused), flush=True)
print("UNUSED_HEADERS_JSON="+json.dumps(unused), flush=True)
if args.write:
    for p in unused:
        (ROOT/p).unlink()
    print("REMOVED_HEADERS_COUNT:",len(unused),flush=True)
