#include "Core/Bot.h"

#include "../Data/Lang.h"

#include "commands.h"

#include <shion/media/image/image.h>

using namespace B12;

namespace
{
	void attachment_add(dpp::message& msg, const dpp::sticker& sticker, const std::string& content)
	{
		if (std::string format = dpp::utility::file_extension(sticker.format_type); !format.empty())
		{
			msg.add_file(
				fmt::format("{}.{}", sticker.name, format),
				content,
				fmt::format("image/{}", format)
			);
		}
		else
			msg.add_file(sticker.name, content);
	}
}

using namespace B12::command;

dpp::coroutine<response> B12::command::server_sticker_grab(dpp::interaction_create_t const &event, dpp::snowflake message_id, optional_param<const dpp::channel &> channel)
{
	dpp::confirmation_callback_t result;
	dpp::cluster *cluster = event.from->creator;

	auto thinking = event.co_thinking();

	result = co_await B12::Bot::bot().co_message_get(message_id, channel.has_value() ? channel->get().id : event.command.channel_id);
	if (result.is_error())
	{
		co_await thinking;
		co_return command::response::edit(fmt::format(
			"{} Error: could not retrieve message\n(Common issues: wrong message or channel ID, or I do not have view permissions)",
			lang::ERROR_EMOJI
		));
	}

	dpp::message message = std::get<dpp::message>(result.value);
	if (message.stickers.empty())
	{
		co_await thinking;
		co_return command::response::edit(fmt::format(
			"{} Error: message does not have stickers!",
			lang::ERROR_EMOJI
		));
	}

	dpp::message ret;

	for (const dpp::sticker& s : message.stickers)
	{
		decltype(auto) download_result = co_await cluster->co_request(s.get_url(), dpp::m_get);

		if (download_result.status >= 300)
		{
			ret.content.append(fmt::format("{} Could not download image data", lang::ERROR_EMOJI));
			continue;
		}
		std::string sticker_data = std::move(download_result.body);
		result = co_await cluster->co_nitro_sticker_get(s.id);

		if (result.is_error())
		{
			ret.content.append(fmt::format(
				"{} Could not request sticker details, was it deleted? Adding image as attachment for manual addition.",
				lang::ERROR_EMOJI
			));
			attachment_add(ret, s, sticker_data);
			continue;
		}

		const dpp::sticker &grabbed_sticker = result.get<dpp::sticker>();

		using shion::image;

		dpp::sticker to_add;
		shion::image img = image::png_info(shion::to_bytes(sticker_data));
		bool resized = false;

		if (img.height > 320 || img.width > 320) {
			img = image::from_png(shion::to_bytes(sticker_data));
			resized = img.truncate(320, 320);
			std::size_t size = img.write_png(shion::to_bytes(sticker_data));
			sticker_data.resize(size);
		}
		to_add.filecontent  = sticker_data;
		to_add.guild_id     = event.command.guild_id;
		to_add.sticker_user = cluster->me;
		to_add.name         = grabbed_sticker.name;
		to_add.description  = grabbed_sticker.description;
		to_add.filename     = to_add.name + ".png";
		to_add.tags         = grabbed_sticker.tags;
		to_add.type         = dpp::st_guild;
		std::ranges::replace(to_add.filename, ' ', '_');

		result = co_await cluster->co_guild_sticker_create(to_add);
		if (result.is_error())
		{
			ret.content.append(fmt::format(
				"{} Failed to add sticker `{}`: \"{}\"\nAdding image as attachment for manual addition",
				lang::ERROR_EMOJI,
				grabbed_sticker.name,
				result.get_error().message
			));
			attachment_add(ret, s, to_add.filecontent);
		}
		else
		{
			ret.content.append(fmt::format(
				"{} Added sticker \"{}\"!",
				lang::SUCCESS_EMOJI,
				grabbed_sticker.name
			));
		}
		if (resized)
			ret.content.append(" (note : the image was resized due to being too large)");
	}
	co_return command::response::edit(ret);
}
