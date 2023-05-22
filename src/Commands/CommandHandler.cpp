#include "CommandHandler.h"
#include "Data/Lang.h"

#include <shion/utils/visitor.h>

using namespace B12;

CommandHandler::CommandHandler(const dpp::slashcommand_t& e) :
	_cluster{e.from->creator},
	_issuer{&e.command.get_issuing_user()},
	_guild_id{e.command.guild_id},
	_member_issuer{&e.command.member},
	_channel{&e.command.channel},
	_source{e} {}

CommandHandler::CommandHandler(const dpp::button_click_t& e) :
	_cluster{e.from->creator},
	_issuer{&e.command.get_issuing_user()},
	_guild_id{e.command.guild_id},
	_member_issuer{&e.command.member},
	_channel{&e.command.channel},
	_source{e} {}

void CommandHandler::_process(CommandResponse& response)
{
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
						_source.id(),
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

	std::visit(post_process, response.type);
	for (const auto& m : response.content.other_messages)
		_cluster->message_create(m);
	if (std::holds_alternative<CommandResponse::None>(response.type))
		return;
	response.format();
	_source.reply(response);
}

auto CommandHandler::_getInteraction() -> CommandSource::Interaction &
{
	return (std::get<CommandSource::Interaction>(_source.source));
}
