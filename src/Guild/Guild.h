#ifndef B12_GUILD_H_
#define B12_GUILD_H_

#include "B12.h"

#include "Commands/Command.h"
#include "Commands/CommandResponse.h"
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
		const std::optional<dpp::role> &        studyRole() const;
		const std::optional<dpp::channel> &     studyChannel() const;
		const dpp::guild_member &               b12Member() const;
		const dpp::guild &                      dppGuild() const;

		void loadGuildSettings();
		bool saveSettings();

		dpp::permission getPermissions(
			const dpp::guild_member& user,
			const dpp::channel&      channel
		) const;

		void forgetButton(uint64 id);

		// TODO: make button actions their own API containing this
		static auto makeForgetButtonsCallback(
			dpp::snowflake                  guild_id,
			std::span<const dpp::component> components
		);

		template <typename T>
			requires (invocable_r<T, CommandResponse, const dpp::button_click_t&>)
		dpp::component createButtonAction(
			dpp::snowflake  id,
			T&&             action,
			dpp::permission permission
		)
		{
			std::scoped_lock lock(_mutex);

			auto& buttons = _buttonActions[id];

			buttons.buttons.emplace_back(std::forward<T>(action), permission);
			return (dpp::component{}.set_id(
				fmt::format("action:{}:{}"sv, id, buttons.buttons.size() - 1)
			));
		}

		using button_timestamp = std::chrono::steady_clock::time_point;

		void setMessageButtonsExpiration(dpp::snowflake id, button_timestamp);

		bool handleButtonClick(const dpp::button_click_t& event);

		std::optional<dpp::role>    findRole(dpp::snowflake id) const;
		std::optional<dpp::channel> findChannel(dpp::snowflake id) const;

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
		std::optional<dpp::role>                         _studyRole{};
		std::optional<dpp::channel>                      _studyChannel{};
		std::optional<dpp::message>                      _studyMessage{};
		std::map<dpp::snowflake, MessageButtons>         _buttonActions{};
		std::vector<std::pair<button_timestamp, uint64>> _expiringButtons;
		DataStores::GuildSettings::Entry                 _settings;
		std::mutex                                       _mutex;
	};
} // namespace B12

inline auto B12::Guild::makeForgetButtonsCallback(
	dpp::snowflake                  guild_id,
	std::span<const dpp::component> components
)
{
	std::vector<uint64> buttons;

	constexpr auto is_action = [](const dpp::component& c) -> bool
	{
		return (c.custom_id.starts_with(button_action_prefix));
	};
	constexpr auto get_id = [](const dpp::component& c) -> uint64
	{
		return (std::stoull(c.custom_id.substr(button_action_prefix.size())));
	};

	for (const dpp::component& row : components)
	{
		std::ranges::copy(
			row.components | std::views::filter(is_action) | std::views::transform(get_id),
			std::back_inserter(buttons)
		);
	}
	return (
		[guild_id, buttons = std::move(buttons)](
		const dpp::confirmation_callback_t& r
	)
		{
			if (r.is_error())
				return;

			Guild* guild = fetchGuild(guild_id);

			if (!guild)
				return;
			for (uint64 b : buttons)
				guild->forgetButton(b);
		}
	);
}

#endif
