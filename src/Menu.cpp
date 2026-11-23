/*
 * NPC AI AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#include "PCH.h"
#include "Menu.h"
#include "Core.h"
#include "Lang.h"

#include "SKSEMenuFramework.h"

namespace Menu
{
	namespace
	{
		using namespace ImGuiMCP;

		// Every text the pages draw goes through Lang::T (a whole string) first: these are the ImGui calls of this file with the text translated.
		// Texts the plugin builds (reasons, states, table cells) go through Lang::P where they are built.
		std::string Fmt(const char* a_fmt, va_list a_args)
		{
			char buf[4096];
			std::vsnprintf(buf, sizeof(buf), a_fmt, a_args);
			return buf;
		}

		void LTextUnformatted(const char* a_text) { ImGuiMCP::TextUnformatted(Lang::T(a_text)); }

		void LText(const char* a_fmt, ...)
		{
			va_list args;
			va_start(args, a_fmt);
			const auto s = Fmt(Lang::T(a_fmt), args);
			va_end(args);
			ImGuiMCP::TextUnformatted(s.c_str());
		}

		void LTextWrapped(const char* a_fmt, ...)
		{
			va_list args;
			va_start(args, a_fmt);
			const auto s = Fmt(Lang::T(a_fmt), args);
			va_end(args);
			ImGuiMCP::TextWrapped("%s", s.c_str());
		}

		void LTextDisabled(const char* a_fmt, ...)
		{
			va_list args;
			va_start(args, a_fmt);
			const auto s = Fmt(Lang::T(a_fmt), args);
			va_end(args);
			ImGuiMCP::TextDisabled("%s", s.c_str());
		}

		void LTextColored(const ImVec4 a_col, const char* a_fmt, ...)
		{
			va_list args;
			va_start(args, a_fmt);
			const auto s = Fmt(Lang::T(a_fmt), args);
			va_end(args);
			ImGuiMCP::TextColored(a_col, "%s", s.c_str());
		}

		void LSeparatorText(const char* a_label) { ImGuiMCP::SeparatorText(Lang::T(a_label)); }
		bool LButton(const char* a_label) { return ImGuiMCP::Button(Lang::T(a_label)); }
		bool LSmallButton(const char* a_label) { return ImGuiMCP::SmallButton(Lang::T(a_label)); }
		bool LCheckbox(const char* a_label, bool* a_v) { return ImGuiMCP::Checkbox(Lang::T(a_label), a_v); }
		bool LInputText(const char* a_label, char* a_buf, std::size_t a_size) { return ImGuiMCP::InputText(Lang::T(a_label), a_buf, a_size); }

		void LTableSetupColumn(const char* a_label, ImGuiTableColumnFlags a_flags, float a_weight)
		{
			ImGuiMCP::TableSetupColumn(Lang::T(a_label), a_flags, a_weight);
		}

		bool LCombo(const char* a_label, int* a_current, const char* const a_items[], int a_count)
		{
			std::vector<const char*> items;
			for (int i = 0; i < a_count; ++i) {
				items.push_back(Lang::T(a_items[i]));
			}
			return ImGuiMCP::Combo(Lang::T(a_label), a_current, items.data(), a_count);
		}

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
				LTextUnformatted(tip);
				PopTextWrapPos();
				EndTooltip();
			}
		}

		void Flag(const char* a_name, bool a_on)
		{
			LTextColored(a_on ? ImVec4(0.55f, 0.85f, 0.6f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s %s", Lang::T(a_on ? "loaded:" : "not loaded:"), a_name);
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
			Lang::Ensure(Core::Cfg().language);
			const auto& info = Core::Info();
			auto&       cfg = Core::Cfg();
			LSeparatorText("NPC AI AutoPatch");
			LTextWrapped("Works with the AI overhauls of your load order, with none of them as a master. At game start it gives the people of mods the AI an overhaul gives "
			            "its own citizens, and puts an overhaul's AI back on the NPCs a later mod overrides. A plugin that must win (Better Vampires) is never overruled. "
			            "Nothing is written to your save and no plugin is added.");
			Spacing();
			LSeparatorText("AI overhauls");
			int icaioN = 0;
			for (const auto& r : Core::Rows()) {
				icaioN += (r.status == Core::Status::Patched && r.town.rfind("forward", 0) != 0) ? 1 : 0;
			}
			if (BeginTable("##ov", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				LTableSetupColumn("Overhaul", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				LTableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.0f);
				LTableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				LTableSetupColumn("Use", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				TableHeadersRow();
				TableNextRow();
				TableSetColumnIndex(0);
				LTextUnformatted("Immersive Citizens - AI Overhaul (mod-added town people)");
				TableSetColumnIndex(1);
				LTextUnformatted(!info.icaio ? "not loaded" : cfg.icaioStep ? "active" : "off");
				TableSetColumnIndex(2);
				LText("%d", icaioN);
				TableSetColumnIndex(3);
				if (info.icaio) {
					bool v = cfg.icaioStep;
					if (LCheckbox("##icaio", &v)) {
						cfg.icaioStep = v;
						Core::SaveConfig();
						needsRestart = true;
					}
				}
				const auto row = [&](const std::string& n, const std::string& state, int people, bool can) {
					TableNextRow();
					TableSetColumnIndex(0);
					LTextUnformatted(n.c_str());
					TableSetColumnIndex(1);
					ImGuiMCP::TextUnformatted(Lang::P(state).c_str());
					TableSetColumnIndex(2);
					LText("%d", people);
					TableSetColumnIndex(3);
					if (can) {
						const std::string key = Lower(n);
						bool              v = cfg.offProfiles.count(key) == 0;
						if (LCheckbox(("##p" + n).c_str(), &v)) {
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
			LTextDisabled("A loaded overhaul is used automatically; untick one to leave it out. Where an overhaul has several versions (AI Overhaul 1.9 and 1.9.5, for example) the one that fits the loaded plugin best is used. Profiles: Data/SKSE/Plugins/NPCAIAutoPatch/profiles.");
			if (LCheckbox("Apply the changes", &cfg.apply)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("Off: the pages show what would be done, nothing is changed in the game.");
			if (LCheckbox("Give the town AI to followers and marriage candidates too", &cfg.followers)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("People who can become your followers get the town's package like everyone else. The package only acts in rain, snow, "
			    "or when a dragon or vampire attacks, and never while the person follows you.");
			Spacing();
			{
				std::vector<std::string> codes{ "auto" }, labels{ std::string(Lang::T("Windows language")) + ": " };
				for (const auto& [code, name] : Lang::Available()) {
					codes.push_back(code);
					labels.push_back(name);
					if (code == Lang::Detected()) {
						labels[0] += name;
					}
				}
				int cur = 0;
				for (std::size_t i = 0; i < codes.size(); ++i) {
					cur = codes[i] == cfg.language ? static_cast<int>(i) : cur;
				}
				std::vector<const char*> items;
				for (const auto& l : labels) {
					items.push_back(l.c_str());
				}
				SetNextItemWidth(260.0f);
				if (ImGuiMCP::Combo(Lang::T("Language"), &cur, items.data(), static_cast<int>(items.size()))) {
					cfg.language = codes[static_cast<std::size_t>(cur)];
					Core::SaveConfig();
				}
				if (const auto font = Lang::FontNeeded(); !font.empty()) {
					ImGuiMCP::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", ("This language needs a font: set " + font + " = true in Data/SKSE/Plugins/SKSEMenuFramework.ini (section [Fonts]) and restart the game. Until then the pages stay in English.").c_str());
				}
			}
			if (needsRestart) {
				LTextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Saved. Restart the game for it to take effect.");
			}
			Spacing();
			LSeparatorText("This game session");
			LText("Changed: %d    Planned: %d    Left alone: %d", Count(Core::Status::Patched), Count(Core::Status::Planned), Count(Core::Status::Left));
			if (!info.townsDisabled.empty()) {
				std::string t;
				for (const auto& n : info.townsDisabled) {
					t += (t.empty() ? "" : ", ") + n;
				}
				LTextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Towns left out (ICAIO records not found): %s", t.c_str());
			}
			if (!info.perTown.empty() && BeginTable("##towns", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				LTableSetupColumn("Town (mod-added people)", ImGuiTableColumnFlags_WidthStretch, 3.0f);
				LTableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				TableHeadersRow();
				for (const auto& [town, n] : info.perTown) {
					TableNextRow();
					TableSetColumnIndex(0);
					LTextUnformatted(town.c_str());
					TableSetColumnIndex(1);
					LText("%d", n);
				}
				EndTable();
			}
			LTextDisabled("Full detail: Data/SKSE/Plugins/NPCAIAutoPatch/report.json");
		}

		struct PageState
		{
			int  filter = 0;
			char search[64] = "";
		};
		PageState npcs;

		void __stdcall RenderNpcs()
		{
			Lang::Ensure(Core::Cfg().language);
			if (!Core::Info().ran) {
				LTextDisabled("Nothing to show.");
				return;
			}
			static const char* kFilters[] = { "Everything", "Given the AI / planned", "Left alone" };
			SetNextItemWidth(220.0f);
			LCombo("Show##n", &npcs.filter, kFilters, 3);
			SameLine();
			SetNextItemWidth(220.0f);
			LInputText("Search##n", npcs.search, sizeof(npcs.search));
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
			LText("%s", (std::to_string(total) + " " + Lang::T(total == 1 ? "match" : "matches") + (total > 400 ? Lang::T(" (the first 400 are listed: narrow the search)") : "")).c_str());
			const float tableHeight = std::max(180.0f, GetContentRegionAvail().y - 10.0f);
			if (BeginTable("##npcs", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
			               ImVec2(0.0f, tableHeight))) {
				TableSetupScrollFreeze(0, 1);
				LTableSetupColumn("Person", ImGuiTableColumnFlags_WidthStretch, 2.5f);
				LTableSetupColumn("Mod", ImGuiTableColumnFlags_WidthStretch, 2.0f);
				LTableSetupColumn("Town", ImGuiTableColumnFlags_WidthFixed, 130.0f);
				LTableSetupColumn("What", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				TableHeadersRow();
				for (const auto* r : shown) {
					TableNextRow();
					TableSetColumnIndex(0);
					LTextUnformatted(r->name.c_str());
					TableSetColumnIndex(1);
					LTextUnformatted(r->plugin.c_str());
					TableSetColumnIndex(2);
					ImGuiMCP::TextUnformatted(Lang::P(r->town).c_str());
					TableSetColumnIndex(3);
					ImGuiMCP::TextUnformatted(Lang::P(r->reason).c_str());
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
			Lang::Ensure(Core::Cfg().language);
			LSeparatorText("Check one person");
			LTextWrapped("Shows what the game has on a person right now (packages, factions, override lists, flags, confidence) next to what the AI overhaul gives them. "
			            "Green: the overhaul's value is there. Red: it is not. Grey: information, nothing to compare. Nothing here changes the game.");
			if (LButton("Person selected in the console")) {
				auto ref = RE::Console::GetSelectedRef();
				PickRef(ref.get());
			}
			Tip("Open the console (~), click the person, then press this. Opening the menu does not clear the console's selection.");
			SameLine();
			if (LButton("Person under the crosshair")) {
				auto* pick = RE::CrosshairPickData::GetSingleton();
				auto  ref = pick ? pick->target.get() : RE::NiPointer<RE::TESObjectREFR>();
				PickRef(ref.get());
			}
			SameLine();
			if (ins.cur.valid && LButton("Refresh")) {
				Refresh();
			}
			SetNextItemWidth(220.0f);
			LInputText("Find by name##i", ins.search, sizeof(ins.search));
			Tip("Searches the people the loaded AI overhaul profile changes. The record is shown; the actor values only when you picked the person in the world.");
			if (ins.search[0]) {
				for (const auto& [label, id] : Core::FindInProfile(ins.search, 12)) {
					if (LSmallButton((label + "##" + std::to_string(id)).c_str())) {
						ins.npc = id;
						ins.live = RE::ActorHandle();
						ins.msg.clear();
						Refresh();
					}
				}
			}
			if (!ins.msg.empty()) {
				LTextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", Lang::T(ins.msg.c_str()));
			}
			if (!ins.cur.valid) {
				return;
			}
			Spacing();
			ImGuiMCP::SeparatorText(ins.cur.title.c_str());
			LText("Record last changed by: %s", ins.cur.winner.c_str());
			if (!ins.cur.profile.empty()) {
				LText("Compared with: %s", ins.cur.profile.c_str());
			}
			if (!ins.cur.note.empty()) {
				ImGuiMCP::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", Lang::P(ins.cur.note).c_str());
			}
			const float h = std::max(180.0f, GetContentRegionAvail().y - 10.0f);
			if (BeginTable("##insp", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
			               ImVec2(0.0f, h))) {
				TableSetupScrollFreeze(0, 1);
				LTableSetupColumn("What", ImGuiTableColumnFlags_WidthStretch, 3.0f);
				LTableSetupColumn("In the game now", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				LTableSetupColumn("The overhaul gives", ImGuiTableColumnFlags_WidthStretch, 4.0f);
				LTableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
				TableHeadersRow();
				for (const auto& r : ins.cur.rows) {
					const ImVec4 col = r.state == 1 ? ImVec4(0.55f, 0.85f, 0.6f, 1.0f) : r.state == 2 ? ImVec4(0.95f, 0.45f, 0.4f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
					TableNextRow();
					TableSetColumnIndex(0);
					ImGuiMCP::TextColored(col, "%s", Lang::P(r.what).c_str());
					TableSetColumnIndex(1);
					ImGuiMCP::TextColored(col, "%s", (r.now.find('|') == std::string::npos ? Lang::P(r.now) : r.now).c_str());
					TableSetColumnIndex(2);
					ImGuiMCP::TextColored(col, "%s", Lang::T(r.want.c_str()));
					TableSetColumnIndex(3);
					ImGuiMCP::TextColored(col, "%s", r.state == 1 ? Lang::T("ok") : r.state == 2 ? "!!" : "");
				}
				EndTable();
			}
		}

		void __stdcall RenderMods()
		{
			Lang::Ensure(Core::Cfg().language);
			if (!Core::Info().ran) {
				LTextDisabled("Nothing to show.");
				return;
			}
			LSeparatorText("Mods whose people are never touched");
			LTextWrapped("Tick a mod to leave all of its people out. Takes effect at the next game start.");
			std::map<std::string, int> perMod;
			for (const auto& r : Core::Rows()) {
				++perMod[r.plugin];
			}
			auto& cfg = Core::Cfg();
			for (const auto& [plugin, n] : perMod) {
				bool off = cfg.exclude.count(plugin) > 0;
				const std::string label = plugin + "  (" + std::to_string(n) + " " + Lang::T("people") + ")";
				if (LCheckbox(label.c_str(), &off)) {
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
				LTextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Saved. Restart the game for it to take effect.");
			}
		}

		void __stdcall RenderSkipped()
		{
			Lang::Ensure(Core::Cfg().language);
			const auto& info = Core::Info();
			if (!info.ran && Core::Rows().empty()) {
				LTextDisabled("Nothing to show.");
				return;
			}
			LSeparatorText("Why people were left alone");
			if (BeginTable("##skipped", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				LTableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch, 5.0f);
				LTableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				TableHeadersRow();
				for (const auto& [reason, n] : info.skipped) {
					TableNextRow();
					TableSetColumnIndex(0);
					ImGuiMCP::TextUnformatted(Lang::P(reason).c_str());
					TableSetColumnIndex(1);
					LText("%d", n);
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
