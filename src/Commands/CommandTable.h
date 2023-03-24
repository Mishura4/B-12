#pragma once

#include "B12.h"
#include "Commands/Command.h"
#include "Core/Bot.h"

using namespace B12;

template <string_literal Name, command_handler_type auto Handler, command_option_type... Options>
constexpr auto make_command(std::string_view description, Options&&... options)
{
	return (Command<Name, Handler, Options...>(description, std::forward<Options>(options)...));
}

template <string_literal Name, command_option_type... Options>
constexpr auto make_command(std::string_view description, Options&&... options)
{
	return (
		Command<Name, Bot::command<Name>, Options...>(description, std::forward<Options>(options)...));
}

template <string_literal Name, command_option_type... Options>
constexpr auto make_command(
	std::string_view description,
	uint64_t         user_permissions,
	uint64_t         bot_permissions,
	Options&&...     options
)
{
	return (Command<Name, Bot::command<Name>, Options...>(
		description,
		user_permissions,
		bot_permissions,
		std::forward<Options>(options)...
	));
}

template <shion::string_literal Name, command_handler_type auto Handler = Bot::command<Name>>
constexpr auto make_command(std::string_view description)
{
	return (Command<Name, Handler>(description));
}

template <shion::string_literal Name, dpp_command_option_type... CommandTypes>
constexpr auto make_option(std::string_view description, bool required, CommandTypes&&... types)
{
	return (CommandOption<Name, sizeof...(CommandTypes)>(
		description,
		required,
		std::forward<CommandTypes>(types)...
	));
}

inline constexpr std::tuple COMMAND_TABLE = std::make_tuple(
	make_command<"meow">("meow to me!"),
	make_command<"study">("Toggle study mode", 0, dpp::p_manage_roles),
	make_command<"bigmoji">(
		"Make an emoji big!",
		dpp::p_use_external_emojis,
		dpp::p_embed_links | dpp::p_send_messages,
		make_option<"emoji">("Emoji to make bigger - for example :sadcat:", true, dpp::co_string)
	),
	make_command<"settings", shion::noop>("Change bot settings"),
	make_command<"settings server", shion::noop>("Change server-specific bot settings"),
	make_command<"settings server study">(
		"Set up study mode",
		dpp::p_manage_roles | dpp::p_manage_channels,
		dpp::p_manage_roles,
		CommandOption<"role", 1>{"Role that will be used for the study mode", false, dpp::co_role},
		CommandOption<"channel", 1>{
			"Channel that will be used for the study mode",
			false,
			dpp::co_channel
		}
	)
);
