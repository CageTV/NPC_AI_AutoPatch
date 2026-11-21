/*
 * ICAIO AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#include "PCH.h"
#include "Core.h"

#include <nlohmann/json.hpp>

namespace Core
{
	namespace
	{
		Config           g_cfg;
		Summary          g_info;
		std::vector<Row> g_rows;

		std::filesystem::path Dir() { return std::filesystem::path("Data/SKSE/Plugins/ICAIOAutoPatch"); }

		std::string Lower(std::string a_s)
		{
			std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		constexpr auto kIcaio = "Immersive Citizens - AI Overhaul.esp";
		constexpr auto kSky = "Skyrim.esm";

		// The towns ICAIO has "extra citizen" records for. crime = the hold's crime faction, town = the town's faction (both Skyrim.esm). package and combat
		// are ICAIO's own (Immersive Citizens - AI Overhaul.esp, 0.4.0.2): the town's NPCO<Town>ExtraCitizen... package and its NPCO<Town>ExtraCitizen form list.
		struct TownRule
		{
			const char*   name;
			std::uint32_t crime, town, package, combat;
		};
		constexpr TownRule kTowns[] = {
			{ "Whiterun", 0x0267EA, 0x028172, 0xA74D3C, 0x31E2DA },
			{ "Riverwood", 0x0267EA, 0x013481, 0xABBB88, 0x238A7B },
			{ "Falkreath", 0x028170, 0x028177, 0xAD509D, 0x238FE8 },
			{ "Rorikstead", 0x0267EA, 0x028174, 0xABBB89, 0x238FE2 },
			{ "Ivarstead", 0x02816B, 0x028185, 0xAC0C89, 0x238FE4 },
			{ "Darkwater Crossing", 0x0267E3, 0x028183, 0xAC0C8B, 0x23955D },
			{ "Riften", 0x02816B, 0x028186, 0xAD50A0, 0x3284E5 },
			{ "Dawnstar", 0x02816E, 0x02817F, 0xAD509B, 0x32D5FD },
			{ "Morthal", 0x02816D, 0x02817D, 0xAD509F, 0x238FEE },
			{ "Dragon Bridge", 0x029DB0, 0x02817B, 0xABBB8D, 0x238FF4 },
			{ "Windhelm", 0x0267E3, 0x028173, 0xAD50A2, 0x3284EF },
			{ "Shor's Stone", 0x02816B, 0x028184, 0xAC0C8C, 0x23955B },
			{ "Kynesgrove", 0x0267E3, 0x028182, 0xABBB8C, 0x239AC1 },
			{ "Solitude", 0x029DB0, 0x02817C, 0xAD50A1, 0x3284E9 },
			{ "Stone Hills", 0x02816D, 0x02817E, 0xA74D3D, 0x238FEB },
			{ "Winterhold", 0x02816F, 0x028181, 0x332712, 0x239AC7 },
			{ "Markarth", 0x02816C, 0x028178, 0xAD509E, 0x3284EC },
			{ "Karthwasten", 0x02816C, 0x02817A, 0xAC0C8A, 0x238FF1 },
		};

		// factions whose members ICAIO's quests, or the people who use them, treat on their own: guards, the war, Thalmor, Penitus Oculatus, the Companions, the
		// College, the Dark Brotherhood, the Thieves Guild, the Forsworn, the Imperial justiciars and the Dawnguard faction ICAIO's quests exclude
		constexpr std::uint32_t kBlackSky[] = { 0x086EEE, 0x02BF9A, 0x02BF9B, 0x028849, 0x02584B, 0x039F26, 0x048362, 0x01F259, 0x01BDB3, 0x029DA9, 0x043599, 0x0B79AC };
		constexpr std::uint32_t kBlackDawnguard = 0x003375;
		constexpr std::uint32_t kCitizens = 0x2FDA89, kExclusion = 0x237FB4;  // ICAIO's NPCO_CitizensFaction and NPCO_ExclusionFaction
		constexpr std::uint32_t kPotentialFollower = 0x05C84D;

		const std::set<std::string> kOfficial = { "skyrim.esm", "update.esm", "dawnguard.esm", "hearthfires.esm", "dragonborn.esm" };

		void LoadConfig()
		{
			g_cfg = Config{};
			g_cfg.excludeNames = { "corpse" };
			std::ifstream in(Dir() / "config.json");
			if (!in) {
				return;
			}
			const auto j = nlohmann::json::parse(in, nullptr, false, true);
			if (j.is_discarded()) {
				SKSE::log::error("config.json is not valid JSON: using the defaults");
				return;
			}
			g_cfg.apply = j.value("apply", true);
			g_cfg.followers = j.value("followers", true);
			if (j.contains("excludePlugins")) {
				for (const auto& e : j["excludePlugins"]) {
					g_cfg.exclude.insert(Lower(e.get<std::string>()));
				}
			}
			if (j.contains("excludeNames")) {
				g_cfg.excludeNames.clear();
				for (const auto& e : j["excludeNames"]) {
					g_cfg.excludeNames.insert(Lower(e.get<std::string>()));
				}
			}
		}

		struct Town
		{
			const TownRule*   rule;
			RE::TESFaction*   crime{ nullptr };
			RE::TESFaction*   town{ nullptr };
			RE::TESPackage*   package{ nullptr };
			RE::BGSListForm*  combat{ nullptr };
			bool              ok{ false };
		};
	}

	Config&                 Cfg() { return g_cfg; }
	const Summary&          Info() { return g_info; }
	const std::vector<Row>& Rows() { return g_rows; }

	void SaveConfig()
	{
		std::error_code ec;
		std::filesystem::create_directories(Dir(), ec);
		nlohmann::json j;
		j["apply"] = g_cfg.apply;
		j["followers"] = g_cfg.followers;
		j["excludePlugins"] = nlohmann::json::array();
		for (const auto& e : g_cfg.exclude) {
			j["excludePlugins"].push_back(e);
		}
		j["excludeNames"] = nlohmann::json::array();
		for (const auto& e : g_cfg.excludeNames) {
			j["excludeNames"].push_back(e);
		}
		std::ofstream out(Dir() / "config.json");
		out << j.dump(2);
	}

	// who wins the NPCs each AI overhaul touches? (diagnostic for the planned "forward the overhaul's AI onto NPCs that a later mod overrides" step)
	namespace
	{
		nlohmann::json Audit(RE::TESDataHandler* a_dh)
		{
			static const char* kPlugins[] = { "AI Overhaul.esp", "Immersive Citizens - AI Overhaul.esp", "Better Vampires.esp", "3DNPC.esp" };
			nlohmann::json out = nlohmann::json::object();
			for (const char* name : kPlugins) {
				if (!a_dh->LookupModByName(name)) {
					continue;
				}
				const std::string lname = Lower(name);
				std::map<std::string, int> winners;
				std::map<std::string, int> between;  // plugins loaded after the overhaul that also touch the NPC
				int                        touched = 0, ownDefined = 0;
				for (auto* npc : a_dh->GetFormArray<RE::TESNPC>()) {
					if (!npc) {
						continue;
					}
					std::vector<std::string> chain;
					for (std::uint32_t i = 0; i < 64; ++i) {
						const auto* f = npc->GetFile(i);
						if (!f) {
							break;
						}
						chain.push_back(Lower(std::string(f->GetFilename())));
					}
					const auto it = std::find(chain.begin(), chain.end(), lname);
					if (it == chain.end()) {
						continue;
					}
					++touched;
					if (chain.front() == lname) {
						++ownDefined;
					}
					const auto& winner = chain.back();
					++winners[winner];
					for (auto k = it + 1; k != chain.end(); ++k) {
						++between[*k];
					}
				}
				out[name] = { { "touches", touched }, { "definesItself", ownDefined }, { "winners", winners }, { "alsoOverriddenBy", between } };
				SKSE::log::info("audit: {} touches {} NPCs ({} of them it defines); winners: {}", name, touched, ownDefined, nlohmann::json(winners).dump());
			}
			return out;
		}
	}

	void Run()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			return;
		}
		LoadConfig();
		g_rows.clear();
		g_info = Summary{};
		const auto audit = Audit(dh);
		g_info.icaio = dh->LookupModByName(kIcaio) != nullptr;
		g_info.three = dh->LookupModByName("3DNPC.esp") != nullptr;
		SKSE::log::info("Immersive Citizens - AI Overhaul: {}, Interesting NPCs (3DNPC): {}", g_info.icaio ? "loaded" : "NOT loaded", g_info.three ? "loaded" : "not loaded");
		if (!g_info.icaio) {
			return;
		}
		g_info.ran = true;

		// ---- ICAIO's records and the vanilla factions, looked up by id; a town whose records are not all found is left out and says so
		std::vector<Town> towns;
		for (const auto& r : kTowns) {
			Town t{ &r };
			t.crime = dh->LookupForm<RE::TESFaction>(r.crime, kSky);
			t.town = dh->LookupForm<RE::TESFaction>(r.town, kSky);
			t.package = dh->LookupForm<RE::TESPackage>(r.package, kIcaio);
			t.combat = dh->LookupForm<RE::BGSListForm>(r.combat, kIcaio);
			t.ok = t.crime && t.town && t.package && t.combat;
			if (!t.ok) {
				g_info.townsDisabled.push_back(r.name);
				SKSE::log::warn("{}: ICAIO records not found (crime {} town {} package {} combat list {}): this town is left out", r.name, !!t.crime, !!t.town, !!t.package, !!t.combat);
			} else {
				++g_info.towns;
			}
			towns.push_back(t);
		}
		auto* citizens = dh->LookupForm<RE::TESFaction>(kCitizens, kIcaio);
		auto* exclusion = dh->LookupForm<RE::TESFaction>(kExclusion, kIcaio);
		if (!citizens || !exclusion) {
			SKSE::log::error("ICAIO's citizens or exclusion faction was not found: nothing is changed");
			return;
		}
		std::set<const RE::TESFaction*> black;
		for (const auto id : kBlackSky) {
			if (auto* f = dh->LookupForm<RE::TESFaction>(id, kSky)) {
				black.insert(f);
			}
		}
		if (auto* f = dh->LookupForm<RE::TESFaction>(kBlackDawnguard, "Dawnguard.esm")) {
			black.insert(f);
		}
		black.insert(exclusion);
		auto* follower = dh->LookupForm<RE::TESFaction>(kPotentialFollower, kSky);

		// ---- every NPC that a mod defines
		for (auto* npc : dh->GetFormArray<RE::TESNPC>()) {
			if (!npc || npc->IsDeleted()) {
				continue;
			}
			const auto* file = npc->GetFile(0);
			const std::string plugin = file ? Lower(std::string(file->GetFilename())) : std::string();
			if (plugin.empty() || kOfficial.count(plugin) || plugin == Lower(kIcaio)) {
				continue;
			}
			// does it stand in a town and its hold's crime faction? (cheap test first: most NPCs of a mod are not citizens)
			const Town* hit = nullptr;
			bool        isBlack = false, isFollower = false, already = false;
			for (const auto& fr : npc->factions) {
				if (!fr.faction) {
					continue;
				}
				isBlack = isBlack || black.count(fr.faction) > 0;
				already = already || fr.faction == citizens;
				isFollower = isFollower || fr.faction == follower;
			}
			for (const auto& t : towns) {
				if (!t.ok) {
					continue;
				}
				bool inTown = false, inCrime = false;
				for (const auto& fr : npc->factions) {
					inTown = inTown || fr.faction == t.town;
					inCrime = inCrime || fr.faction == t.crime;
				}
				if (inTown && inCrime) {
					hit = &t;
					break;
				}
			}
			if (!hit) {
				continue;
			}
			++g_info.looked;
			Row row;
			row.name = npc->GetName() ? npc->GetName() : "";
			row.plugin = plugin;
			row.town = hit->rule->name;
			const auto skip = [&](const char* why) {
				row.status = Status::Left;
				row.reason = why;
				++g_info.skipped[why];
				g_rows.push_back(row);
			};
			if (g_cfg.exclude.count(plugin)) {
				skip("the mod is excluded in the menu");
				continue;
			}
			if (row.name.empty()) {
				skip("no name");
				continue;
			}
			const auto lname = Lower(row.name);
			bool       named = false;
			for (const auto& w : g_cfg.excludeNames) {
				named = named || lname.find(w) != std::string::npos;
			}
			if (named) {
				skip("its name contains an excluded word");
				continue;
			}
			if (!npc->race || !npc->race->HasKeywordString("ActorTypeNPC")) {
				skip("not a person (race without ActorTypeNPC)");
				continue;
			}
			if (isBlack) {
				skip("in a guard, war, Thalmor, guild or other faction that ICAIO leaves to its own rules");
				continue;
			}
			if (isFollower && !g_cfg.followers) {
				skip("can be a follower (followers are switched off)");
				continue;
			}
			if (already) {
				skip("already has ICAIO's citizens faction");
				continue;
			}
			if (npc->actorData.templateUseFlags.any(RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kFactions, RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kAIPackages)) {
				skip("takes its factions or packages from a template");
				continue;
			}
			bool hasIcaioPackage = false;
			for (const auto* pkg : npc->aiPackages.packages) {
				const auto* pf = pkg ? pkg->GetFile(0) : nullptr;
				hasIcaioPackage = hasIcaioPackage || (pf && Lower(std::string(pf->GetFilename())) == Lower(kIcaio));
			}
			if (hasIcaioPackage) {
				skip("already has an ICAIO package");
				continue;
			}
			if (g_cfg.apply) {
				npc->aiPackages.packages.push_front(hit->package);
				npc->factions.push_back({ citizens, 0 });
				npc->factions.push_back({ exclusion, 0 });
				if (!npc->enterCombatOverRidePackList) {
					npc->enterCombatOverRidePackList = hit->combat;
				}
				row.status = Status::Patched;
				row.reason = "given the town's ICAIO package, factions and combat list";
			} else {
				row.status = Status::Planned;
				row.reason = "would be given the town's ICAIO package, factions and combat list (apply is off)";
			}
			++g_info.perTown[row.town];
			g_rows.push_back(row);
		}
		int nPatched = 0, nPlanned = 0, nLeft = 0;
		for (const auto& r : g_rows) {
			(r.status == Status::Patched ? nPatched : r.status == Status::Planned ? nPlanned : nLeft)++;
		}
		SKSE::log::info("{} NPCs of mods stand in a town and its crime faction: {} patched, {} planned, {} left alone ({} towns with ICAIO records)", g_info.looked, nPatched, nPlanned, nLeft, g_info.towns);

		try {
			std::error_code ec;
			std::filesystem::create_directories(Dir(), ec);
			nlohmann::json j;
			j["audit"] = audit;
			j["icaio"] = g_info.icaio;
			j["three"] = g_info.three;
			j["apply"] = g_cfg.apply;
			j["looked"] = g_info.looked;
			j["skipped"] = g_info.skipped;
			j["perTown"] = g_info.perTown;
			j["townsDisabled"] = g_info.townsDisabled;
			auto& rows = j["npcs"] = nlohmann::json::array();
			for (const auto& r : g_rows) {
				rows.push_back({ { "name", r.name }, { "plugin", r.plugin }, { "town", r.town },
					{ "status", r.status == Status::Patched ? "patched" : r.status == Status::Planned ? "planned" : "left" }, { "reason", r.reason } });
			}
			std::ofstream out(Dir() / "report.json");
			out << j.dump(1);
		} catch (const std::exception& e) {
			SKSE::log::warn("could not write report.json: {}", e.what());
		}
	}
}
