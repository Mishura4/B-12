#pragma once

#include <variant>
#include <tuple>
#include <span>
#include <algorithm>
#include <type_traits>
#include <string_view>
#include <optional>
#include <utility>

#include <dpp/dpp.h>

namespace B12 {

namespace command {

	struct resolved_user {
		dpp::user const &user;
		dpp::guild_member const* member;
		dpp::permission const *member_permissions;
	};

	template <typename T>
	constexpr inline std::false_type option_api_type;

	template <>
	constexpr inline auto option_api_type<dpp::role> = dpp::co_role;

	template <>
	constexpr inline auto option_api_type<dpp::user> = dpp::co_user;

	template <>
	constexpr inline auto option_api_type<dpp::guild_member> = dpp::co_user;

	template <>
	constexpr inline auto option_api_type<resolved_user> = dpp::co_user;

	template <>
	constexpr inline auto option_api_type<dpp::channel> = dpp::co_channel;

	template <>
	constexpr inline auto option_api_type<dpp::message> = dpp::co_string;

	template <std::convertible_to<std::string_view> Stringlike>
	constexpr inline auto option_api_type<Stringlike> = dpp::co_string;

	template <>
	constexpr inline auto option_api_type<bool> = dpp::co_boolean;

	template <typename Rep, typename Ratio>
	constexpr inline auto option_api_type<std::chrono::duration<Rep, Ratio>> = dpp::co_string;

	template <typename Rep>
	constexpr inline auto option_api_type<std::chrono::duration<Rep>> = dpp::co_string;

	template <typename T>
	constexpr inline bool is_optional_v = false;

	template <typename T>
	constexpr inline bool is_optional_v<std::optional<T>> = true;

	template <typename T>
	struct strip_param_s {
		using type = T;
	};

	template <typename T>
	struct strip_param_s<std::reference_wrapper<T>> {
		using type = T;
	};

	template <typename T>
	struct strip_param_s<std::optional<T>> {
		using type = typename strip_param_s<T>::type;
	};

	template <typename T>
	using strip_param = typename strip_param_s<T>::type;

	template <typename T>
	constexpr inline bool is_valid_command_option_v = !std::is_same_v<decltype(option_api_type<strip_param<std::remove_cvref_t<T>>>), std::false_type>;

	template <typename T>
	struct command_info;

	struct str_view {
		constexpr str_view() = default;
		constexpr str_view(const str_view &) = default;
		constexpr str_view(str_view &&) = default;

		template <size_t N>
		constexpr str_view(const char (&str_)[N]) : str_view{std::string_view{str_}} {}
		constexpr str_view(std::string_view sv) noexcept : str{sv.data()}, sz{sv.size()} {}

		constexpr str_view& operator=(const str_view&) = default;
		constexpr str_view& operator=(str_view&&) = default;

		constexpr const char *data() const noexcept { return str; }

		constexpr size_t size() const noexcept { return size(); }

		constexpr auto operator<=>(std::string_view sv) const noexcept {
			return std::string_view{*this} <=> sv;
		}

		constexpr auto operator==(std::string_view sv) const noexcept {
			return std::string_view{*this} == sv;
		}

		operator std::string_view() const noexcept { return std::string_view{str, sz}; }

		const char* str;
		size_t sz;
	};

	constexpr auto operator<=>(std::string_view lhs, str_view rhs) noexcept {
		return lhs <=> std::string_view{rhs};
	}

	struct command_option_info {
		str_view name;
		str_view description;
	};

	template <typename T>
	using command_data_type = std::remove_cvref_t<strip_param<T>>;

	template <typename T>
	struct command_option {
		using type = T;

		command_option_info info;
		dpp::command_option_type api_type = option_api_type<command_data_type<T>>;
		bool is_optional = is_optional_v<T>;
	};

	template <typename R, typename... Args>
	requires (sizeof...(Args) > 0 && (is_valid_command_option_v<Args> && ...))
	struct command_info<R (*)(dpp::interaction_create_t const &event, Args...)> {
		using handler_t = R (*)(dpp::interaction_create_t const &event, Args...);
		static constexpr size_t args_n = sizeof...(Args);

		consteval command_info(std::string_view name, string_view desc, handler_t fun, std::initializer_list<command_option_info> names) :
			command_name(name), description(desc), handler(fun)
		{
			[]<size_t... Ns>(command_info &self, const auto &names, std::index_sequence<Ns...>) {
				((std::get<Ns>(self.options).info = std::data(names)[Ns]), ...);
			}(*this, names, std::make_index_sequence<sizeof...(Args)>{});
		}

