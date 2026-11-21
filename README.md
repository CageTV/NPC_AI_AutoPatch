# NPC AI AutoPatch

An SKSE add-on that gives the people that other mods add to Skyrim's towns (Interesting NPCs 3DNPC and many more) the AI of **Immersive Citizens - AI Overhaul** (ICAIO), at game start, with no patch plugin.

ICAIO hands its AI out through quests. A person that a mod adds to a town is in the town's factions but not in any of those quests, so they walk through rain and dragon attacks like a vanilla NPC. This gives each of them what ICAIO gives the extra citizens of that town: its package (`NPCO<Town>ExtraCitizen...`), its combat override list, its citizens faction and its exclusion faction (so ICAIO's quest slots stay free for other people).

## Rules
A person is patched when a mod defines them, they are a person (race with ActorTypeNPC), they are in a town's faction and that hold's crime faction (18 towns), and they are not in a guard, war, Thalmor, Penitus Oculatus, Companions, College, Dark Brotherhood, Thieves Guild, Forsworn, Imperial justiciar or Dawnguard faction, not already in ICAIO's citizens or exclusion faction, do not already have an ICAIO package, and do not take their factions or packages from a template. Followers and marriage candidates are patched too (a switch turns that off). People whose name contains "corpse" are left alone.

Nothing is written to your save. Report: `Data/SKSE/Plugins/NPCAIAutoPatch/report.json` and the SKSE Menu Framework pages (Overview, People, Mods, Skipped).

## Settings
`Data/SKSE/Plugins/NPCAIAutoPatch/config.json` (created when you change a switch): `apply` (default true; false only reports), `followers` (true), `excludePlugins` (plugin names, lower case), `excludeNames` (words in a name; default `corpse`).

## Requirements
Skyrim SE/AE 1.6.1170 or newer, SKSE, Address Library, Immersive Citizens - AI Overhaul 0.4.0.2. SKSE Menu Framework for the pages (optional).

## Credits
Immersive Citizens - AI Overhaul is by Arnaud dOrchymont. The idea of a patcher that gives mod NPCs ICAIO's AI comes from Wolfman5750's ICAIO AI for Modded NPCs (a Synthesis patcher); no code from it is used here.

## License
GNU General Public License, version 3 or later; see `LICENSE`.
