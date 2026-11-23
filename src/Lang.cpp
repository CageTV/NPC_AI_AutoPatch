/*
 * NPC AI AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#include "PCH.h"
#include "Lang.h"
#include "Core.h"

#include <nlohmann/json.hpp>

namespace Lang
{
	namespace
	{
		std::string                                  g_setting = "\x01";  // never equal to a real setting: the first Ensure loads
		std::string                                  g_code = "en";
		std::string                                  g_windowsCjk;  // auto mode fell back to English: the Windows language is this one and its font is off
		std::unordered_map<std::string, std::string> g_strings;
		std::vector<std::pair<std::string, std::string>> g_phrases;  // longest English part first
		std::unordered_map<std::string, std::string>     g_cache;      // translated strings handed out by T (the pointers must stay valid)

		std::string Lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		std::filesystem::path LangDir() { return Core::DataDir() / "lang"; }

		// the SKSE Menu Framework setting that loads the font of a language ("" when the default font has its letters)
		std::string FontSetting(const std::string& a_code)
		{
			return a_code == "ja" ? "EnableJapanese" : a_code == "zh" ? "EnableChinese" : a_code == "ko" ? "EnableKorean" : std::string();
		}

		bool FontEnabled(const std::string& a_setting)
		{
			if (a_setting.empty()) {
				return true;
			}
			std::ifstream in("Data/SKSE/Plugins/SKSEMenuFramework.ini");
			std::string   line;
			const auto    key = Lower(a_setting);
			while (std::getline(in, line)) {
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				std::string name;
				for (const char c : line.substr(0, eq)) {
					if (std::isalnum(static_cast<unsigned char>(c))) {
						name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
					}
				}
				if (name == key) {
					const auto v = Lower(line.substr(eq + 1));
					return v.find("true") != std::string::npos || v.find('1') != std::string::npos;
				}
			}
			return false;
		}

		std::string EnglishName(const std::string& a_code)
		{
			return a_code == "ja" ? "Japanese" : a_code == "zh" ? "Chinese" : a_code == "ko" ? "Korean" : std::string();
		}

		std::string DetectWindows()
		{
			const LANGID id = GetUserDefaultUILanguage();
			switch (PRIMARYLANGID(id)) {
			case LANG_SPANISH:
				return "es";
			case LANG_GERMAN:
				return "de";
			case LANG_JAPANESE:
				return "ja";
			case LANG_CHINESE:
				return "zh";
			case LANG_KOREAN:
				return "ko";
			default:
				return "en";
			}
		}

		bool HasFile(const std::string& a_code)
		{
			std::error_code ec;
			return a_code == "en" || std::filesystem::is_regular_file(LangDir() / (a_code + ".json"), ec);
		}

		void Load(const std::string& a_code)
		{
			g_strings.clear();
			g_phrases.clear();
			g_cache.clear();
			g_code = "en";
			if (a_code == "en" || !HasFile(a_code)) {
				return;
			}
			std::ifstream in(LangDir() / (a_code + ".json"), std::ios::binary);
			const auto    j = nlohmann::json::parse(in, nullptr, false, true);
			if (j.is_discarded() || !j.is_object()) {
				SKSE::log::warn("lang/{}.json is not valid JSON: English is used", a_code);
				return;
			}
			if (j.contains("strings") && j["strings"].is_object()) {
				for (const auto& [k, v] : j["strings"].items()) {
					if (v.is_string()) {
						g_strings[k] = v.get<std::string>();
					}
				}
			}
			if (j.contains("phrases") && j["phrases"].is_object()) {
				for (const auto& [k, v] : j["phrases"].items()) {
					if (v.is_string() && !k.empty()) {
						g_phrases.emplace_back(k, v.get<std::string>());
					}
				}
				std::sort(g_phrases.begin(), g_phrases.end(), [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
			}
			g_code = a_code;
			SKSE::log::info("language of the pages: {} ({} strings, {} phrases)", a_code, g_strings.size(), g_phrases.size());
		}

		std::string Replace(std::string s)
		{
			// one pass from left to right, the longest phrase that starts at each position wins; what was translated is never looked at again
			std::string out;
			std::size_t i = 0;
			while (i < s.size()) {
				bool hit = false;
				for (const auto& [from, to] : g_phrases) {
					if (s.compare(i, from.size(), from) == 0) {
						out += to;
						i += from.size();
						hit = true;
						break;
					}
				}
				if (!hit) {
					out += s[i++];
				}
			}
			return out;
		}
	}

	void Ensure(const std::string& a_setting)
	{
		if (a_setting == g_setting) {
			return;
		}
		g_setting = a_setting;
		g_windowsCjk.clear();
		const bool  autoMode = a_setting.empty() || a_setting == "auto";
		std::string want = autoMode ? DetectWindows() : Lower(a_setting);
		if (autoMode && !FontEnabled(FontSetting(want))) {
			// the default font has no letters for this language: starting in English is better than a page of question marks
			g_windowsCjk = want;
			want = "en";
		}
		Load(want);
	}

	const char* T(const char* a_english)
	{
		if (!a_english || g_code == "en" || g_strings.empty()) {
			return a_english;
		}
		const std::string key = a_english;
		const auto        cached = g_cache.find(key);
		if (cached != g_cache.end()) {
			return cached->second.c_str();
		}
		// a label can carry an ImGui id after "##": only the text before it is translated
		const auto  hash = key.find("##");
		std::string text = hash == std::string::npos ? key : key.substr(0, hash);
		const auto  it = g_strings.find(text);
		if (it != g_strings.end()) {
			text = it->second;
		}
		if (hash != std::string::npos) {
			text += key.substr(hash);
		}
		return g_cache.emplace(key, std::move(text)).first->second.c_str();
	}

	std::string P(const std::string& a_english)
	{
		if (g_code == "en" || g_phrases.empty()) {
			return a_english;
		}
		static const std::string kWinner = " (winner: ";
		const auto               w = a_english.find(kWinner);
		if (w == std::string::npos) {
			return Replace(a_english);
		}
		// the plugin name after "(winner: " is not text: keep it as it is
		return Replace(a_english.substr(0, w)) + Replace(kWinner) + a_english.substr(w + kWinner.size());
	}

	std::string FontNeeded()
	{
		const auto code = g_windowsCjk.empty() ? g_code : g_windowsCjk;
		const auto setting = FontSetting(code);
		return !setting.empty() && !FontEnabled(setting) ? setting : std::string();
	}

	std::string Code() { return g_code; }

	std::string Detected()
	{
		const auto d = DetectWindows();
		return HasFile(d) ? d : "en";
	}

	std::vector<std::pair<std::string, std::string>> Available()
	{
		std::vector<std::pair<std::string, std::string>> out;
		out.emplace_back("en", "English");
		std::error_code ec;
		std::vector<std::filesystem::path> files;
		for (const auto& e : std::filesystem::directory_iterator(LangDir(), ec)) {
			if (e.path().extension() == ".json") {
				files.push_back(e.path());
			}
		}
		std::sort(files.begin(), files.end());
		for (const auto& f : files) {
			const auto code = Lower(f.stem().string());
			if (code == "en") {
				continue;
			}
			std::ifstream in(f, std::ios::binary);
			const auto    j = nlohmann::json::parse(in, nullptr, false, true);
			std::string name = !j.is_discarded() && j.is_object() ? j.value("name", code) : code;
			const auto  english = !j.is_discarded() && j.is_object() ? j.value("englishName", EnglishName(code)) : EnglishName(code);
			if (!english.empty() && english != name) {
				name += " (" + english + ")";  // readable even when the font of the language is not loaded
			}
			out.emplace_back(code, name);
		}
		return out;
	}
}
