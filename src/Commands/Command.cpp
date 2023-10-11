#include "B12.h"

#include "Command.h"
#include "Core/Bot.h"

#include "Guild/Guild.h"

#include <regex>

#include "CommandHandler.h"

#include "commands.h"

using namespace B12;

dpp::coroutine<command::response> command::meow(dpp::interaction_create_t const &event)
{
	static constexpr auto INTRO_URL =
		"https://cdn.discordapp.com/attachments/1066393377236594699/1066779084845220020/b-12.mp4"sv;

	auto reply = dpp::message{ std::string{INTRO_URL} };
	std::string id = event.command.id.str();
	co_return response::reply(reply);
}

dpp::coroutine<command::response> command::bigmoji(dpp::interaction_create_t const &event, const std::string &emoji)
{
	using namespace std::string_view_literals;

	std::regex pattern{"^(?:\\s*)<(a?):([a-zA-Z0-9_]+):([0-9]+)>(?:\\s*)$"};
	std::string match;
	std::match_results<std::string::const_iterator> results{};
	if (!std::regex_match(emoji, results, pattern) || results.size() < 4)
		co_return {{"Please give a custom emoji as the parameter."}};
	auto id = results[3];
	co_return {{
		format("https://cdn.discordapp.com/emojis/{}{}?size=256&quality=lossless",
		std::string_view{id.first, id.second},
		(results[1].length() ? ".gif"sv : ".webp"sv))
	}};
}
