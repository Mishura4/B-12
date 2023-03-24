#pragma once

#include "B12.h"

#include <concepts>
#include <span>
#include <type_traits>
#include <utility>

namespace B12
{
	template <typename T>
	concept dpp_command_option_type = std::same_as<T, dpp::command_option_type>;

	template <shion::string_literal Name, size_t NumTypes>
	struct CommandOption
	{
		static constexpr auto NAME = Name;

		constexpr CommandOption(std::string_view description_, bool required_, auto&&... types_)
			requires (dpp_command_option_type<std::remove_cvref_t<decltype(types_)>> && ...) :
			description{description_},
			possible_types{types_...},
			required(required_) {}

		template <decltype(NAME)key>
			requires(key == NAME)
		constexpr const auto &get_option() const
		{
			return (*this);
		}

		template <decltype(NAME)key>
			requires(key == NAME)
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
		struct is_command_option_s
		{
			static constexpr bool value = false;
		};

		template <shion::string_literal Name, size_t NumTypes>
		struct is_command_option_s<CommandOption<Name, NumTypes>>
		{
			static constexpr bool value = true;
		};

		template <typename T>
		// because visual studio struggles with name resolution of template pack parameters
		static constexpr auto command_option_name = T::NAME;
	}

	template <typename T>
	concept command_option_type = _::is_command_option_s<T>::value;

	// minimal permission for the bot to use a command for the purpose of command default constructors : send messages
	constexpr static size_t COMMAND_DEFAULT_BOT_PERMISSIONS = dpp::permissions::p_send_messages;

	// minimal permission for a user to use a command for the purpose of command default constructors : administrator
	constexpr static size_t COMMAND_DEFAULT_USER_PERMISSIONS = (1uLL << 31);

	template <typename T>
	concept command_handler_type = std::invocable<
		T,
		const dpp::interaction_create_t&,
		const dpp::interaction&,
		std::span<const dpp::command_data_option>>;

	using command_option_view = std::span<const dpp::command_data_option>;

	using command_handler = void (*)(
		const dpp::interaction_create_t&,
		const dpp::interaction&,
		std::span<const dpp::command_data_option>
	);

	template <
		shion::string_literal Name,
		command_handler_type auto Handler,
		command_option_type... Options>
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
		using option_names = shion::value_list<_::command_option_name<Options>...>;

		static constexpr auto name           = Name;
		static constexpr auto handler        = Handler;
		static constexpr auto num_parameters = sizeof...(Options);

		std::string_view description;
		uint64_t         user_permissions{dpp::permissions::p_use_application_commands};
		uint64_t         bot_permissions{dpp::permissions::p_send_messages};
	};
} // namespace B12
