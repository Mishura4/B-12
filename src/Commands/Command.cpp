#include "B12.h"

#include "Command.h"
#include "Core/Bot.h"

#include "Guild/Guild.h"

#include <regex>

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

template <>
void Bot::command<"meow">(
	const dpp::interaction_create_t& e,
	const dpp::interaction&          /*interaction*/,
	command_option_view              /*options*/
)
{
	constexpr char INTRO_URL[] =
		"https://cdn.discordapp.com/attachments/1066393377236594699/1066779084845220020/b-12.mp4";
	dpp::message ret{INTRO_URL, dpp::message_type::mt_reply};

	e.reply(ret);
}

template <>
void Bot::command<"bigmoji">(
	const dpp::interaction_create_t& e,
	const dpp::interaction&          /*interaction*/,
	command_option_view              options
)
{
	using namespace std::string_view_literals;

	if (options.empty() || !std::holds_alternative<std::string>(options[0].value))
	{
		e.reply(dpp::message("Error: wrong parameter type").set_flags(dpp::m_ephemeral));
		return;
	}
	const std::string& input = std::get<std::string>(options[0].value);
	std::regex pattern{"^(?:\\s*)<(a?):([a-zA-Z0-9_]+):([0-9]+)>(?:\\s*)$"};
	std::string match;
	std::match_results<std::string::const_iterator> results{};
	if (!std::regex_match(input, results, pattern) || results.size() < 4)
	{
		e.reply(
			dpp::message("Please give a custom emoji as the parameter.").set_flags(dpp::m_ephemeral)
		);
		return;
	}
	auto id  = results[3];
	auto URL = fmt::format(
		"https://cdn.discordapp.com/emojis/{}{}?size=256&quality=lossless",
		std::string_view{id.first, id.second},
		(results[1].length() ? ".gif"sv : ".webp"sv)
	);
	e.reply(URL);
}
