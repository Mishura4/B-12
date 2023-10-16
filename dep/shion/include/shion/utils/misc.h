#ifndef SHION_UTILS_MISC_H_
#define SHION_UTILS_MISC_H_

#include <span>
#include <ranges>
#include <utility>
#include <type_traits>

namespace shion {

constexpr auto to_bytes(auto&& data)
requires(
	std::ranges::contiguous_range<decltype(data)> &&
	std::ranges::sized_range<decltype(data)> &&
	std::is_standard_layout_v<std::ranges::range_value_t<decltype(data)>>
) {
	if constexpr (requires {std::as_writable_bytes(std::span{std::declval<decltype(data)>()});}) {
		return (std::as_writable_bytes(std::span{data}));
	} else {
		return (std::as_bytes(std::span{data}));
	}
}

template <typename From, typename To>
using mimic_const_t = std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, std::add_const_t<To>, To>;

template <typename From, typename To>
using mimic_volatile_t = std::conditional_t<std::is_volatile_v<std::remove_reference_t<From>>, std::add_volatile_t<To>, To>;

template <typename From, typename To>
using mimic_cv_t = mimic_const_t<From, mimic_volatile_t<From, To>>;

}

#endif /* SHION_UTILS_MISC_H_*/
