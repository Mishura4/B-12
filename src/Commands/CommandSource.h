#ifndef B12_COMMAND_SOURCE_H_
#define B12_COMMAND_SOURCE_H_

#include "B12.h"

namespace B12
{
	struct CommandSource
	{
		
		struct Interaction
		{
			enum Type
			{
				UNKNOWN = 0,
				SLASH_COMMAND,
				BUTTON_CLICK
			};
			
			const dpp::interaction_create_t &event;
			Type type{UNKNOWN};
		};
		
		CommandSource(const dpp::slashcommand_t &event);
		CommandSource(const dpp::button_click_t &event);

		using PossibleTypes = shion::type_list<Interaction>;

		PossibleTypes::as_variant source;
		
		bool has_replied{false};
		
		AsyncExecutor<dpp::confirmation> thinking_executor{[this](const dpp::confirmation &)
		{
			has_replied = true;
		}};

		void sendThink(bool ephemeral = true);
		void reply(const CommandResponse &response);
		dpp::snowflake id() const;

		template <typename T>
		void reply(const CommandResponse &response);

		bool isInteraction() const;
	};
}

#endif