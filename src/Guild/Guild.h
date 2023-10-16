#ifndef B12_GUILD_H_
#define B12_GUILD_H_

#include "B12.h"

#include "Data/DataStore.h"
#include "Data/DataStores.h"
#include "Data/DataStructures.h"

#include <ranges>

#include "../Core/Bot.h"

namespace B12
{
	class Guild
	{
	public:
		explicit Guild(dpp::snowflake _id);

		void studyRole(const dpp::role* role);
		void studyChannel(const dpp::channel* channel);
		void studyMessage(dpp::snowflake id);

		const DataStores::GuildSettings::Entry &settings() const;
		const std::optional<dpp::snowflake> &        studyRole() const;
		const std::optional<dpp::snowflake> &     studyChannel() const;
		const dpp::guild_member &               b12Member() const;
		const dpp::guild &                      dppGuild() const;

		void loadGuildSettings();
		bool saveSettings();

		dpp::permission getPermissions(
			const dpp::guild_member& user,
			const dpp::channel&      channel
		) const;


	private:
		struct ButtonAction
		{
			std::function<button_callback> fun;
			dpp::permission                permissions;
		};

		struct MessageButtons
		{
			timestamp                 expire_time;
			std::vector<ButtonAction> buttons;
		};

		bool _handleActionButton(const dpp::button_click_t& event);

		dpp::guild                                       _guild;
		dpp::guild_member                                _me;
		std::optional<dpp::snowflake>                         _studyRole{};
		std::optional<dpp::snowflake>                      _studyChannel{};
		std::optional<dpp::snowflake>                      _studyMessage{};
		DataStores::GuildSettings::Entry                 _settings;
		std::mutex                                       _mutex;
	};
} // namespace B12

#endif
