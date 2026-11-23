/*
 * NPC AI AutoPatch
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
		std::map<std::uint32_t, std::pair<std::string, nlohmann::json>> g_expected;  // NPC form id -> (profile used, its entry): for the Inspect page

		std::filesystem::path Dir() { return std::filesystem::path("Data/SKSE/Plugins/NPCAIAutoPatch"); }

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
			g_cfg.wins = { "better vampires.esp", "relationship dialogue overhaul.esp", "nwsfollowerframework.esp", "nwsff_noteammagicdamage.esp" };
			std::ifstream in(Dir() / "config.json");
			if (!in) {
				in.open("Data/SKSE/Plugins/ICAIOAutoPatch/config.json");  // the settings of the earlier name, read once
			}
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
			g_cfg.forward = j.value("forward", true);
			g_cfg.icaioStep = j.value("icaio", true);
			g_cfg.profile = Lower(j.value("profile", std::string()));
			g_cfg.language = Lower(j.value("language", std::string("auto")));
			if (j.contains("offProfiles")) {
				for (const auto& e : j["offProfiles"]) {
					g_cfg.offProfiles.insert(Lower(e.get<std::string>()));
				}
			}
			if (j.contains("wins")) {
				g_cfg.wins.clear();
				for (const auto& e : j["wins"]) {
					g_cfg.wins.insert(Lower(e.get<std::string>()));
				}
			}
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
	std::filesystem::path   DataDir() { return Dir(); }

	void SaveConfig()
	{
		std::error_code ec;
		std::filesystem::create_directories(Dir(), ec);
		nlohmann::json j;
		j["apply"] = g_cfg.apply;
		j["followers"] = g_cfg.followers;
		j["forward"] = g_cfg.forward;
		j["icaio"] = g_cfg.icaioStep;
		j["language"] = g_cfg.language;
		if (!g_cfg.profile.empty()) {
			j["profile"] = g_cfg.profile;
		}
		j["offProfiles"] = g_cfg.offProfiles;
		j["wins"] = nlohmann::json::array();
		for (const auto& e : g_cfg.wins) {
			j["wins"].push_back(e);
		}
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

	// ---- "forward" profiles: put an AI overhaul's AI back on the NPCs a later mod overrides (what the Mutagen "AI Overhaul Patcher" does at build time).
	// A profile (profiles/*.json, made by tools/build_forward_profile.py from the overhaul's plugin) lists, per NPC, the overhaul's package list, the packages it
	// removed, the factions it added, the four override lists, the Protected / Essential flags and the confidence. Nothing in it is a master.
	namespace
	{
		RE::TESForm* FindRef(RE::TESDataHandler* a_dh, const std::string& a_ref)
		{
			const auto bar = a_ref.find('|');
			if (bar == std::string::npos) {
				return nullptr;
			}
			try {
				return a_dh->LookupForm(static_cast<RE::FormID>(std::stoul(a_ref.substr(bar + 1), nullptr, 16)), a_ref.substr(0, bar));
			} catch (...) {
				return nullptr;
			}
		}

		std::vector<std::string> ChainOf(const RE::TESForm* a_f)
		{
			std::vector<std::string> out;
			for (std::uint32_t i = 0; i < 128; ++i) {
				const auto* f = a_f->GetFile(i);
				if (!f) {
					break;
				}
				out.push_back(Lower(std::string(f->GetFilename())));
			}
			return out;
		}

		int  Confidence(const RE::TESNPC* a_npc) { return (reinterpret_cast<const std::uint8_t*>(&a_npc->aiData)[0] >> 2) & 7; }
		void SetConfidence(RE::TESNPC* a_npc, int a_v)
		{
			auto* raw = reinterpret_cast<std::uint8_t*>(&a_npc->aiData);
			raw[0] = static_cast<std::uint8_t>((raw[0] & ~(7 << 2)) | ((a_v & 7) << 2));
		}

		void Forward(RE::TESDataHandler* a_dh, std::vector<Row>& a_rows, Summary& a_info)
		{
			std::error_code ec;
			const auto      dir = Dir() / "profiles";
			if (!std::filesystem::is_directory(dir, ec)) {
				return;
			}
			// The profiles of one family are the versions of one overhaul (AI Overhaul 1.9 and 1.9.5 are not the same plugin: the 1.9 version has USSEP and
			// CC Fish as masters and uses USSEP's fix packages where 1.9.5 ships copies of its own). Each is a candidate; only the one the loaded game
			// resolves best (fewest references that name no record) is applied, so a version is never applied twice and every player gets the profile made from
			// the version they have. A profile without "family" is a family of its own. config "profile" forces one by name.
			struct Candidate
			{
				std::string              name, family;
				int                      priority = 0, refs = 0, unresolved = 0;
				std::vector<std::string> examples;  // a few references that name no record
				nlohmann::json           prof;
			};
			std::vector<Candidate> cands;
			for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
				if (entry.path().extension() != ".json") {
					continue;
				}
				std::ifstream in(entry.path());
				const auto    prof = nlohmann::json::parse(in, nullptr, false, true);
				if (prof.is_discarded() || prof.value("type", std::string()) != "forward") {
					continue;
				}
				const std::string name = prof.value("name", entry.path().stem().string());
				const std::string requires_ = prof.value("requires", std::string());
				if (!requires_.empty() && !a_dh->LookupModByName(requires_)) {
					SKSE::log::info("forward profile '{}': {} is not loaded, skipped", name, requires_);
					a_info.profilesMissing.push_back(name);
					continue;
				}
				if (g_cfg.offProfiles.count(Lower(name))) {
					a_info.profilesOff.push_back(name);
					continue;
				}
				Candidate c;
				c.name = name;
				c.family = Lower(prof.value("family", name));
				c.priority = prof.value("priority", 0);
				c.prof = prof;
				cands.push_back(std::move(c));
			}
			std::map<std::string, bool> known;  // reference -> names a record in this game
			const auto                  resolves = [&](const std::string& a_ref) {
                const auto it = known.find(a_ref);
                if (it != known.end()) {
                    return it->second;
                }
                return known[a_ref] = FindRef(a_dh, a_ref) != nullptr;
			};
			for (auto& c : cands) {
				const auto check = [&](const nlohmann::json& a_v) {
					if (a_v.is_string()) {
						++c.refs;
						if (!resolves(a_v.get<std::string>())) {
							++c.unresolved;
							if (c.examples.size() < 6) {
								c.examples.push_back(a_v.get<std::string>());
							}
						}
					}
				};
				for (const auto& e : c.prof["npcs"]) {
					check(e.value("npc", nlohmann::json()));
					for (const char* key : { "packages", "packagesRemoved" }) {
						for (const auto& r : e.value(key, nlohmann::json::array())) {
							check(r);
						}
					}
					for (const auto& fa : e.value("factionsAdded", nlohmann::json::array())) {
						check(fa.value("form", nlohmann::json()));
					}
					if (e.contains("lists")) {
						for (const auto& l : e["lists"]) {
							check(l.value("from", nlohmann::json()));
							check(l.value("to", nlohmann::json()));
						}
					}
				}
			}
			std::vector<Candidate> picked;
			{
				std::map<std::string, std::vector<std::size_t>> families;
				for (std::size_t i = 0; i < cands.size(); ++i) {
					families[cands[i].family].push_back(i);
				}
				for (auto& [family, idx] : families) {
					std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
						const auto &x = cands[a], &y = cands[b];
						return std::tie(x.unresolved, y.priority, x.name) < std::tie(y.unresolved, x.priority, y.name);
					});
					std::size_t best = idx.front();
					for (const auto i : idx) {
						if (!g_cfg.profile.empty() && Lower(cands[i].name) == g_cfg.profile) {
							best = i;  // chosen by hand in config.json
						}
					}
					for (const auto i : idx) {
						const auto& c = cands[i];
						a_info.profileUnresolved[c.name] = c.unresolved;
						SKSE::log::info("forward profile '{}' (family '{}'): {} references, {} name no record in this game{}", c.name, family, c.refs, c.unresolved,
							i == best ? (idx.size() > 1 ? " <- used" : "") : " <- not used");
						if (c.unresolved && i == best) {
							for (const auto& ex : c.examples) {
								SKSE::log::info("    e.g. {}", ex);
							}
						}
						if (i != best) {
							a_info.profilesNotChosen.push_back(c.name);
						}
					}
					picked.push_back(cands[best]);
				}
			}
			for (const auto& cand : picked) {
				const auto&       prof = cand.prof;
				const std::string name = cand.name;
				std::set<std::string> own;
				for (const auto& o : prof.value("ownPlugins", nlohmann::json::array())) {
					own.insert(Lower(o.get<std::string>()));
				}
				a_info.profiles.push_back(name);
				int forwarded = 0, missing = 0;
				for (const auto& e : prof["npcs"]) {
					Row row;
					row.name = e.value("name", std::string());
					row.town = "forward: " + name;
					auto* f0 = FindRef(a_dh, e.value("npc", std::string()));
					auto* npc = f0 ? f0->As<RE::TESNPC>() : nullptr;
					const auto skip = [&](const std::string& why) {
						row.status = Status::Left;
						row.reason = why;
						++a_info.skipped["forward: " + why];
						a_rows.push_back(row);
					};
					if (!npc) {
						++missing;
						skip("the NPC is not in this game");
						continue;
					}
					g_expected[npc->GetFormID()] = { name, e };
					const auto chain = ChainOf(npc);
					row.plugin = chain.empty() ? std::string() : chain.back();
					if (chain.empty() || own.count(chain.back())) {
						skip("the overhaul's own record already wins");
						continue;
					}
					bool wins = false;
					for (const auto& c : chain) {
						wins = wins || g_cfg.wins.count(c) > 0;
					}
					if (wins) {
						skip("a plugin that must win (Better Vampires) changes this NPC");
						continue;
					}
					std::vector<std::string> done;
					// packages: the overhaul's list in its order, then what later mods added that the overhaul did not remove
					if (e.contains("packages") && !npc->actorData.templateUseFlags.any(RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kAIPackages)) {
						std::vector<RE::TESPackage*> want, cur, removed;
						std::vector<std::string>     bad;
						bool                         ok = true;
						for (const auto& r : e["packages"]) {
							auto* f = FindRef(a_dh, r.get<std::string>());
							auto* pk = f ? f->As<RE::TESPackage>() : nullptr;
							if (pk) {
								want.push_back(pk);
							} else {
								ok = false;
								bad.push_back(r.get<std::string>());
							}
						}
						for (const auto& r : e.value("packagesRemoved", nlohmann::json::array())) {
							if (auto* f = FindRef(a_dh, r.get<std::string>())) {
								if (auto* pk = f->As<RE::TESPackage>()) {
									removed.push_back(pk);
								}
							}
						}
						for (auto* pk : npc->aiPackages.packages) {
							cur.push_back(pk);
						}
						if (ok) {
							std::vector<RE::TESPackage*> next = want;
							for (auto* pk : cur) {
								if (std::find(want.begin(), want.end(), pk) == want.end() && std::find(removed.begin(), removed.end(), pk) == removed.end()) {
									next.push_back(pk);
								}
							}
							if (next != cur) {
								if (g_cfg.apply) {
									auto& l = npc->aiPackages.packages;
									l.clear();
									for (auto it = next.rbegin(); it != next.rend(); ++it) {
										l.push_front(*it);
									}
								}
								done.push_back("packages");
							}
						} else {
							++missing;
							// one unresolved package keeps the whole package list as the winner has it: say which NPC and which reference
							std::string what;
							for (const auto& b : bad) {
								what += (what.empty() ? "" : ", ") + b;
							}
							SKSE::log::info("forward profile '{}': {} keeps its packages, no record for {}", name, row.name, what);
						}
					}
					// factions the overhaul added
					for (const auto& fa : e.value("factionsAdded", nlohmann::json::array())) {
						auto* f = FindRef(a_dh, fa.value("form", std::string()));
						auto* fac = f ? f->As<RE::TESFaction>() : nullptr;
						if (!fac) {
							continue;
						}
						bool has = false;
						for (const auto& fr : npc->factions) {
							has = has || fr.faction == fac;
						}
						if (!has) {
							if (g_cfg.apply) {
								npc->factions.push_back({ fac, static_cast<std::int8_t>(fa.value("rank", 0)) });
							}
							done.push_back("faction");
						}
					}
					// the four override lists: set when a later mod did not change them from what the overhaul found
					static const std::pair<const char*, RE::BGSListForm* RE::BGSOverridePackCollection::*> kLists[] = {
						{ "spectator", &RE::BGSOverridePackCollection::spectatorOverRidePackList }, { "combat", &RE::BGSOverridePackCollection::enterCombatOverRidePackList },
						{ "observe", &RE::BGSOverridePackCollection::observeCorpseOverRidePackList }, { "guardWarn", &RE::BGSOverridePackCollection::guardWarnOverRidePackList }
					};
					for (const auto& [key, member] : kLists) {
						if (!e.contains("lists") || !e["lists"].contains(key)) {
							continue;
						}
						const auto& l = e["lists"][key];
						auto*       fromF = l["from"].is_string() ? FindRef(a_dh, l["from"].get<std::string>()) : nullptr;
						auto*       toF = l["to"].is_string() ? FindRef(a_dh, l["to"].get<std::string>()) : nullptr;
						auto*       to = toF ? toF->As<RE::BGSListForm>() : nullptr;
						if (!to) {
							continue;
						}
						auto*& slot = npc->*member;
						auto*  from = fromF ? fromF->As<RE::BGSListForm>() : nullptr;
						if (slot == to) {
							continue;
						}
						if (slot == from) {
							if (g_cfg.apply) {
								slot = to;
							}
							done.push_back(std::string(key) + " list");
						}
					}
					if (e.contains("flagsAdded")) {
						for (const auto& fl : e["flagsAdded"]) {
							const auto flag = fl.get<std::string>() == "Essential" ? RE::ACTOR_BASE_DATA::Flag::kEssential : RE::ACTOR_BASE_DATA::Flag::kProtected;
							if (npc->actorData.actorBaseFlags.none(flag)) {
								if (g_cfg.apply) {
									npc->actorData.actorBaseFlags.set(flag);
								}
								done.push_back(fl.get<std::string>() == "Essential" ? "essential" : "protected");
							}
						}
					}
					if (e.contains("confidence")) {
						const int to = e["confidence"].value("to", 2);
						const int from = e["confidence"].value("from", -1);
						const int now = Confidence(npc);
						// the winner left the value as the base game has it: the overhaul's value applies (up or down).
						// the winner chose its own value: only the lower (more cowardly) of the two is kept.
						if (now != to && (now == from || to < now)) {
							if (g_cfg.apply) {
								SetConfidence(npc, to);
							}
							done.push_back("confidence");
						}
					}
					if (done.empty()) {
						skip("already has the overhaul's AI");
						continue;
					}
					row.status = g_cfg.apply ? Status::Patched : Status::Planned;
					std::string what;
					for (const auto& d : done) {
						what += (what.empty() ? "" : ", ") + d;
					}
					row.reason = std::string(g_cfg.apply ? "put back: " : "would put back: ") + what + " (winner: " + row.plugin + ")";
					++a_info.forwarded[name];
					++forwarded;
					a_rows.push_back(row);
				}
				SKSE::log::info("forward profile '{}': {} NPCs got the overhaul's AI back, {} references not found", name, forwarded, missing);
			}
		}
	}

	// ---- the Inspect page: what the game has on one NPC against what the overhaul's profile says
	namespace
	{
		std::string RefText(const RE::TESForm* a_f)
		{
			if (!a_f) {
				return "(none)";
			}
			const auto id = a_f->GetFormID();
			const auto local = (id >> 24) == 0xFE ? (id & 0xFFF) : (id & 0xFFFFFF);
			const auto* file = a_f->GetFile(0);
			char       hex[16];
			std::snprintf(hex, sizeof(hex), "%06X", local);
			std::string s;
			if (const char* ed = a_f->GetFormEditorID(); ed && *ed) {
				s = std::string(ed) + "  ";
			}
			return s + (file ? std::string(file->GetFilename()) : std::string("?")) + "|" + hex;
		}
	}

	Inspection Inspect(RE::TESNPC* a_npc, RE::Actor* a_live)
	{
		Inspection r;
		auto*      dh = RE::TESDataHandler::GetSingleton();
		if (!a_npc || !dh) {
			return r;
		}
		r.valid = true;
		char hex[16];
		std::snprintf(hex, sizeof(hex), "%08X", a_npc->GetFormID());
		r.title = std::string(a_npc->GetName()) + "  [" + hex + "]";
		const auto chain = ChainOf(a_npc);
		r.winner = chain.empty() ? std::string() : chain.back();
		const auto add = [&](const std::string& w, const std::string& now, const std::string& want, int st) { r.rows.push_back({ w, now, want, st }); };
		const auto it = g_expected.find(a_npc->GetFormID());
		const nlohmann::json* e = it == g_expected.end() ? nullptr : &it->second.second;
		if (e) {
			r.profile = it->second.first;
		} else {
			r.note = "The loaded AI overhaul profile does not change this person. What the game has is listed for information.";
		}
		if (a_npc->actorData.templateUseFlags.any(RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kAIPackages)) {
			r.note += (r.note.empty() ? "" : " ") + std::string("This NPC takes its packages from a template, so they are not changed.");
		}

		// packages
		std::vector<RE::TESPackage*> cur;
		for (auto* pk : a_npc->aiPackages.packages) {
			cur.push_back(pk);
		}
		std::set<RE::TESPackage*> expectedPk;
		if (e) {
			std::size_t n = 0;
			for (const auto& ref : e->value("packages", nlohmann::json::array())) {
				++n;
				auto*      f = FindRef(dh, ref.get<std::string>());
				auto*      pk = f ? f->As<RE::TESPackage>() : nullptr;
				const auto pos = pk ? std::find(cur.begin(), cur.end(), pk) : cur.end();
				const bool has = pk && pos != cur.end();
				if (pk) {
					expectedPk.insert(pk);
				}
				add("Package " + std::to_string(n), has ? "present (#" + std::to_string(pos - cur.begin() + 1) + ")" : "missing",
					pk ? RefText(pk) : ref.get<std::string>() + "  (no record in this game)", has ? 1 : 2);
			}
			for (const auto& ref : e->value("packagesRemoved", nlohmann::json::array())) {
				auto* f = FindRef(dh, ref.get<std::string>());
				auto* pk = f ? f->As<RE::TESPackage>() : nullptr;
				if (pk) {
					const bool still = std::find(cur.begin(), cur.end(), pk) != cur.end();
					add("Package the overhaul removes", still ? "still there" : "removed", RefText(pk), still ? 2 : 1);
				}
			}
		}
		for (auto* pk : cur) {
			if (!expectedPk.count(pk)) {
				add(e ? "Other package (kept from the winner)" : "Package", "present", RefText(pk), 0);
			}
		}

		// factions the overhaul adds
		if (e) {
			for (const auto& fa : e->value("factionsAdded", nlohmann::json::array())) {
				auto* f = FindRef(dh, fa.value("form", std::string()));
				auto* fac = f ? f->As<RE::TESFaction>() : nullptr;
				if (!fac) {
					continue;
				}
				bool has = false;
				for (const auto& fr : a_npc->factions) {
					has = has || fr.faction == fac;
				}
				add("Faction", has ? "present" : "missing", RefText(fac), has ? 1 : 2);
			}
		}

		// the four override lists
		static const std::pair<const char*, RE::BGSListForm* RE::BGSOverridePackCollection::*> kLists[] = {
			{ "spectator", &RE::BGSOverridePackCollection::spectatorOverRidePackList }, { "combat", &RE::BGSOverridePackCollection::enterCombatOverRidePackList },
			{ "observe", &RE::BGSOverridePackCollection::observeCorpseOverRidePackList }, { "guardWarn", &RE::BGSOverridePackCollection::guardWarnOverRidePackList }
		};
		for (const auto& [key, member] : kLists) {
			auto* slot = (*a_npc).*member;
			const std::string slotText = slot ? RefText(slot) : std::string("(none)");
			if (e && e->contains("lists") && (*e)["lists"].contains(key)) {
				const auto& l = (*e)["lists"][key];
				auto*       toF = l["to"].is_string() ? FindRef(dh, l["to"].get<std::string>()) : nullptr;
				auto*       fromF = l["from"].is_string() ? FindRef(dh, l["from"].get<std::string>()) : nullptr;
				if (toF) {
					add(std::string(key) + " override list", slotText, RefText(toF), slot == toF ? 1 : (slot == fromF ? 2 : 0));
					continue;
				}
			}
			if (slot) {
				add(std::string(key) + " override list", slotText, "", 0);
			}
		}

		// flags
		if (e) {
			for (const auto& fl : e->value("flagsAdded", nlohmann::json::array())) {
				const bool ess = fl.get<std::string>() == "Essential";
				const auto flag = ess ? RE::ACTOR_BASE_DATA::Flag::kEssential : RE::ACTOR_BASE_DATA::Flag::kProtected;
				const bool has = a_npc->actorData.actorBaseFlags.any(flag);
				add(ess ? "Essential" : "Protected", has ? "set" : "not set", "set", has ? 1 : 2);
			}
		}

		// confidence of the base record. The live actor value is not read here: the actor structure differs between game builds and reading it crashed on 1.7.104.
		// A save can keep an actor value the base record no longer has: check that one with `getav confidence` in the console.
		{
			const int now = Confidence(a_npc);
			if (e && e->contains("confidence")) {
				const int to = (*e)["confidence"].value("to", 2);
				const int from = (*e)["confidence"].value("from", -1);
				add("Confidence (base record)", std::to_string(now), std::to_string(to), now == to ? 1 : (now == from ? 2 : 0));
			} else {
				add("Confidence (base record)", std::to_string(now), "", 0);
			}
			(void)a_live;
		}
		return r;
	}

	std::vector<std::pair<std::string, std::uint32_t>> FindInProfile(const std::string& a_needle, std::size_t a_max)
	{
		std::vector<std::pair<std::string, std::uint32_t>> out;
		const auto                                         needle = Lower(a_needle);
		if (needle.empty()) {
			return out;
		}
		for (const auto& [id, pr] : g_expected) {
			auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(id);
			if (!npc) {
				continue;
			}
			const std::string name = npc->GetName();
			const std::string ed = pr.second.value("name", std::string());
			if (Lower(name).find(needle) != std::string::npos || Lower(ed).find(needle) != std::string::npos) {
				out.emplace_back(name + "  (" + ed + ")", id);
				if (out.size() >= a_max) {
					break;
				}
			}
		}
		return out;
	}

	void Run()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			return;
		}
		LoadConfig();
		g_rows.clear();
		g_expected.clear();
		g_info = Summary{};
		const auto audit = Audit(dh);
		auto finish = [&]() {
		if (g_cfg.forward) {
			Forward(dh, g_rows, g_info);
			g_info.ran = g_info.ran || !g_info.profiles.empty();
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
		};
		g_info.icaio = dh->LookupModByName(kIcaio) != nullptr;
		g_info.three = dh->LookupModByName("3DNPC.esp") != nullptr;
		SKSE::log::info("Immersive Citizens - AI Overhaul: {}, Interesting NPCs (3DNPC): {}", g_info.icaio ? "loaded" : "NOT loaded", g_info.three ? "loaded" : "not loaded");
		if (!g_info.icaio || !g_cfg.icaioStep) {
			finish();
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
			finish();
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
			{
				bool wins = false;
				for (const auto& c : ChainOf(npc)) {
					wins = wins || g_cfg.wins.count(c) > 0;
				}
				if (wins) {
					skip("a plugin that must win (Better Vampires) changes this NPC");
					continue;
				}
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
		finish();
	}
}
