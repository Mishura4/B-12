#include "B12.h"

#include "Commands/CommandTable.h"

#include "Data/DataStores.h"
#include "Data/DataStructures.h"
#include "Data/Lang.h"

#include <array>

using namespace B12;

Bot* Bot::_s_instance{nullptr};

#ifdef B12_DEBUG
constexpr auto CONFIG_FILE = "config_debug.json"sv;
#else
constexpr auto CONFIG_FILE = "config.json"sv;
#endif

constexpr auto dpp_log = [](const dpp::log_t& log)
{
	using LogLevel = Bot::LogLevel;

	LogLevel level;

	switch (log.severity)
	{
		case dpp::ll_critical:
		case dpp::ll_error:
		case dpp::ll_warning:
			level = LogLevel::ERROR;
			break;

		case dpp::ll_debug:
			level = LogLevel::DEBUG;
			break;

		case dpp::ll_info:
			level = LogLevel::INFO;
			break;

		case dpp::ll_trace:
			level = LogLevel::TRACE;
			break;

		default:
			level = LogLevel::DEBUG;
			break;
	}
	Bot::log(level, "dpp: {}", log.message);
};

std::string Bot::_fetchToken(const char* console_arg) const
{
	if (console_arg)
		return (console_arg);
	if (auto value = _config.find("api_token"); value != _config.end() && !value->empty())
		return (*value);
	if (const char* from_env = std::getenv("DISCORD_TOKEN"); from_env != nullptr)
		return (from_env);
	return (std::string{});
}

Bot::Bot(const char* discord_token)
{
	struct BotSingletonException : public FatalException
	{
		using FatalException::FatalException;
		using FatalException::operator=;

		const char *what() const noexcept override
		{
			return ("B12 Singleton already exists");
		}
	};

	if (_s_instance)
		throw BotSingletonException();
	_s_instance = this;
	_readConfig(CONFIG_FILE);
	try // we do this with try/catch as opposed to the iostream::fail API so we can have the error message
	{
		_logFile.exceptions(std::ios::failbit);
		_logFile.open("latest.log", std::ios::out | std::ios::trunc);
		_logFile.exceptions(std::ios::iostate{});
		_debugLogFile.exceptions(std::ios::failbit);
		_debugLogFile.open("debug.log", std::ios::out | std::ios::trunc);
		_debugLogFile.exceptions(std::ios::iostate{});
	}
	catch (const std::exception& e)
	{
		log(LogLevel::ERROR, "could not open log file: {}", e.what());
	}
	try
	{
		_bot					= std::make_unique<dpp::cluster>(_fetchToken(discord_token));
		_bot->intents = dpp::intents::i_message_content | dpp::intents::i_guild_messages;
		_bot->on_log(dpp_log);
	}
	catch (const std::exception &e)
	{
		log(LogLevel::ERROR, "could not start bot: {}", e.what());
		throw;
	}
}

Bot::~Bot()
{
	_writeConfig(CONFIG_FILE);
}

void Bot::_readConfig(const stdfs::path& config_file_path)
{
	constexpr auto make_default_configuration = [](dpp::json& json)
	{
		json["api_token"] = "";
		json["guilds"]    = json::array();
	};

	if (std::error_code err; !exists(config_file_path, err))
	{
		if (err && err != std::errc::no_such_file_or_directory)
		{
			log(LogLevel::ERROR, "error while trying to fetch configuration file: {}", err.message());
			throw FatalException{"error while trying to fetch configuration file"};
		}
		log(LogLevel::BASIC, "configuration file does not exist - using default");
		make_default_configuration(_config);
		return;
	} // else
	std::ifstream config_file;

	config_file.exceptions(std::ios::failbit);
	log(LogLevel::BASIC, "loading configuration {}", config_file_path.string());
	try
	{
		config_file.open(config_file_path, std::ios::in);
		_config = dpp::json::parse(config_file);
	}
	catch (std::exception& e)
	{
		log(LogLevel::ERROR, "error while reading configuration file: {}", e.what());
		log(LogLevel::ERROR, "using default configuration");
		make_default_configuration(_config);
	}
}

