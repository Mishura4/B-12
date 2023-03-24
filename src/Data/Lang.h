#ifndef B12_LANG_H_
#define B12_LANG_H_

#include "B12.h"

#include <fmt/format.h>
#include <shion/containers/registry.h>

template <>
struct fmt::formatter<dpp::role>
{
	constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
	{
		return (ctx.begin());
	}

	template <typename FormatContext>
	auto format(const dpp::role& role, FormatContext& ctx) const -> decltype(ctx.out())
	{
		return (fmt::format_to(ctx.out(), "{}", role.get_mention()));
	}
};

template <>
struct fmt::formatter<dpp::channel>
{
	constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
	{
		return (ctx.begin());
	}

	template <typename FormatContext>
	auto format(const dpp::channel& channel, FormatContext& ctx) const -> decltype(ctx.out())
	{
		return (fmt::format_to(ctx.out(), "{}", channel.get_mention()));
	}
};

template <>
struct fmt::formatter<dpp::permission>
{
	constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
	{
		return (ctx.begin());
	}

	template <typename FormatContext>
	auto format(const dpp::permission& permission, FormatContext& ctx) const -> decltype(ctx.out());
};

namespace B12
{
	namespace lang
	{
		namespace _
		{
			template <size_t N>
			consteval auto argPlaceholder()
			{
				return (string_literal("{}"));
			}

			template <size_t... Ns>
				requires (sizeof...(Ns) > 1)
			consteval auto genArgsPlaceholders(std::index_sequence<Ns...>)
			{
				return (shion::literal_concat<"", (argPlaceholder<Ns>(), ..., ", ")>());
			}

			template <size_t N>
			consteval auto genLangPlaceholder()
			{
				return (shion::literal_concat<
					"MISSING LANG TEXT (",
					genArgsPlaceholders(std::make_index_sequence<N>()),
					")">());
			}

			template <>
			consteval auto genLangPlaceholder<0>()
			{
				return ("MISSING LANG TEXT"_sl);
			}
		} // namespace _

		template <typename... T>
		struct fmt
		{
			::fmt::basic_format_string<char, const T&...> _fmt{"(missing lang text)"sv};

			auto format(const T&... args) const
			{
				return (::fmt::format(_fmt, args...));
			}
		};

		template <>
		struct fmt<>
		{
			std::basic_string_view<char> _fmt{"(missing lang text)"sv};

			operator std::basic_string_view<char>() const
			{
				return _fmt;
			}

			operator std::basic_string<char>() const
			{
				return std::basic_string<char>{_fmt};
			}
		};

		struct lang
		{
			fmt<uint64>                        DEV_WRONG_CHANNEL;
			fmt<dpp::channel, dpp::permission> PERMISSION_BOT_MISSING;
			fmt<uint64>                        PERMISSION_USER_MISSING;
			fmt<>                              ERROR_GENERIC_COMMAND_FAILURE;
			fmt<uint64>                        ERROR_CHANNEL_NOT_FOUND;
			fmt<uint64>                        ERROR_ROLE_NOT_FOUND;
			fmt<uint64>                        ERROR_ROLE_FETCH_FAILED;
			fmt<>                              ERROR_STUDY_BAD_SETTINGS;

			fmt<>             STUDY_CHANNEL_WELCOME;
			fmt<dpp::channel> COMMAND_STUDY_ADDED;
			fmt<>             COMMAND_STUDY_REMOVED;

			fmt<> PERMISSION_SEND_MESSAGES;
			fmt<> PERMISSION_CREATE_INVITE;
			fmt<> PERMISSION_USE_APPLICATION_COMMANDS;

			fmt<> BUTTON_LABEL_STUDY_TOGGLE;
		};
	} // namespace lang

