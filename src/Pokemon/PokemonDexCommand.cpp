#include "B12.h"

#include "Commands/Command.h"
#include "Commands/CommandHandler.h"
#include "Core/Bot.h"

#include "PokemonAPI.h"

#include "API/APICache.h"

#include <regex>

#include "../Commands/CommandResponse.h"

using namespace B12;

template <>
CommandResponse CommandHandler::command<"pokemon dex">(
	command_option_view options
)
{
	static constexpr auto error = "Please provide a valid pokemon name or national number."sv;
	std::string api_param;
	
	for (const auto &opt : options)
	{
		if (opt.type == dpp::co_string && opt.name == "name-or-number")
		{
			auto truncated = std::get<std::string>(opt.value) | std::views::drop_while(is_whitespace);
			api_param = {truncated.begin(), truncated.end()};
		}
	}
	if (api_param.empty() || !std::regex_match(api_param.begin(), api_param.end(), std::regex{"([0-9a-zA-Z\xC3\x80-\xC3\x96\xC3\x98-\xC3\xB6\xC3\xB8-\xC9\x8F-])+", std::regex_constants::extended}))
		return {CommandResponse::UsageError{}, {std::string{error}}};

	std::string url = POKE_API.pokemon_endpoint.url(api_param);
	CommandResponse response{CommandResponse::InternalError{}};
	
	_source.sendThink(false);
	_source.thinking_executor.wait();
	auto entry = Bot::pokemon_cache->pokemon_cache.request(_getInteraction().event.from->creator, std::stoull(api_param));

	std::shared_ptr<PokemonResource> wee = entry.get();

	if (!wee)
		return {response};
	
	auto species_entry = Bot::pokemon_cache->pokemon_species_cache.request(_getInteraction().event.from->creator, std::stoull(api_param));
	auto species = species_entry.get();
	
	if (!species)
		return {response};
	
	dpp::message foo;
	dpp::embed embed;
	const nlohmann::json &json = wee->resource;
	auto name = std::string{json["name"]};
	auto sprites = json["sprites"];

	to_upper(name[0]);
	embed.set_image(json["sprites"]["other"]["official-artwork"]["front_default"]);
	embed.set_url(fmt::format("https://bulbapedia.bulbagarden.net/wiki/{}_(Pok%C3%A9mon)", name));
	embed.set_title(name);
	std::string generation = species->resource["generation"]["name"];
	to_upper(generation[0]);
	for (char &c : generation | std::views::drop_while([](char c){return (c != '-');}) | std::views::drop(1))
		to_upper(c);
	embed.set_footer(dpp::embed_footer{generation});
	if (const auto &bwsprite = sprites["versions"]["generation-v"]["black-white"]["animated"]["front_default"]; !bwsprite.is_null())
		embed.set_thumbnail(bwsprite);
	else
		embed.set_thumbnail(sprites["front_default"]);
	for (const auto &s : json["stats"])
	{
		std::string stat = s["stat"]["name"];
		embed.add_field(stat, fmt::format("{}", s["base_stat"], true));
	}
	foo.embeds.push_back(std::move(embed));
	response = {CommandResponse::Success{}, {std::move(foo)}};
	return {response};
}

