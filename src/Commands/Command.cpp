#include "B12.h"

#include "Command.h"
#include "Core/Bot.h"

#include "Guild/Guild.h"

#include <regex>

#include "CommandHandler.h"

#include "Coro_test.h"

using namespace B12;

constexpr auto is_utf8_head = [](char c) constexpr
{
	return ((c & 0b11111000) == 0b11110000);
};
constexpr auto is_utf8_tail = [](char c) constexpr
{
	return ((c & 0b11000000) == 0b10000000);
};
constexpr auto is_utf8_emoji_maybe = [](auto begin, auto end) constexpr {
	auto it = begin;

	return (
		is_utf8_head(*it) &&
		std::distance(begin, end) >= 4 &&
		is_utf8_tail(*(++it)) &&
		is_utf8_tail(*(++it)) &&
		is_utf8_tail(*(++it))
	);
};

template <>
CommandResponse CommandHandler::command<"meow">(
	command_option_view /*options*/

)
{
	static constexpr auto INTRO_URL =
		"https://cdn.discordapp.com/attachments/1066393377236594699/1066779084845220020/b-12.mp4"sv;

	dpp::message response =
	{ std::string{INTRO_URL} };

	_source.sendThink(false);
	auto *cluster = &Bot::bot();
	auto test = [](dpp::cluster *cluster, dpp::interaction command) -> ::co_task<bool> {
		auto channel_id = command.channel_id;
		auto guild_id = command.guild_id;

		auto nested = [](dpp::cluster *cluster, dpp::interaction command) -> ::co_task<std::optional<dpp::message>> {
			auto get_nickname = [](dpp::cluster *cluster, dpp::interaction command) -> ::co_task<std::optional<std::string>> {
				B12::log(LogLevel::BASIC, "get nickname");
				dpp::confirmation_callback_t c = co_await cluster->co_guild_get_member(command.guild_id, command.get_issuing_user().id);
				B12::log(LogLevel::BASIC, "got");
				if (c.is_error())
					co_return {std::nullopt};
				auto member = c.get<dpp::guild_member>();
				B12::log(LogLevel::BASIC, "member {}", member.nickname);
				co_return {member.nickname};
			};
			dpp::message m = dpp::message{"testlol"}.set_channel_id(command.channel_id);
			::co_task nickname_coro = get_nickname(cluster, command);
			
				B12::log(LogLevel::BASIC, "after get nickname");
			dpp::confirmation_callback_t c = co_await cluster->co_message_create(m);
				B12::log(LogLevel::BASIC, "after message_create");
			if (c.is_error())
				co_return {std::nullopt};
			m = c.get<dpp::message>();
			if (c.is_error())
				co_return {std::nullopt};
				B12::log(LogLevel::BASIC, "co_await nickname_coro");
			std::optional<std::string> nickname = co_await nickname_coro;

			m.content = fmt::format("{} oh hi {}", m.content, *nickname);
			B12::log(LogLevel::BASIC, "editing");
			c = co_await cluster->co_message_edit(m);
			B12::log(LogLevel::BASIC, "edited");
			co_return {m};
		};

		::co_task<std::optional<dpp::message>> tasks[3];

		for (auto &task : tasks)
		{
			task = nested(cluster, command);
		}
		for (auto &task : tasks)
		{
			auto result = co_await task;
			B12::log(LogLevel::BASIC, "result : {}", result.has_value());
			if (!result.has_value())
				co_return false;
			co_await cluster->co_message_add_reaction(*result, std::string{lang::SUCCESS_EMOJI});
		}
		co_return true;
	};
	auto task = new co_task<bool>(std::move(test(cluster, std::get<0>(this->_source.source).event.command)));
	B12::log(LogLevel::BASIC, "boom");
	std::cout << "boom" << std::endl;
	return {CommandResponse::Success{}, dpp::message{"meow acknowledged <:hewwo:846148573782999111>"}};
}

