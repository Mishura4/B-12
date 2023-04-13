#include "Core/Bot.h"

#include "Command.h"
#include "../Data/Lang.h"

#include <CImg.h>
#include <png.h>
#include <nonstd/expected.hpp>

#include "CommandHandler.h"

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

	std::optional<dpp::message> message_get(
		dpp::snowflake message_id,
		dpp::snowflake channel_id
	)
	{
		std::optional<dpp::message> message{std::nullopt};

		auto                      message_getter = AsyncExecutor<dpp::message>(
			[&](const dpp::message& m)
			{
				message = m;
			}
		);
		message_getter(&dpp::cluster::message_get, &Bot::bot(), message_id, channel_id);
		message_getter.wait();
		return (message);
	}

	auto sticker_get(dpp::snowflake id)
	{
		std::optional<dpp::sticker> ret;
		auto                        sticker_retriever = AsyncExecutor<dpp::sticker>{
			[&](const dpp::sticker&   s0)
			{
				ret = s0;
			}
		};

		sticker_retriever(&dpp::cluster::nitro_sticker_get, &Bot::bot(), id).wait();
		return (ret);
	}

	auto sticker_content_get(const dpp::sticker& sticker)
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

		content_retriever(request, &Bot::bot(), sticker.get_url());
		content_retriever.wait();
		return (content);
	}

	nonstd::expected<dpp::sticker, dpp::error_info> sticker_add(dpp::sticker& sticker)
	{
		auto ret      = nonstd::expected<dpp::sticker, dpp::error_info>{};
		auto on_error = [&](const dpp::error_info& err)
		{
			ret = nonstd::make_unexpected(err);
		};
		auto                      executor = AsyncExecutor<dpp::sticker>{
			[&](const dpp::sticker& result)
			{
				ret = result;
			},
			on_error
		};

		executor(&dpp::cluster::guild_sticker_create, &Bot::bot(), sticker);
		executor.wait();
		return (ret);
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
		msg.set_file_content(content);
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

template <>
CommandResponse CommandHandler::command<"server sticker grab">(
	command_option_view options
)
{
	dpp::snowflake channel_id{_channel->id};
	dpp::snowflake message_id{0};

	for (const dpp::command_data_option& opt : options)
	{
		if (opt.type == dpp::co_mentionable && opt.name == "channel")
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
	auto message = message_get(message_id, channel_id);

	if (!message)
		return {
			CommandResponse::UsageError{},
			{{"Error: message not found (do I have view permissions in this channel?)"}}
		};
	if (message->stickers.empty())
		return {CommandResponse::UsageError{}, {{"Error: message does not have stickers!"}}};

	CommandResponse ret{CommandResponse::Success{}};

	sendThink(false);
	for (const dpp::sticker& s : message->stickers)
	{
		auto content = sticker_content_get(s);
		if (!content)
		{
			ret.append("{} Could not download image data", lang::ERROR_EMOJI);
			continue;
		}
		auto grabbed_sticker = sticker_get(s.id);
		if (!grabbed_sticker)
		{
			ret.append(
				"{} Could not request sticker details, was it deleted? Adding image as attachment for manual addition.",
				lang::ERROR_EMOJI
			);
			attachment_add(ret.content.message, s, *content);
			continue;
		}

		dpp::sticker to_add;
		auto         resize_result = image_process(content);

		to_add.filecontent  = std::move(*content);
		to_add.guild_id     = _guild_id;
		to_add.sticker_user = _cluster->me;
		to_add.name         = grabbed_sticker->name;
		to_add.description  = grabbed_sticker->description;
		to_add.filename     = to_add.name + ".png";
		to_add.tags         = grabbed_sticker->tags;
		to_add.type         = dpp::st_guild;
		std::ranges::replace(to_add.filename, ' ', '_');

		auto add_result = sticker_add(to_add);
		if (!add_result.has_value())
		{
			ret.append(
				"{} Failed to add sticker `{}`: \"{}\"\nAdding image as attachment for manual addition",
				lang::ERROR_EMOJI,
				grabbed_sticker->name,
				add_result.error().message
			);
			attachment_add(ret.content.message, s, to_add.filecontent);
		}
		else
		{
			ret.append(
				"{} Added sticker \"{}\"!",
				lang::SUCCESS_EMOJI,
				grabbed_sticker->name
			);
		}
		if (resize_result == ImageProcessResult::RESIZED)
			ret.append(" (note : the image was resized due to being too large)");
	}
	return (ret);
}