		std::string_view command_name;
		std::string_view description;
		handler_t handler;
		std::tuple<command_option<Args>...> options;
	};

	template <typename R>
	struct command_info<R (*)(dpp::interaction_create_t const &event)> {
		using handler_t = R (*)(dpp::interaction_create_t const &event);
		static constexpr inline size_t args_n = 0;

		std::string_view command_name;
		std::string_view description;
		handler_t handler;
	};

	template <typename R, typename... Args>
	command_info(std::string_view, std::string_view, R (*handler)(dpp::interaction_create_t const &, Args...)) -> command_info<decltype(handler)>;

	template <typename R, typename... Args>
	command_info(std::string_view, std::string_view, R (*handler)(dpp::interaction_create_t const &, Args...), std::initializer_list<command_option_info>) -> command_info<decltype(handler)>;

	template <typename... Subs>
	struct command_group {
		static constexpr inline size_t args_n = sizeof...(Subs);
		consteval command_group(std::string_view group_name, Subs&&... subs) :
			name{group_name}, subobjects{std::forward<Subs>(subs)...} {}

		std::string_view name;
		std::tuple<Subs...> subobjects;
	};

	template <typename... Commands>
	struct command_table {
		static constexpr inline size_t args_n = sizeof...(Commands);

		consteval command_table(Commands... commands) : subcommands{std::forward<Commands>(commands)...} {}

		std::tuple<Commands...> subcommands;
	};

	template <typename... Subs>
	command_group(std::string_view, Subs...) -> command_group<Subs...>;

	enum class command_error {
		internal_error,
		syntax_error
	};

	template <typename T>
	struct command_result {
		using data = std::variant<T, command_error>;

		data value;

		constexpr bool is_error() const noexcept {
			return std::holds_alternative<command_error>(value);
		}

		T &get() & {
			return std::get<T>(value);
		}

		T const &get() const & {
			return std::get<T>(value);
		}

		T &&get() && {
			return std::get<T>(value);
		}

		T *operator->() {
			return &get();
		}

		T const *operator->() const {
			return &get();
		}

		command_error get_error() const {
			return std::get<command_error>(value);
		}
	};

	template <>
	struct command_result<std::monostate> {
		using data = std::variant<std::monostate, command_error>;

		data value;

		constexpr bool is_error() const noexcept {
			return std::holds_alternative<command_error>(value);
		}

		command_error get_error() const {
			return std::get<command_error>(value);
		}
	};

	template <typename E = dpp::slashcommand_t, typename R = command_result<std::monostate>>
	struct command_handler;

	template <typename R>
	struct command_handler<dpp::slashcommand_t, R> {
		struct command_node {
			using parser_t = dpp::coroutine<command_result<R>> (*)(dpp::slashcommand_t const &event, std::span<dpp::command_data_option const>);

			std::string_view name{};
			parser_t parser{};
			std::vector<command_node> subcommands{};

			command_node const* search(string_view cmd) const;
		};

		template <const auto &CommandTable>
		static command_handler<dpp::slashcommand_t, R> from_command_table();

		command_node root{};

		dpp::coroutine<command_result<R>> operator()(dpp::slashcommand_t const &event) const;
	};

	template <typename R>
	using slashcommand_handler = command_handler<dpp::slashcommand_t, R>;

	template <typename R>
	auto command_handler<dpp::slashcommand_t, R>::command_node::search(std::string_view cmd) const -> command_handler<dpp::slashcommand_t, R>::command_node const * {
		auto it = std::lower_bound(std::begin(subcommands), std::end(subcommands), cmd, [](const command_node &node, string_view cmd) { return std::less<>{}(node.name, cmd); });

		if (it == std::end(subcommands) || it->name != cmd)
			return nullptr;
		return &(*it);
	}

	template <typename R>
	dpp::coroutine<command_result<R>> command_handler<dpp::slashcommand_t, R>::operator()(dpp::slashcommand_t const &event) const {
		auto const& data = std::get<dpp::command_interaction>(event.command.data);
		std::span<dpp::command_data_option const> opts = data.options;
		command_node const *command = root.search(data.name);

		if (command == nullptr)
			co_return {command_error::syntax_error};
		if (size_t num_options = data.options.size(); num_options > 0) {
			dpp::command_option_type last_type = data.options[0].type;

			if (last_type == dpp::co_sub_command_group) {
				if (opts[0].options.size() < 1)
					co_return {command_error::syntax_error};
				command = command->search(opts[0].name);

				if (!command)
					co_return {command_error::syntax_error};
				opts = opts[0].options;
				last_type = opts[0].type;
			}
			if (last_type == dpp::co_sub_command) {
				command = command->search(opts[0].name);

				if (!command)
					co_return {command_error::syntax_error};
				opts = opts[0].options;
			}
		}
		co_return {co_await command->parser(event, opts)};
	}

