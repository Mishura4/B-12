#pragma once

#include <optional>
#include <string_view>

#include <dpp/dpp.h>

#include "command_handler.h"
#include "Core/Bot.h"

namespace B12 {

namespace command {

	template <typename T>
	using optional_param = std::conditional_t<std::is_reference_v<T>, std::optional<std::reference_wrapper<std::remove_reference_t<T>>>, std::optional<T>>;

	struct response {
		enum class action_t {
			reply,
			edit,
			none
		};

		static dpp::message internal_error() {
			return {"Internal error (woopsie)"};
		}

		static dpp::message internal_error(std::string_view message) {
			return {fmt::format("Internal error: ", message)};
		}

		static dpp::message usage_error() {
			return {"Usage error"};
		}

		static dpp::message usage_error(std::string_view message) {
			return {fmt::format("Usage error: {}", message)};
		}

		static dpp::message success() {
			return {"thumbs up!"};
		}

		static dpp::message success(std::string_view message) {
			return {fmt::format("Yes! {}", message)};
		}

		static dpp::message aborted() {
			return {"Command aborted"};
		}

		template <typename... Args>
		static response reply(Args&&... args) {
			return {{std::forward<Args>(args)...}, action_t::reply};
		}

		template <typename... Args>
		static response edit(Args&&... args) {
			return {{std::forward<Args>(args)...}, action_t::edit};
		}

		template <typename... Args>
		static response none() {
			return {{}, action_t::none};
		}

		dpp::message message{success()};
		action_t action{action_t::reply};
	};

	dpp::coroutine<response> meow(dpp::interaction_create_t const &event);
	dpp::coroutine<response> study(dpp::interaction_create_t const &event);
	dpp::coroutine<response> server_settings_study(dpp::interaction_create_t const &event, optional_param<const dpp::role &> role, optional_param<const dpp::channel &> channel);
	dpp::coroutine<response> server_sticker_grab(dpp::interaction_create_t const &event, dpp::snowflake message_id, optional_param<const dpp::channel &> channel);
	dpp::coroutine<response> bigmoji(dpp::interaction_create_t const &event, const std::string &emoji);
	dpp::coroutine<response> ban(dpp::interaction_create_t const &event, resolved_user user, optional_param<std::chrono::seconds> duration, optional_param<std::string_view> reason);
	dpp::coroutine<response> pokemon_dex(dpp::interaction_create_t const &event, const std::string &name_or_number);
	dpp::coroutine<response> poll(
		dpp::interaction_create_t const &event,
		std::string_view title,
		std::string_view option1,
		std::string_view option2,
		optional_param<std::string_view> option3,
		optional_param<std::string_view> option4,
		optional_param<std::string_view> option5,
		optional_param<std::string_view> option6,
		optional_param<std::string_view> option7,
		optional_param<std::string_view> option8,
		optional_param<bool> create_thread,
		optional_param<dpp::role> ping_role
	);
} /* namespace command */

} /* namespace B12 */
