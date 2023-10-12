#include "Core/Bot.h"
#include "B12.h"
#include "Data/Lang.h"
#include "commands.h"

#include <dpp/unicode_emoji.h>
#include <source_location>

#include "proto_dpp.h"

namespace {
	constexpr std::array numbers = std::to_array<const char*>({
		dpp::unicode_emoji::one,
		dpp::unicode_emoji::two,
		dpp::unicode_emoji::three,
		dpp::unicode_emoji::four,
		dpp::unicode_emoji::five,
		dpp::unicode_emoji::six,
		dpp::unicode_emoji::seven,
		dpp::unicode_emoji::eight
	});
}

namespace {

constexpr auto is_alpha = [](char c) constexpr noexcept {
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
};

constexpr auto is_whitespace = [](char c) constexpr noexcept {
	switch (c) {
		case ' ':
		case '\t':
			return (true);
		default:
			return (false);
	}
};

struct poll_choice {
	struct emoji_t {
		enum emoji_type {
			none,
			utf8,
			custom
		};

		std::string name = {};
		emoji_type type = emoji_type::none;

		explicit operator bool() const noexcept {
			return (type != emoji_type::none);
		}
	};

	// index 1 is utf8, index2 is custom emoji
	std::string_view original;
	std::string name;
	emoji_t emoji = {};
	dpp::task<bool> react_task = {};
};

auto parse_choice(std::string_view text) -> poll_choice {
	text = {std::ranges::find_if_not(text, is_whitespace), text.end()};
	if (text.starts_with('\\'))
		return {.original = text, .name = std::string{text.begin() + 1, text.end()}};
	else if (text.starts_with('<')) {
		if (auto separator = std::ranges::find(text.begin() + 1, text.end(), '>'); separator != text.end()) {
			return {
				.original = text,
				.name = std::string{separator + 1, text.end()},
				.emoji = {std::string{text.begin() + 1, separator}, poll_choice::emoji_t::custom},
			};
		}
	} else if (auto first_alpha = std::ranges::find_if(text, [](char c) constexpr { return (is_alpha(c) || is_whitespace(c)); }); first_alpha != text.end() && first_alpha != text.begin()) {
		return {
			.original = text,
			.name = std::string{first_alpha, text.end()},
			.emoji = {std::string{text.begin(), first_alpha}, poll_choice::emoji_t::utf8}
		};
	}
	return {.original = text, .name = std::string{text.begin(), text.end()}};
}

static_assert(std::is_move_constructible_v<poll_choice>);

void try_parse_choice(std::vector<poll_choice> &v, B12::command::optional_param<std::string_view> text) {
	if (!text.has_value())
		return;
	v.emplace_back(parse_choice(*text));
}

constexpr bool is_valid_poll_param(std::string_view str) noexcept {
	return (str.size() <= 60);
}

constexpr bool is_valid_poll_param(B12::command::optional_param<std::string_view> str) noexcept {
	return (str.has_value() ? is_valid_poll_param(*str) : true);
};

}

