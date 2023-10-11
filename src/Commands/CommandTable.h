#pragma once

#include <type_traits>

#include "B12.h"
#include "CommandHandler.h"
#include "CommandResponse.h"

#include "Commands/Command.h"
#include "Core/Bot.h"

using namespace B12;

template <shion::basic_string_literal Command>
constexpr inline auto default_command_handler = &CommandHandler::command<Command>;

template <shion::basic_string_literal Name, command_handler_type auto Handler, typename... Options>
#ifndef __INTELLISENSE__
requires (true && ... && _::is_command_option<std::remove_cvref_t<Options>>)
#endif
constexpr auto make_command(std::string_view description, Options&&... options)
{
	return (Command<Name, Handler, std::remove_cvref_t<Options>...>(description, std::forward<Options>(options)...));
}

template <shion::basic_string_literal Name, typename... Options>
#ifndef __INTELLISENSE__
requires (true && ... && _::is_command_option<std::remove_cvref_t<Options>>)
#endif
constexpr auto make_command(std::string_view description, Options&&... options)
{
	return (
		Command<Name, default_command_handler<Name>, std::remove_cvref_t<Options>...>(description, std::forward<Options>(options)...)
	);
}

template <shion::basic_string_literal Name, typename... Options>
#ifndef __INTELLISENSE__
requires (true && ... && _::is_command_option<std::remove_cvref_t<Options>>)
#endif
constexpr auto make_command(
	std::string_view description,
	uint64_t         user_permissions,
	uint64_t         bot_permissions,
	Options&&...     options
)
{
	return (Command<Name, default_command_handler<Name>, std::remove_cvref_t<Options>...>(
		description,
		user_permissions,
		bot_permissions,
		std::forward<Options>(options)...
	));
}

template <shion::basic_string_literal Name, command_handler_type auto Handler = default_command_handler<Name>>
constexpr auto make_command(std::string_view description)
{
	return (Command<Name, Handler>(description));
}

template <shion::basic_string_literal Name, typename... CommandTypes>
requires (std::same_as<dpp::command_option_type, CommandTypes> && ...)
constexpr auto make_option(std::string_view description, bool required, CommandTypes... types)
{
	return (CommandOption<Name, sizeof...(CommandTypes)>(
		description,
		required,
		std::forward<CommandTypes>(types)...
	));
}

constexpr inline auto noop_command = [](auto...) -> CommandResponse
{
	return {CommandResponse::InternalError{}, {{"Invalid command"}}};
};

static inline constexpr auto test = _::is_command_option<CommandOption<"role", 1>>;

inline constexpr std::tuple COMMAND_TABLE = std::make_tuple(
	make_command<"bigmoji">(
		"Make an emoji big!",
		dpp::p_use_external_emojis,
		dpp::p_embed_links | dpp::p_send_messages,
		make_option<"emoji">("Emoji to make bigger - for example :sadcat:", true, dpp::co_string)
	),
	make_command<"server", nullptr>("Server-level commands"),
	make_command<"server settings", nullptr>("Change server-specific bot settings"),
	make_command<"server settings study">(
		"Set up study mode",
		dpp::p_manage_roles | dpp::p_manage_channels,
		dpp::p_manage_roles,
		CommandOption<"role", 1>{"Role that will be used for the study mode", false, dpp::co_role},
		CommandOption<"channel", 1>{
			"Channel that will be used for the study mode",
			false,
			dpp::co_channel}),
	make_command<"server sticker", nullptr>("Sticker-related commands"),
	make_command<"server sticker grab">(
		"Add a sticker from a message",
		dpp::p_manage_emojis_and_stickers,
		dpp::p_manage_emojis_and_stickers,
		CommandOption<"message", 1>{"Message ID to grab from", true, dpp::co_string},
		CommandOption<"channel", 1>{"Channel to grab from", false, dpp::co_channel}
	),
	make_command<"ban">(
		"Ban a user",
		dpp::p_ban_members,
		dpp::p_ban_members,
		CommandOption<"user", 1>{"User to ban or retrieve ban information", true, dpp::co_user},
		CommandOption<"time", 1>{"Time to ban a user for", false, dpp::co_string},
		CommandOption<"reason", 1>{"Reason for the ban", false, dpp::co_string}
	),
	make_command<"pokemon", nullptr>("Pokemon-related commands"),
	make_command<"pokemon dex">(
		"Show information from a pokemon",
		dpp::p_send_messages,
		dpp::p_send_messages,
		CommandOption<"name-or-number", 1>{"Name or national number of the pokemon", true, dpp::co_string}
	),
	make_command<"poll">(
		"Create a poll",
		dpp::p_send_messages,
		dpp::p_send_messages,
		CommandOption<"title", 1>{"Title of the poll", true, dpp::co_string},
		CommandOption<"choice-1", 1>{"Choice 1 for the poll", true, dpp::co_string},
		CommandOption<"choice-2", 1>{"Choice 2 for the poll", true, dpp::co_string},
		CommandOption<"choice-3", 1>{"Choice 3 for the poll", false, dpp::co_string},
		CommandOption<"choice-4", 1>{"Choice 4 for the poll", false, dpp::co_string},
		CommandOption<"choice-5", 1>{"Choice 5 for the poll", false, dpp::co_string},
		CommandOption<"choice-6", 1>{"Choice 6 for the poll", false, dpp::co_string},
		CommandOption<"choice-7", 1>{"Choice 7 for the poll", false, dpp::co_string},
		CommandOption<"choice-8", 1>{"Choice 8 for the poll", false, dpp::co_string},
		CommandOption<"create-thread", 1>{"Whether to create a thread or not (default: no)", false, dpp::co_boolean},
		CommandOption<"ping-role", 1>{"Role to ping / to grab into the thread (default: none)", false, dpp::co_role}
	)
);
