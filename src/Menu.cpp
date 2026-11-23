/*
 * NPC AI AutoPatch
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
			auto&       cfg = Core::Cfg();
			SeparatorText("NPC AI AutoPatch");
			TextWrapped("Works with the AI overhauls of your load order, with none of them as a master. At game start it gives the people of mods the AI an overhaul gives "
			            "its own citizens, and puts an overhaul's AI back on the NPCs a later mod overrides. A plugin that must win (Better Vampires) is never overruled. "
			            "Nothing is written to your save and no plugin is added.");
			Spacing();
			SeparatorText("AI overhauls");
			int icaioN = 0;
			for (const auto& r : Core::Rows()) {
				icaioN += (r.status == Core::Status::Patched && r.town.rfind("forward", 0) != 0) ? 1 : 0;
			}
			if (BeginTable("##ov", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				TableSetupColumn("Overhaul", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.0f);
				TableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				TableSetupColumn("Use", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				TableHeadersRow();
				TableNextRow();
				TableSetColumnIndex(0);
				TextUnformatted("Immersive Citizens - AI Overhaul (mod-added town people)");
				TableSetColumnIndex(1);
				TextUnformatted(!info.icaio ? "not loaded" : cfg.icaioStep ? "active" : "off");
				TableSetColumnIndex(2);
				Text("%d", icaioN);
				TableSetColumnIndex(3);
				if (info.icaio) {
					bool v = cfg.icaioStep;
					if (Checkbox("##icaio", &v)) {
						cfg.icaioStep = v;
						Core::SaveConfig();
						needsRestart = true;
					}
				}
				const auto row = [&](const std::string& n, const std::string& state, int people, bool can) {
					TableNextRow();
					TableSetColumnIndex(0);
					TextUnformatted(n.c_str());
					TableSetColumnIndex(1);
					TextUnformatted(state.c_str());
					TableSetColumnIndex(2);
					Text("%d", people);
					TableSetColumnIndex(3);
					if (can) {
						const std::string key = Lower(n);
						bool              v = cfg.offProfiles.count(key) == 0;
						if (Checkbox(("##p" + n).c_str(), &v)) {
							if (v) {
								cfg.offProfiles.erase(key);
							} else {
								cfg.offProfiles.insert(key);
							}
							Core::SaveConfig();
							needsRestart = true;
						}
					}
				};
				// the number after the state is how many of the profile's references name no record in this game (nothing shown = the profile fits the loaded plugin)
				const auto lacking = [&](const std::string& n) {
					const auto u = info.profileUnresolved.find(n);
					return u == info.profileUnresolved.end() || u->second == 0 ? std::string() : " (" + std::to_string(u->second) + " not found)";
				};
				for (const auto& n : info.profiles) {
					const auto it = info.forwarded.find(n);
					row(n, "active" + lacking(n), it == info.forwarded.end() ? 0 : it->second, true);
				}
				for (const auto& n : info.profilesNotChosen) {
					row(n, "not used: another version fits better" + lacking(n), 0, false);
				}
				for (const auto& n : info.profilesOff) {
					row(n, "off", 0, true);
				}
				for (const auto& n : info.profilesMissing) {
					row(n, "not loaded", 0, false);
				}
				EndTable();
			}
			TextDisabled("A loaded overhaul is used automatically; untick one to leave it out. Where an overhaul has several versions (AI Overhaul 1.9 and 1.9.5, for example) the one that fits the loaded plugin best is used. Profiles: Data/SKSE/Plugins/NPCAIAutoPatch/profiles.");
			if (Checkbox("Apply the changes", &cfg.apply)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("Off: the pages show what would be done, nothing is changed in the game.");
			if (Checkbox("Give the town AI to followers and marriage candidates too", &cfg.followers)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("People who can become your followers get the town's package like everyone else. The package only acts in rain, snow, "
			    "or when a dragon or vampire attacks, and never while the person follows you.");
			if (needsRestart) {
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Saved. Restart the game for it to take effect.");
			}
			Spacing();
			SeparatorText("This game session");
			Text("Changed: %d    Planned: %d    Left alone: %d", Count(Core::Status::Patched), Count(Core::Status::Planned), Count(Core::Status::Left));
			if (!info.townsDisabled.empty()) {
				std::string t;
				for (const auto& n : info.townsDisabled) {
					t += (t.empty() ? "" : ", ") + n;
				}
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Towns left out (ICAIO records not found): %s", t.c_str());
			}
			if (!info.perTown.empty() && BeginTable("##towns", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				TableSetupColumn("Town (mod-added people)", ImGuiTableColumnFlags_WidthStretch, 3.0f);
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
			TextDisabled("Full detail: Data/SKSE/Plugins/NPCAIAutoPatch/report.json");
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

		// ---- Inspect: what the game has on one person against what the AI overhaul's profile says
		struct InspectState
		{
			Core::Inspection cur;
			std::uint32_t    npc = 0;
			RE::ActorHandle  live;
			char             search[64] = "";
			std::string      msg;
		};
		InspectState ins;

		void Refresh()
		{
			auto* npc = ins.npc ? RE::TESForm::LookupByID<RE::TESNPC>(ins.npc) : nullptr;
			auto  a = ins.live.get();
			ins.cur = Core::Inspect(npc, a.get());
		}

		void PickRef(RE::TESObjectREFR* a_ref)
		{
			auto* actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
			auto* base = actor ? actor->GetActorBase() : nullptr;
			if (!base) {
				ins.msg = "That is not a person. Click one in the console (open it with ~) or aim at one.";
				return;
			}
			ins.msg.clear();
			ins.npc = base->GetFormID();
			ins.live = actor->CreateRefHandle();
			Refresh();
		}

		void __stdcall RenderInspect()
		{
			SeparatorText("Check one person");
			TextWrapped("Shows what the game has on a person right now (packages, factions, override lists, flags, confidence) next to what the AI overhaul gives them. "
			            "Green: the overhaul's value is there. Red: it is not. Grey: information, nothing to compare. Nothing here changes the game.");
			if (Button("Person selected in the console")) {
				auto ref = RE::Console::GetSelectedRef();
				PickRef(ref.get());
			}
			Tip("Open the console (~), click the person, then press this. Opening the menu does not clear the console's selection.");
			SameLine();
			if (Button("Person under the crosshair")) {
				auto* pick = RE::CrosshairPickData::GetSingleton();
				auto  ref = pick ? pick->target.get() : RE::NiPointer<RE::TESObjectREFR>();
				PickRef(ref.get());
			}
			SameLine();
			if (ins.cur.valid && Button("Refresh")) {
				Refresh();
			}
			SetNextItemWidth(220.0f);
			InputText("Find by name##i", ins.search, sizeof(ins.search));
			Tip("Searches the people the loaded AI overhaul profile changes. The record is shown; the actor values only when you picked the person in the world.");
			if (ins.search[0]) {
				for (const auto& [label, id] : Core::FindInProfile(ins.search, 12)) {
					if (SmallButton((label + "##" + std::to_string(id)).c_str())) {
						ins.npc = id;
						ins.live = RE::ActorHandle();
						ins.msg.clear();
						Refresh();
					}
				}
			}
			if (!ins.msg.empty()) {
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", ins.msg.c_str());
			}
			if (!ins.cur.valid) {
				return;
			}
			Spacing();
			SeparatorText(ins.cur.title.c_str());
			Text("Record last changed by: %s", ins.cur.winner.c_str());
			if (!ins.cur.profile.empty()) {
				Text("Compared with: %s", ins.cur.profile.c_str());
			}
			if (!ins.cur.note.empty()) {
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", ins.cur.note.c_str());
			}
			const float h = std::max(180.0f, GetContentRegionAvail().y - 10.0f);
			if (BeginTable("##insp", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
			               ImVec2(0.0f, h))) {
				TableSetupScrollFreeze(0, 1);
				TableSetupColumn("What", ImGuiTableColumnFlags_WidthStretch, 3.0f);
				TableSetupColumn("In the game now", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				TableSetupColumn("The overhaul gives", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
				TableHeadersRow();
				for (const auto& r : ins.cur.rows) {
					const ImVec4 col = r.state == 1 ? ImVec4(0.55f, 0.85f, 0.6f, 1.0f) : r.state == 2 ? ImVec4(0.95f, 0.45f, 0.4f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
					TableNextRow();
					TableSetColumnIndex(0);
					TextColored(col, "%s", r.what.c_str());
					TableSetColumnIndex(1);
					TextColored(col, "%s", r.now.c_str());
					TableSetColumnIndex(2);
					TextColored(col, "%s", r.want.c_str());
					TableSetColumnIndex(3);
					TextColored(col, "%s", r.state == 1 ? "ok" : r.state == 2 ? "!!" : "");
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
			if (!info.ran && Core::Rows().empty()) {
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
		SKSEMenuFramework::SetSection("NPC AI AutoPatch");
		SKSEMenuFramework::AddSectionItem("Overview", RenderOverview);
		SKSEMenuFramework::AddSectionItem("People", RenderNpcs);
		SKSEMenuFramework::AddSectionItem("Inspect", RenderInspect);
		SKSEMenuFramework::AddSectionItem("Mods", RenderMods);
		SKSEMenuFramework::AddSectionItem("Skipped", RenderSkipped);
		SKSE::log::info("Registered with SKSE Menu Framework {}", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
