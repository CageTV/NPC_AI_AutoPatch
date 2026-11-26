# NPC AI AutoPatch

An SKSE plugin that makes the AI overhauls of your load order reach the NPCs other mods add or replace, at game start. No patch plugin, no master, nothing written to your save.

It does three things:

1. **Puts an AI overhaul's AI back on the NPCs a later mod overrides.** Look replacers (High Poly NPC Overhaul and the many single-NPC replacers) win the NPC record and drop the AI overhaul's packages, factions, override lists, confidence, combat style, aggression, assistance, skills, stat offsets, spells and items. This restores them, the way a Synthesis or xEdit patch would, without a plugin. Supported: **AI Overhaul** (SpiderAkira / mnikjom), versions 1.9 and 1.9.5.
2. **Puts a bug-fix patch's gameplay data back on the NPCs a later mod took over.** A look replacer built from the vanilla record also drops what **USSEP** (and **USMP**) fixed on that NPC: the class, the skill values, the perks, spells and keywords, the crime faction and combat style, the factions and the packages. For every NPC the patch changes, the plugin compares the winning record with the base game's: where the winner still has the base game's value, the patch's value is put back; a value the look mod chose itself is kept. Nothing visual is ever touched (race, face, hair, outfit, voice, name). People who take a field from a template are left alone. USSEP's own record is never overruled, and an AI Overhaul profile decides the packages of the people it covers.
3. **Gives the people mods add to Skyrim's towns the AI of Immersive Citizens - AI Overhaul** (ICAIO): Interesting NPCs (3DNPC) and many more. ICAIO hands its AI out through quests, and a person a mod adds to a town is in none of them, so they walk through rain and dragon attacks like a vanilla NPC. They get what ICAIO gives the extra citizens of that town: its package, its combat override list, its citizens faction and its exclusion faction.

A plugin that must win (Better Vampires, Relationship Dialogue Overhaul, Nether's Follower Framework, Serana - Lamae's Daughter (Vampire Lord AI), NPC Spell Variance) is never overruled: an NPC one of them touches is left exactly as it is.

## Versions of an overhaul
AI Overhaul 1.9 (needs USSEP and uses its fix packages) and 1.9.5 (ships its own copies) are not the same plugin. Each has its own profile, and the plugin applies **only the one that fits the loaded game best** (the fewest references that name no record). You do not choose; the menu and the log show which one was used and how many references it could not find. To force one, put `"profile": "<profile name in lower case>"` in `config.json`.

## In-game pages
SKSE Menu Framework (optional) adds the section **NPC AI AutoPatch**:

- **Overview**: the overhauls found, which profile is used, switches, counts.
- **People**: every NPC that was changed or left alone, and why. Searchable.
- **Inspect** (AI overhaul and bug-fix rows): pick a person (selected in the console, under the crosshair, or by name) and see what the game has on them next to what the overhaul gives them: packages in order, the package the overhaul removes, factions, the four override lists, Essential / Protected, confidence, removed factions, combat style, aggression, assistance, stat offsets, skills, spells, items, template flags, and for the bug-fix patch the class, skills, perks, keywords and factions it gives them. Green = the overhaul's value is there, red = it is not, grey = information. Grey "Not changed by ..." rows say why a step left this person alone. Read-only. A save can keep an actor value the base record no longer has: check the live value with `getav confidence` in the console.
- **Mods**: the plugins whose changes always win (add or remove one, back to the default list; each shows whether it is loaded and how many people it kept the patcher away from) and the mods whose people are left out. **Skipped**: why people were skipped.

Full detail is also written to `Data/SKSE/Plugins/NPCAIAutoPatch/report.json`.

## Rules for the town step
The towns, packages and factions come from a profile too (`profiles/icaio.json`, type `town`), so nothing about ICAIO is built into the plugin. A person is given ICAIO's AI when a mod defines them, they are a person (race with ActorTypeNPC), they are in a town's faction and that hold's crime faction (18 towns), and they are not in a guard, war, Thalmor, Penitus Oculatus, Companions, College, Dark Brotherhood, Thieves Guild, Forsworn, Imperial justiciar or Dawnguard faction, not already in ICAIO's citizens or exclusion faction, do not already have an ICAIO package, and do not take their factions or packages from a template. Followers and marriage candidates are included (a switch turns that off). People whose name contains "corpse" are left alone.

## Settings
`Data/SKSE/Plugins/NPCAIAutoPatch/config.json` (created when you change a switch): `apply` (default true; false only reports), `followers` (true), `forward` (true), `baseline` (true: the bug-fix patches), `icaio` (true), `offProfiles` (profiles switched off), `profile` (see above), `wins` (plugins whose NPCs are never overruled; editable on the Mods page), `excludePlugins` (plugin names, lower case), `excludeNames` (words in a name; default `corpse`).

## Install
With your mod manager. The installer (FOMOD) asks which DLL you want: **Skyrim 1.6.1170 and newer** (SE / AE, including 1.7.x), or **Skyrim VR and 1.6.1130 and older**. On 1.6.1170 either works. The files are `SKSE/Plugins/NPCAIAutoPatch.dll`, `SKSE/Plugins/NPCAIAutoPatch/profiles/*.json` and `SKSE/Plugins/NPCAIAutoPatch/lang/*.json`.

