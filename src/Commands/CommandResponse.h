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

		struct InternalError {};

		struct APIError {};

		struct ConfigError {};

		struct UsageError {};

		struct Confirm
		{
			std::function<button_callback> action;
			seconds                        lifetime{0};
		};

		using PossibleTypes = shion::type_list<
			Success,
			SuccessEdit,
			SuccessAction,
			InternalError,
			APIError,
			ConfigError,
			UsageError,
			Confirm
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
			Content()               = default;
			Content(const Content&) = default;
			Content(Content&&)      = default;

			Content &operator=(const Content&) = default;
			Content &operator=(Content&&)      = default;

			template <typename... Ts>
				requires (sizeof...(Ts) > 0)
			Content(fmt::format_string<Ts...> fmt, Ts&&... args) :
				message{fmt::format(fmt, std::forward<Ts>(args)...)} { }

			Content(dpp::message msg) :
				message{msg} { }

			Content(std::string msg) :
				Content(dpp::message{msg}) { }

			Content(
				dpp::message              message_,
				std::vector<std::string>  warnings_,
				std::vector<dpp::message> other_messages_ = {}
			) :
				message{message_},
				warnings{warnings_},
				other_messages{other_messages_} { }

			dpp::message              message{};
			std::vector<std::string>  warnings{};
			std::vector<dpp::message> other_messages{};
		};

		CommandResponse()                       = default;
		CommandResponse(const CommandResponse&) = default;
		CommandResponse(CommandResponse&&)      = default;

		CommandResponse(Data response_type) :
			type{response_type} { }
		
		CommandResponse(
			Data                            response_type_,
			Content                         content_,
			dpp::command_completion_event_t callback_ = dpp::utility::log_error()
		) :
			type{response_type_},
			content{content_},
			callback{callback_} { }

		CommandResponse &operator=(const CommandResponse&) = default;
		CommandResponse &operator=(CommandResponse&&)      = default;

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

		struct EmptyData { };

		struct ConfirmData { };

		Data                                          type{InternalError{}};
		Content                                       content{};
		std::optional<std::function<button_callback>> confirm_action{};
		dpp::command_completion_event_t               callback = dpp::utility::log_error();
	};
}

#endif
