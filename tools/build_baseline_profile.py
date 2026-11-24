"""Builds a "baseline" profile: what a bug-fix patch (USSEP) changed on NPC records, as data, so the patcher can put it back on NPCs a later mod
(a look replacer built from the vanilla record) took over.

Same idea as build_forward_profile.py, but it carries the NPC's gameplay data instead of its AI: class, skill values and offsets, health / magicka /
stamina, perks, spells, keywords, crime faction, combat style, packages, factions, Protected / Essential and the confidence. It never carries anything
visual (race, head parts, tints, weight, outfits, voice, names): those belong to the look mod.

For every NPC the patch overrides, the record it overrides (the master's yaml) is the "from" side and the patch's record the "to" side.

Usage: python build_baseline_profile.py <patch yaml dir> <patch plugin file name> <profile id> <profile name> <out json> <masters-yaml: Plugin=dir ...>
"""
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_forward_profile import CR, blocks, confidence, factions, flags, packages, ref, single  # noqa: E402

REF = r"([0-9A-F]{6}:[^\n]+)"


def refs_of(b, key):
    return [ref(x) for x in re.findall(r"^- " + REF + "$", b.get(key, ""), re.M)]


def perks(b):
    out = collections.OrderedDict()
    for m in re.finditer(r"^- Perk: " + REF + r"\n(?:  Rank: (-?\d+)\n)?", b.get("Perks", ""), re.M):
        out[ref(m.group(1))] = int(m.group(2) or 1)
    return out


def skills(b):
    t = b.get("PlayerSkills", "")
    out = {}
    for part, name in (("SkillValues", "values"), ("SkillOffsets", "offsets")):
        m = re.search(r"  " + part + r":\n((?:  - Key: \w+\n    Value: -?\d+\n)+)", t)
        out[name] = {k: int(v) for k, v in re.findall(r"Key: (\w+)\n    Value: (-?\d+)", m.group(1))} if m else {}
    for key in ("Health", "Magicka", "Stamina"):
        m = re.search(r"^  " + key + r": (-?\d+)$", t, re.M)
        if m:
            out[key.lower()] = int(m.group(1))
    return out


def main(a):
    src, plugin, pid, pname, out_json = a[0], a[1], a[2], a[3], a[4]
    mast = dict(x.split("=", 1) for x in a[5:])
    # prior=<yaml Npcs dir>: an earlier patch this one sits on (USMP on USSEP): where it has the NPC, ITS record is the "from" side
    # priority=<n>: the order the profiles are applied in (the engine goes from the lowest up)
    prior_dir = mast.pop("prior", None)
    priority = int(mast.pop("priority", 0))
    npcs_dir = os.path.join(src, "Npcs")
    prior = {}
    if prior_dir:
        for g in os.listdir(prior_dir):
            m = re.match(r".* - ([0-9A-F]{6})_(.+?)\.yaml$", g)
            if m:
                prior[(m.group(1), m.group(2))] = os.path.join(prior_dir, g)
    index = {}
    for origin, d in mast.items():
        for g in os.listdir(d):
            m = re.match(r".* - ([0-9A-F]{6})_(.+?)\.yaml$", g)
            if m and m.group(2) == origin:
                index[(m.group(1), origin)] = os.path.join(d, g)
    entries = []
    skipped = collections.Counter()
    for f in sorted(os.listdir(npcs_dir)):
        m = re.match(r"(.*) - ([0-9A-F]{6})_(.+?)\.yaml$", f)
        name, fid, origin = m.groups()
        orig = prior.get((fid, origin)) or index.get((fid, origin))
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
        fl = sorted((flags(A) - flags(P)) & {"Protected", "Essential"})
        if fl:
            e["flagsAdded"] = fl
        ca, cp = confidence(A), confidence(P)
        if ca is not None and ca != cp:
            e["confidence"] = {"from": cp, "to": ca}

        for key, short in (("Class", "class"), ("CrimeFaction", "crimeFaction"), ("CombatStyle", "combatStyle")):
            x, y = single(P, key), single(A, key)
            if y and x != y:
                e[short] = {"from": x, "to": y}

        sa, sp = skills(A), skills(P)
        sk = {}
        for part in ("values", "offsets"):
            ch = {k: {"from": sp.get(part, {}).get(k), "to": v} for k, v in sa.get(part, {}).items() if sp.get(part, {}).get(k) != v}
            if ch:
                sk[part] = ch
        for key in ("health", "magicka", "stamina"):
            if key in sa and sa[key] != sp.get(key):
                sk[key] = {"from": sp.get(key), "to": sa[key]}
        if sk:
            e["skills"] = sk

        pk_a, pk_p = perks(A), perks(P)
        pk = [{"perk": k, "rank": v} for k, v in pk_a.items() if k not in pk_p]
        if pk:
            e["perksAdded"] = pk
        sp_a, sp_p = refs_of(A, "ActorEffect"), refs_of(P, "ActorEffect")
        if [x for x in sp_a if x not in sp_p]:
            e["spellsAdded"] = [x for x in sp_a if x not in sp_p]
        kw_a, kw_p = refs_of(A, "Keywords"), refs_of(P, "Keywords")
        if [x for x in kw_a if x not in kw_p]:
            e["keywordsAdded"] = [x for x in kw_a if x not in kw_p]

        if len(e) > 2:
            entries.append(e)
        else:
            skipped["no gameplay difference"] += 1
    prof = {"format": 1, "id": pid, "name": pname, "type": "forward", "family": pid, "baseline": True, "priority": priority,
            "requires": plugin, "ownPlugins": [plugin], "npcs": entries}
    os.makedirs(os.path.dirname(out_json) or ".", exist_ok=True)
    json.dump(prof, open(out_json, "w", encoding="utf-8"), indent=1)
    kinds = collections.Counter(k for e in entries for k in e if k not in ("npc", "name", "packagesRemoved", "packagesBefore"))
    print("profile", pid, ":", len(entries), "NPCs; skipped", dict(skipped))
    print("what it carries:", dict(kinds))


if __name__ == "__main__":
    main(sys.argv[1:])
