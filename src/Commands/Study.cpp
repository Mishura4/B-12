#include "B12.h"

#include "Core/Bot.h"

#include "Study.h"
#include "Command.h"
#include "Data/DataStores.h"
#include "Data/Lang.h"

#include "CommandHandler.h"

#include <algorithm>

using namespace B12;

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

namespace
{
	CommandResponse get_study_info_message(Guild* guild)
	{
		const auto& curRole    = guild->studyRole();
		const auto& curChannel = guild->studyChannel();

		return {
			CommandResponse::Success{},
			{
				{
					fmt::format(
						"Welcome to the `/study` installation wizard! "
						"<:catthumbsup:1066427078284681267>\n\nCurrent "
						"study role is {}\nCurrent study channel is {}\n\nTo change these settings, please use "
						"`/server study settings` with command parameters.",
						(curRole ? curRole->get_mention() : "(none)"),
						(curChannel ? curChannel->get_mention() : "(none)")
					)
				}
			}
		};
	}

	struct StudySettingsHelper
	{
		Guild*                      guild;
		std::optional<dpp::role>    newRole{std::nullopt};
		std::optional<dpp::channel> newChannel{std::nullopt};
		std::vector<dpp::message>   additionalMessages;
		std::string                 responseNotes;

		using set_result = std::tuple<CommandResponse::Data, std::string>;

		dpp::message build_success_message()
		{
			std::stringstream ss;

			ss << "Successfully set ";
			if (newRole.has_value())
			{
				ss << fmt::format("study role to {}", newRole.value());
				guild->studyRole(&newRole.value());
				if (newChannel.has_value())
					ss << " and ";
			}
			if (newChannel.has_value())
			{
				guild->studyChannel(&newChannel.value());
				ss << fmt::format("study channel to {}", newChannel.value());
			}
			ss << '!';
			if (!responseNotes.empty())
			{
				ss << responseNotes;
			}
			return {ss.str()};
		}

		set_result set_role(dpp::snowflake role_id)
		{
			try
			{
				dpp::role_map role_map =
					Bot::bot().roles_get_sync(
						guild->settings().get<"snowflake">()
					); // TODO: give access to id from guild

				auto it = role_map.find(role_id);
				if (it == role_map.end())
					return {
						CommandResponse::UsageError{},
						lang::DEFAULT.ERROR_ROLE_NOT_FOUND.format(role_id)
					};
				newRole = it->second;
				guild->studyRole(&newRole.value());
			}
			catch (const dpp::rest_exception&)
			{
				return {CommandResponse::APIError{}, lang::DEFAULT.ERROR_ROLE_FETCH_FAILED.format(role_id)};
			}
			return {CommandResponse::Success{}, {}};
		}

		set_result set_channel(dpp::snowflake channel_id)
		{
			newChannel = guild->findChannel(channel_id);
			if (!newChannel)
				return {
					CommandResponse::APIError{},
					lang::DEFAULT.ERROR_CHANNEL_NOT_FOUND.format(channel_id)
				};

			dpp::permission myPermissions     = guild->getPermissions(guild->b12Member(), *newChannel);
			dpp::permission permissionsNeeded = dpp::p_send_messages | dpp::p_use_application_commands;

			if (!myPermissions.has(permissionsNeeded))
			{
				return {
					CommandResponse::APIError{},
					lang::DEFAULT.PERMISSION_BOT_MISSING.format(
						newChannel.value(),
						permissionsNeeded ^ (myPermissions & permissionsNeeded)
					)
				};
			}
			return (post_study_message());
		}

		set_result post_study_message()
		{
			dpp::message studyMessage{lang::DEFAULT.STUDY_CHANNEL_WELCOME};
			auto         messageCallback = [&](const dpp::message& m)
			{
				studyMessage = m;
			};
			B12::AsyncExecutor<dpp::message> executor(messageCallback);

			studyMessage.set_guild_id(newChannel->guild_id);
			studyMessage.set_channel_id(newChannel->id);
			studyMessage.add_component(
				dpp::component().add_component(
					dpp::component{}.set_emoji("\xF0\x9F\x93\x96").set_id(STUDY_COMMAND_BUTTON_ID)
				)
			);
			executor(&dpp::cluster::message_create, &Bot::bot(), studyMessage);
			executor.wait();
			if (!studyMessage.sent)
				return {CommandResponse::InternalError{}, lang::DEFAULT.ERROR_GENERIC_COMMAND_FAILURE};
			return {CommandResponse::Success{}, {}};
		}

		set_result handle_opt(const dpp::command_data_option& opt)
		{
			if (std::holds_alternative<dpp::snowflake>(opt.value))
			{
				auto id = std::get<dpp::snowflake>(opt.value);

				if (opt.type == dpp::co_role)
				{
					if (set_result result = set_role(id);
						!std::holds_alternative<CommandResponse::Success>(std::get<0>(result)))
						return (result);
				}
				else if (opt.type == dpp::co_channel)
				{
					if (set_result result = set_channel(id);
						!std::holds_alternative<CommandResponse::Success>(std::get<0>(result)))
						return (result);
				}
			}
			return {CommandResponse::Success{}, {}};
		}
	};
} // namespace

template <>
CommandResponse CommandHandler::command<"server settings study">(
	command_option_view options
)
{
	if (options.empty())
	{
		Guild* guild = Bot::fetchGuild(_guild_id);

		return (get_study_info_message(guild));
	}
	StudySettingsHelper       helper{.guild = Bot::fetchGuild(_guild_id)};
	std::vector<dpp::message> additional_messages;

	for (const auto& opt : options)
	{
		if (auto result = helper.handle_opt(opt);
			!std::holds_alternative<CommandResponse::Success>(std::get<0>(result)))
			return {std::get<0>(result), {std::get<1>(result)}};
	}
	std::vector<std::string> warnings;

	if (!helper.guild->saveSettings())
	{
		warnings.emplace_back(
			"\n\xE2\x9A\xA0**There was an issue saving the settings, "
			"they will be lost after next bot restart**\xE2\x9A\xA0"
		);
	}
	return {
		CommandResponse::Success{},
		{helper.build_success_message(), warnings, additional_messages}
	};
}

namespace
{
	template <typename T>
	constexpr auto to_array_actually(auto&&... args)
	{
		return (std::array<T, sizeof...(args)>{T{args}...});
	}

	constexpr auto handlers = to_array_actually<bool (*)(const dpp::message&, StudySetupMCP&)>(
		[](const dpp::message& msg, StudySetupMCP& mcp)
		{
			dpp::message reply{"step 2!"};

			reply.set_channel_id(msg.channel_id);
			reply.set_guild_id(msg.guild_id);
			reply.set_reference(msg.id, msg.guild_id, msg.channel_id);
			Bot::bot().message_create(reply, mcp.callback);
			return (true);
		},
		[](const dpp::message& msg, StudySetupMCP& mcp)
		{
			dpp::message reply{"step 3 hehe"};

			reply.set_channel_id(msg.channel_id);
			reply.set_guild_id(msg.guild_id);
			reply.set_reference(msg.id, msg.guild_id, msg.channel_id);
			Bot::bot().message_create(reply, mcp.callback);
			return (true);
		}
	);
} // namespace

bool StudySetupMCP::step(const dpp::message& msg)
{
	if (handlers[step_index](msg, *this))
	{
		++step_index;
		return (true);
	}
	return (false);
}

uint32_t StudySetupMCP::nbSteps() const
{
	return (std::tuple_size_v<decltype(handlers)>);
}