	namespace detail {
		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		struct store_param_s;

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (std::convertible_to<command_data_type<Type>, std::string_view>) // stringoid
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> &opts, const dpp::command_resolved &) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					return std::get<std::string>(opt.value);
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (std::same_as<command_data_type<Type>, dpp::user>)
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> opts, const dpp::command_resolved &resolved) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					dpp::snowflake id = std::get<dpp::snowflake>(opt.value);

					auto it = resolved.users.find(id);
					if constexpr (optional) {
						if (it == resolved.users.end()) [[unlikely]]
							return std::nullopt;
						return it->second;
					} else {
						if (it == resolved.users.end()) [[unlikely]]
							throw dpp::parse_exception("could not resolve option " + std::string{OptionName});
						return it->second;
					}
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (std::same_as<command_data_type<Type>, dpp::guild_member>)
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> opts, const dpp::command_resolved &resolved) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					dpp::snowflake id = std::get<dpp::snowflake>(opt.value);

					auto it = resolved.members.find(id);
					if constexpr (optional) {
						if (it == resolved.members.end()) [[unlikely]]
							return std::nullopt;
						return it->second;
					} else {
						if (it == resolved.members.end()) [[unlikely]]
							throw dpp::parse_exception("could not resolve option " + std::string{OptionName});
						return it->second;
					}
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (std::same_as<command_data_type<Type>, resolved_user>)
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> &opts, const dpp::command_resolved &resolved) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					dpp::snowflake id = std::get<dpp::snowflake>(opt.value);

					auto user_it = resolved.users.find(id);
					if constexpr (optional) {
						if (user_it == resolved.users.end()) [[unlikely]]
							return std::nullopt;
					} else {
						if (user_it == resolved.users.end()) [[unlikely]]
							throw dpp::parse_exception("could not resolve option " + std::string{OptionName});
					}

					if (auto member_it = resolved.members.find(id); member_it == resolved.members.end())
						return {user_it->second, nullptr, nullptr};
					else {
						auto perm_it = resolved.member_permissions.find(id);
						return {user_it->second, &(member_it->second), perm_it != resolved.member_permissions.end() ? &(perm_it->second) : nullptr};
					}
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (std::same_as<command_data_type<Type>, dpp::role>)
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> &opts, const dpp::command_resolved &resolved) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					dpp::snowflake id = std::get<dpp::snowflake>(opt.value);

					opts = {std::next(opts.begin()), opts.end()}; // relies on the options being in order
					auto it = resolved.roles.find(id);
					if constexpr (optional) {
						if (it == resolved.roles.end()) [[unlikely]]
							return std::nullopt;
						return it->second;
					} else {
						if (it == resolved.roles.end()) [[unlikely]]
							throw dpp::parse_exception("could not resolve option " + std::string{OptionName});
						return it->second;
					}
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (std::same_as<command_data_type<Type>, dpp::channel>)
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> &opts, const dpp::command_resolved &resolved) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					dpp::snowflake id = std::get<dpp::snowflake>(opt.value);

					opts = {std::next(opts.begin()), opts.end()}; // relies on the options being in order
					auto it = resolved.channels.find(id);
					if constexpr (optional) {
						if (it == resolved.channels.end()) [[unlikely]]
							return std::nullopt;
						return it->second;
					} else {
						if (it == resolved.channels.end()) [[unlikely]]
							throw dpp::parse_exception("could not resolve option " + std::string{OptionName});
						return it->second;
					}
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (std::same_as<command_data_type<Type>, bool>)
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> &opts, const dpp::command_resolved &) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					return std::get<bool>(opt.value);
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename T>
		struct duration_param_s {
			static constexpr inline bool value = false;
		};

		template <typename Rep, typename Period>
		struct duration_param_s<std::chrono::duration<Rep, Period>> {
			using rep = Rep;
			using period = Period;

			static constexpr inline bool value = true;
		};

		template <typename Rep, typename Period>
		struct duration_param_s<std::optional<std::chrono::duration<Rep, Period>>> {
			using rep = Rep;
			using period = Period;

			static constexpr inline bool value = true;
		};

		template <typename T>
		concept duration_param = duration_param_s<T>::value;

		template <typename Type, str_view OptionName, dpp::command_option_type OptionType>
		requires (duration_param<command_data_type<Type>>)
		struct store_param_s<Type, OptionName, OptionType> {
			Type operator()(std::span<dpp::command_data_option const> &opts, const dpp::command_resolved &) const {
				constexpr bool optional = is_optional_v<Type>;

				for (dpp::command_data_option const &opt : opts) {
					if (opt.type != OptionType || opt.name != OptionName)
						continue;

					return {};
				}
				if constexpr (optional) {
					return std::nullopt;
				} else {
					throw dpp::parse_exception("missing option " + std::string{OptionName});
				}
			}
		};

		template <typename T>
		inline constexpr bool is_command_info_v = false;

		template <typename T>
		inline constexpr bool is_command_info_v<command_info<T>> = true;

		template <typename T>
		concept command_info_concept = is_command_info_v<std::remove_cvref_t<T>>;

		template <const command_info_concept auto& Command, typename R>
		constexpr command_handler<dpp::slashcommand_t, R>::command_node make_command_node() requires (std::remove_cvref_t<decltype(Command)>::args_n == 0) {
			return {
				.name = Command.command_name,
				.parser = [](dpp::slashcommand_t const &event, std::span<dpp::command_data_option const>) -> dpp::coroutine<command_result<R>> {
					co_return command_result<R>{co_await std::invoke(Command.handler, event)};
				},
				.subcommands = {}
			};
		}

		template <const command_info_concept auto& Command, typename R>
		constexpr command_handler<dpp::slashcommand_t, R>::command_node make_command_node() requires (std::remove_cvref_t<decltype(Command)>::args_n > 0) {
			return {
				.name = Command.command_name,
				.parser = [](dpp::slashcommand_t const &event, std::span<dpp::command_data_option const> opts) -> dpp::coroutine<command_result<R>> {
					using command_options = std::remove_cvref_t<decltype(Command.options)>;

					constexpr static auto get_args = []<size_t... Ns>(dpp::slashcommand_t const &event, std::span<dpp::command_data_option const> opts_, std::index_sequence<Ns...>) -> auto {
						constexpr static auto get_param = []<size_t N>(dpp::slashcommand_t const &event, std::span<dpp::command_data_option const> opts_) {
							constexpr auto &param = std::get<N>(Command.options);

							return store_param_s<std::remove_cvref_t<decltype(param)>::type, param.info.name, param.api_type>{}(opts_, event.command.resolved);
						};
						return std::make_tuple(std::ref(event), get_param.template operator()<Ns>(event, opts_)...);
					};
					co_return {co_await std::apply(Command.handler, get_args(event, opts, std::make_index_sequence<std::tuple_size_v<command_options>>{}))};
				},
				.subcommands = {}
			};
		}

		template <const auto& CommandGroup, typename R>
		constexpr command_handler<dpp::slashcommand_t, R>::command_node make_command_node() {
			using command_node = command_handler<dpp::slashcommand_t, R>::command_node;

			auto ret = {
				.name = CommandGroup.name,
				.parser = nullptr,
				.subcommands = []<size_t... Ns>(std::index_sequence<Ns...>) -> std::vector<command_handler<dpp::slashcommand_t, R>::command_node> {
					return {make_command_node<std::get<Ns>(CommandGroup.subobjects)>...};
				}(std::make_index_sequence<std::tuple_size_v<decltype(CommandGroup.subobjects)>>{})
			};

			std::sort(std::begin(ret.subcommands), std::end(ret.subcommands), [](const command_node &lhs, const command_node &rhs) { return std::less<>{}(lhs.name, rhs.name); });
			return ret;
		}

		template <const auto &CommandTable, typename R>
		command_handler<dpp::slashcommand_t, R>::command_node make_root() {
			using command_node = command_handler<dpp::slashcommand_t, R>::command_node;

			auto ret = []<size_t... Ns>(std::index_sequence<Ns...>) -> command_handler<dpp::slashcommand_t, R>::command_node {
				return {{}, nullptr, {make_command_node<std::get<Ns>(CommandTable.subcommands), R>()...}};
			}(std::make_index_sequence<CommandTable.args_n>{});

			std::sort(std::begin(ret.subcommands), std::end(ret.subcommands), [](const command_node &lhs, const command_node &rhs) { return std::less<>{}(lhs.name, rhs.name); });
			return ret;
		}

	}

	template <typename R>
	template <const auto &CommandTable>
	static command_handler<dpp::slashcommand_t, R> command_handler<dpp::slashcommand_t, R>::from_command_table() {
		return {detail::make_root<CommandTable, R>()};
	}
}

}
