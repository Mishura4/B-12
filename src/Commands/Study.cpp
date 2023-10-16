#include "B12.h"

#include "Core/Bot.h"

#include "Study.h"
#include "Command.h"
#include "Data/DataStores.h"
#include "Data/Lang.h"

#include "command_handler.h"

#include <algorithm>

using namespace B12;

using namespace B12::command;

namespace
{
	//constexpr auto STUDY_EMOJI = "\xF0\x9F\x93\x96"sv;

	/*void add_study_button(dpp::message& m)
	{
		m.add_component(
			dpp::component{}.add_component(
				dpp::component{}.set_emoji(std::string(STUDY_EMOJI)).set_id(STUDY_COMMAND_BUTTON_ID)
			)
		);
	}*/
}

#include "commands.h"

dpp::coroutine<command::response> command::study(dpp::interaction_create_t const &event)
{
	Guild*          guild = Bot::fetchGuild(event.command.guild_id);
	CommandResponse reply;

	if (!guild->studyRole() || !guild->studyChannel())
		co_return {response::usage_error(lang::DEFAULT.ERROR_STUDY_BAD_SETTINGS)};
	auto *cluster = event.from->creator;
	const dpp::role&         studyRole = guild->studyRole().value();
	const dpp::guild_member& issuer    = event.command.member;

	auto thinking = event.co_thinking(true);
	if (std::ranges::find(issuer.get_roles(), studyRole.id) != issuer.get_roles().end()) {
		auto&& confirm = co_await event.from->creator->co_guild_member_remove_role(event.command.guild_id, event.command.usr.id, studyRole.id);
		co_await thinking;
		if (confirm.is_error()) {
			cluster->log(dpp::ll_error, fmt::format("could not remove study role from {}: {}", event.command.usr.format_username(), confirm.get_error().message));
			co_return {response::internal_error("Could not remove the study role, am I missing permissions?"), response::action_t::edit};
		}
		co_return {response::success(), response::action_t::edit};
	}
	else {
		auto&& confirm = co_await event.from->creator->co_guild_member_add_role(event.command.guild_id, event.command.usr.id, studyRole.id);
		co_await thinking;
		if (confirm.is_error()) {
			cluster->log(dpp::ll_error, fmt::format("could not add study role to {}: {}", event.command.usr.format_username(), confirm.get_error().message));
			co_return {response::internal_error("Could not remove the study role, am I missing permissions?"), response::action_t::edit};
		}
		event.from->creator->message_create(dpp::message{event.command.channel_id, fmt::format(
			"{} was sent to the {} realm. <a:nodyesnod:1078439588021944350>",
			issuer.get_mention(),
			std::get<dpp::command_interaction>(event.command.data).get_mention()
		)});
		co_return {{lang::DEFAULT.COMMAND_STUDY_ADDED.format(*guild->studyChannel())}, response::action_t::edit};
	}
}

dpp::coroutine<response> command::server_settings_study(dpp::interaction_create_t const &event, optional_param<const dpp::role &> role, optional_param<const dpp::channel &> channel)
{
	Guild* guild = Bot::fetchGuild(event.command.guild_id);
	if (!role.has_value() && !channel.has_value())
		co_return (get_study_info_message(guild));
	std::vector<std::string> warnings;
	dpp::cluster *cluster = event.from->creator;

	auto thinking = event.co_thinking();
	std::string msg;

	if (channel.has_value()) {
		const dpp::channel &c = channel->get();
		dpp::message study_message{lang::DEFAULT.STUDY_CHANNEL_WELCOME};

		study_message.set_guild_id(c.guild_id);
		study_message.set_channel_id(c.id);
		study_message.add_component(
			dpp::component().add_component(
				dpp::component{}.set_emoji("\xF0\x9F\x93\x96").set_id(STUDY_COMMAND_BUTTON_ID)
			)
		);

		dpp::confirmation_callback_t result = co_await cluster->co_message_create(study_message);
		if (result.is_error()) {
			co_await thinking;
			B12::log(LogLevel::ERROR, "could not post study message in channel {} of guild {}: {}",
				c.id, c.guild_id, result.get_error().human_readable);
			co_return (response::edit(fmt::format(
				"Could not post study message in channel {} ; do I have the correct permissions?",
				c.get_mention()
			)));
		}
		guild->studyChannel(&channel->get());
		msg = fmt::format("channel to {}", c.get_mention());
	}
	if (role.has_value()) {
		guild->studyRole(&role->get());
		msg = fmt::format("{}role to {}", msg.empty() ? "" : fmt::format("{} and ", msg), role->get().get_mention());
	}
	if (!guild->saveSettings())
	{
		warnings.emplace_back(
			"\n\xE2\x9A\xA0**There was an issue saving the settings, "
			"they will be lost after next bot restart**\xE2\x9A\xA0"
		);
	}
	co_await thinking;
	co_return (response::edit(response::success(fmt::format(
		"Successfully set study {}!{}", msg, fmt::join(warnings, "\n")
	))));
}