namespace B12::command {

dpp::coroutine<response> poll(
	dpp::interaction_create_t const &event,
	std::string_view title,
	std::string_view option1,
	std::string_view option2,
	optional_param<std::string_view> option3,
	optional_param<std::string_view> option4,
	optional_param<std::string_view> option5,
	optional_param<std::string_view> option6,
	optional_param<std::string_view> option7,
	optional_param<std::string_view> option8,
	optional_param<bool> create_thread,
	optional_param<dpp::role> ping_role
) {
	dpp::cluster &cluster = *event.from->creator;
	dpp::permission user_perms = event.command.get_resolved_permission(event.command.get_issuing_user().id);

	if (title.size() > 80) {
		co_return command::response::reply(
			command::response::usage_error("Poll title is too long. (max. 80 characters)")
				.set_flags(dpp::m_ephemeral)
		);
	}
	if(!is_valid_poll_param(option1)
			|| !is_valid_poll_param(option2)
			|| !is_valid_poll_param(option3)
			|| !is_valid_poll_param(option4)
			|| !is_valid_poll_param(option5)
			|| !is_valid_poll_param(option6)
			|| !is_valid_poll_param(option7)
			|| !is_valid_poll_param(option8)) {
		co_return command::response::reply(
			command::response::usage_error("An option is too long. (max. 60 characters)")
				.set_flags(dpp::m_ephemeral)
		);
	}
	if (ping_role.has_value() && !(ping_role->is_mentionable() || user_perms.can(dpp::p_mention_everyone))) {
		B12::log(LogLevel::INFO, "{} ({}) was denied mentioning role {} ({}) in guild {} through /poll",
			event.command.usr.id, event.command.usr.format_username(),
			ping_role->id, ping_role->name,
			event.command.guild_id);
		co_return command::response::reply(
			command::response::usage_error(fmt::format("You do not have permissions to mention {}", ping_role->get_mention()))
				.set_allowed_mentions(false, false, false, false, {}, {})
				.set_flags(dpp::m_ephemeral)
		);
	}
	std::vector<poll_choice> choices = {};
	auto thinking = event.co_thinking();
	choices.emplace_back(parse_choice(option1));
	choices.emplace_back(parse_choice(option2));
	try_parse_choice(choices, option3);
	try_parse_choice(choices, option4);
	try_parse_choice(choices, option5);
	try_parse_choice(choices, option6);
	try_parse_choice(choices, option7);
	try_parse_choice(choices, option8);
	co_await or_throw(thinking);
	try {
		dpp::message message = co_await or_throw<dpp::message>(event.co_get_original_response());
		std::vector<dpp::task<void>> react_tasks;

		message.content = fmt::format("## {} {}", dpp::unicode_emoji::bar_chart, title);
		for (size_t i = 0; i < choices.size(); ++i) {
			choices[i].react_task = ([](dpp::cluster &bot, const dpp::message &m, poll_choice &choice, size_t react_counter) -> dpp::task<bool> {
				if (choice.emoji) {
					auto result = co_await bot.co_message_add_reaction(m, choice.emoji.name);
					if (!result.is_error())
						co_return (true);
				}
				choice.emoji.name = numbers[react_counter];
				choice.emoji.type = poll_choice::emoji_t::utf8;
				auto result = co_await bot.co_message_add_reaction(m, choice.emoji.name);
				co_return (!result.is_error());
			}(cluster, message, choices[i], i));
		}
		std::vector<std::string> lines;
		for (poll_choice &c : choices) {
			bool success = co_await c.react_task;
			if (success) {
				if (c.emoji.type == poll_choice::emoji_t::custom)
					c.emoji.name = fmt::format("<{}>", c.emoji.name);
				lines.emplace_back(fmt::format("{} {}", c.emoji.name, c.name));
			} else {
				lines.emplace_back(c.original);
			}
		}
		const dpp::user &author = event.command.get_issuing_user();
		const dpp::guild_member &author_member = event.command.member;

		message.add_embed(
			dpp::embed{}
			.set_footer(fmt::format("{} ({})", dpp::utility::get_member_display_name(author, author_member), author.username), dpp::utility::get_member_avatar_url(author, author_member))
			.set_description(fmt::format("{}", fmt::join(lines, "\n")))
		);
		co_await or_throw(event.co_edit_original_response(message));
		if (create_thread.value_or(false)) {
			dpp::thread thread = co_await or_throw<dpp::thread>(cluster.co_thread_create_with_message(fmt::format("{} {}", dpp::unicode_emoji::bar_chart, title), message.channel_id, message.id, 60, 0));
			if (ping_role.has_value()) {
				message = dpp::message{fmt::format("New poll started by {}! {}", author.get_mention(), ping_role->get_mention())};

				message.allowed_mentions.roles.push_back(ping_role->id);
				message.allowed_mentions.users.push_back(author.id);
				message.mention_roles.push_back(ping_role->id);
				message.mentions.emplace_back(author, author_member);
				message.set_channel_id(thread.id);
				co_await or_throw(cluster.co_message_create(message));
			}
		}
		co_return command::response::none();
	} catch (const dpp::exception &e) {
		if (B12::isLogEnabled(LogLevel::ERROR))
			B12::log(LogLevel::ERROR, format_command_error(event, e.what()));
		co_return command::response::edit(command::response::internal_error(e.what()));
	}
}

}