	namespace lang
	{
		#define B12_CROSS_MARK "\xE2\x9D\x8C"
		// clang-format off
		inline constexpr lang LANG_EN_US{
			.DEV_WRONG_CHANNEL
			{
				"I cannot use commands in this channel - please go to "
				"<#{}> or use the non-dev bot!"sv
			},
			.PERMISSION_BOT_MISSING
			{
				B12_CROSS_MARK " I cannot do this in {}, I am missing the following permissions : **{}**"sv
			},
			.PERMISSION_USER_MISSING
			{
				"You don't have the permissions for this command : permissions {} are missing!"sv
			},
			.ERROR_GENERIC_COMMAND_FAILURE
			{
				"An error occured during the execution of the command."sv
			},
			.ERROR_CHANNEL_NOT_FOUND
			{
				"Error trying to find channel <#{}>.\nAre you sure it exists and that I have access to it?"sv
			},
			.ERROR_ROLE_NOT_FOUND{"Error trying to find role <@{}>.\nAre you sure it exists?"sv},
			.ERROR_ROLE_FETCH_FAILED{"An error occured while trying to fetch the role list."sv},
			.ERROR_STUDY_BAD_SETTINGS
			{
				B12_CROSS_MARK
				"This server's study role or channel was not found! Use `/settings server study` to enable this feature."sv
			},
			.STUDY_CHANNEL_WELCOME{
				"You are in study mode! This means your access to all the other channels is restricted.\nClick the button below to go back to normal."sv
			},
			.COMMAND_STUDY_ADDED{
				"You have entered study mode! Your access to all the other channels is now locked.\n"
				"The channel you are currently in is effectively a ghost. No new messages will appear, and commands will fail.\n"
				"Head to {} to disable study mode."
			},
			.COMMAND_STUDY_REMOVED{
				"You have been taken out of study mode and have regained access to the server."
			},
			.PERMISSION_SEND_MESSAGES{"Send Messages"sv},
			.PERMISSION_CREATE_INVITE{"Create Instant Invite"sv},
			.PERMISSION_USE_APPLICATION_COMMANDS{"Use Application Commands"sv}
		};
		// clang-format on

		inline constexpr shion::registry LANG{shion::field<"enUS">(LANG_EN_US)};

		inline constexpr shion::string_literal DEFAULT_LANG_CODE{"enUS"};
		inline constexpr const lang&           DEFAULT{LANG.get<DEFAULT_LANG_CODE>()};

		namespace _
		{
			inline constexpr shion::registry permission_lang = {
				shion::field<dpp::p_create_instant_invite>(&lang::lang::PERMISSION_CREATE_INVITE),
				shion::field<dpp::p_send_messages>(&lang::lang::PERMISSION_SEND_MESSAGES),
				shion::field<dpp::p_use_application_commands>(
					&lang::lang::PERMISSION_USE_APPLICATION_COMMANDS
				)
			};

			template <dpp::permissions N>
			void fillPermissionLang(std::vector<std::string_view>& vec, dpp::permission permission)
			{
				if (!permission.has(N))
					return;

				if constexpr (!permission_lang.has_key<N>())
					vec.emplace_back(magic_enum::enum_name(N));
				else
					vec.emplace_back(DEFAULT.*(permission_lang.get<N>()));
			};

			template <size_t... Ns>
			std::string populatePermissionString(dpp::permission permissions, std::index_sequence<Ns...>)
			{
				std::vector<std::string_view> vec;

				(fillPermissionLang<static_cast<dpp::permissions>(1uLL << Ns)>(vec, permissions), ...);
				std::stringstream ss;
				for (auto it = vec.cbegin(); it != vec.cend(); ++it)
				{
					if (it != vec.cbegin())
						ss << ", ";
					ss << *it;
				}
				return (ss.str());
			}
		}; // namespace _
	}    // namespace lang
}      // namespace B12

template <typename FormatContext>
auto fmt::formatter<dpp::permission>::format(
	const dpp::permission& permission,
	FormatContext&         ctx
)
const -> decltype(ctx.out())
{
	return (fmt::format_to(
		ctx.out(),
		"{}",
		lang::_::populatePermissionString(permission, std::make_index_sequence<sizeof(uint64) * 8>())
	));
}

#endif
