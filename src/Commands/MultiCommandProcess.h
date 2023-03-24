#ifndef B12_MULTI_COMMAND_PROCESS_
#define B12_MULTI_COMMAND_PROCESS_

#include "B12.h"

#include "Command.h"

namespace B12
{
	struct MultiCommandProcess
	{
		using timestamp = std::chrono::time_point<std::chrono::steady_clock>;
		using callback_t = std::function<void(const dpp::confirmation_callback_t&)>;

		explicit MultiCommandProcess(dpp::message master_);
		MultiCommandProcess(const MultiCommandProcess&) = delete;
		MultiCommandProcess(MultiCommandProcess&& rval) noexcept;

		MultiCommandProcess &operator=(const MultiCommandProcess& other) = delete;
		MultiCommandProcess &operator=(MultiCommandProcess&& rhs) noexcept;

		void callbackFn(const dpp::confirmation_callback_t& event);

		virtual ~MultiCommandProcess() = default;

		virtual bool   step(const dpp::message& msg) = 0;
		virtual uint32 nbSteps() const = 0;

		std::unique_ptr<dpp::message> master{nullptr};
		timestamp                     expiration{};
		uint32_t                      step_index{};
		bool                          processing{false};
		callback_t                    callback{};
	};
} // namespace B12

#endif
