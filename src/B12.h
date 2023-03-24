#ifndef B12_B12_H_
#define B12_B12_H_

#define NOMINMAX

#include <condition_variable>
#include <mutex>
#include <typeinfo>

#include <fmt/format.h>

#include <shion/containers/registry.h>
#include <shion/io/logger.h>
#include <shion/traits/type_list.h>
#include <shion/traits/value_list.h>
#include <shion/utils/string_literal.h>

#include <boost/pfr.hpp>

#include <magic_enum.hpp>

#include <dpp/dpp.h>

#ifdef ERROR
#  undef ERROR
#endif

using shion::string_literal;
using namespace shion::types;
using namespace std::string_view_literals;

namespace B12
{
	using shion::io::LogLevel;

	void log(LogLevel level, std::string_view str);

	bool isLogEnabled(LogLevel level);

	template <typename... Args>
		requires(sizeof...(Args) > 0)
	void log(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args)
	{
		if (isLogEnabled(level))
			log(level, fmt::format(fmt, std::forward<Args>(args)...));
	}

	template <typename Arg>
	class AsyncExecutor
	{
	public:
		using success_callback = void(const Arg&);
		using error_callback = void(const dpp::error_info& error);
		using rest_callback = void(const dpp::confirmation_callback_t&);

	private:
		std::mutex              _mutex;
		std::condition_variable _cv;
		bool                    _complete{true};

		std::function<success_callback> _on_success = shion::noop;
		std::function<error_callback>   _on_error   = [](const dpp::error_info& error)
		{
			B12::log(B12::LogLevel::ERROR, "error while trying to execute async task: {}", error.message);
		};
		std::function<rest_callback> _rest_callback = [this](const dpp::confirmation_callback_t& result)
		{
			std::unique_lock lock{_mutex};

			if (result.is_error())
				_on_error(result.get_error());
			else
				_on_success(result.get<Arg>());
			_complete = true;
			_cv.notify_all();
		};

	public:
		using arg_type = Arg;

		explicit AsyncExecutor(std::invocable<const Arg&> auto&& success_callback) :
			_on_success(success_callback) {}

		AsyncExecutor(
			std::invocable<const Arg&> auto&&       success_callback,
			std::invocable<dpp::error_info&> auto&& error_callback
		) :
			_on_success(success_callback),
			_on_error(error_callback) { }

		template <typename T, typename U, typename... Args>
			requires(requires(T t, U* u, Args... args)
			{
				std::invoke(t, u, std::forward<Args>(args)..., [](const dpp::confirmation_callback_t&) {});
			})

		void operator()(T&& routine, U* obj, Args&&... args)
		{
			std::scoped_lock lock(_mutex);
			assert(_complete);

			_complete = false;
			std::invoke(routine, obj, std::forward<Args>(args)..., _rest_callback);
		}

		void wait()
		{
			std::unique_lock lock{_mutex};
			auto             wait = [this]()
			{
				return (_complete);
			};

			_cv.wait(lock, wait);
		}
	};
} // namespace B12

#endif
