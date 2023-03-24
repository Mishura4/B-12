#ifndef SHION_LOGGER_H_
#define SHION_LOGGER_H_

#include <functional>
#include <ostream>
#include <vector>

#include <fmt/format.h>

#include "shion/types.h"
#include "shion/utils/owned_resource.h"

namespace shion::io
{
  namespace _
  {
    enum LogLevel : uint32
    {
      NONE  = shion::bitflag<LogLevel>(0),
      BASIC = shion::bitflag<LogLevel>(1),
      INFO  = shion::bitflag<LogLevel>(2),
      ERROR = shion::bitflag<LogLevel>(3),
      DEBUG = shion::bitflag<LogLevel>(4),
      TRACE = shion::bitflag<LogLevel>(5),
      ALL   = shion::bitflag<LogLevel>(uint32(-1))
    };
  }

  using LogLevel = _::LogLevel;

  template <typename T>
  struct LoggerBase;

  template <typename T>
  concept logger_type = requires(T &t) {
    t.write(LogLevel::BASIC, ""sv);
    LogLevel{t.log_level};
  };

  template <typename T>
  struct LoggerBase
  {
      using PrefixGenerator = std::function<std::string(LogLevel, std::string_view)>;
      std::underlying_type_t<LogLevel> log_level{LogLevel::ALL};
      PrefixGenerator prefix_generator{shion::make_empty<std::string>};
      PrefixGenerator suffix_generator{shion::make_empty<std::string>};

      template <typename... Args>
      void log(LogLevel level, std::string_view str)
      {
        auto &self = *static_cast<T *>(this);

        if (!(level & self.log_level))
          return;
        self.write(level, str);
      }

      template <typename... Args>
      requires(sizeof...(Args) > 0)
      void log(LogLevel level, fmt::format_string<Args...> fmt, Args &&...args)
      {
        auto &self = *static_cast<T *>(this);

        if (!(level & self.log_level))
          return;
        self.write(level, fmt::format(fmt, std::forward<Args>(args)...));
      }
  };

  template <typename T>
  struct Logger;

  template <typename T>
  requires(!std::is_scalar_v<T>)
  Logger(T &target, auto...) -> Logger<std::conditional_t<std::is_scalar_v<T>, T, T &>>;

  template <typename T>
  Logger(T target, auto...) -> Logger<T>;

  template <typename T>
  requires(std::is_convertible_v<T &, std::ostream &>)
  struct Logger<T &> : public LoggerBase<Logger<T &>>
  {
      using base = LoggerBase<Logger<T &>>;
      using base::prefix_generator;
      using base::suffix_generator;

      Logger(std::ostream &target_, auto &&...args) : base{args...}, target(target_) {}

      Logger(const Logger &) = delete;
      Logger(Logger &&)      = default;

      std::ostream &target;

      void write(LogLevel level, std::string_view msg)
      {
        if (prefix_generator)
          target << prefix_generator(level, msg);
        target << msg;
        if (suffix_generator)
          target << suffix_generator(level, msg);
        target << '\n';
      }
  };

  template <typename T>
  requires(std::is_convertible_v<T &, std::ostream &>)
  struct Logger<T> : public LoggerBase<Logger<T>>
  {
      using base = LoggerBase<Logger<T>>;
      using base::prefix_generator;
      using base::suffix_generator;

      T target;

      Logger(T &&target_, auto &&...args) : base{args...}, target{std::forward<T>(target_)} {}

      Logger(const Logger &) = delete;
      Logger(Logger &&)      = default;

      void write(LogLevel level, std::string_view msg)
      {
        if (prefix_generator)
          target << prefix_generator(level, msg);
        target << msg;
        if (suffix_generator)
          target << suffix_generator(level, msg);
        target << '\n';
      }
  };

  template <>
  struct Logger<std::FILE *> : public LoggerBase<Logger<std::FILE *>>
  {
    using base = LoggerBase<Logger<std::FILE *>>;
      using base::prefix_generator;
      using base::suffix_generator;
    
      std::FILE *target;

