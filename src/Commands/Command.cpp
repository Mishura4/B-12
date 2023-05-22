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
