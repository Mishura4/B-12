#ifndef B12_CACHED_RESOURCE_H_
#define B12_CACHED_RESOURCE_H_

#include <chrono>

#include <shion/traits/type_list.h>

#include "B12.h"
#include "API.h"
#include "APICache.h"

namespace B12
{
	template <typename Resource>
	constexpr inline auto resource_name = [](const json &) -> std::string_view
	{
		return {};
	};
	
	template <typename Resource>
	constexpr inline auto resource_id = [](const json &) -> std::optional<typename Resource::id_type>
	{
		return {std::nullopt};
	};
	
	template <shion::basic_string_literal Name, typename ID>
	struct APIResource
	{
		using id_type = ID;
		static inline constexpr auto NAME = Name;
		
		json resource;
		ID id = resource_id<APIResource>(resource).value_or(std::numeric_limits<ID>::max());
	};

	template <typename T>
	inline constexpr bool is_api_resource = false;
	
	template <string_literal Name, typename... Identifiers>
	inline constexpr bool is_api_resource<APIResource<Name, Identifiers...>> = true;
}

#endif
