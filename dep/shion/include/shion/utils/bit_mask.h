#ifndef SHION_BITMASK_H_
#define SHION_BITMASK_H_

#include <utility>
#include <type_traits>

namespace shion {

template <typename T>
struct bit_mask;

template <typename T>
requires (std::is_enum_v<T>)
struct bit_mask<T> {
	bit_mask() = default;
	bit_mask(std::underlying_type_t<T> rhs) noexcept : value{rhs} {}
	bit_mask(T rhs) noexcept : value{static_cast<std::underlying_type_t<T>>(rhs)} {}

	std::underlying_type_t<T> value;

	operator T() const {
		return value;
	}

	friend bit_mask operator|(bit_mask lhs, bit_mask rhs) noexcept {
		return {static_cast<T>(lhs.value | rhs.value)};
	}

	friend bit_mask operator&(bit_mask lhs, bit_mask rhs) noexcept {
		return {static_cast<T>(lhs.value & rhs.value)};
	}

	friend bit_mask operator|(bit_mask lhs, T rhs) noexcept {
		return {static_cast<T>(lhs.value | static_cast<std::underlying_type_t<T>>(rhs))};
	}

	friend bit_mask operator&(bit_mask lhs, T rhs) noexcept {
		return {static_cast<T>(lhs.value & static_cast<std::underlying_type_t<T>>(rhs))};
	}

	bit_mask& operator=(T rhs) noexcept {
		value = static_cast<std::underlying_type_t<T>>(rhs);
		return *this;
	}

	explicit operator bool() const noexcept {
		return (value != 0);
	}
};

template <typename T>
bit_mask(T) -> bit_mask<T>;

template <typename T>
bit_mask<T> bit_if(T flag, bool condition) noexcept {
	return (condition ? bit_mask<T>{flag} : bit_mask<T>{});
}

}

#endif /* SHION_BITMASK_H_ */
