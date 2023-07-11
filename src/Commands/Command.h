#pragma once

#include "B12.h"

#include <concepts>
#include <span>
#include <type_traits>
#include <utility>

#include "CommandResponse.h"

namespace B12
{
	class CommandHandler;

	template <shion::basic_string_literal Name, size_t NumTypes>
	struct CommandOption
	{
		static constexpr auto NAME = Name;

		constexpr CommandOption(std::string_view description_, bool required_, auto... types_)
			requires (std::same_as<dpp::command_option_type, decltype(types_)> && ...) :
			description{description_},
			possible_types{types_...},
			required(required_) {}

		template <shion::basic_string_literal key>
			requires(NAME.strict_equals(key))
		constexpr const auto &get_option() const
		{
			return (*this);
		}

		template <shion::basic_string_literal key>
			requires(NAME.strict_equals(key))
		constexpr auto &get_option()
		{
			return (*this);
		}

		std::string_view         description;
		dpp::command_option_type possible_types[NumTypes];
		bool                     required;
	};

	namespace _
	{

		template <typename T>
		inline constexpr bool is_command_option = false;

		template <shion::basic_string_literal Name, size_t NumTypes>
		inline constexpr bool is_command_option<CommandOption<Name, NumTypes>> = true;

		template <typename T>
		// because visual studio struggles with name resolution of template pack parameters
		struct command_option_name_helper
		{
			static inline constexpr auto NAME = T::NAME;
		};
	}

	template <typename T>
	concept command_option_type = _::is_command_option<T>;

	// minimal permission for the bot to use a command for the purpose of command default constructors : send messages
	constexpr static size_t COMMAND_DEFAULT_BOT_PERMISSIONS = dpp::permissions::p_send_messages;

	// minimal permission for a user to use a command for the purpose of command default constructors : administrator
	constexpr static size_t COMMAND_DEFAULT_USER_PERMISSIONS = (1uLL << 31);

	template <typename T, typename Ret, typename... Args>
	concept invocable_r = std::invocable<T, Args...> &&
		std::same_as<Ret, std::invoke_result_t<T, Args...>>;

	template <typename T>
	concept command_handler_type = std::is_null_pointer_v<T> || invocable_r<
		T,
		CommandResponse,
		CommandHandler*,
		std::span<const dpp::command_data_option>>;

	using command_option_view = std::span<const dpp::command_data_option>;

	using command_fun = CommandResponse (CommandHandler::*)(
		command_option_view
	);

	template <
		shion::basic_string_literal Name,
		auto Handler,
		typename... Options>
	requires (command_handler_type<decltype(Handler)> && (true && ... && _::is_command_option<Options>))
	struct Command : Options...
	{
		using Options::get_option...;

		explicit constexpr Command(std::string_view description_, Options&&... options) :
			Options(std::forward<Options>(options))...,
			description(description_) {}

		constexpr Command(
			std::string_view description_,
			uint64_t         user_permissions_,
			uint64_t         bot_permissions_,
			Options&&...     options
		) :
			Options(std::forward<Options>(options))...,
			description(description_),
			user_permissions{user_permissions_},
			bot_permissions{bot_permissions_} {}

		using option_list = shion::type_list<Options...>;
		using option_names = shion::value_list<_::command_option_name_helper<Options>::NAME...>;

		static constexpr auto name           = Name;
		static constexpr auto handler        = Handler;
		static constexpr auto num_parameters = sizeof...(Options);

		std::string_view description;
		uint64_t         user_permissions{dpp::permissions::p_use_application_commands};
		uint64_t         bot_permissions{dpp::permissions::p_send_messages};
	};

	inline constexpr auto button_command_prefix = "command:"sv;
	inline constexpr auto button_action_prefix = "action:"sv;
	inline constexpr auto button_abort = "abort"sv;
	
} // namespace B12