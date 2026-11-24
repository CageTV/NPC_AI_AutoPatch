"""Makes a profile for NPC AI AutoPatch from a plugin you have installed: one command, no hand-made exports.

    python tools/make_profile.py --kind forward  --plugin "<path>/AI Overhaul.esp"  --id ai-overhaul  --name "AI Overhaul 1.9.5" --out profiles/ai-overhaul.json
    python tools/make_profile.py --kind baseline --plugin "<path>/USSEP.esp"        --id ussep        --name "USSEP"            --out profiles/ussep.json
    python tools/make_profile.py --kind baseline --plugin "<path>/USMP.esp" --prior "<path>/USSEP.esp" --priority 1 --id usmp --name "USMP" --out profiles/usmp.json

  forward   an AI overhaul: packages, removed packages, factions, override lists, Protected / Essential, confidence       (build_forward_profile.py)
  baseline  a bug-fix patch: class, skills, perks, spells, keywords, crime faction, combat style, packages, factions ... (build_baseline_profile.py)

What it does: reads the plugin's masters from its header, finds each master in the game's Data folder (or in --search folders), exports the plugin and its masters
with Spriggit (needs `spriggit` on PATH; the exports are cached, so a second profile is fast), lays the masters' NPC records over each other in load order
(Update.esm's version of an NPC is the "before" of a patch that has Update.esm as a master), and runs the builder. The profile names records as 'Plugin.esp|LocalID';
nothing in it is a master. Check the printed counts, then copy the file to the plugin's profiles folder.

  --data DIR      the game's Data folder (default: SKYRIM_DATA, or the Steam default)
  --search DIR    another folder to look for masters in (a mod folder); can be repeated
  --cache DIR     where exports are kept (default: tools/.yaml-cache)
  --yaml Plugin.esm=DIR   use an export you already have (a folder that contains Npcs/); can be repeated
  --prior PLUGIN  baseline only: an earlier patch this one sits on (USMP on USSEP); its records are the "before" where it has the NPC
  --priority N    baseline only: the order profiles are applied in (lowest first)
  --base original|latest   which version of an NPC the patch is compared with. original (default): the record as the plugin that first defined it has it
                  (Skyrim.esm's NPC, so a patch like USSEP also carries what Update.esm fixed: a look replacer built from the vanilla record loses those too).
                  latest: Update.esm's and the other masters' overrides count as the "before".
"""
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def masters_of(path):
    """The master file names listed in a plugin's TES4 header, in order."""
    with open(path, "rb") as f:
        head = f.read(24)
        assert head[:4] == b"TES4", f"{path} is not a plugin"
        size = struct.unpack("<I", head[4:8])[0]
        data = f.read(size)
    out, i = [], 0
    while i + 6 <= len(data):
        kind, n = data[i:i + 4], struct.unpack("<H", data[i + 4:i + 6])[0]
        body = data[i + 6:i + 6 + n]
        if kind == b"MAST":
            out.append(body.split(b"\0")[0].decode("utf-8", "replace"))
        i += 6 + n
    return out


def find_file(name, dirs):
    for d in dirs:
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
        if os.path.isdir(d):  # case-insensitive match
            for g in os.listdir(d):
                if g.lower() == name.lower():
                    return os.path.join(d, g)
    return None


def spriggit_version():
    out = subprocess.run(["spriggit", "--version"], capture_output=True, text=True).stdout
    m = re.search(r"(\d+\.\d+\.\d+)", out)
    assert m, "spriggit is not on PATH (or `spriggit --version` printed nothing)"
    return m.group(1)