      Logger(std::FILE *target_, auto &&...args) : LoggerBase{args...}, target(target_) {}

      Logger(const Logger &) = delete;
      Logger(Logger &&)      = default;

      void write(LogLevel level, std::string_view msg)
      {
        if (prefix_generator)
          _write(prefix_generator(level, msg));
        _write(msg);
        if (suffix_generator)
          _write(suffix_generator(level, msg));
        _write('\n');
      }

    private:
      std::size_t _write(std::string_view str)
      {
        return (std::fwrite(str.data(), sizeof(char), str.size(), target));
      }

      std::size_t _write(char c) { return (std::fwrite(&c, sizeof(char), 1, target)); }
  };

  template <>
  struct Logger<shion::utils::owned_stdfile> :
      public LoggerBase<Logger<shion::utils::owned_stdfile>>
  {
    using base = LoggerBase<Logger<shion::utils::owned_stdfile>>;
      using base::prefix_generator;
      using base::suffix_generator;
      using owned_stdfile = shion::utils::owned_stdfile;

      owned_stdfile target;

      Logger(owned_stdfile &&target_, auto &&...args) :
          LoggerBase{args...}, target(std::move(target_))
      {
      }

      Logger(const Logger &) = delete;
      Logger(Logger &&)      = default;

      void write(LogLevel level, std::string_view msg)
      {
        if (prefix_generator)
          _write(prefix_generator(level, msg));
        _write(msg);
        if (suffix_generator)
          _write(suffix_generator(level, msg));
        _write('\n');
      }

    private:
      std::size_t _write(std::string_view str)
      {
        return (std::fwrite(str.data(), sizeof(char), str.size(), target.get()));
      }

      std::size_t _write(char c) { return (std::fwrite(&c, sizeof(char), 1, target.get())); }
  };

  template <logger_type... Loggers>
  class LoggerSystem
  {
    public:
      template <logger_type... LoggersArgs>
      explicit constexpr LoggerSystem(LoggersArgs &...args)
      {
        (addLogger(args), ...);
      }

      template <logger_type T>
      constexpr bool addLogger(T &logger)
      {
        constexpr size_t tuplePos = _findLoggerVectorPos<T, 0>();

        if constexpr (tuplePos == std::numeric_limits<size_t>::max())
        {
          static_assert(tuplePos != std::numeric_limits<size_t>::max(), "unsupported logger type");
          return (false);
        }
        else
        {
          _collectiveLogLevel |= static_cast<LogLevel>(logger.log_level);
          std::get<tuplePos>(_loggers).emplace_back(&logger);
          return (true);
        }
      }

      template <typename... Args>
      requires(sizeof...(Args) > 0)
      void log(LogLevel level, fmt::format_string<Args...> fmt, Args &&...args)
      {
        if (!((_collectiveLogLevel & level) == level))
          return;
        auto line = fmt::format(fmt, std::forward<Args>(args)...);

        _log<0>(level, line);
      }

      void log(LogLevel level, std::string_view line) { _log<0>(level, line); }

      bool isLogEnabled(LogLevel level) const { return {(_collectiveLogLevel & level) == level}; }

    private:
      using LoggerList = std::tuple<std::vector<Loggers *>...>;

      template <size_t N>
      void _log(LogLevel level, std::string_view line)
      {
        if constexpr (std::tuple_size_v < LoggerList >> N)
        {
          for (auto logger : std::get<N>(_loggers))
            logger->log(level, line);
          _log<N + 1>(level, line);
        }
      }

      template <logger_type T, size_t N>
      static constexpr size_t _findLoggerVectorPos()
      {
        if constexpr (std::tuple_size_v < LoggerList >> N)
        {
          if constexpr (std::is_same_v<std::vector<T *>, std::tuple_element_t<N, LoggerList>>)
            return (N);
          else
            return (_findLoggerVectorPos<T, N + 1>());
        }
        else
          return (std::numeric_limits<size_t>::max());
      }

      LogLevel _collectiveLogLevel{LogLevel::NONE};
      LoggerList _loggers;
  };
}  // namespace shion::io

#endif
