#ifndef B12_POKEMON_API_H_
#define B12_POKEMON_API_H_

#include "B12.h"

#include "../API/APICache.h"

#include "API/API.h"

namespace B12
{
	using PokemonResource = APIResource<"pokemon", uint16>;
	using PokemonSpeciesResource = APIResource<"pokemon-species", uint16>;

	template <>
	constexpr inline auto resource_name<PokemonResource> = [](const json &resource) -> std::string_view
	{
	  return (resource["name"].get<std::string_view>());
	};

	template <>
	constexpr inline auto resource_name<PokemonSpeciesResource> = [](const json &resource) -> std::string_view
	{
	  return (resource["name"].get<std::string_view>());
	};

	struct PokeAPI : API<"pokeapi", "https://pokeapi.co/api/v2/">
	{
		Endpoint<PokemonResource, true> pokemon_endpoint{};
		Endpoint<PokemonSpeciesResource, true> pokemon_species_endpoint{};
	};

	inline constexpr PokeAPI POKE_API{};

	template <auto V>
	  using cache_for = ResourceCache<std::remove_cvref_t<decltype(V)>>;

	struct PokeAPICache : APICache<PokeAPI>
	{
		PokeAPICache(dpp::cluster *cluster) :
			pokemon_cache{cluster},
			pokemon_species_cache{cluster}
		{
			
		}
		
		cache_for<POKE_API.pokemon_endpoint> pokemon_cache;
		cache_for<POKE_API.pokemon_species_endpoint> pokemon_species_cache;
	};
}

#endif /* B12_POKEMON_API_H_ */ 
