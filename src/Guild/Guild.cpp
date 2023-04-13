#include "B12.h"

#include "Guild.h"

#include "../Commands/CommandHandler.h"

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

	template <typename T>
	constexpr auto from_string = [](std::string_view str) -> std::optional<T>
	{
		T ret;

		auto [ptr, err] = std::from_chars(str.data(), str.data() + str.size(), ret);
		if (err != std::errc{} || ptr != str.data() + str.size())
			return {std::nullopt};
		return {ret};
	};

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
				if (params.empty())
					return {std::nullopt};
				auto end    = params.find(':');
				auto substr = params.substr(0, end);
				auto value  = from_string<boost::pfr::tuple_element_t<N, ButtonID>>(substr);
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

std::optional<dpp::role> Guild::findRole(dpp::snowflake id) const
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

std::optional<dpp::channel> Guild::findChannel(dpp::snowflake id) const
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
		[&]
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

dpp::permission Guild::getPermissions(
	const dpp::guild_member& user,
	const dpp::channel&      channel
) const
{
	return (_guild.permission_overwrites(user, channel));
}

void Guild::forgetButton(uint64_t id)
{
	std::scoped_lock lock(_mutex);

	if (auto it = _buttonActions.find(id); it != _buttonActions.end())
		_buttonActions.erase(it);
}

bool Guild::handleButtonClick(const dpp::button_click_t& event)
{
	if (event.custom_id.starts_with(button_command_prefix))
	{
		CommandHandler handler{event};

		handler.exec<"study">({});
		return (true);
	}
	if (event.custom_id.starts_with(button_action_prefix))
		return (_handleActionButton(event));
	if (event.custom_id == button_abort)
	{
		dpp::message                m = event.command.msg;
		std::vector<dpp::component> components;

		m.content = "Action cancelled.";
		m.components.swap(components);
		event.from->creator->interaction_response_create(
			event.command.id,
			event.command.token,
			{
				dpp::ir_update_message,
				m
			},
			makeForgetButtonsCallback(_guild.id, components)
		);
	}
	event.reply(dpp::message("Unknown interaction").set_flags(dpp::m_ephemeral));
	return (false);
}

void Guild::setMessageButtonsExpiration(dpp::snowflake id, button_timestamp timestamp) {}

bool Guild::_handleActionButton(const dpp::button_click_t& event)
{
	auto button_info = parse_button_id(event.custom_id.substr(button_action_prefix.size()));

	if (!button_info)
	{
		event.reply(dpp::message{"Unknown button"}.set_flags(dpp::m_ephemeral));
		return (false);
	}
	auto buttons = _buttonActions.find(button_info->message);

	if (buttons == _buttonActions.end())
	{
		event.reply(dpp::message{"Unknown button"}.set_flags(dpp::m_ephemeral));
		log(
			LogLevel::INFO,
			"button in guild {} is tied to message {} which was not found",
			_guild.id,
			button_info->message
		);
		return (false);
	}
	if (button_info->action >= buttons->second.buttons.size())
	{
		event.reply(dpp::message{"Unknown button"}.set_flags(dpp::m_ephemeral));
		log(
			LogLevel::INFO,
			"button in guild {} is tied to message {}:{} which was out of range",
			_guild.id,
			button_info->message,
			button_info->action
		);
		return (false);
	}
	auto action = buttons->second.buttons[button_info->action];
	log(
		LogLevel::BASIC,
		"executing button {}:{} for guild {}",
		uint64(button_info->message),
		button_info->action,
		_guild.id
	);
	if (!getPermissions(event.command.member, event.command.channel).has(action.permissions))
	{
		event.reply(
			dpp::message(
				"You do not have the permissions required to complete this action.\n(Note: buttons require administrator for now, as i'm not done implementing them)"
			).set_flags(dpp::m_ephemeral)
		);
		return (false);
	}

	CommandHandler handler(event);

	handler.exec(action.fun, event);
	return (true);
}

void Guild::studyMessage(dpp::snowflake) {}

bool Guild::saveSettings()
{
	return (DataStores::guild_settings.save(_settings));
}

auto Guild::studyChannel() const -> const std::optional<dpp::channel>&
{
	return (_studyChannel);
}

auto Guild::studyRole() const -> const std::optional<dpp::role>&
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
