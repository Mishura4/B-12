#include "Core/Bot.h"

#include "Command.h"
#include "CommandResponse.h"
#include "../Data/Lang.h"

#include <CImg.h>
#include <png.h>
#include <nonstd/expected.hpp>

#ifdef None // X11 included from CImg
#undef None
#endif

#include "CommandHandler.h"

#ifdef Success
#undef Success
#endif

using namespace B12;

namespace
{
	constexpr auto sticker_formats = std::to_array<std::pair<dpp::sticker_format, std::string_view>>(
		{
			{dpp::sf_png, "png"sv},
			{dpp::sf_apng, "png"sv},
			{dpp::sf_gif, "gif"sv}
		}
	);

	constexpr auto        find_sticker_format = [](
		dpp::sticker_format format
	) constexpr -> std::optional<std::string_view>
	{
		constexpr auto proj = [](const std::iter_value_t<decltype(sticker_formats)>& n)
		{
			return (n.first);
		};

		if (auto it = std::ranges::find(sticker_formats, format, proj); it != std::end(sticker_formats))
			return {it->second};
		return {std::nullopt};
	};

	auto sticker_get(dpp::cluster *cluster, dpp::snowflake id) -> dpp::co_task<std::optional<dpp::sticker>>
	{
		auto confirm = co_await cluster->co_nitro_sticker_get(id);

		if (confirm.is_error())
			co_return {std::nullopt};
		else
			co_return (confirm.get<dpp::sticker>());
	}

	auto sticker_content_get(dpp::cluster *cluster, const dpp::sticker& sticker) -> dpp::co_task<std::optional<std::string>>
	{
		auto               content = std::optional<std::string>{std::nullopt};
		auto               request =
			[](dpp::cluster* bot, const std::string& url, const dpp::http_completion_event& callback)
		{
			bot->request(url, dpp::m_get, callback);
		};
		auto content_retriever =
			AsyncExecutor<dpp::http_request_completion_t, dpp::http_request_completion_t>{
				[&](const dpp::http_request_completion_t& result)
				{
					if (result.status != 200)
						return;
					content = result.body;
				}
			};

		auto awaitable = cluster->co_request(sticker.get_url(), dpp::m_get);
		auto result = co_await awaitable;

		if (result.status >= 300)
			co_return {std::nullopt};
		else
			co_return (result.body);
	}

	auto sticker_add(dpp::cluster *cluster, dpp::sticker& sticker) -> dpp::co_task<nonstd::expected<dpp::sticker, dpp::error_info>>
	{
		auto confirm = co_await cluster->co_guild_sticker_create(sticker);

		if (confirm.is_error())
			co_return nonstd::make_unexpected(confirm.get_error());
		else
			co_return {confirm.get<dpp::sticker>()};
	}

	void attachment_add(dpp::message& msg, const dpp::sticker& sticker, const std::string& content)
	{
		if (std::optional format = find_sticker_format(sticker.format_type); format.has_value())
		{
			msg.add_file(
				fmt::format("{}.{}", sticker.name, *format),
				content,
				fmt::format("image/{}", *format)
			);
		}
		else
			msg.add_file(sticker.name, content);
	}

	enum class ImageProcessResult
	{
		ERROR   = 0,
		RESIZED = 1,
		NOOP    = 2
	};

	ImageProcessResult image_process(std::optional<std::string>& content)
	{
		using enum ImageProcessResult;

		constexpr auto MAX_SIZE   = 320;
		constexpr auto MAX_SIZE_F = static_cast<float>(MAX_SIZE);
		//auto           file = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		//auto           info = png_create_info_struct(file);

		//setjmp(png_jmpbuf(file));
		png_image image{};

		image.version = PNG_IMAGE_VERSION;
		image.format  = PNG_FORMAT_RGBA;
		png_image_begin_read_from_memory(&image, content->data(), content->size());
		if (image.width <= MAX_SIZE && image.height <= MAX_SIZE)
		{
			png_image_finish_read(&image, nullptr, nullptr, 0, nullptr);
			return (NOOP);
		}
		auto og_size = PNG_IMAGE_SIZE(image);
		auto buffer  = std::make_unique<float[]>(og_size / sizeof(float));
		if (png_image_finish_read(&image, 0, buffer.get(), 0, nullptr) == 0)
			return (ERROR);
		auto og_cimg = cimg_library::CImg<float>{buffer.get(), image.width, image.height, 1, 1, true};
		uint32_t w{MAX_SIZE};
		uint32_t h{MAX_SIZE};

		if (image.width != image.height)
		{
			float wf = static_cast<float>(image.width);
			float hf = static_cast<float>(image.height);

			if (image.width > image.height)
			{
				hf = (hf / wf) * MAX_SIZE_F;
				wf = MAX_SIZE_F;
			}
			else if (image.height > image.width)
			{
				wf = (hf / wf) * MAX_SIZE_F;
				hf = MAX_SIZE_F;
			}
			w = static_cast<uint32_t>(floorf(wf));
			h = static_cast<uint32_t>(floorf(hf));
		}
		image.width  = w;
		image.height = h;
		std::string      out;
		png_alloc_size_t out_size = PNG_IMAGE_SIZE(image);

		auto resized = og_cimg.get_resize(w, h, -100. - 100, 6);
		out.resize(out_size);
		if (!png_image_write_to_memory(
			&image,
			out.data(),
			&out_size,
			0,
			resized.data(),
			w * sizeof(*resized.data()),
			nullptr
		))
			return (ERROR);
		out.resize(out_size);
		content = std::move(out);
		return (RESIZED);
	}
}

