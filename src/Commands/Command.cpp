#include "B12.h"

#include "Command.h"
#include "Core/Bot.h"

#include "Guild/Guild.h"

#include <regex>

#include "CommandHandler.h"

using namespace B12;

// proof of concept for webhook messages
/* Guild *guild = Bot::fetchGuild(interaction.guild_id);

  dpp::message test{"test"};
  dpp::user author   = interaction.get_issuing_user();
  std::string name =
	test.member.nickname.empty() ? author.username : test.member.nickname;
  std::string avatar = test.member.get_avatar_url();

  if (avatar.empty())
	avatar = author.get_avatar_url();
  test.member = interaction.member;
  test.author = Bot::bot().me;
  test.set_channel_id(interaction.channel_id);
  test.set_guild_id(interaction.guild_id);
  Bot::bot().get_webhook(
	1082795079166599228,
	[=](const dpp::confirmation_callback_t &confirm) {
	  auto hook = std::get<dpp::webhook>(confirm.value);

	  hook.name   = std::format("{} (@{})", name, guild->dppGuild().name);
	  hook.avatar = avatar;
	  Bot::bot().execute_webhook(hook, test);
	}
  );*/

struct promise;

struct test_coroutine : std::coroutine_handle<promise>
{
	using promise_type = ::promise;
};

struct promise
{
	test_coroutine get_return_object() { return {test_coroutine::from_promise(*this)}; }
	
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
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
	/*auto test = [&]() -> dpp::task {
		auto channel_id = std::get<0>(this->_source.source).event.command.channel_id;
		auto guild_id = std::get<0>(this->_source.source).event.command.guild_id;
		dpp::confirmation_callback_t confirm;
		std::optional<dpp::thread> threads[3];

		for (int i = 0; i < 3; ++i)
		{
			confirm = co_await Bot::bot().co_thread_create(fmt::format("test {}", i), channel_id, 60, dpp::CHANNEL_PUBLIC_THREAD, false, 60);
			if (confirm.is_error())
			{
				co_await Bot::bot().co_message_create(dpp::message{"failed to create thread"}.set_channel_id(channel_id));
			}
		}

		confirm = co_await Bot::bot().co_threads_get_active(guild_id);

		if (confirm.is_error())
		{
			co_await Bot::bot().co_message_create(dpp::message{"failed to get threads"}.set_channel_id(channel_id));
		}
		else
		{
			const auto &active_threads = confirm.get<dpp::active_threads>();

			for (const auto &threads : active_threads)
			{
				const auto &active_thread = threads.second;

				if (active_thread.active_thread.parent_id == channel_id)
				{
					confirm = co_await Bot::bot().co_message_create(dpp::message{fmt::format("found thread : {}", active_thread.active_thread.name)}.set_channel_id(channel_id));
					if (active_thread.bot_member || !(co_await Bot::bot().co_current_user_join_thread(active_thread.active_thread.id)).is_error())
					{
						co_await Bot::bot().co_message_create(dpp::message("hello").set_channel_id(active_thread.active_thread.id));
					}
					confirm = co_await Bot::bot().co_channel_delete(active_thread.active_thread.id);
				}
			}
		}
	};

	auto task = test();*/
	
	return {CommandResponse::Success{}, response};
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
	auto respond = [&]() -> dpp::task
	{
		auto event = std::get<0>(_source.source).event;

		using choice = std::pair<std::optional<std::string>, std::string>;

		std::vector<std::optional<choice>> choices;
		std::optional<dpp::role> role_ping;
		bool create_thread = false;
		bool limit_1 = false;
		std::string title;
		auto cluster = event.from->creator;
		auto event_ = event;
		dpp::confirmation_callback_t confirm;

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
							if (auto separator = str_value.find(':'); separator != std::string::npos)
							{
								if (separator != 0 && str_value[separator-1] == '\\')
									str_value.erase(separator-1, 1);
								else
									choices[choice_id] = choice{str_value.substr(0, separator), str_value.substr(separator+1)};
							}
							else
							{
								choices[choice_id] = choice{std::nullopt, str_value};
							}
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
						dpp::snowflake role_id = std::get<dpp::snowflake>(opt.value);

						if (auto *cached_role = dpp::find_role(role_id); cached_role != nullptr)
							role_ping = *cached_role;
						else
						{
							confirm = co_await cluster->co_roles_get(event.command.guild_id);
							const auto &role_map = confirm.get<dpp::role_map>();
							if (auto it = role_map.find(role_id); it != role_map.end())
								role_ping = it->second;
						}
						if (!role_ping.has_value() || (role_ping->is_mentionable() && !(event.command.get_resolved_permission(event.command.get_issuing_user().id) & (dpp::p_administrator | dpp::p_mention_everyone))))
						{
							event.reply(dpp::message{"You do not have permissions to mention this role."}.set_flags(dpp::m_ephemeral));
							co_return;
						}
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
		confirm = co_await cluster->co_interaction_response_create(event_.command.id, event_.command.token, dpp::interaction_response{dpp::ir_deferred_channel_message_with_source});
		if (confirm.is_error())
			co_return;
		confirm = co_await cluster->co_interaction_followup_get_original(event_.command.token);
		if (confirm.is_error())
			co_return;
		auto message = confirm.get<dpp::message>();
		int num = 0;
		std::vector<std::string> lines;
		for (auto i = 0; i < choices.size(); ++i)
		{
			if (const auto &c = choices[i]; c.has_value())
			{
				std::string emoji;
				bool reacted = false;
				if (c->first.has_value())
				{
					emoji = *c->first;
					confirm = co_await cluster->co_message_add_reaction(message.id, message.channel_id, emoji);
					if (!confirm.is_error())
						reacted = true;
				}
				if (!reacted)
				{
					emoji = numbers[num];
					confirm = co_await cluster->co_message_add_reaction(message.id, message.channel_id, emoji);
				}
				if (confirm.is_error())
				{
					co_await cluster->co_interaction_response_edit(event.command.token, message.set_content("Failed to add a reaction."));
					co_return;
				}
				auto skipped = c->second | std::ranges::views::drop_while([](char c){ return (c == ' ' || c == '\t'); });
				lines.push_back(fmt::format("{} {}", emoji, std::string_view{skipped.begin(), skipped.end()}));
				++num;
			}
		}
		const auto &author = event_.command.get_issuing_user();
		message.add_embed(
			dpp::embed{}
			.set_footer(fmt::format("{}#{}", author.username, author.discriminator), "")
			.set_description(fmt::format("{}", fmt::join(lines, "\n")))
		);
		message.content = fmt::format("\xF0\x9F\x93\x8A {}", title);
		cluster->interaction_response_edit(event_.command.token, message, [cluster, channel_id = event_.command.channel_id](const dpp::confirmation_callback_t &confirmation)
		{
			if (confirmation.is_error())
				cluster->message_create(dpp::message{"Fatal error : could not edit message"}.set_channel_id(channel_id));
		});
		if (create_thread)
		{
			cluster->thread_create_with_message(title, message.channel_id, message.id, 60, 0,
				[cluster, role_ping, channel_id = event_.command.channel_id, author = event_.command.usr, author_member = event_.command.member](const dpp::confirmation_callback_t &confirmation)
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
	auto coro = respond();
	return (CommandResponse{CommandResponse::None{}});
}