void Bot::_writeConfig(const stdfs::path& config_file_path, bool workaround)
{
	const stdfs::path* write_path = &config_file_path;
	std::error_code    err;

	log(LogLevel::BASIC, "saving configuration...");
	try
	{
		auto backup_path = config_file_path;
		backup_path.append(".backup");
		if (
			exists(config_file_path, err) && !copy_file(config_file_path, backup_path, err) &&
			err != std::errc::no_such_file_or_directory)
		{
			log(LogLevel::ERROR, "could not back up configuration file: {}", err.message());
			if (!workaround)
				throw FatalException("could not back up configuration file, refusing to overwrite");
			// else
			backup_path = config_file_path;
			backup_path += ".tmp";
			write_path = &backup_path;
			log(LogLevel::ERROR, "configuration will be written at: {}", write_path->string());
		}
		std::ofstream f;
		std::string   content = _config.dump(1, '\t');

		f.exceptions(std::ios::failbit);
		f.open(config_file_path, std::ios::out | std::ios::trunc);
		f << content;
		f.close();
	}
	catch (const std::system_error& ex)
	{
		log(LogLevel::ERROR, "exception while saving configuration file : {}", ex.what());
	}
}

bool Bot::_initDatabases()
{
	stdfs::path     globalDbPath{"data/global.sqlite"};
	std::error_code errcode;

	log(LogLevel::BASIC, "loading database {}", globalDbPath.string());
	if (!stdfs::create_directories(globalDbPath.parent_path(), errcode) && errcode)
	{
		log(LogLevel::ERROR, "could not create data directory: {}", errcode.message());
		// TODO: exception
		return (false);
	}
	if (!_dbGlobalData.open(globalDbPath))
	{
		// TODO: better errors
		log(LogLevel::ERROR, "could not load database");
		return (false);
	}
	log(LogLevel::BASIC, "database loaded");
	return (true);
}

bool Bot::_initDatastores()
{
	DataStores::guild_settings.setDatabase(_dbGlobalData);

	log(LogLevel::BASIC, "loading datastores");
	DataStores::guild_settings.loadAll();
	log(LogLevel::BASIC, "datastores loading complete");
	return (true);
}

namespace {}