namespace {
	auto sticker_grab_task(dpp::cluster *cluster, dpp::interaction_create_t event, dpp::snowflake message_id, dpp::snowflake channel_id) -> dpp::co_task<void>
	{
		dpp::confirmation_callback_t confirm = co_await dpp::awaitable(event, &dpp::interaction_create_t::thinking, false);

		if (confirm.is_error())
			co_return;

		confirm = co_await dpp::awaitable{&event, &dpp::interaction_create_t::get_original_response};

		if (confirm.is_error())
			co_return;

		dpp::message original_response = std::move(confirm.get<dpp::message>());

		confirm = co_await B12::Bot::bot().co_message_get(message_id, channel_id);
		if (confirm.is_error())
		{
			event.edit_original_response(original_response.set_content(fmt::format(
				"{} Error: could not retrieve message\n(Common issues: wrong message or channel ID, or I do not have view permissions)",
				lang::ERROR_EMOJI
			)));
			co_return;
		}

		dpp::message message = std::move(confirm.get<dpp::message>());
		if (message.stickers.empty())
		{
			event.edit_original_response(original_response.set_content(fmt::format(
				"{} Error: message does not have stickers!",
				lang::ERROR_EMOJI
			)));
			co_return;
		}

		dpp::message ret;

		for (const dpp::sticker& s : message.stickers)
		{
			auto content = co_await sticker_content_get(cluster, s);
			if (!content)
			{
				ret.content.append(fmt::format("{} Could not download image data", lang::ERROR_EMOJI));
				continue;
			}
			auto grabbed_sticker = co_await sticker_get(cluster, s.id);
			if (!grabbed_sticker)
			{
				ret.content.append(fmt::format(
					"{} Could not request sticker details, was it deleted? Adding image as attachment for manual addition.",
					lang::ERROR_EMOJI
				));
				attachment_add(ret, s, *content);
				continue;
			}

			dpp::sticker to_add;
			auto         resize_result = image_process(content);

			to_add.filecontent  = std::move(*content);
			to_add.guild_id     = event.command.guild_id;
			to_add.sticker_user = cluster->me;
			to_add.name         = grabbed_sticker->name;
			to_add.description  = grabbed_sticker->description;
			to_add.filename     = to_add.name + ".png";
			to_add.tags         = grabbed_sticker->tags;
			to_add.type         = dpp::st_guild;
			std::ranges::replace(to_add.filename, ' ', '_');

			auto add_result = co_await sticker_add(cluster, to_add);
			if (!add_result.has_value())
			{
				ret.content.append(fmt::format(
					"{} Failed to add sticker `{}`: \"{}\"\nAdding image as attachment for manual addition",
					lang::ERROR_EMOJI,
					grabbed_sticker->name,
					add_result.error().message
				));
				attachment_add(ret, s, to_add.filecontent);
			}
			else
			{
				ret.content.append(fmt::format(
					"{} Added sticker \"{}\"!",
					lang::SUCCESS_EMOJI,
					grabbed_sticker->name
				));
			}
			if (resize_result == ImageProcessResult::RESIZED)
				ret.content.append(" (note : the image was resized due to being too large)");
			event.edit_original_response(ret, [](const dpp::confirmation_callback_t &c){
				if (c.is_error())
					B12::log(LogLevel::BASIC, "an error occured while trying to edit `server sticker grab` command response:\n{}", c.get_error().message);
			});
		}
	}
}

template <>
CommandResponse CommandHandler::command<"server sticker grab">(
	command_option_view options
)
{
	dpp::snowflake channel_id{_channel->id};
	dpp::snowflake message_id{0};

	for (const dpp::command_data_option& opt : options)
	{
		if (opt.type == dpp::co_channel && opt.name == "channel")
		{
			channel_id = std::get<dpp::snowflake>(opt.value);
		}
		else if (opt.type == dpp::co_string && opt.name == "message")
		{
			auto value = std::get<std::string>(opt.value);
			message_id = std::stoull(value);
		}
	}
	if (!message_id)
		return {CommandResponse::UsageError{}, {{"Error: could not parse message id"}}};

	sticker_grab_task(&Bot::bot(), std::get<B12::CommandSource::Interaction>(_source.source).event, message_id, channel_id);
	return {CommandResponse::None{}};
}