def export(plugin, cache, version):
    """Spriggit-exports a plugin into the cache; returns the export folder."""
    key = re.sub(r"[^A-Za-z0-9._-]", "_", os.path.basename(plugin).lower())
    dest = os.path.join(cache, key)
    stamp = os.path.join(dest, ".source")
    sig = f"{os.path.getsize(plugin)}:{int(os.path.getmtime(plugin))}"
    if os.path.isfile(stamp) and open(stamp).read() == sig and os.path.isdir(os.path.join(dest, "yaml")):
        return os.path.join(dest, "yaml")
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(dest)
    print(f"  exporting {os.path.basename(plugin)} ...", flush=True)
    r = subprocess.run(["spriggit", "serialize", "-i", os.path.abspath(plugin), "-o", os.path.join(os.path.abspath(dest), "yaml"), "-g", "SkyrimSE",
                        "-p", "Spriggit.Yaml.Skyrim", "--PackageVersion", version], capture_output=True, text=True)
    if not os.path.isdir(os.path.join(dest, "yaml")):  # a plugin with no NPC records has no Npcs folder: that is fine
        sys.exit(f"Spriggit could not export {plugin}:\n{r.stdout[-600:]}\n{r.stderr[-600:]}")
    open(stamp, "w").write(sig)
    return os.path.join(dest, "yaml")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--kind", choices=("forward", "baseline"), required=True)
    ap.add_argument("--plugin", required=True)
    ap.add_argument("--id", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--data", default=os.environ.get("SKYRIM_DATA", r"C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data"))
    ap.add_argument("--search", action="append", default=[])
    ap.add_argument("--cache", default=os.path.join(HERE, ".yaml-cache"))
    ap.add_argument("--yaml", action="append", default=[])
    ap.add_argument("--prior")
    ap.add_argument("--priority", type=int, default=0)
    ap.add_argument("--base", choices=("original", "latest"), default="original")
    a = ap.parse_args()

    version = spriggit_version()
    given = {}
    for y in a.yaml:
        name, d = y.split("=", 1)
        given[name.lower()] = d if os.path.isdir(os.path.join(d, "Npcs")) else os.path.join(d, "yaml")
    dirs = [os.path.dirname(os.path.abspath(a.plugin))] + a.search + [a.data]
    plugin_name = os.path.basename(a.plugin)

    def yaml_of(file_path_or_name):
        n = os.path.basename(file_path_or_name)
        if n.lower() in given:
            return given[n.lower()]
        p = file_path_or_name if os.path.isfile(file_path_or_name) else find_file(n, dirs)
        if not p:
            sys.exit(f"cannot find {n}: give --data or --search")
        return export(p, a.cache, version)

    print("plugin:", plugin_name)
    ms = masters_of(a.plugin)
    print("masters:", ", ".join(ms))
    # the masters' NPC records laid over each other in load order: a later master's version of an NPC replaces an earlier one's
    merged = os.path.join(a.cache, "_merged_" + re.sub(r"[^A-Za-z0-9._-]", "_", plugin_name.lower()), "Npcs")
    shutil.rmtree(os.path.dirname(merged), ignore_errors=True)
    os.makedirs(merged)
    origins = []
    have = {}  # (local id, plugin that defined the record) -> file name in merged
    for m in ms:
        d = os.path.join(yaml_of(m), "Npcs")
        n = 0
        if not os.path.isdir(d):
            print(f"  {m}: no NPC records")
            continue
        for g in os.listdir(d):
            mm = re.match(r".* - ([0-9A-F]{6})_(.+?)\.yaml$", g)
            if not mm:
                continue
            key = (mm.group(1), mm.group(2))
            if a.base == "original" and key[1].lower() != m.lower():
                continue  # an override of another plugin's NPC: not the original record
            # a later master's copy of a record replaces an earlier one's (it is the version a patch on top of it starts from)
            if key in have and have[key] != g:
                os.remove(os.path.join(merged, have[key]))
            shutil.copyfile(os.path.join(d, g), os.path.join(merged, g))
            have[key] = g
            n += 1
            if key[1] not in origins:
                origins.append(key[1])
        print(f"  {m}: {n} NPC records")
    src = yaml_of(a.plugin)
    pairs = [f"{o}={os.path.dirname(merged)}/Npcs" for o in origins]
    out = os.path.abspath(a.out)
    if a.kind == "forward":
        import build_forward_profile as b
        b.main([src, plugin_name, a.id, a.name, out] + pairs)
    else:
        import build_baseline_profile as b
        extra = [f"priority={a.priority}"]
        if a.prior:
            extra.append("prior=" + os.path.join(yaml_of(a.prior), "Npcs"))
        b.main([src, plugin_name, a.id, a.name, out] + pairs + extra)
    print("wrote", out)
    print("A forward profile also wants \"family\" (versions of one overhaul) and \"priority\" in its header if it has siblings; see README, Making a profile.")


if __name__ == "__main__":
    main()
