# NPC AI AutoPatch

An SKSE plugin that makes the AI overhauls of your load order reach the NPCs other mods add or replace, at game start. No patch plugin, no master, nothing written to your save.

It does two things:

1. **Puts an AI overhaul's AI back on the NPCs a later mod overrides.** Look replacers (High Poly NPC Overhaul and the many single-NPC replacers) win the NPC record and drop the AI overhaul's packages, factions, override lists and confidence. This restores them, the way a Synthesis or xEdit patch would, without a plugin. Supported: **AI Overhaul** (SpiderAkira / mnikjom), versions 1.9 and 1.9.5.
2. **Gives the people mods add to Skyrim's towns the AI of Immersive Citizens - AI Overhaul** (ICAIO): Interesting NPCs (3DNPC) and many more. ICAIO hands its AI out through quests, and a person a mod adds to a town is in none of them, so they walk through rain and dragon attacks like a vanilla NPC. They get what ICAIO gives the extra citizens of that town: its package, its combat override list, its citizens faction and its exclusion faction.

A plugin that must win (Better Vampires, Relationship Dialogue Overhaul, Nether's Follower Framework) is never overruled: an NPC one of them touches is left exactly as it is.

## Versions of an overhaul
AI Overhaul 1.9 (needs USSEP and uses its fix packages) and 1.9.5 (ships its own copies) are not the same plugin. Each has its own profile, and the plugin applies **only the one that fits the loaded game best** (the fewest references that name no record). You do not choose; the menu and the log show which one was used and how many references it could not find. To force one, put `"profile": "<profile name in lower case>"` in `config.json`.

## In-game pages
SKSE Menu Framework (optional) adds the section **NPC AI AutoPatch**:

- **Overview**: the overhauls found, which profile is used, switches, counts.
- **People**: every NPC that was changed or left alone, and why. Searchable.
- **Inspect**: pick a person (selected in the console, under the crosshair, or by name) and see what the game has on them next to what the overhaul gives them: packages in order, the package the overhaul removes, factions, the four override lists, Essential / Protected, confidence. Green = the overhaul's value is there, red = it is not. Read-only. A save can keep an actor value the base record no longer has: check the live value with `getav confidence` in the console.
- **Mods** and **Skipped**: leave a mod's people out; why people were skipped.

Full detail is also written to `Data/SKSE/Plugins/NPCAIAutoPatch/report.json`.

## Rules for the town step
A person is given ICAIO's AI when a mod defines them, they are a person (race with ActorTypeNPC), they are in a town's faction and that hold's crime faction (18 towns), and they are not in a guard, war, Thalmor, Penitus Oculatus, Companions, College, Dark Brotherhood, Thieves Guild, Forsworn, Imperial justiciar or Dawnguard faction, not already in ICAIO's citizens or exclusion faction, do not already have an ICAIO package, and do not take their factions or packages from a template. Followers and marriage candidates are included (a switch turns that off). People whose name contains "corpse" are left alone.

## Settings
`Data/SKSE/Plugins/NPCAIAutoPatch/config.json` (created when you change a switch): `apply` (default true; false only reports), `followers` (true), `forward` (true), `icaio` (true), `offProfiles` (profiles switched off), `profile` (see above), `wins` (plugins whose NPCs are never overruled), `excludePlugins` (plugin names, lower case), `excludeNames` (words in a name; default `corpse`).

## Install
With your mod manager. The installer (FOMOD) asks which DLL you want: **Skyrim 1.6.1170 and newer** (SE / AE, including 1.7.x), or **Skyrim VR and 1.6.1130 and older**. On 1.6.1170 either works. The files are `SKSE/Plugins/NPCAIAutoPatch.dll`, `SKSE/Plugins/NPCAIAutoPatch/profiles/*.json` and `SKSE/Plugins/NPCAIAutoPatch/lang/*.json`.

## Languages
The in-game pages follow the language Windows is set to: English, Spanish, German, Japanese, Simplified Chinese and Korean. Change it in **Overview > Language**, or set `language` in `config.json` (`auto`, `en`, `es`, `de`, `ja`, `zh`, `ko`). The translations are AI-made: corrections are welcome. Add a language by adding `lang/<code>.json` (the English text of a string is its key; what a file lacks stays English).

Japanese, Chinese and Korean need a font that SKSE Menu Framework only loads when you turn it on: set `EnableJapanese`, `EnableChinese` or `EnableKorean` to `true` in `Data/SKSE/Plugins/SKSEMenuFramework.ini` (section `[Fonts]`) and restart. Until then, an automatic setting stays in English and the page says which setting to turn on.

## Requirements
Skyrim SE/AE 1.6.1170 or newer (or Skyrim VR / 1.6.1130 and older with the older DLL), SKSE and Address Library. The two overhauls are independent and each is optional: with only AI Overhaul (1.9 or 1.9.5) it restores its AI on overridden NPCs; with only Immersive Citizens - AI Overhaul 0.4.0.2 it gives its AI to mod-added town people; with both it does both. Whichever is not installed is skipped. SKSE Menu Framework for the in-game pages (optional).

## Making a profile
`profiles/*.json` hold, per NPC, the references an overhaul's plugin gives it (packages, removed packages, factions, override lists, flags, confidence). `tools/build_forward_profile.py` builds one from a Spriggit export of the overhaul's plugin; it can be rerun for a new version. Nothing in a profile is a master.

## Credits
- **AI Overhaul** is by SpiderAkira and mnikjom (Nexus mod 21654). The profiles list references taken from its plugin; this plugin needs the mod installed and contains none of its files. Please endorse and support it.
- **Immersive Citizens - AI Overhaul** is by Arnaud dOrchymont (Nexus mod 173).
- The idea of a patcher that gives mod NPCs ICAIO's AI comes from Wolfman5750's ICAIO AI for Modded NPCs (a Synthesis patcher); no code from it is used here.

## AI use
The code was written with AI assistance for Verification and tested in game by the author. Scripts and results were checked by hand against xEdit and the game.


## License
GNU General Public License, version 3 or later; see `LICENSE`.
