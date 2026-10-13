#!/usr/bin/env python3
"""Audit UI files from real Lite build dependencies; never prune used UI."""
import json, re, shlex, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
compdb=root/"build/Lite/compile_commands.json"
if not compdb.is_file():
    raise SystemExit("Missing Lite compile_commands.json")
compiled=json.loads(compdb.read_text())
deps=set()
units=[]
for entry in compiled:
    src=Path(entry["file"]).resolve()
    if not src.is_relative_to(root/"sphaira/source"):
        continue
    units.append(src.relative_to(root).as_posix())
    cmd=entry.get("arguments") or shlex.split(entry["command"])
    if "-MF" in cmd:
        depfile=Path(cmd[cmd.index("-MF")+1])
    elif "-o" in cmd:
        depfile=Path(cmd[cmd.index("-o")+1]+".d")
    else:
        raise SystemExit("Cannot locate compiler dependencies for "+str(src))
    if not depfile.is_absolute():
        depfile=Path(entry["directory"])/depfile
    if not depfile.is_file():
        raise SystemExit("Missing compiler dependency file "+str(depfile))
    contents=re.sub(r"\\\r?\n"," ",depfile.read_text())
    sep=contents.find(": ")
    if sep<0:
        raise SystemExit("Invalid dependency file "+str(depfile))
    for part in shlex.split(contents[sep+2:]):
        dep=Path(part)
        if not dep.is_absolute():
            dep=Path(entry["directory"])/dep
        dep=dep.resolve()
        if dep.is_relative_to(root):
            deps.add(dep.relative_to(root).as_posix())
if len(units)<50:
    raise SystemExit(f"Unexpected compiled Sphaira unit count: {len(units)}")
tracked=subprocess.check_output(["git","ls-files","-z"],cwd=root).decode().split("\0")
ui=[
    p for p in tracked
    if p.startswith(("sphaira/include/ui/","sphaira/source/ui/"))
]
unused=sorted(
    p for p in ui
    if p not in deps and p not in units
)
print("COMPILED_UNITS:",len(units),flush=True)
print("UNUSED_UI_FILES_JSON="+json.dumps(unused),flush=True)
print("ACTIVE_UI_FILES_JSON="+json.dumps(sorted(set(ui)-set(unused))),flush=True)
