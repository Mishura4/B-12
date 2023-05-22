#include "CommandSource.h"
#include "CommandResponse.h"

using namespace B12;

CommandSource::CommandSource(const dpp::slashcommand_t& event) :
	source{std::in_place_type_t<Interaction>{}, event, Interaction::SLASH_COMMAND}
{
	
}

CommandSource::CommandSource(const dpp::button_click_t& event) :
	source{std::in_place_type_t<Interaction>{}, event, Interaction::BUTTON_CLICK}
{
	
}

void CommandSource::sendThink(bool ephemeral)
{
	auto                     think = shion::utils::visitor{
		[this, ephemeral](Interaction& source)
		{
			thinking_executor(&dpp::interaction_create_t::thinking, source.event, ephemeral);
		}
	};

	std::visit(think, source);
}

template <>
void CommandSource::reply<CommandSource::Interaction>(const CommandResponse& response)
{
	auto &interaction = std::get<Interaction>(source);
	auto &event = interaction.event;
	
	thinking_executor.wait();
	auto handler = [&]<typename T>(T&& value) -> bool
	{
		using type = std::remove_cvref_t<T>;
		
		if constexpr (std::is_same_v<type, CommandResponse::SuccessEdit>)
		{
			if (has_replied)
				event.delete_original_response();
			event.from->creator->interaction_response_create(
				event.command.id,
				event.command.token,
				{
					dpp::ir_update_message,
					response.content.message
				},
				response.callback
			);
			return (true);
		}
		else if constexpr (std::is_same_v<type, CommandResponse::Thinking>)
		{
			if (has_replied)
				return (true);
			event.thinking(value.ephemeral, response.callback);
			return (false);
		}
		else
			return (false);
	};
	if (std::visit(handler, response.type))
		return;
	if (has_replied)
		interaction.event.edit_original_response(response.content.message, response.callback);
	else
		interaction.event.reply(response.content.message, response.callback);
}

#ifdef _MSC_VER
# define B12_UNREACHABLE __assume(false)
#else
# define B12_UNREACHABLE __builtin_unreachable()
#endif

void CommandSource::reply(const CommandResponse& response)
{
	switch (source.index())
	{
		case PossibleTypes::type_index<Interaction>:
			reply<Interaction>(response);
			break;

		default:
			static_assert(!(PossibleTypes::size > 1));
			B12_UNREACHABLE;
	}
}

dpp::snowflake CommandSource::id() const
{
	constexpr auto          get_interaction_id = shion::utils::visitor{
		[](const Interaction& source) -> dpp::snowflake
		{
			return (source.event.command.id);
		}
	};

	return (std::visit(get_interaction_id, source));
}


bool CommandSource::isInteraction() const
{
	return (std::holds_alternative<Interaction>(source));
}