## Languages
The in-game pages follow the language Windows is set to: English, Spanish, German, Japanese, Simplified Chinese and Korean. Change it in **Overview > Language**, or set `language` in `config.json` (`auto`, `en`, `es`, `de`, `ja`, `zh`, `ko`). The translations are AI-made: corrections are welcome. Add a language by adding `lang/<code>.json` (the English text of a string is its key; what a file lacks stays English).

Japanese, Chinese and Korean need a font that SKSE Menu Framework only loads when you turn it on: set `EnableJapanese`, `EnableChinese` or `EnableKorean` to `true` in `Data/SKSE/Plugins/SKSEMenuFramework.ini` (section `[Fonts]`) and restart. Until then, an automatic setting stays in English and the page says which setting to turn on.

## Requirements
Skyrim SE/AE 1.6.1170 or newer (or Skyrim VR / 1.6.1130 and older with the older DLL), SKSE and Address Library. The two overhauls are independent and each is optional: with only AI Overhaul (1.9 or 1.9.5) it restores its AI on overridden NPCs; with only Immersive Citizens - AI Overhaul 0.4.0.2 it gives its AI to mod-added town people; with both it does both. Whichever is not installed is skipped. SKSE Menu Framework for the in-game pages (optional).

## Making a profile
A profile (`profiles/*.json`) names, per NPC, what a plugin gives that NPC, as `Plugin.esp|LocalID` references; nothing in it is a master. Three kinds:

- **forward** (an AI overhaul): packages, removed packages, factions added and removed, the four override lists, Protected / Essential, confidence, combat style, aggression, assistance, skills, health / magicka / stamina offsets, spells, inventory items, template flags it turned off. Header: `"type": "forward"`, `"requires": "<plugin>"`, `"ownPlugins": [...]`, optional `"family"` (versions of one overhaul: only the best-fitting one is used) and `"priority"`.
- **town** (an overhaul that has "extra citizen" records per town, like ICAIO): `citizens` and `exclusion` (the factions given to the people), `skipFactions` (factions whose members are left alone), `follower` (the faction that marks a possible follower) and `towns`: per town its `crime` faction, its `town` faction, the `package` and the `combat` override list its extra citizens get. Header: `"type": "town"`, `"requires"`, `"ownPlugins"`. Written by hand: see `profiles/icaio.json`.
- **baseline** (a bug-fix patch): class, skill values, health / magicka / stamina, perks, spells, keywords, crime faction, combat style, packages, factions, Protected / Essential, confidence. Header adds `"baseline": true`; `"priority"` is the order profiles are applied in (lowest first).

To make one from a plugin you have, with one command (needs Python 3 and [Spriggit](https://github.com/Mutagen-Modding/Spriggit) on your PATH):

```
python tools/make_profile.py --kind forward  --plugin "<path>/AI Overhaul.esp" --id my-overhaul --name "My Overhaul" --out profiles/my-overhaul.json
python tools/make_profile.py --kind baseline --plugin "<path>/SomePatch.esp"   --id some-patch  --name "Some Patch"  --out profiles/some-patch.json
```

It reads the plugin's masters from its header, finds them in the game's Data folder (`--data`, or `--search <mod folder>` for masters that live in a mod), exports them with Spriggit (cached, so the next profile is fast), compares the plugin's NPC records with the originals and writes the profile. `--prior <plugin>` makes a baseline patch start from an earlier patch's records (USMP on USSEP); `--base latest` compares with the masters' latest records instead of the original ones (default `original`: a patch like USSEP then also carries what Update.esm fixed, which a look replacer built from the vanilla record loses too). Read the printed counts, then copy the file to `Data/SKSE/Plugins/NPCAIAutoPatch/profiles/`. The lower-level scripts `tools/build_forward_profile.py` and `tools/build_baseline_profile.py` take an existing export. Rerun it for a new version of the plugin.

Shipped profiles: AI Overhaul 1.9 (422 NPCs) and 1.9.5 (420), Immersive Citizens - AI Overhaul 0.4.0.2 (18 towns), USSEP 4.3.9c (498) and USMP 2.6.8b (24; mostly housecarl packages, because USMP's NPC changes are mainly on dragons and creatures). Each is used only when its plugin is loaded.

## Credits
- **AI Overhaul** is by SpiderAkira and mnikjom (Nexus mod 21654). The profiles list references taken from its plugin; this plugin needs the mod installed and contains none of its files. Please endorse and support it.
- **Unofficial Skyrim Special Edition Patch (USSEP)** is by The Unofficial Patch Project Team (Nexus mod 266) and **Unofficial Skyrim Modder's Patch (USMP)** by The Community, PlagueHush and sattyre (Nexus mod 49616). The profiles list the values their plugins give an NPC; this plugin needs the mods installed and contains none of their files. Please endorse and support them.
- **Immersive Citizens - AI Overhaul** is by Arnaud dOrchymont (Nexus mod 173).
- The idea of a patcher that gives mod NPCs ICAIO's AI comes from Wolfman5750's ICAIO AI for Modded NPCs (a Synthesis patcher); no code from it is used here.

## AI use
The code was written with AI assistance for Verification and tested in game by the author. Scripts and results were checked by hand against xEdit and the game.


## License
GNU General Public License, version 3 or later; see `LICENSE`.
