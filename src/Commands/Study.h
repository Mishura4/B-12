#ifndef B12_STUDY_H_
#define B12_STUDY_H_

#include "B12.h"

#include "Commands/MultiCommandProcess.h"
#include "Guild/Guild.h"

namespace B12
{
	struct StudySetupMCP : MultiCommandProcess
	{
		using MultiCommandProcess::MultiCommandProcess;
		using MultiCommandProcess::operator=;

		bool   step(const dpp::message& msg) final;
		uint32 nbSteps() const final;

		Guild*             guild{nullptr};
		GuildSettingsEntry guildSettingsEntry{};
	};

	constexpr auto STUDY_COMMAND_BUTTON_ID = "study_toggle";
} // namespace B12

#endif
