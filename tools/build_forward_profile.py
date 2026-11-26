"""Builds a "forward" profile for the AI overhaul patcher from an overhaul plugin's Spriggit yaml.

For every NPC the overhaul overrides, compares its record with the record it overrides (the master's yaml) and writes only the AI part of the difference:
packages (the overhaul's list in order, and the ones it removed), factions it added, the spectator / combat / observe / guard-warn override lists, the
Protected / Essential flags and the confidence; since 0.5.0 also factions it removed, the combat style, aggression, assistance, health / magicka /
stamina offsets, spells it added and inventory items it added. The result is data: it names records by 'Plugin.esp|LocalID', no overhaul is a master of anything.

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
BEHAVIOUR = False  # make_profile.py --behaviour: a behaviour mod (Serana - Lamae's Daughter) also carries class, combat style "none", removed spells and added perks
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


AGGR = ["Unaggressive", "Aggressive", "VeryAggressive", "Frenzied"]
ASSIST = ["HelpsNobody", "HelpsAllies", "HelpsFriendsAndAllies"]  # Spriggit / Mutagen names (not "HelpsFriends")
TEMPLATE_FLAGS = ["Traits", "Stats", "Factions", "SpellList", "AIData", "AIPackages", "ModelAnimation", "BaseData", "Inventory", "Script",
                  "DefPackList", "AttackData", "Keywords"]


def template_flags(b):
    t = b.get("Configuration", "")
    m = re.search(r"  TemplateFlags:\n((?:  - [^\n]+\n)+)", t)
    return set(re.findall(r"- (\w+)", m.group(1))) if m else set()


def ai_level(b, key, names):
    # Spriggit leaves out a value that is the default (index 0)
    if "AIData" not in b:
        return None
    m = re.search(r"^  " + key + r": (\w+)$", b["AIData"], re.M)
    return names.index(m.group(1)) if m and m.group(1) in names else 0


def offset(b, key):
    # Configuration HealthOffset / MagickaOffset / StaminaOffset; left out when 0
    if "Configuration" not in b:
        return None
    m = re.search(r"^  " + key + r": (-?\d+)$", b["Configuration"], re.M)
    return int(m.group(1)) if m else 0


def spells(b):
    return [ref(x) for x in re.findall(r"^- ([0-9A-F]{6}:[^\n]+)$", b.get("ActorEffect", ""), re.M)]


def items(b):
    out = collections.OrderedDict()
    for m in re.finditer(r"^- Item:\n    Item: ([0-9A-F]{6}:[^\n]+)\n    Count: (-?\d+)$", b.get("Items", ""), re.M):
        out[ref(m.group(1))] = out.get(ref(m.group(1)), 0) + int(m.group(2))
    return out


def main(a):
    src, plugin, pid, pname, out_json = a[0], a[1], a[2], a[3], a[4]
    mast = dict(x.split("=", 1) for x in a[5:])
    mast_l = {k.lower(): v for k, v in mast.items()}
    npcs_dir = os.path.join(src, "Npcs")
    entries = []
    skipped = collections.Counter()
    for f in sorted(os.listdir(npcs_dir)):
        m = re.match(r"(.*) - ([0-9A-F]{6})_(.+?)\.yaml$", f)
        name, fid, origin = m.groups()
        d = mast.get(origin) or mast_l.get(origin.lower())  # plugin names differ in case between plugins' headers
        orig = None
        if d:
            for g in os.listdir(d):
                if g.lower().endswith((" - %s_%s.yaml" % (fid, origin)).lower()):
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
        # 0.5.0: the rest of what an AI overhaul changes on the NPC itself (found in the community SkyPatcher patch for AI Overhaul)
        gone = [k for k in fp if k not in fa]
        if gone:
            e["factionsRemoved"] = gone
        x, y = single(P, "CombatStyle"), single(A, "CombatStyle")
        if y and x != y:
            e["combatStyle"] = {"from": x, "to": y}
        if BEHAVIOUR:
            from build_baseline_profile import perks
            cx, cy = single(P, "Class"), single(A, "Class")
            if cy and cx != cy:
                e["class"] = {"from": cx, "to": cy}
            if x and not y:
                e["combatStyle"] = {"from": x, "to": ""}  # the behaviour mod clears the combat style
            pk_a, pk_p = perks(A), perks(P)
            pk = [{"perk": k, "rank": v} for k, v in pk_a.items() if k not in pk_p]
            if pk:
                e["perksAdded"] = pk
            gone_s = [s for s in spells(P) if s not in spells(A)]
            if gone_s:
                e["spellsRemoved"] = gone_s
        for key, short, names in (("Aggression", "aggression", AGGR), ("Assistance", "assistance", ASSIST)):
            va, vp = ai_level(A, key, names), ai_level(P, key, names)
            if va is not None and va != vp:
                e[short] = {"from": vp if vp is not None else 0, "to": va}
        offs = {}
        for key, short in (("HealthOffset", "health"), ("MagickaOffset", "magicka"), ("StaminaOffset", "stamina")):
            va, vp = offset(A, key), offset(P, key)
            if va is not None and va != vp:
                offs[short] = {"from": vp if vp is not None else 0, "to": va}
        if offs:
            e["statOffsets"] = offs
        sa, sp = spells(A), spells(P)
        if [s for s in sa if s not in sp]:
            e["spellsAdded"] = [s for s in sa if s not in sp]
        ia, ip = items(A), items(P)
        add_items = [{"item": k, "count": v} for k, v in ia.items() if k not in ip]
        if add_items:
            e["itemsAdded"] = add_items
        # skill values / offsets and base health / magicka / stamina (AI Overhaul retunes some College mages); same shape as a baseline profile
        from build_baseline_profile import skills  # late import: that module imports this one
        sa_, sp_ = skills(A), skills(P)
        sk = {}
        for part in ("values", "offsets"):
            ch = {k: {"from": sp_.get(part, {}).get(k), "to": v} for k, v in sa_.get(part, {}).items() if sp_.get(part, {}).get(k) != v}
            if ch:
                sk[part] = ch
        for key in ("health", "magicka", "stamina"):
            if key in sa_ and sa_[key] != sp_.get(key):
                sk[key] = {"from": sp_.get(key), "to": sa_[key]}
        if sk:
            e["skills"] = sk
        # template flags the overhaul turned off (Brutius: "use the template's AI packages" off, so the overhaul's own packages count)
        tf = sorted(f for f in template_flags(P) - template_flags(A) if f in TEMPLATE_FLAGS)
        if tf:
            e["templateFlagsRemoved"] = tf
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
