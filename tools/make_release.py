"""Builds dist/NPC_AI_AutoPatch_<version>.zip: the DLL, the profiles, README and LICENSE, laid out for a mod manager.

    python tools/make_release.py <path to NPCAIAutoPatch.dll> <version>
"""
import hashlib
import json
import os
import sys
import zipfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
dll, version = sys.argv[1], sys.argv[2]
out_dir = os.path.join(root, "dist")
os.makedirs(out_dir, exist_ok=True)
out = os.path.join(out_dir, f"NPC_AI_AutoPatch_{version}.zip")

files = [(dll, "SKSE/Plugins/NPCAIAutoPatch.dll")]
pdir = os.path.join(root, "profiles")
for n in sorted(os.listdir(pdir)):
    if n.endswith(".json"):
        json.load(open(os.path.join(pdir, n), encoding="utf-8"))  # must parse
        files.append((os.path.join(pdir, n), "SKSE/Plugins/NPCAIAutoPatch/profiles/" + n))
files.append((os.path.join(root, "README.md"), "README.md"))
files.append((os.path.join(root, "LICENSE"), "LICENSE"))

with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for src, arc in files:
        z.write(src, arc)

with zipfile.ZipFile(out) as z:
    for i in z.infolist():
        print(f"{i.file_size:>10}  {i.filename}")
print(out)
print("sha256", hashlib.sha256(open(out, "rb").read()).hexdigest())
