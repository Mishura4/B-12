#ifndef SHION_EXCEPTION_H_
#define SHION_EXCEPTION_H_

#include <exception>
#include <string>
#include <source_location>
#include <string_view>
#include <format>

#include <shion/utils/string_literal.h>

namespace shion {

class exception : public std::exception {
protected:
	template <typename T>
	requires (std::constructible_from<std::string, T>)
	exception(std::string_view n, T&& msg, std::source_location loc = std::source_location::current()) :
		name{n}, message{std::forward<T>(msg)}, location{loc} {}

public:
	exception(std::source_location loc = std::source_location::current()) :
		exception{"shion::exception", "<unknown>", loc} {}

	template <typename T>
	requires (std::constructible_from<std::string, T>)
	exception(T&& msg, std::source_location loc = std::source_location::current()) :
		exception{"shion::exception", std::forward<T>(msg), loc} {}

	const char *what() const override {
		return (message.c_str());
	}

	const std::source_location &where() const {
		return (location);
	}

	virtual std::string format_source(bool full = false) const {
		if (full) {
			return (
				std::format("{} at {}({}:{}) in {}:\n\t{}", name,
				location.file_name(), location.line(), location.column(), location.function_name(), what())
			);
		} else {
			return (std::format("{} in {}:\n\t{}", name, location.function_name(), what()));
		}
	}

	virtual std::string format() const {
		return (std::format("{}: {}", name, what()));
	}

private:
	std::string_view name;
	std::string message;
	std::source_location location;
};

#define SHION_DECLARE_EXCEPTION(name, parent) class name : public parent {\
protected:\
	template <typename T>\
	requires (std::constructible_from<std::string, T>)\
	name(std::string_view n, T&& msg, std::source_location loc = std::source_location::current()) :\
		parent{n, std::forward<T>(msg), loc} {}\
\
public:\
	template <typename T>\
	requires(std::constructible_from<std::string, T>)\
	name(T&& msg, std::source_location loc = std::source_location::current()) : \
		parent{"shion::" #name, std::forward<T>(msg), loc} {}\
};

SHION_DECLARE_EXCEPTION(logic_exception, shion::exception)
SHION_DECLARE_EXCEPTION(bad_argument_exception, shion::exception)
SHION_DECLARE_EXCEPTION(error_exception, shion::exception)

}

#endif