template <>
CommandResponse CommandHandler::command<"bigmoji">(
	command_option_view options
)
{
	using namespace std::string_view_literals;

	if (options.empty() || !std::holds_alternative<std::string>(options[0].value))
		return {CommandResponse::UsageError{}, {{"Error: wrong parameter type"}}};
	const std::string& input = std::get<std::string>(options[0].value);
	std::regex pattern{"^(?:\\s*)<(a?):([a-zA-Z0-9_]+):([0-9]+)>(?:\\s*)$"};
	std::string match;
	std::match_results<std::string::const_iterator> results{};
	if (!std::regex_match(input, results, pattern) || results.size() < 4)
		return {CommandResponse::UsageError{}, {{"Please give a custom emoji as the parameter."}}};
	auto id = results[3];
	return CommandResponse{
		CommandResponse::Success{},
		{
			format("https://cdn.discordapp.com/emojis/{}{}?size=256&quality=lossless",
			std::string_view{id.first, id.second},
			(results[1].length() ? ".gif"sv : ".webp"sv))
		}
	};
}

template <>
CommandResponse CommandHandler::command<"poll">(
	command_option_view options
)
{
	static const std::string_view numbers[] = {
		"\x31\xef\xb8\x8f\xe2\x83\xa3",
		"\x32\xef\xb8\x8f\xe2\x83\xa3",
		"\x33\xef\xb8\x8f\xe2\x83\xa3",
		"\x34\xef\xb8\x8f\xe2\x83\xa3",
		"\x35\xef\xb8\x8f\xe2\x83\xa3",
		"\x36\xef\xb8\x8f\xe2\x83\xa3",
		"\x37\xef\xb8\x8f\xe2\x83\xa3",
		"\x38\xef\xb8\x8f\xe2\x83\xa3"
	};
	using choice = std::pair<std::optional<std::string>, std::string>;
	std::vector<std::optional<choice>> choices;
	std::optional<dpp::snowflake> role_id;
	bool create_thread = false;
	bool limit_1 = false;
	std::string title;

	choices.resize(8);
	for (const auto &opt : options)
	{
		constexpr auto choice_start = std::string_view("choice-");
		switch (opt.type)
		{
			case dpp::co_string:
			{
				if (opt.name.starts_with(choice_start))
				{
					if (auto choice_id = std::stoi(opt.name.substr(choice_start.size())); choice_id > 0 && choice_id < 9)
					{
						auto str_value = std::get<std::string>(opt.value);
						--choice_id;
						if (str_value.starts_with('<'))
						{
							if (auto separator = str_value.find('>'); separator != std::string::npos)
								choices[choice_id] = choice{str_value.substr(1, separator-1), str_value.substr(separator+1)};
						}
						else if (str_value.starts_with('\\'))
							str_value.erase(str_value.begin());
						else if (auto it = str_value.begin(); is_utf8_emoji_maybe(it, str_value.end()))
						{
							do { it += 4; }
							while (is_utf8_emoji_maybe(it, str_value.end()));

							std::string emoji{str_value.begin(), it};

							choices[choice_id] = choice{emoji, str_value.substr(emoji.length())};
						}
						if (!choices[choice_id].has_value())
							choices[choice_id] = choice{std::nullopt, str_value};
					}
				}
				else if (opt.name == "title")
					title = std::get<std::string>(opt.value);
			}
			break;

			case dpp::co_role:
			{
				if (opt.name == "ping-role")
				{
					role_id = std::get<dpp::snowflake>(opt.value);
				}
			}
			break;

			case dpp::co_boolean:
			{
				if (opt.name == "create-thread")
					create_thread = std::get<bool>(opt.value);
			}
		}
	}

	auto respond = [](
		dpp::cluster *cluster,
		dpp::interaction_create_t event,
		std::string title,
		std::vector<std::optional<choice>> choices,
		bool create_thread,
		bool limit_1,
		std::optional<dpp::snowflake> ping_role_id
	) -> ::co_task<void>
	{
		std::optional<dpp::role> role_ping;
		dpp::confirmation_callback_t confirm;
		
		confirm = co_await cluster->co_interaction_response_create(event.command.id, event.command.token, dpp::interaction_response{dpp::ir_deferred_channel_message_with_source});
		if (confirm.is_error())
			co_return;
		if (ping_role_id.has_value())
		{
			if (auto *cached_role = dpp::find_role(*ping_role_id); cached_role != nullptr)
				role_ping = *cached_role;
			else
			{
				confirm = co_await cluster->co_roles_get(event.command.guild_id);
				const auto &role_map = confirm.get<dpp::role_map>();
				if (auto it = role_map.find(*ping_role_id); it != role_map.end())
					role_ping = it->second;
			}
			if (!role_ping.has_value() || (role_ping->is_mentionable() && !(event.command.get_resolved_permission(event.command.get_issuing_user().id) & (dpp::p_administrator | dpp::p_mention_everyone))))
			{
				cluster->co_interaction_response_edit(event.command.token, dpp::message{"You do not have permissions to mention this role."}.set_flags(dpp::m_ephemeral));
				co_return;
			}
		}
		confirm = co_await dpp::awaitable(cluster, &dpp::cluster::interaction_followup_get_original, event.command.token);
		if (confirm.is_error())
			co_return;
		auto message = confirm.get<dpp::message>();

		message.content = fmt::format("\xF0\x9F\x93\x8A {}", title);
		int num = 0;
		std::vector<std::string> lines;
		::co_task<std::optional<std::string>> choice_coroutines[8];

		for (auto i = 0; i < choices.size(); ++i)
		{
			if (const auto &c = choices[i]; c.has_value())
			{
				std::cout << "hi" << std::endl;
				choice_coroutines[i] = [](dpp::cluster *cluster, dpp::snowflake message_id, dpp::snowflake channel_id, std::optional<choice> c, int num) -> ::co_task<std::optional<std::string>>
				{
					dpp::confirmation_callback_t confirm;
					std::string emoji;
					bool reacted = false;
					if (c->first.has_value())
					{
						emoji = *c->first;
						confirm = co_await cluster->co_message_add_reaction(message_id, channel_id, emoji);
						if (!confirm.is_error())
							reacted = true;
					}
					if (!reacted)
					{
						emoji = numbers[num];
						confirm = co_await cluster->co_message_add_reaction(message_id, channel_id, emoji);
					}
					if (confirm.is_error())
						co_return {};
					co_return {emoji};
				}(cluster, message.id, message.channel_id, c, num);
				++num;
			}
		}
		for (auto i = 0; i < choices.size(); ++i)
		{
			if (const auto &c = choices[i]; c.has_value())
			{
				std::cout << "choice " << i << std::endl;
				auto emoji = co_await(choice_coroutines[i]);
				if (!emoji.has_value())
				{
					cluster->interaction_response_edit(event.command.token, message.set_content("Failed to set up message reactions."));
					co_return;
				}
				auto skipped = c->second | std::ranges::views::drop_while([](char c){ return (c == ' ' || c == '\t'); });
				lines.push_back(fmt::format("{} {}", is_utf8_emoji_maybe(emoji->begin(), emoji->end()) ? *emoji : std::format("<{}>", *emoji), std::string_view{skipped.begin(), skipped.end()}));
			}
		}
		const auto &author = event.command.get_issuing_user();

		message.add_embed(
			dpp::embed{}
			.set_footer(fmt::format("{}#{}", author.username, author.discriminator), "")
			.set_description(fmt::format("{}", fmt::join(lines, "\n")))
		);
		std::cout << "edit " << std::endl;
		confirm = co_await cluster->co_interaction_response_edit(event.command.token, message);
		if (confirm.is_error())
			cluster->interaction_response_edit(event.command.token, message.set_content("Failed to set up message."));
		if (create_thread)
		{
			cluster->thread_create_with_message(title, message.channel_id, message.id, 60, 0,
				[cluster, role_ping, channel_id = event.command.channel_id, author = event.command.usr, author_member = event.command.member](const dpp::confirmation_callback_t &confirmation)
				{
					if (confirmation.is_error())
					{
						cluster->message_create(dpp::message{"(could not create thread)"}.set_channel_id(channel_id));
						return;
					}
					dpp::thread thread = confirmation.get<dpp::thread>();
					if (role_ping)
					{
						dpp::message message{fmt::format("New poll started by {}! {}", author.get_mention(), role_ping->get_mention())};

						message.allowed_mentions.roles.push_back(role_ping->id);
						message.allowed_mentions.users.push_back(author.id);
						message.mention_roles.push_back(role_ping->id);
						message.mentions.emplace_back(author, author_member);
						message.set_channel_id(thread.id);
						cluster->message_create(message);
					}
				});
		}
	};
	auto coro = respond(&Bot::bot(), std::get<0>(_source.source).event, std::move(title), std::move(choices), create_thread, limit_1, role_id);
	return (CommandResponse{CommandResponse::None{}});
}