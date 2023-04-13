#include "CommandHandler.h"
#include "Data/Lang.h"

#include <shion/utils/visitor.h>

using namespace B12;

namespace
{
	constexpr auto success_prefix = shion::literal_concat<
		shion::string_literal<char, lang::SUCCESS_EMOJI.size()>{lang::SUCCESS_EMOJI.data()}, " "
	>();
	constexpr auto confirm_prefix = shion::literal_concat<"\u26A0\uFE0F", " ">();
	constexpr auto error_prefix   = shion::literal_concat<
		shion::string_literal<char, lang::ERROR_EMOJI.size()>{lang::ERROR_EMOJI.data()}, " "
	>();

	std::string format_response(const CommandResponse& r)
	{
		constexpr auto get_type_prefix = [](uint64 index) constexpr -> std::string_view
		{
			using Types = CommandResponse::PossibleTypes;

			switch (index)
			{
				case Types::type_index<CommandResponse::Success>:
				case Types::type_index<CommandResponse::SuccessEdit>:
					break;

				case Types::type_index<CommandResponse::SuccessAction>:
					return (success_prefix);

				case Types::type_index<CommandResponse::Confirm>:
					return (confirm_prefix);

				case Types::type_index<CommandResponse::ConfigError>:
				case Types::type_index<CommandResponse::APIError>:
				case Types::type_index<CommandResponse::InternalError>:
				case Types::type_index<CommandResponse::UsageError>:
					return (error_prefix);
			}
			return {};
		};
		return (fmt::format(
			"{}{}{}{}",
			get_type_prefix(r.type.index()),
			r.content.message.content,
			r.content.warnings.empty() ? "\n"sv : ""sv,
			fmt::join(r.content.warnings, "\n"sv)
		));
	};
}

CommandHandler::CommandHandler(const dpp::slashcommand_t& e) :
	_cluster{e.from->creator},
	_issuer{&e.command.get_issuing_user()},
	_guild_id{e.command.guild_id},
	_member_issuer{&e.command.member},
	_channel{&e.command.channel},
	_source{
		std::in_place_type_t<InteractionSource>{},
		&e,
		InteractionSource::SLASH_COMMAND
	} {}

CommandHandler::CommandHandler(const dpp::button_click_t& e) :
	_cluster{e.from->creator},
	_issuer{&e.command.get_issuing_user()},
	_guild_id{e.command.guild_id},
	_member_issuer{&e.command.member},
	_channel{&e.command.channel},
	_source{
		std::in_place_type_t<InteractionSource>{},
		&e,
		InteractionSource::BUTTON_CLICK
	} {}

void CommandHandler::sendThink(bool ephemeral)
{
	auto                     think = shion::utils::visitor{
		[=](InteractionSource& source)
		{
			if (source.event)
				source.thinking_executor(&dpp::interaction_create_t::thinking, source.event, ephemeral);
		}
	};

	std::visit(think, _source);
}

void CommandHandler::_process(CommandResponse& response)
{
	constexpr auto          get_interaction_id = shion::utils::visitor{
		[](InteractionSource& source) constexpr -> uint64
		{
			return (source.event->command.id);
		}
	};

	auto                            post_process = shion::utils::visitor{
		[&](CommandResponse::Confirm& confirm) -> void
		{
			if (!_guild_id)
			{
				response = {
					CommandResponse::InternalError{},
					{{"Error: unsupported dialog outside of a guild"}}
				};
			}
			else
			{
				Guild* guild = fetchGuild(_guild_id);
				auto   root  = dpp::component{};

				root.add_component(
					guild->createButtonAction(
						std::visit(get_interaction_id, _source),
						confirm.action,
						dpp::p_administrator
					).set_label("Yes").set_style(dpp::cos_success)
				);
				root.add_component(
					dpp::component{}.set_id(std::string{button_abort}).set_label("No").set_style(
						dpp::cos_danger
					)
				);
				response.content.message.add_component(root);
			}
		},
		shion::noop
	};

	auto                     sender = shion::utils::visitor{
		[&](InteractionSource& source)
		{
			source.thinking_executor.wait();
			if (response.has<CommandResponse::SuccessEdit>())
			{
				auto event = source.event;

				if (source.has_replied)
					event->delete_original_response();
				event->from->creator->interaction_response_create(
					event->command.id,
					event->command.token,
					{
						dpp::ir_update_message,
						response.content.message
					},
					response.callback
				);
				return;
			}
			response.content.message.content = format_response(response);
			if (source.has_replied)
				source.event->edit_original_response(response.content.message, response.callback);
			else
				source.event->reply(response.content.message, response.callback);
		}
	};

	std::visit(post_process, response.type);
	for (const auto& m : response.content.other_messages)
		_cluster->message_create(m);
	std::visit(sender, _source);
}

bool CommandHandler::isInteraction() const
{
	return (std::holds_alternative<InteractionSource>(_source));
}

auto CommandHandler::_getInteraction() -> InteractionSource &
{
	return (std::get<InteractionSource>(_source));
}
