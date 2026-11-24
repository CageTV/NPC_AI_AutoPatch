/*
 * NPC AI AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE.
 */
#include "PCH.h"
#include "Core.h"
#include "Menu.h"

namespace
{
	void SetupLog()
	{
		const auto dir = SKSE::log::log_directory();
		if (!dir) {
			return;
		}
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "NPCAIAutoPatch.log").string(), true);
		auto log = std::make_shared<spdlog::logger>("NPCAIAutoPatch", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
		spdlog::set_default_logger(std::move(log));
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kPostLoad) {
			Menu::Register();
		} else if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			try {
				Core::Run();
			} catch (const std::exception& e) {
				SKSE::log::error("failed: {}", e.what());
			}
		} else if (a_msg->type == SKSE::MessagingInterface::kInputLoaded) {
			Core::Recheck("the main menu");
		} else if (a_msg->type == SKSE::MessagingInterface::kNewGame) {
			Core::Recheck("a new game");
		} else if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame) {
			Core::Recheck("a save loaded");
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();
	SKSE::log::info("NPCAIAutoPatch 0.4.2, game {}", REL::Module::get().version().string());
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
