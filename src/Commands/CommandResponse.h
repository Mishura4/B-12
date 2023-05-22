#ifndef B12_COMMAND_RESPONSE_H_
#define B12_COMMAND_RESPONSE_H_

#include <fmt/format.h>

#include "B12.h"

namespace B12
{
	struct CommandResponse
	{
		struct Success {};

		struct SuccessEdit {};

		struct SuccessAction {};

		struct Thinking
		{
			bool ephemeral{false};
		};

		struct InternalError {};

		struct APIError {};

		struct ConfigError {};

		struct UsageError {};

		struct Confirm
		{
			std::function<button_callback> action;
			seconds                        lifetime{0};
		};

		struct None {};

		using PossibleTypes = shion::type_list<
			Success,
			SuccessEdit,
			SuccessAction,
			Thinking,
			InternalError,
			APIError,
			ConfigError,
			UsageError,
			Confirm,
			None
		>;

		using Data = PossibleTypes::as_variant;

		template <typename T>
		static constexpr bool is_valid_data_type = PossibleTypes::has_type<T>;

		template <typename T>
			requires (is_valid_data_type<T>)
		consteval static auto typeId()
		{
			return (PossibleTypes::type_index<T>);
		};

		struct Content
		{
			dpp::message              message{};
			std::vector<std::string>  warnings{};
			std::vector<dpp::message> other_messages{};
		};

		template <typename T>
			requires (is_valid_data_type<T>)
		bool has() const
		{
			return (std::holds_alternative<T>(type));
		}

		template <typename... Ts>
			requires (sizeof...(Ts) > 0)
		CommandResponse &set(fmt::format_string<Ts...> fmt, Ts&&... args)
		{
			return (operator=(fmt::format(fmt, std::forward<Ts>(args)...)));
		}

		CommandResponse &set(std::string str)
		{
			content.message.content = std::move(str);
			return (*this);
		}

		template <typename... Ts>
			requires (sizeof...(Ts) > 0)
		CommandResponse &append(fmt::format_string<Ts...> fmt, Ts&&... args)
		{
			return (append(fmt::format(fmt, std::forward<Ts>(args)...)));
		}

		CommandResponse &append(std::string_view str)
		{
			content.message.content = fmt::format("{}{}", content.message.content, str);
			return (*this);
		}

		void format();

		Data                                          type{InternalError{}};
		Content                                       content{};
		dpp::command_completion_event_t               callback{dpp::utility::log_error()};
	};
}

#endif
