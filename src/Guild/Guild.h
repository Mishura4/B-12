#ifndef B12_GUILD_H_
#define B12_GUILD_H_

#include "B12.h"

#include "Data/DataStore.h"
#include "Data/DataStores.h"
#include "Data/DataStructures.h"

namespace B12
{
	class Guild
	{
	public:
		explicit Guild(dpp::snowflake _id);

		void loadGuildSettings();
		void studyRole(const dpp::role* role);
		void studyChannel(const dpp::channel* channel);
		void studyMessage(dpp::snowflake id);

		const DataStores::GuildSettings::Entry &settings() const;

		bool saveSettings();

		const auto &studyRole() const
		{
			return (_studyRole);
		}

		const auto &studyChannel() const
		{
			return (_studyChannel);
		}

		const auto &b12User() const
		{
			return (_me);
		}

		const auto &dppGuild() const
		{
			return (_guild);
		}

		dpp::permission getPermissions(const dpp::guild_member& user, const dpp::channel& channel) const
		{
			return (_guild.permission_overwrites(user, channel));
		}

		bool handleButtonClick(const dpp::button_click_t& event);

		std::optional<dpp::role>    findRole(dpp::snowflake id) const;
		std::optional<dpp::channel> findChannel(dpp::snowflake id) const;

	private:
		dpp::guild                       _guild;
		dpp::guild_member                _me;
		std::optional<dpp::role>         _studyRole{};
		std::optional<dpp::channel>      _studyChannel{};
		std::optional<dpp::message>      _studyMessage{};
		DataStores::GuildSettings::Entry _settings;
	};
} // namespace B12

#endif