int Bot::run()
{
	stdfs::path currentDir = stdfs::current_path();

	_running = true;

	constexpr static std::array init_routines = {&Bot::_initDatabases, &Bot::_initDatastores};

	std::apply(
		[this](auto... routine)
		{
			(std::invoke(routine, this), ...);
		},
		init_routines
	);

	try
	{
		_bot->on_ready(
			[](const auto& e)
			{
				_s_instance->_onReadyEvent(e);
			}
		);
		_bot->on_slashcommand(
			[](const auto& e)
			{
				_s_instance->_onSlashCommandEvent(e);
			}
		);
		_bot->on_message_create(
			[](const auto& e)
			{
				_s_instance->_onMessageCreateEvent(e);
			}
		);
		_bot->on_button_click(
			[](const dpp::button_click_t& e)
			{
				Guild* guild = _s_instance->fetchGuild(e.command.guild_id);
				if (!guild)
				{
					e.reply(
						dpp::message{lang::DEFAULT.ERROR_GENERIC_COMMAND_FAILURE}.set_flags(dpp::m_ephemeral)
					);
					return;
				}
				guild->handleButtonClick(e);
			}
		);

		log(LogLevel::BASIC, "Starting bot cluster...");
		_bot->start(dpp::start_type::st_return);
	}
	catch (const dpp::exception& e)
	{
		log(LogLevel::ERROR, "D++ exception thrown: {}", e.what());
		throw;
	}

	log(LogLevel::BASIC, "Starting lifetime loop");
	while (_running)
	{
		auto now      = clock::now();
		_tickDuration = now - _lastUpdate;
		_lastUpdate   = now;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	log(LogLevel::BASIC, "shutting down...");
	return (0);
}

void Bot::_onMessageCreateEvent(const dpp::message_create_t& e)
{
	const auto& message = e.msg;

	if (auto message_reply_id = message.message_reference.message_id; message_reply_id > 0)
	{
		std::scoped_lock MCPlock{_MCPmutex};

		if (auto it = _MCPs.find(message_reply_id); it != _MCPs.end())
		{
			MultiCommandProcess* mcp = it->second.get();

			assert(mcp);
			if (!mcp->processing)
			{
				// TODO: move expiration into MCP callback
				mcp->processing = true;
				mcp->expiration = _lastUpdate + std::chrono::seconds(60);
				mcp->step(message);
			}
			else
			{
				dpp::message reply("This operation is already running!");

				reply.set_flags(dpp::m_ephemeral);
				e.reply(reply);
			}
		}
	}
}

void Bot::MCPUpdate(dpp::snowflake id)
{
	std::scoped_lock MCPlock{_s_instance->_MCPmutex};
	auto             it = _s_instance->_MCPs.find(id);

	if (it == _s_instance->_MCPs.end() || !it->second)
	{
		Bot::log("could not find MCP with id {}", id);
		return;
	}
	std::unique_ptr mcp = std::move(it->second);
	_s_instance->_MCPs.emplace(mcp->master->id, std::move(mcp));
	_s_instance->_MCPs.erase(id);
}

template <size_t CommandN, size_t N>
constexpr auto Bot::_gatherParameter() -> Bot::CommandParameter
{
	constexpr auto& entry        = std::get<CommandN>(COMMAND_TABLE);
	constexpr auto& option_name_ =
		std::remove_reference_t<decltype(entry)>::option_list::template at<N>::NAME;
	constexpr auto& option = entry.template get_option<option_name_>();

	return {
		.name{option_name_},
		.description{option.description},
		.possible_types{option.possible_types},
		.required{option.required}
	};
}

template <size_t CommandN, size_t... Is>
constexpr auto Bot::_gatherParameters(
	std::index_sequence<Is...>
)
	-> std::array<CommandParameter, sizeof...(Is)>
{
	return {_gatherParameter<CommandN, Is>()...};
}

template <size_t N>
constexpr void Bot::_gatherCommand(CommandNode& base)
{
	constexpr auto&            entry   = std::get<N>(COMMAND_TABLE);
	constexpr std::string_view name    = entry.name;
	Bot::CommandNode*          current = &base;

	size_t begin = 0;
	while (begin < name.size())
	{
		size_t end = name.find(' ', begin);
		if (end == std::string_view::npos)
			end = name.size();
		std::string_view word = name.substr(begin, end - begin);
		auto             it   = (std::ranges::find_if(
			current->sub_commands,
			[word](const Bot::CommandNode& node) constexpr
			{
				return (node.name == word);
			}
		));
		if (it == current->sub_commands.end())
		{
			auto before = std::ranges::upper_bound(
				current->sub_commands,
				word,
				std::less<std::string_view>(),
				CommandNode::searchProj
			);
			auto node = current->sub_commands.emplace(before, std::string{word});
			current   = &(*node);
			begin     = end + 1;
		}
		else if (it->name == word)
		{
			current = &(*it);
			begin   = end + 1;
		}
		else
			begin = std::string_view::npos;
	}
	if constexpr (entry.num_parameters > 0)
	{
		std::copy_n(
			_gatherParameters<N>(std::make_index_sequence<entry.num_parameters>()).begin(),
			entry.num_parameters,
			std::back_inserter(current->parameters)
		);
	}
	current->user_permissions = entry.user_permissions;
	current->bot_permissions  = entry.bot_permissions;
	current->description      = entry.description;
	current->handler          = entry.handler;
	current->index            = N;
}

template <size_t... Is>
constexpr auto Bot::_gatherCommands(std::index_sequence<Is...>) -> CommandNode
{
	CommandNode ret;

	(_gatherCommand<Is>(ret), ...);
	return (ret);
}

dpp::command_option Bot::_populateSubCommand(const CommandNode& node)
{
	dpp::command_option ret{
		(node.sub_commands.empty() ? dpp::co_sub_command : dpp::co_sub_command_group),
		std::string(node.name),
		std::string(node.description)
	};

	_populateCommandOptions(ret, node);
	for (const CommandNode& child : node.sub_commands)
	{
		ret.add_option(_populateSubCommand(child));
	}
	return (ret);
}

template <typename T>
void Bot::_populateCommandOptions(T& cmd, const CommandNode& node) const
{
	for (const auto& param : node.parameters)
	{
		if (param.possible_types.size() > 1)
		{
			log("possible_types > 1 is unsupported");
		}
		else
		{
			dpp::command_option opt{
				param.possible_types[0],
				std::string(param.name),
				std::string(param.description),
				param.required
			};

			cmd.add_option(opt);
		}
	}
}

void Bot::_registerGuild(dpp::snowflake id)
{
	for (const auto& existing_cmd : _bot->guild_commands_get_sync(id))
	{
		auto it = std::ranges::find(
			_commandTable.sub_commands,
			existing_cmd.second.name,
			[](const Bot::CommandNode& node) -> std::string_view
			{
				return (node.name);
			}
		);
		if (it == _commandTable.sub_commands.end())
			_bot->guild_command_delete(existing_cmd.second.id, id);
	}
	for (auto& node : _commandTable.sub_commands)
	{
		if (node.index < 0)
		{
			Bot::log(
				LogLevel::ERROR,
				"Error: command {} was gathered but not found in the command table",
				_commandTable.name
			);
			continue;
		}
		dpp::slashcommand cmd(std::string{node.name}, std::string{node.description}, _bot->me.id);
		cmd.set_default_permissions(node.user_permissions);
		_populateCommandOptions(cmd, node);
		for (const CommandNode& child : node.sub_commands)
		{
			cmd.add_option(_populateSubCommand(child));
		}
		_bot->guild_command_create(cmd, id);
	}
}

void Bot::_onReadyEvent(const dpp::ready_t&)
{
	// TODO: cleanup
	if (dpp::run_once<struct registerBotCommands>())
	{
		_commandTable =
			_gatherCommands(std::make_index_sequence<std::tuple_size_v<decltype(COMMAND_TABLE)>>());
		#ifdef B12_DEBUG
		for (auto& node : _commandTable.sub_commands)
		{
			node.name = fmt::format("dev_{}", node.name);
		}
		#endif
		auto guilds_json = _config.find("guilds");
		if (guilds_json != _config.end() && guilds_json->is_array())
		{
			for (auto& i : guilds_json->get_ref<const json::array_t&>())
			{
				if (i.is_string())
				{
					auto&  as_str = i.get_ref<const std::string&>();
					uint64 id     = stoull(as_str); // TODO: ERROR HANDLING

					_registerGuild(id);
				}
			}
		}
	}
}

auto Bot::_findCommand(
	const dpp::slashcommand_t& e,
	const CommandNode&         node,
	const std::string*&        cmd_name,
	const auto&                command_options,
	dpp::interaction&          interact
) -> const CommandNode*
{
	auto it = std::ranges::lower_bound(
		node.sub_commands,
		*cmd_name,
		std::less<std::string_view>(),
		CommandNode::searchProj
	);
	if (it == std::end(node.sub_commands))
	{
		Bot::log(
			LogLevel::INFO,
			"user {} sent unknown command {}",
			interact.get_issuing_user().format_username(),
			interact.get_command_name()
		);
		return (nullptr);
	}
	for (const dpp::command_data_option& opt : command_options)
	{
		if (opt.type == dpp::co_sub_command_group || opt.type == dpp::co_sub_command)
		{
			cmd_name = &opt.name;
			return (_findCommand(e, *it, cmd_name, opt.options, interact));
		}
	}
	if (!interact.app_permissions.has(it->bot_permissions))
	{
		dpp::message reply{
			lang::DEFAULT.PERMISSION_BOT_MISSING.format(
				interact.get_resolved_channel(interact.channel_id),
				dpp::permission(it->bot_permissions)
			)
		};

		reply.set_flags(dpp::message_flags::m_ephemeral);
		e.reply(reply);
		return (&(*it));
	}
	const auto& perms        = interact.resolved.member_permissions;
	auto        member_perms = perms.find(interact.get_issuing_user().id);
	if (
		member_perms == perms.end() || !(member_perms->second.has(dpp::permissions::p_administrator) ||
																		 member_perms->second.has(it->user_permissions)))
	{
		dpp::message reply{lang::DEFAULT.PERMISSION_USER_MISSING.format(it->user_permissions)};

		#ifndef B12_DEBUG
		reply.set_flags(dpp::message_flags::m_ephemeral);
		#endif
		e.reply(reply);
		return (&(*it));
	}
	it->handler(e, interact, std::span<const dpp::command_data_option>{command_options});
	return (&(*it));
}

void Bot::_onSlashCommandEvent(const dpp::slashcommand_t& e)
{
	dpp::interaction         interaction  = e.command;
	dpp::command_interaction cmd_data     = interaction.get_command_interaction();
	const std::string*       command_name = &cmd_data.name;

	#ifdef B12_DEBUG
	if (auto debug_channel_json = _config.find("debug_channel"); debug_channel_json != _config.end())
	{
		if (debug_channel_json->is_string())
		{
			auto&  as_str = debug_channel_json->get_ref<const std::string&>();
			uint64 id     = stoull(as_str); // TODO: ERROR HANDLING

			if (interaction.channel_id != dpp::snowflake{id})
			{
				e.reply(
					dpp::message{lang::DEFAULT.DEV_WRONG_CHANNEL.format(id)}.set_flags(dpp::m_ephemeral)
				);
				return;
			}
		}
	}
	#endif
	_findCommand(e, _commandTable, command_name, cmd_data.options, interaction);
}

auto Bot::fetchGuild(dpp::snowflake id) -> observer_ptr<Guild>
{
	auto& ptr = _s_instance->_guilds[id];

	if (!ptr)
		ptr = std::make_unique<Guild>(id);
	return {ptr.get()};
}
