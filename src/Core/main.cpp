#include "B12.h"

#include <iostream>

#include "Bot.h"

// #include <shion/utils/string_literal.h>
// #include <shion/containers/registry.h>

#include <variant>
#include <tuple>

#include <shion/traits/value_list.h>

#include <shion/traits/type_list.h>
#include <shion/containers/registry.h>

using namespace shion::literals;

namespace
{
	std::mutex              app_mutex;
	std::condition_variable app_cv;
	bool                    cleaned_up{false};

	void hook_close_event()
	{
		#ifdef _WIN32
		constexpr auto handler_routine = [](DWORD eventCode)-> BOOL
		{
			switch (eventCode)
			{
				case CTRL_CLOSE_EVENT:
				case CTRL_C_EVENT:
					B12::Bot::stop();
					std::unique_lock lock(app_mutex);

					app_cv.wait(
						lock,
						[]()
						{
							return (cleaned_up);
						}
					);
					return (TRUE);
			}
			return (TRUE);
		};
		SetConsoleCtrlHandler(handler_routine, TRUE);
		#else
		#endif

		// TODO: POSIX
	}
}

int main(int argc, char* argv[])
{
	std::span<const char*const> args{argv, argv + argc};

	const char*    token{nullptr};
	constexpr auto run = [](const char* _token)
	{
		std::scoped_lock lock(app_mutex);

		B12::Bot bot(_token);

		hook_close_event();
		return (bot.run());
	};

	if (args.size() > 1)
		token = args[1];

	int ret = EXIT_FAILURE;

	try
	{
		ret = run(token);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Top-level exception caught : " << e.what() << std::endl;
		throw;
	}

	cleaned_up = true;
	app_cv.notify_all();
	return (ret);
}
