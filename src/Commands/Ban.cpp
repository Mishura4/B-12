#include "Core/Bot.h"

#include "B12.h"
#include "Command.h"
#include "CommandHandler.h"

#include "../Data/Lang.h"

#include <ranges>

using namespace B12;

namespace
{
	struct BanConfirmButtonAction
	{
		dpp::snowflake user_id;
		dpp::snowflake issuer_id;
		std::string issuer_name;
		std::string reason;

		CommandResponse operator()(
			const dpp::button_click_t &event
		)
		{
			namespace chr = std::chrono;
			std::string                      error;
			AsyncExecutor<dpp::confirmation> ban_executor{
				shion::noop,
				[&](const dpp::error_info& err)
				{
					error = err.message;
				}
			};
			using duration = chr::duration<double, std::ratio<1>>;
			auto now = chr::time_point<chr::local_t, duration>(duration{event.command.get_creation_time()});
			auto today = chr::year_month_day{chr::floor<chr::days>(now)};
			event.from->creator->set_audit_reason(
				fmt::format(
					"Requested by {} on {}/{}/{}: {}",
					issuer_name,
					static_cast<unsigned>(today.month()),
					static_cast<unsigned>(today.day()),
					static_cast<int>(today.year()),
					reason)
			);
			ban_executor(&dpp::cluster::guild_ban_add, event.from->creator, event.command.guild_id, user_id, 0);
			ban_executor.wait();
			if (!error.empty())
				return CommandResponse{CommandResponse::APIError{}, {fmt::format("Error: ", error)}};
			
			dpp::message edit{event.command.msg};
			auto components = std::vector<dpp::component>{};

			std::swap(components, edit.components);
			edit.content = fmt::format(
				"<@{}> was banned by <@{}>, citing \"{}\".",
				user_id,
				issuer_id,
				reason
			);
			return {
				CommandResponse::SuccessEdit{},
				{edit},
				Guild::makeForgetButtonsCallback(event.command.guild_id, components)
			};
		}
	};
}

template <>
CommandResponse CommandHandler::command<"ban">(
	command_option_view options
)
{
	dpp::snowflake user_id;
	std::string_view reason;
	std::string_view time_str;
	dpp::interaction_response r;

	// TODO: do this through TMP with the command table
	for (const dpp::command_data_option &opt : options)
	{
		bool handled = false;
		switch (opt.type)
		{
			case dpp::co_user:
				user_id = std::get<dpp::snowflake>(opt.value);
				handled = true;
				break;

			case dpp::co_string:
			{
				if (opt.name == "time")
				{
					time_str = std::get<std::string>(opt.value);
					handled = true;
				}
				else if (opt.name == "reason")
				{
					reason = std::get<std::string>(opt.value);
					handled = true;
				}
				break;
			}

			default:
				break;
		}
		if (!handled)
			return {CommandResponse::InternalError{}, {fmt::format("Unknown or invalid option {} of type {}"sv, opt.name, magic_enum::enum_name(opt.type))}};
	}
	
	sendThink(false);

	std::optional<dpp::ban> ban{std::nullopt};
	AsyncExecutor<dpp::ban> ban_retriever{[&](const dpp::ban &b)
	{
		ban = b;
	}, shion::noop};
	
	ban_retriever(&dpp::cluster::guild_get_ban, _cluster, _guild_id, user_id);
	ban_retriever.wait();
	if (reason.empty())
	{
		if (ban.has_value())
		{
			return {
				CommandResponse::Success{},
				{fmt::format("User <@{}> is currently banned. ({})",
				user_id,
				ban->reason.empty() ? "no reason specified"sv : ban->reason)}
			};
		}
		return {CommandResponse::Success{}, {fmt::format("User <@{}> is not currently banned.", user_id)}};
	}
	if (ban.has_value())
		return {CommandResponse::Success{}, {fmt::format("User <@{}> is already banned. ({})", user_id, ban->reason.empty() ? "no reason specified"sv : ban->reason)}};

	Guild *guild = Bot::fetchGuild(_guild_id);

	if (!guild)
		return {CommandResponse::InternalError{}, {{"Unknown server"}}};
	
	dpp::message reply{
		fmt::format(
			"About to ban <@{}>, with reason \"{}\". Confirm?",
			user_id,
			reason
		)
	};
	auto confirm = BanConfirmButtonAction{
		user_id,
		_issuer->id,
		_issuer->format_username(),
		std::string{reason}
	};
	/*reply.add_component(
		dpp::component{}
		.add_component(
			guild->createButtonAction(confirm, dpp::p_ban_members, std::chrono::system_clock::now() + 30s).set_emoji("\u2611")
		)
	);*/
	return {CommandResponse::Confirm{confirm, 10s}, {reply}};
}
