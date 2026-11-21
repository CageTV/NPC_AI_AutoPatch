/*
 * ICAIO AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#include "PCH.h"
#include "Menu.h"
#include "Core.h"

#include "SKSEMenuFramework.h"

namespace Menu
{
	namespace
	{
		using namespace ImGuiMCP;

		bool needsRestart = false;

		std::string Lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		void Tip(const char* tip)
		{
			if (IsItemHovered()) {
				BeginTooltip();
				PushTextWrapPos(420.0f);
				TextUnformatted(tip);
				PopTextWrapPos();
				EndTooltip();
			}
		}

		void Flag(const char* a_name, bool a_on)
		{
			TextColored(a_on ? ImVec4(0.55f, 0.85f, 0.6f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s %s", a_on ? "loaded:" : "not loaded:", a_name);
		}

		int Count(Core::Status s)
		{
			int n = 0;
			for (const auto& r : Core::Rows()) {
				n += r.status == s ? 1 : 0;
			}
			return n;
		}

		void __stdcall RenderOverview()
		{
			const auto& info = Core::Info();
			SeparatorText("ICAIO AutoPatch");
			TextWrapped("Immersive Citizens - AI Overhaul gives its AI to the people of Skyrim's towns through quests. The people that other mods add to a town (Interesting NPCs and "
			            "many more) are in the town's factions but not in any of ICAIO's quests, so they walk through rain and dragon attacks like vanilla NPCs. At game start this "
			            "gives each of them what ICAIO gives its extra citizens of that town: its package, its combat override list and its citizens faction (and the exclusion "
			            "faction, so ICAIO's quest slots stay free for other people). Nothing is written to your save and no plugin is added.");
			Spacing();
			Flag("Immersive Citizens - AI Overhaul", info.icaio);
			Flag("Interesting NPCs (3DNPC)", info.three);
			if (!info.ran) {
				TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f), "Immersive Citizens - AI Overhaul is not loaded: nothing is changed.");
				return;
			}
			Spacing();
			auto& cfg = Core::Cfg();
			SeparatorText("Switches (applied at the next game start)");
			if (Checkbox("Apply the changes", &cfg.apply)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("Off: the pages show what would be done, nothing is changed in the game.");
			if (Checkbox("Give the AI to followers and marriage candidates too", &cfg.followers)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("People who can become your followers (many of Interesting NPCs' are) get the town's package like everyone else. The package only acts in rain, snow, "
			    "or when a dragon or vampire attacks, and never while the person follows you.");
			if (needsRestart) {
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Saved. Restart the game for it to take effect.");
			}
			Spacing();
			SeparatorText("This game session");
			Text("%d people of mods stand in a town and its hold's crime faction.", info.looked);
			Text("Given the AI: %d    Planned: %d    Left alone: %d", Count(Core::Status::Patched), Count(Core::Status::Planned), Count(Core::Status::Left));
			if (!info.townsDisabled.empty()) {
				std::string t;
				for (const auto& n : info.townsDisabled) {
					t += (t.empty() ? "" : ", ") + n;
				}
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Towns left out (ICAIO records not found): %s", t.c_str());
			}
			if (BeginTable("##towns", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				TableSetupColumn("Town", ImGuiTableColumnFlags_WidthStretch, 3.0f);
				TableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				TableHeadersRow();
				for (const auto& [town, n] : info.perTown) {
					TableNextRow();
					TableSetColumnIndex(0);
					TextUnformatted(town.c_str());
					TableSetColumnIndex(1);
					Text("%d", n);
				}
				EndTable();
			}
			TextDisabled("Full detail: Data/SKSE/Plugins/ICAIOAutoPatch/report.json");
		}

		struct PageState
		{
			int  filter = 0;
			char search[64] = "";
		};
		PageState npcs;

		void __stdcall RenderNpcs()
		{
			if (!Core::Info().ran) {
				TextDisabled("Nothing to show.");
				return;
			}
			static const char* kFilters[] = { "Everything", "Given the AI / planned", "Left alone" };
			SetNextItemWidth(220.0f);
			Combo("Show##n", &npcs.filter, kFilters, 3);
			SameLine();
			SetNextItemWidth(220.0f);
			InputText("Search##n", npcs.search, sizeof(npcs.search));
			const std::string needle = Lower(npcs.search);
			std::vector<const Core::Row*> shown;
			int                           total = 0;
			for (const auto& r : Core::Rows()) {
				if (npcs.filter == 1 && r.status == Core::Status::Left) {
					continue;
				}
				if (npcs.filter == 2 && r.status != Core::Status::Left) {
					continue;
				}
				if (!needle.empty() && Lower(r.name).find(needle) == std::string::npos && Lower(r.plugin).find(needle) == std::string::npos && Lower(r.town).find(needle) == std::string::npos &&
					Lower(r.reason).find(needle) == std::string::npos) {
					continue;
				}
				++total;
				if (shown.size() < 400) {
					shown.push_back(&r);
				}
			}
			Text("%d match%s%s", total, total == 1 ? "" : "es", total > 400 ? " (the first 400 are listed: narrow the search)" : "");
			const float tableHeight = std::max(180.0f, GetContentRegionAvail().y - 10.0f);
			if (BeginTable("##npcs", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
			               ImVec2(0.0f, tableHeight))) {
				TableSetupScrollFreeze(0, 1);
				TableSetupColumn("Person", ImGuiTableColumnFlags_WidthStretch, 2.5f);
				TableSetupColumn("Mod", ImGuiTableColumnFlags_WidthStretch, 2.0f);
				TableSetupColumn("Town", ImGuiTableColumnFlags_WidthFixed, 130.0f);
				TableSetupColumn("What", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				TableHeadersRow();
				for (const auto* r : shown) {
					TableNextRow();
					TableSetColumnIndex(0);
					TextUnformatted(r->name.c_str());
					TableSetColumnIndex(1);
					TextUnformatted(r->plugin.c_str());
					TableSetColumnIndex(2);
					TextUnformatted(r->town.c_str());
					TableSetColumnIndex(3);
					TextUnformatted(r->reason.c_str());
				}
				EndTable();
			}
		}

		void __stdcall RenderMods()
		{
			if (!Core::Info().ran) {
				TextDisabled("Nothing to show.");
				return;
			}
			SeparatorText("Mods whose people are never touched");
			TextWrapped("Tick a mod to leave all of its people out. Takes effect at the next game start.");
			std::map<std::string, int> perMod;
			for (const auto& r : Core::Rows()) {
				++perMod[r.plugin];
			}
			auto& cfg = Core::Cfg();
			for (const auto& [plugin, n] : perMod) {
				bool off = cfg.exclude.count(plugin) > 0;
				const std::string label = plugin + "  (" + std::to_string(n) + " people)";
				if (Checkbox(label.c_str(), &off)) {
					if (off) {
						cfg.exclude.insert(plugin);
					} else {
						cfg.exclude.erase(plugin);
					}
					Core::SaveConfig();
					needsRestart = true;
				}
			}
			if (needsRestart) {
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Saved. Restart the game for it to take effect.");
			}
		}

		void __stdcall RenderSkipped()
		{
			const auto& info = Core::Info();
			if (!info.ran) {
				TextDisabled("Nothing to show.");
				return;
			}
			SeparatorText("Why people were left alone");
			if (BeginTable("##skipped", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch, 5.0f);
				TableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				TableHeadersRow();
				for (const auto& [reason, n] : info.skipped) {
					TableNextRow();
					TableSetColumnIndex(0);
					TextUnformatted(reason.c_str());
					TableSetColumnIndex(1);
					Text("%d", n);
				}
				EndTable();
			}
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			SKSE::log::info("SKSE Menu Framework is not installed: the pages are not available");
			return;
		}
		SKSEMenuFramework::SetSection("ICAIO AutoPatch");
		SKSEMenuFramework::AddSectionItem("Overview", RenderOverview);
		SKSEMenuFramework::AddSectionItem("People", RenderNpcs);
		SKSEMenuFramework::AddSectionItem("Mods", RenderMods);
		SKSEMenuFramework::AddSectionItem("Skipped", RenderSkipped);
		SKSE::log::info("Registered with SKSE Menu Framework {}", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
