#include "Core/Bot.h"
#include "B12.h"
#include "Data/Lang.h"
#include "commands.h"

#include <ranges>

using namespace B12;

namespace {
	command::response do_ban_check(const command::resolved_user &user, const dpp::confirmation_callback_t &get_ban_result) {
		if (!get_ban_result.is_error()) {
			const dpp::ban &ban = get_ban_result.get<dpp::ban>();

			return (command::response::edit(
				fmt::format("User <@{}> is currently banned. ({})",
				user.user.id,
				ban.reason.empty() ? "no reason specified"sv : ban.reason)
			));
		}
		return (command::response::edit(fmt::format("User <@{}> is not currently banned.", user.user.id)));
	}

	dpp::message make_ban_confirmation(const dpp::user &user, std::string_view reason, const std::string &interaction_id) {
		return (dpp::message{fmt::format("About to ban <@{}>, with reason \"{}\". Confirm?", user.id, reason )}
			.add_component(dpp::component{}
				.add_component(dpp::component{}.set_type(dpp::cot_button).set_label("Yes").set_style(dpp::cos_success).set_id(interaction_id + "_confirm"))
				.add_component(dpp::component{}.set_type(dpp::cot_button).set_label("No").set_style(dpp::cos_danger).set_id(interaction_id + "_abort"))
			));
	}
}

auto command::ban(
	dpp::interaction_create_t const &event,
	resolved_user user,
	optional_param<std::chrono::seconds> duration,
	optional_param<std::string_view> reason
) -> dpp::coroutine<command::response> {
	auto thinking = event.co_thinking(false);

	dpp::confirmation_callback_t result = co_await event.from->creator->co_guild_get_ban(event.command.guild_id, user.user.id);
	co_await thinking;
	if (!reason.has_value())
		co_return do_ban_check(user, result);

	if (!result.is_error()) {
		const dpp::ban &ban = result.get<dpp::ban>();

		co_return (command::response::edit(
			fmt::format("User <@{}> is already banned. ({})",
			user.user.id,
			ban.reason.empty() ? "no reason specified"sv : ban.reason
		)));
	}

	std::string interaction_id = event.command.id.str();
	result = co_await event.co_edit_original_response(make_ban_confirmation(user.user, *reason, interaction_id));
	if (result.is_error()) {
		std::string log_message = fmt::format(
			"Could not edit message for ban command {} from user {}: {}",
			interaction_id, user.user.id, result.get_error().human_readable
		);
		event.from->creator->log(dpp::ll_error, log_message);

		co_return command::response::edit(command::response::internal_error());
	}

	bool finished = false;
	while (!finished) {
		auto response = co_await dpp::when_any{
			event.from->creator->on_button_click.when([&interaction_id](const dpp::button_click_t &b) {
				return b.custom_id.starts_with(interaction_id);
			}),
			event.from->creator->co_sleep(15)
		};
		if (response.index() == 0) {
			const dpp::button_click_t &button_clicked = response.get<0>();

			if (button_clicked.custom_id.ends_with("_confirm")) {
				if (!button_clicked.command.get_resolved_permission(button_clicked.command.usr.id).can(dpp::p_ban_members)) {
					button_clicked.reply(dpp::message{"You do not have the permissions to do this!"}.set_flags(dpp::m_ephemeral));
					continue;
				}
				thinking = button_clicked.co_reply(dpp::ir_deferred_update_message, dpp::message{}.set_flags(dpp::m_loading));
				event.from->creator->set_audit_reason(std::string{*reason});
				result = co_await event.from->creator->co_guild_ban_add(event.command.guild_id, user.user.id);
				co_await thinking;
				if (result.is_error()) {
					std::string error_message = fmt::format("Could not ban user {}: {}", user.user.get_mention(), result.get_error().human_readable);

					co_return command::response::edit(
						command::response::internal_error(error_message).set_allowed_mentions(false, false, false, false, {}, {})
					);
				}
				co_return command::response::edit(
					command::response::success(fmt::format("Successfully banned user {} for reason {}", user.user.get_mention(), *reason))
				);
			}
		}
	}
	co_return (command::response::edit(command::response::aborted()));
}
