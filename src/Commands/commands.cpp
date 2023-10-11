#include <nonstd/expected.hpp>

#include "commands.h"
#include "command_table.h"

namespace B12 {

namespace command {
	namespace {
		template <typename T>
		void add_option(dpp::slashcommand &s, const command_option<T> &opt) {
			s.add_option(dpp::command_option{opt.api_type, std::string{opt.info.name}, std::string{opt.info.description}, !opt.is_optional});
		}

		template <typename T>
		void add_option(dpp::command_option &o, const command_option<T> &opt) {
			o.add_option(dpp::command_option{opt.api_type, std::string{opt.info.name}, std::string{opt.info.description}, !opt.is_optional});
		}

		template <typename... Ts>
		void add_options(auto &obj, const std::tuple<command_option<Ts>...> &options) {
			[]<size_t... Ns>(auto &obj_, const std::tuple<command_option<Ts>...> &options_, std::index_sequence<Ns...>) {
				(add_option(obj_, std::get<Ns>(options_)), ...);
			}(obj, options, std::index_sequence_for<Ts...>{});
		}

		template <typename T>
		dpp::slashcommand to_slashcommand(const command_info<T> &info, dpp::snowflake app_id) {
			static constexpr size_t n_args = command_info<T>::args_n;

			if constexpr (n_args > 0) {
				dpp::slashcommand s{std::string{info.command_name}, std::string{info.description}, app_id};
				add_options(s, info.options);
				return s;
			} else {
				return {std::string{info.command_name}, std::string{info.description}, app_id};
			}
		}

		template <typename T>
		dpp::command_option to_subcommand(const command_info<T> &info) {
			static constexpr size_t n_args = command_info<T>::args_n;

			dpp::command_option s;
			s.name = info.command_name;
			s.description = info.description;
			if constexpr (n_args > 0) {
				add_options(s, info.options);
			}
			return (s);
		}

		template <typename... Ts>
		dpp::command_option to_subcommand(const command_group<Ts...> &group) {
			dpp::command_option s;
			s.name = group.name;
			[]<size_t... Ns>(dpp::command_option &s_, const std::tuple<Ts...> &group_, std::index_sequence<Ns...>) {
				(s_.add_option(to_subcommand(std::get<Ns>(group_))), ...);
			}(s, group.subobjects, std::make_index_sequence<sizeof...(Ts)>{});
			return (s);
		}

		template <typename... Ts>
		dpp::slashcommand to_slashcommand(const command_group<Ts...> &group, dpp::snowflake app_id) {
			dpp::slashcommand s;

			s.name = group.name;
			s.application_id = app_id;
			[]<size_t... Ns>(dpp::slashcommand &s_, const std::tuple<Ts...> &group_, std::index_sequence<Ns...>) {
				(s_.add_option(to_subcommand(std::get<Ns>(group_))), ...);
			}(s, group.subobjects, std::make_index_sequence<sizeof...(Ts)>{});
			return (s);
		}
	}

	std::vector<dpp::slashcommand> get_api_commands(dpp::snowflake app_id) {
		return []<size_t... Ns>(dpp::snowflake app_id_, std::index_sequence<Ns...>) -> std::vector<dpp::slashcommand> {
			return {to_slashcommand(std::get<Ns>(COMMAND_TABLE.subcommands), app_id_)...};
		}(app_id, std::make_index_sequence<decltype(COMMAND_TABLE)::args_n>{});
	}
} /* namespace command */

} /* namespace B12 */
