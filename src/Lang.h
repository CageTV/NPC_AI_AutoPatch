/*
 * NPC AI AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#pragma once

#include <string>
#include <utility>
#include <vector>

// The language of the in-game pages. The English text of a string is its key: Data/SKSE/Plugins/NPCAIAutoPatch/lang/<code>.json holds
// {"name": "Espanol", "strings": {"English text": "texto"}, "phrases": {"English part": "parte"}}. What a file does not have stays English.
namespace Lang
{
	// Loads the language to use: "auto" is the language Windows is set to, otherwise a code (en, es, de, ja, zh, ko, or any lang/<code>.json).
	// Called at the start of every page; it only does work when the setting changed.
	void Ensure(const std::string& a_setting);

	// A whole string (a label, a sentence). Not translated: returned as it is.
	const char* T(const char* a_english);

	// A text built by the plugin (a reason, a state, a table cell): the known phrases in it are translated, a " (winner: plugin)" tail is kept as it is.
	std::string P(const std::string& a_english);

	// Japanese, Chinese and Korean need a font that SKSE Menu Framework only loads when its ini says so (EnableJapanese / EnableChinese / EnableKorean).
	// The name of that setting when the language in use (or the Windows language, in auto mode) needs it and it is off, otherwise empty.
	std::string FontNeeded();

	std::string Code();                                            // the language in use
	std::string Detected();                                        // the language Windows is set to (the code, "en" when there is no file for it)
	std::vector<std::pair<std::string, std::string>> Available();  // code, native name; English first
}
