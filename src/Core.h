/*
 * ICAIO AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Core
{
	// Data/SKSE/Plugins/ICAIOAutoPatch/config.json
	struct Config
	{
		bool                  apply = true;       // false: only write the report
		bool                  followers = true;   // NPCs that can become followers (3DNPC's, for example) get the AI too
		std::set<std::string> exclude;            // plugin file names (lower case) whose NPCs are never touched
		std::set<std::string> excludeNames;       // NPC names containing one of these words (lower case) are never touched
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
	};

	// Finds the NPCs that mods add to Skyrim's towns and that Immersive Citizens - AI Overhaul's quests would never pick up, and gives them the AI that
	// ICAIO gives to the extra citizens of that town (its package, its combat override list, its citizens faction and its exclusion faction).
	void Run();

	Config&                 Cfg();
	void                    SaveConfig();
	const Summary&          Info();
	const std::vector<Row>& Rows();
}
