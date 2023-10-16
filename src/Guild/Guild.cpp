#include "B12.h"

#include "Guild.h"

#include "Core/Bot.h"
#include "Data/DataStores.h"
#include "Data/Lang.h"

using namespace B12;

namespace
{
	struct ButtonID
	{
		uint64 message;
		uint64 action;
	};

	/*template <typename T>
	constexpr auto from_string = [](std::string_view str) -> std::optional<T>
	{
		T ret;

		auto [ptr, err] = std::from_chars(str.data(), str.data() + str.size(), ret);
		if (err != std::errc{} || ptr != str.data() + str.size())
			return {std::nullopt};
		return {ret};
	};*/

	constexpr auto parse_button_id = [](std::string_view params) -> std::optional<ButtonID>
	{
		constexpr auto     impl = []<size_t... Ns>(
			std::string_view params,
			std::index_sequence<Ns...>
		) -> std::optional<ButtonID>
		{
			constexpr auto      parse = []<size_t N>(
				std::string_view& params
			) -> std::optional<boost::pfr::tuple_element_t<N, ButtonID>>
			{
				using type = boost::pfr::tuple_element_t<N, ButtonID>;
				if (params.empty())
					return {std::nullopt};
				auto end    = params.find(':');
				auto substr = std::string{params.substr(0, end)};
				type value;

				if constexpr (std::is_integral_v<type>)
				{
					if constexpr (std::is_signed_v<type>)
						value = static_cast<type>(std::stoll(substr));
					else
						value = static_cast<type>(std::stoul(substr));
				}
				else if constexpr (std::is_floating_point_v<type>)
					value = static_cast<type>(std::stold(substr));
				params      = (end == std::string::npos ? std::string_view{} : params.substr(end + 1));

				if (!value)
					return {std::nullopt};
				return {value};
			};

			std::tuple values{parse.template operator()<Ns>(params)...};

			if (((!std::get<Ns>(values)) || ...))
				return {std::nullopt};
			return (ButtonID{(*std::get<Ns>(values))...});
		};

		return (impl(params, std::make_index_sequence<boost::pfr::tuple_size_v<ButtonID>>()));
	};
}

Guild::Guild(dpp::snowflake id) :
	_settings{DataStores::guild_settings.get(id)}
{
	loadGuildSettings();
}

void Guild::loadGuildSettings()
{
	auto studyChannelID = _settings.get<"study_channel">().value;
	auto studyRoleID    = _settings.get<"study_role">().value;
	/*auto studyMessageID = _settings.get<"study_react_message">().value;*/

	if (studyRoleID)
		_studyRole = studyRoleID;
	if (studyChannelID)
		_studyChannel = studyChannelID;
}

// TODO: do these better, cache roles, cache channels
void Guild::studyRole(const dpp::role* role)
{
	if (role)
	{
		_studyRole                    = role->id;
		_settings.get<"study_role">() = role->id;
	}
	else
	{
		_studyRole                    = std::nullopt;
		_settings.get<"study_role">() = 0;
	}
}

void Guild::studyChannel(const dpp::channel* channel)
{
	if (channel)
	{
		_studyChannel                    = channel->id;
		_settings.get<"study_channel">() = channel->id;
	}
	else
	{
		_studyChannel                    = std::nullopt;
		_settings.get<"study_channel">() = 0;
	}
}

dpp::permission Guild::getPermissions(
	const dpp::guild_member& user,
	const dpp::channel&      channel
) const
{
	return (_guild.permission_overwrites(user, channel));
}

void Guild::studyMessage(dpp::snowflake) {}

bool Guild::saveSettings()
{
	return (DataStores::guild_settings.save(_settings));
}

auto Guild::studyChannel() const -> const std::optional<dpp::snowflake>&
{
	return (_studyChannel);
}

auto Guild::studyRole() const -> const std::optional<dpp::snowflake>&
{
	return (_studyRole);
}

auto Guild::settings() const -> const DataStores::GuildSettings::Entry&
{
	return (_settings);
}

auto Guild::b12Member() const -> const dpp::guild_member&
{
	return (_me);
}

auto Guild::dppGuild() const -> const dpp::guild&
{
	return (_guild);
}
