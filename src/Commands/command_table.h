#pragma once

#include "commands.h"
#include "command_handler.h"

namespace B12 {

namespace command {

	inline constexpr command_table COMMAND_TABLE = {
		command_info{"meow", "Meow to me!", &meow},
		command_info{"study", "Toggle study mode", &study},
		command_group{"server", "Server settings",
			command_group{"settings", "Server settings",
				command_info{"study", "Set the server's study role and channel", &server_settings_study,
					{{"role", "New study role"}, {"channel", "New study channel"}}
				}
			},
			command_group{"sticker", "Server stickers",
				command_info{"grab", "Grab a sticker from a message", &server_sticker_grab,
					{{"message", "Message to grab from"}, {"channel", "Channel to grab from (defaults to current)"}}
				}
			}
		},
		command_info{"bigmoji", "Make an emoji big", &bigmoji, {{"emoji", "Emoji to show"}}},
		command_info{"ban", "Ban a user", &ban,
			{{"user", "User to ban"}, {"time", "Duration of the ban"}, {"reason", "Reason for the ban"}}
		},
		//command_info{"pokemon_dex", &command::pokemon_dex, {"name-or-number"}},
		command_info{"poll", "Create a poll", &poll, {
				{"title", "Title of the poll"},
				{"option1", "Option 1 for the poll"},
				{"option2", "Option 2 for the poll"},
				{"option3", "Option 3 for the poll"},
				{"option4", "Option 4 for the poll"},
				{"option5", "Option 5 for the poll"},
				{"option6", "Option 6 for the poll"},
				{"option7", "Option 7 for the poll"},
				{"option8", "Option 8 for the poll"},
				{"create-thread", "Whether to create a thread for the poll or not"},
				{"ping-role", "Optional role to ping"}
			}
		}
	};

	std::vector<dpp::slashcommand> get_api_commands(dpp::snowflake app_id);
}

}
