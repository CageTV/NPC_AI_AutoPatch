"""Builds a "forward" profile for the AI overhaul patcher from an overhaul plugin's Spriggit yaml.

For every NPC the overhaul overrides, compares its record with the record it overrides (the master's yaml) and writes only the AI part of the difference:
packages (the overhaul's list in order, and the ones it removed), factions it added, the spectator / combat / observe / guard-warn override lists, the
Protected / Essential flags and the confidence. The result is data: it names records by 'Plugin.esp|LocalID', no overhaul is a master of anything.

Usage: python build_forward_profile.py <overhaul yaml dir> <overhaul plugin file name> <profile id> <profile name> <out json> <masters-yaml: Plugin=dir ...>
Example: python build_forward_profile.py work/aio "AI Overhaul.esp" ai-overhaul "AI Overhaul SSE" profiles/ai-overhaul.json Skyrim.esm=E:/.../yaml_vanilla/Npcs
"""
import collections
import json
import os
import re
import sys

NL = chr(10)
CR = chr(13)
LISTS = {"SpectatorOverridePackageList": "spectator", "CombatOverridePackageList": "combat", "ObserveDeadBodyOverridePackageList": "observe", "GuardWarnOverridePackageList": "guardWarn"}
CONF = ["Cowardly", "Cautious", "Average", "Brave", "Foolhardy"]


def blocks(t):
    out = collections.OrderedDict()
    cur = None
    for line in t.split(NL):
        m = re.match(r"^([A-Za-z]\w*):(.*)$", line)
        if m:
            cur = m.group(1)
            out[cur] = line + NL
        elif cur:
            out[cur] += line + NL
    return out


def ref(fk):
    i, p = fk.strip().split(":", 1)
    return "%s|%s" % (p.strip(), i)


def packages(b):
    return [ref(x) for x in re.findall(r"^- ([0-9A-F]{6}:[^\n]+)$", b.get("Packages", ""), re.M)]


def factions(b):
    out = collections.OrderedDict()
    for m in re.finditer(r"^- Faction: ([0-9A-F]{6}:[^\n]+)\n(?:  Rank: (-?\d+)\n)?", b.get("Factions", ""), re.M):
        out[ref(m.group(1))] = int(m.group(2) or 0)
    return out


def single(b, key):
    m = re.match(key + r": ([0-9A-F]{6}:[^\n]+)", b.get(key, ""))
    return ref(m.group(1)) if m else None


def flags(b):
    t = b.get("Configuration", "")
    m = re.search(r"  Flags:\n((?:  - [^\n]+\n)+)", t)
    return set(re.findall(r"- (\w+)", m.group(1))) if m else set()


def confidence(b):
    m = re.search(r"Confidence: (\w+)", b.get("AIData", ""))
    return CONF.index(m.group(1)) if m and m.group(1) in CONF else 0 if "AIData" in b else None


def main(a):
    src, plugin, pid, pname, out_json = a[0], a[1], a[2], a[3], a[4]
    mast = dict(x.split("=", 1) for x in a[5:])
    npcs_dir = os.path.join(src, "Npcs")
    entries = []
    skipped = collections.Counter()
    for f in sorted(os.listdir(npcs_dir)):
        m = re.match(r"(.*) - ([0-9A-F]{6})_(.+?)\.yaml$", f)
        name, fid, origin = m.groups()
        d = mast.get(origin)
        orig = None
        if d:
            for g in os.listdir(d):
                if g.endswith(" - %s_%s.yaml" % (fid, origin)):
                    orig = os.path.join(d, g)
                    break
        if not orig:
            skipped["original record not available (%s)" % origin] += 1
            continue
        A = blocks(open(os.path.join(npcs_dir, f), encoding="utf-8").read().replace(CR, ""))
        P = blocks(open(orig, encoding="utf-8").read().replace(CR, ""))
        e = {"npc": "%s|%s" % (origin, fid), "name": name}
        pa, pp = packages(A), packages(P)
        if pa != pp:
            e["packages"] = pa
            e["packagesRemoved"] = [x for x in pp if x not in pa]
            e["packagesBefore"] = pp
        fa, fp = factions(A), factions(P)
        add = [{"form": k, "rank": v} for k, v in fa.items() if k not in fp]
        if add:
            e["factionsAdded"] = add
        lists = {}
        for key, short in LISTS.items():
            x, y = single(P, key), single(A, key)
            if x != y:
                lists[short] = {"from": x, "to": y}
        if lists:
            e["lists"] = lists
        fl_a, fl_p = flags(A), flags(P)
        fl = sorted((fl_a - fl_p) & {"Protected", "Essential"})
        if fl:
            e["flagsAdded"] = fl
        ca, cp = confidence(A), confidence(P)
        if ca is not None and ca != cp:
            e["confidence"] = {"from": cp, "to": ca}
        if len(e) > 2:
            entries.append(e)
        else:
            skipped["no AI difference"] += 1
    prof = {"format": 1, "id": pid, "name": pname, "type": "forward", "requires": plugin,
            "ownPlugins": [plugin], "npcs": entries}
    os.makedirs(os.path.dirname(out_json) or ".", exist_ok=True)
    json.dump(prof, open(out_json, "w", encoding="utf-8"), indent=1)
    kinds = collections.Counter(k for e in entries for k in e if k not in ("npc", "name", "packagesRemoved", "packagesBefore"))
    print("profile", pid, ":", len(entries), "NPCs with an AI difference; skipped", dict(skipped))
    print("what it carries:", dict(kinds))


if __name__ == "__main__":
    main(sys.argv[1:])
