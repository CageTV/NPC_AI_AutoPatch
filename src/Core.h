/*
 * NPC AI AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace RE
{
	class TESNPC;
	class Actor;
}

namespace Core
{
	// Data/SKSE/Plugins/NPCAIAutoPatch/config.json
	struct Config
	{
		bool                  apply = true;       // false: only write the report
		bool                  followers = true;   // NPCs that can become followers (3DNPC's, for example) get the AI too
		std::set<std::string> exclude;            // plugin file names (lower case) whose NPCs are never touched
		std::set<std::string> excludeNames;       // NPC names containing one of these words (lower case) are never touched
		bool                  forward = true;     // put an AI overhaul's AI back on the NPCs a later mod overrides (profiles/*.json of type forward)
		bool                  icaioStep = true;   // Immersive Citizens - AI Overhaul: give its AI to mod-added town NPCs
		std::set<std::string> offProfiles;        // forward profiles (lower case names) switched off in the menu
		std::string           profile;            // forward profile (lower case name) to use for its family, whatever fits the loaded plugin best (config.json only)
		std::set<std::string> wins;               // plugins whose changes win over every overhaul: an NPC one of them touches is left exactly as it is
	};

	enum class Status { Patched, Planned, Left };

	// one NPC that was looked at
	struct Row
	{
		Status      status = Status::Left;
		std::string name, plugin, town, reason;
	};

	struct Summary
	{
		bool                       ran = false, icaio = false, three = false;
		int                        looked = 0, towns = 0;
		std::map<std::string, int> skipped;           // reason -> NPCs
		std::map<std::string, int> perTown;           // town -> patched or planned
		std::vector<std::string>   townsDisabled;     // towns whose ICAIO records were not found
		std::map<std::string, int> forwarded;         // profile name -> NPCs whose AI was put back
		std::vector<std::string>   profilesMissing;   // profiles whose overhaul plugin is not loaded
		std::vector<std::string>   profilesOff;       // profiles switched off
		int                        icaioGiven = 0;    // NPCs given ICAIO's AI
		std::vector<std::string>   profiles;          // profiles found and used (plugin loaded)
		std::vector<std::string>   profilesNotChosen; // another edition of the same overhaul fits the loaded plugin better
		std::map<std::string, int> profileUnresolved; // profile name -> references that name no record in this game
	};

	// Finds the NPCs that mods add to Skyrim's towns and that Immersive Citizens - AI Overhaul's quests would never pick up, and gives them the AI that
	// ICAIO gives to the extra citizens of that town (its package, its combat override list, its citizens faction and its exclusion faction).
	void Run();

	// What the loaded game has on one NPC next to what the overhaul's profile says it should have (the Inspect page).
	struct InspectRow
	{
		std::string what, now, want;
		int         state = 0;  // 0 information, 1 matches the overhaul, 2 differs from the overhaul
	};

	struct Inspection
	{
		bool                    valid = false;
		std::string             title, winner, profile, note;
		std::vector<InspectRow> rows;
	};

	Inspection                                         Inspect(RE::TESNPC* a_npc, RE::Actor* a_live);
	std::vector<std::pair<std::string, std::uint32_t>> FindInProfile(const std::string& a_needle, std::size_t a_max);

	Config&                 Cfg();
	void                    SaveConfig();
	const Summary&          Info();
	const std::vector<Row>& Rows();
}
