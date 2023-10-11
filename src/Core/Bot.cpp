#include "B12.h"

#include "Commands/CommandTable.h"

#include "Data/DataStores.h"
#include "Data/DataStructures.h"
#include "Data/Lang.h"

#include <array>

#include "Commands/commands.h"
#include "Commands/command_table.h"

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
	std::locale::global(std::locale("en_US.UTF-8"));

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
	try
	// we do this with try/catch as opposed to the iostream::fail API so we can have the error message
	{

		_logFile.open("latest.log", std::ios::out | std::ios::trunc);
		_logFile.exceptions(std::ios::failbit);
		_logFile.exceptions(std::ios::iostate{});
		_debugLogFile.open("debug.log", std::ios::out | std::ios::trunc);
		_debugLogFile.exceptions(std::ios::failbit);
		_debugLogFile.exceptions(std::ios::iostate{});
	}
	catch (const std::exception& e)
	{
		log(LogLevel::ERROR, "could not open log file: {}", e.what());
	}
	try
	{
		_bot          = std::make_unique<dpp::cluster>(_fetchToken(discord_token));
		_bot->intents = dpp::intents::i_message_content | dpp::intents::i_guild_messages;
		_bot->on_log(dpp_log);
		log(LogLevel::BASIC, "Loading resource caches");
		pokemon_cache = std::make_unique<PokeAPICache>(_bot.get());
	}
	catch (const std::exception& e)
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

#include "Commands/commands.h"

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
		_bot->on_slashcommand([handler = command::command_handler<dpp::slashcommand_t, command::response>::from_command_table<command::COMMAND_TABLE>()](dpp::slashcommand_t event) -> dpp::job {
			command::command_result<command::response> response = co_await handler(event);

			if (response.is_error()) {
				switch (response.get_error()) {
					case command::command_error::internal_error:
						event.reply(command::response::internal_error());
						break;

					case command::command_error::syntax_error:
						event.reply("Invalid command or params");
						break;
				}
				co_return;
			}
			const command::response& r = response.get();

			switch (r.action) {
				using enum command::response::action_t;

				case none:
					break;

				case reply:
					event.reply(r.message);
					break;

				case edit:
					event.edit_original_response(r.message);
					break;
			}
		});
		_bot->on_message_create(
			[](const auto& e)
			{
				_s_instance->_onMessageCreateEvent(e);
			}
		);
		_bot->on_button_click(
			[](const dpp::button_click_t& e)
			{
				return;
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
#ifdef B12_DEBUG
		/*
		_bot->on_message_create.co_attach(
			[](const dpp::message_create_t& e) -> dpp::task<void>
			{
				if (e.msg.author == e.from->creator->me.id)
					co_return;
				if (auto debug_channel_json = _config.find("debug_channel"); debug_channel_json != _config.end())
				{
					if (debug_channel_json->is_string())
					{
						auto&  as_str = debug_channel_json->get_ref<const std::string&>();
						uint64 id     = stoull(as_str); // TODO: ERROR HANDLING

						if (id != e.msg.channel_id)
							co_return;
					}
				}
				auto *cluster = &Bot::bot();
				auto test = [](dpp::cluster *cluster, dpp::message event) -> dpp::task<void> {
					dpp::confirmation_callback_t confirm = co_await cluster->co_message_create(dpp::message{"Retrieving emoji list"}.set_channel_id(event.channel_id));
					dpp::message original = confirm.get<dpp::message>();
					
					confirm = co_await cluster->co_guild_emojis_get(event.guild_id);

					const auto &map = confirm.get<dpp::emoji_map>();

					int i = 0;
					for (auto it = map.begin(); it != map.end() && i < 5; ++i)
					{
						dpp::message message{};
					
						message = fmt::format("{}",
							fmt::join(
								std::ranges::views::iota(it, map.end())
								| std::ranges::views::transform([](auto &&elem){ return (elem->second.get_mention()); })
								| std::ranges::views::take(10),
							" "));
						co_await cluster->co_message_create(message.set_channel_id(event.channel_id));
						std::ranges::advance(it, 10, map.end());
					}
				};
				co_await test(cluster, e.msg);
			}
		);*/
#endif

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

void Bot::_onReadyEvent(const dpp::ready_t &event)
{
	static const std::array activities = std::to_array<dpp::activity>(
		{
			{
				dpp::at_streaming,
				"\"; DROP DATABASE database",
				"test",
				"https://www.youtube.com/watch?v=qwxZU0VkWII&pp=ygUEbWVvdw"
			},
			{
				dpp::at_streaming,
				"Segmentation fault",
				"test",
				"https://www.youtube.com/watch?v=qwxZU0VkWII&pp=ygUEbWVvdw"
			},
			{
				dpp::at_streaming,
				"abort() speedrun Any%",
				"test",
				"https://www.youtube.com/watch?v=qwxZU0VkWII&pp=ygUEbWVvdw"
			}
		}
	);
	event.from->creator->set_presence(
		{dpp::ps_online, activities[static_cast<size_t>(std::rand()) % (activities.size())]}
	);
	
	// TODO: cleanup
	if (dpp::run_once<struct registerBotCommands>())
	{
		event.from->creator->global_bulk_command_create(command::get_api_commands(event.from->creator->me.id));
		/*
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

					_registerGuild(event.from->creator, id);
				}
			}
		}*/
	}
}

auto Bot::fetchGuild(dpp::snowflake id) -> observer_ptr<Guild>
{
	auto& ptr = _s_instance->_guilds[id];

	if (!ptr)
		ptr = std::make_unique<Guild>(id);
	return {ptr.get()};
}
