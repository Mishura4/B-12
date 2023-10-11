#pragma once

#include <chrono>
#include <exception>
#include <filesystem>

#include "../API/APICache.h"

#include "../Pokemon/PokemonAPI.h"

#include "Commands/MultiCommandProcess.h"
#include "Data/Database.h"
#include "Guild/Guild.h"

namespace B12
{
	class Guild;
	struct MultiCommandProcess;

	namespace stdfs = std::filesystem;

	class Bot
	{
		using clock = std::chrono::steady_clock;

	public:
		using timestamp = std::chrono::time_point<clock>;
		using duration = timestamp::duration;

		using LogLevel = shion::io::LogLevel;

		template <typename T>
		using observer_ptr = shion::utils::observer_ptr<T>;

		struct FatalException : public std::exception
		{
			using exception::exception;
			using exception::operator=;

			const char* msg{nullptr};

			FatalException(const char* msg_) :
				msg(msg_) { }

			const char *what() const noexcept override
			{
				return (msg);
			}
		};

		explicit Bot(const char* discordToken);
		~Bot();

		int run();

		template <typename... Args>
			requires(sizeof...(Args) > 0)
		static void log(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args)
		{
			_s_instance->_logger.log(level, fmt, std::forward<Args>(args)...);
		}

		template <typename... Args>
			requires(sizeof...(Args) > 0)
		static void log(fmt::format_string<Args...> fmt, Args&&... args)
		{
			_s_instance->_logger.log(LogLevel::BASIC, fmt, std::forward<Args>(args)...);
		}

		static void log(LogLevel level, std::string_view str)
		{
			_s_instance->_logger.log(level, str);
		}

		static void log(std::string_view str)
		{
			_s_instance->_logger.log(LogLevel::BASIC, str);
		}

		static observer_ptr<Guild> fetchGuild(dpp::snowflake id);

		template <string_literal CommandName>
		static CommandResponse command(
			const dpp::interaction_create_t&          e,
			const dpp::interaction&                   interaction,
			std::span<const dpp::command_data_option> options
		);

		static dpp::cluster &bot() noexcept
		{
			return (*_s_instance->_bot);
		}

		static const timestamp &lastUpdate() noexcept
		{
			return (_s_instance->_lastUpdate);
		}

		static bool isLogEnabled(B12::LogLevel level) noexcept
		{
			return (_s_instance->_logger.isLogEnabled(level));
		}

		static void MCPUpdate(dpp::snowflake id);

		static void stop() noexcept
		{
			_s_instance->_running = false;
		}

		static inline std::unique_ptr<PokeAPICache> pokemon_cache = nullptr;

	private:
		std::string _fetchToken(const char* console_arg) const;

		static Bot* _s_instance;

		void _readConfig(const stdfs::path& config_file_path);
		void _writeConfig(const stdfs::path& config_file_path, bool workaround = true);

		// TODO: change init functions to throw exceptions instead of return a bool
		bool _initDatabases();
		bool _initDatastores();

		void _registerGuild(dpp::cluster *cluster, dpp::snowflake id);

		template <typename T>
		using Logger = shion::io::Logger<T>;

		template <typename... Ts>
		using LoggerSystem = shion::io::LoggerSystem<Logger<Ts>...>;

		void _onReadyEvent(const dpp::ready_t& e);
		void _onSlashCommandEvent(const dpp::slashcommand_t& e);
		void _onMessageCreateEvent(const dpp::message_create_t& e);

		// TODO: rewrite this mess so LoggerSystem owns each logger, each logger owns its file
		std::fstream          _logFile{};
		std::fstream          _debugLogFile{};
		Logger<std::fstream&> _fileLogger{
			_logFile,
			LogLevel::BASIC | LogLevel::INFO | LogLevel::ERROR
		};
		Logger<std::fstream&> _debugLogger{
			_debugLogFile,
			LogLevel::BASIC | LogLevel::INFO | LogLevel::DEBUG | LogLevel::ERROR
		};
		Logger<std::ostream&> _coutLogger{std::cout, LogLevel::TRACE | LogLevel::BASIC | LogLevel::INFO};
		Logger<std::ostream&> _cerrLogger{std::cerr, LogLevel::ERROR};

		LoggerSystem<std::fstream&, std::ostream&> _logger{
			_fileLogger,
			_debugLogger,
			_coutLogger,
			_cerrLogger
		};

		// _bot is a unique_ptr because we want to initialize loggers and config before it and the constructor does not allow it
		std::unique_ptr<dpp::cluster> _bot{nullptr};
		Database                      _dbGlobalData;
		bool                          _running{true};

		timestamp _lastUpdate{};
		duration  _tickDuration{};

		template <typename T>
		using cache = std::unordered_map<dpp::snowflake, std::unique_ptr<T>>;

		cache<Guild>               _guilds{};
		cache<MultiCommandProcess> _MCPs{};

		dpp::json   _config{};
		std::mutex  _MCPmutex{};
	};
} // namespace B12
