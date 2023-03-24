#include "B12.h"

#include "Guild.h"

#include "Core/Bot.h"
#include "Data/DataStores.h"
#include "Data/Lang.h"

using namespace B12;

Guild::Guild(dpp::snowflake id) :
	_settings{DataStores::guild_settings.get(id)}
{
	try
	{
		_guild = Bot::bot().guild_get_sync(id);
		_me    = Bot::bot().guild_get_member_sync(id, Bot::bot().me.id);
	}
	catch (const dpp::rest_exception& e)
	{
		B12::log(B12::LogLevel::ERROR, "Error while resolving data for guild {} : {}", id, e.what());
	}
	loadGuildSettings();
}

auto Guild::findRole(dpp::snowflake id) const -> std::optional<dpp::role>
{
	if (dpp::role* role = dpp::find_role(id); role != nullptr)
		return {*role};
	try
	{
		dpp::role_map role_map = Bot::bot().roles_get_sync(_guild.id);

		auto it = role_map.find(id);
		if (it != role_map.end())
			return {it->second};
	}
	catch (const dpp::rest_exception& e)
	{
		B12::log(
			B12::LogLevel::ERROR,
			"Error while resolving role {} for guild {} : {}",
			id,
			_guild.id,
			e.what()
		);
	}
	return {std::nullopt};
}

auto Guild::findChannel(dpp::snowflake id) const -> std::optional<dpp::channel>
{
	if (dpp::channel* c = dpp::find_channel(id); c != nullptr)
		return (*c);
	std::mutex                  m;
	std::condition_variable     cv;
	std::unique_lock            lock{m};
	std::optional<dpp::channel> ret{};
	bool                        complete{false};

	auto callback = [&](const dpp::confirmation_callback_t& result)
	{
		std::unique_lock _lock{m};

		if (result.is_error())
		{
			B12::log(
				B12::LogLevel::TRACE,
				"Error while resolving role {} for guild {} : {}",
				id,
				_guild.id,
				result.get_error().message
			);
		}
		else
			ret = result.get<dpp::channel>();
		complete = true;
		cv.notify_all();
	};

	Bot::bot().channel_get(id, callback);
	cv.wait(
		lock,
		[&]()
		{
			return (complete);
		}
	);
	return {ret};
}

void Guild::loadGuildSettings()
{
	auto studyChannelID = _settings.get<"study_channel">().value;
	auto studyRoleID    = _settings.get<"study_role">().value;
	/*auto studyMessageID = _settings.get<"study_react_message">().value;*/

	if (studyRoleID)
		_studyRole = findRole(studyRoleID);
	if (studyChannelID)
		_studyChannel = findChannel(studyChannelID);
}

// TODO: do these better, cache roles, cache channels
void Guild::studyRole(const dpp::role* role)
{
	if (role)
	{
		_studyRole                    = *role;
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
		_studyChannel                    = *channel;
		_settings.get<"study_channel">() = channel->id;
	}
	else
	{
		_studyChannel                    = std::nullopt;
		_settings.get<"study_channel">() = 0;
	}
}

bool Guild::handleButtonClick(const dpp::button_click_t& event)
{
	if (event.custom_id == "study_toggle")
	{
		Bot::command<"study">(event, event.command, {});
		return (true);
	}
	return (false);
}

void Guild::studyMessage(dpp::snowflake) {}

bool Guild::saveSettings()
{
	return (DataStores::guild_settings.save(_settings));
}

const DataStores::GuildSettings::Entry &Guild::settings() const
{
	return (_settings);
}
