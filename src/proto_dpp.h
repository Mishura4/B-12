// Random pieces to maybe add to DPP later

#include <dpp/coro/coroutine.h>
#include <dpp/coro/async.h>
#include <dpp/restresults.h>

#include <variant>
#include <utility>
#include <string>

namespace dpp {

class api_error_exception : public dpp::exception {
protected:
	dpp::error_info err;

public:
	api_error_exception() = default;
	api_error_exception(const dpp::error_info& error) : exception{error.human_readable}, err{error}
	{}
	api_error_exception(dpp::error_info&& error) : exception{error.human_readable}, err{std::move(error)}
	{}

	const dpp::error_info &get_error() const {
		return err;
	}
};

template <typename T>
dpp::coroutine<T> or_throw(dpp::async<dpp::confirmation_callback_t>&& a) {
	dpp::confirmation_callback_t &&result = std::move(co_await a);

	if (!std::holds_alternative<T>(result.value)) {
		if (result.is_error()) [[likely]] {
			throw api_error_exception{result.get_error()};
		}
		else {
			dpp::error_info err;

			err.message = "wrong type supplied to or_throw";
			err.human_readable = err.message;
			throw api_error_exception{std::move(err)};
		}
	}
	co_return std::get<T>(std::move(result).value);
}

template <typename T>
dpp::coroutine<T> or_throw(const dpp::async<dpp::confirmation_callback_t>& a) {
	dpp::confirmation_callback_t result = co_await a;

	if (!std::holds_alternative<T>(result.value)) {
		if (result.is_error()) [[likely]] {
			throw api_error_exception{result.get_error()};
		}
		else {
			dpp::error_info err;

			err.message = "wrong type supplied to or_throw";
			err.human_readable = err.message;
			throw api_error_exception{std::move(err)};
		}
	}
	co_return std::get<T>(result.value);
}

dpp::coroutine<void> or_throw(dpp::async<dpp::confirmation_callback_t> &a) {
	decltype(auto) result = co_await a;

	if (result.is_error()) {
		throw api_error_exception{result.get_error()};
	} else {
		co_return;
	}
}

dpp::coroutine<void> or_throw(dpp::async<dpp::confirmation_callback_t> &&a) {
	decltype(auto) result = co_await a;

	if (result.is_error()) {
		throw api_error_exception{result.get_error()};
	} else {
		co_return;
	}
}

namespace utility {

std::string get_member_display_name(const dpp::user &user, const dpp::guild_member &member) {
	std::string member_nick = member.get_nickname();

	if (!member_nick.empty())
		return member_nick;
	return user.global_name;
}

std::string get_member_avatar_url(const dpp::user &user, const dpp::guild_member &member) {
	std::string member_av = member.get_avatar_url();

	if (member_av.empty())
		return user.get_avatar_url();
	return member_av;
}

}

}
