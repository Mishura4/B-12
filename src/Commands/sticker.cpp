#include "Core/Bot.h"

#include "Command.h"
#include "CommandResponse.h"
#include "../Data/Lang.h"

#include <CImg.h>
#include <png.h>

#ifdef None // X11 included from CImg
#undef None
#endif

#include "commands.h"

#ifdef Success
#undef Success
#endif

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

	enum class ImageProcessResult
	{
		ERROR   = 0,
		RESIZED = 1,
		NOOP    = 2
	};

	constexpr auto destroy_pngstruct = [](png_structp ptr) {
		png_destroy_read_struct(&ptr, nullptr, nullptr);
	};

	template <typename T, typename D = std::default_delete<T>>
	struct raii_helper;

	template <typename T, typename D>
	struct raii_helper<T*, D> {
		T* ptr = nullptr;
		D deleter;

		raii_helper() = default;

		raii_helper(T *p) requires(std::is_default_constructible_v<D>) : ptr{p}, deleter{} {
		}

		raii_helper(T *p, const D& d) : ptr{p}, deleter{d} {
		}

		raii_helper(const raii_helper&) = delete;

		raii_helper(raii_helper&& rhs) : ptr(std::exchange(rhs.ptr, nullptr)), deleter{rhs.deleter} {
		}

		raii_helper &operator=(const raii_helper&) = delete;

		raii_helper &operator=(raii_helper&& rhs) {
			ptr = std::exchange(rhs.ptr, nullptr);
			deleter = rhs.deleter;
			return *this;
		}

		raii_helper &operator=(T* p) {
			release();
			ptr = p;
		}

		void release() {
			if (ptr) {
				deleter(ptr);
				ptr = nullptr;
			}
		}

		T *get() const {
			return ptr;
		}

		~raii_helper() {
			if (ptr)
				deleter(ptr);
		}

		explicit operator bool() const {
			return (ptr);
		}
	};

	template <typename T>
	raii_helper(T) -> raii_helper<T>;

	template <typename T, typename D>
	raii_helper(T, D) -> raii_helper<T, D>;

	template <typename T, typename U>
	constexpr T convert_pixel(U from) noexcept {
		if constexpr (std::is_floating_point_v<T>) {
			return (static_cast<T>(from) / static_cast<T>(std::numeric_limits<U>::max()));
		}
		return (static_cast<T>(from));
	}

	template <bool Reverse, size_t Spectrum, size_t Plane>
	const auto do_convert_pixel = []<typename T, typename U>(T *out, const U *in, size_t pixel_pos, size_t image_size) constexpr noexcept {
		size_t interleaved_pos = pixel_pos * Spectrum + Plane;
		size_t planar_pos = pixel_pos + Plane * image_size;

		if constexpr (!Reverse)
			out[planar_pos] = convert_pixel<T>(in[interleaved_pos]);
		else
			out[interleaved_pos] = convert_pixel<T>(in[planar_pos]);
	};

	template <bool Reverse>
	const auto do_convert_pixels = []<typename T, typename U, size_t... Ns>(T *out, const U *in, size_t image_size, std::index_sequence<Ns...>) constexpr noexcept {
		for (size_t i = 0; i < image_size; ++i) {
			((do_convert_pixel<Reverse, sizeof...(Ns), Ns>(out, in, i, image_size)), ...);
		}
	};

	template <bool Reverse, size_t Spectrum>
	const auto convert_pixels = []<typename T, typename U>(T *out, const U *in,  size_t image_size) constexpr noexcept {
		return do_convert_pixels<Reverse>(out, in, image_size, std::make_index_sequence<Spectrum>{});
	};

	ImageProcessResult image_process(std::string& content)
	{
		using enum ImageProcessResult;

		constexpr auto MAX_SIZE   = 320;
		constexpr auto MAX_SIZE_F = static_cast<float>(MAX_SIZE);
		//auto           file = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		//auto           info = png_create_info_struct(file);

		//setjmp(png_jmpbuf(file));
		png_image image{};

		image.version = PNG_IMAGE_VERSION;
		png_image_begin_read_from_memory(&image, content.data(), content.size());
		if (image.width <= MAX_SIZE && image.height <= MAX_SIZE)
		{
			png_image_finish_read(&image, nullptr, nullptr, 0, nullptr);
			return (NOOP);
		}
		auto og_size = PNG_IMAGE_SIZE(image);
		auto buffer  = std::make_unique_for_overwrite<unsigned char[]>(og_size);
		if (png_image_finish_read(&image, 0, buffer.get(), 0, nullptr) == 0)
			return (ERROR);
		auto cimg_buffer = std::make_unique<unsigned char[]>(og_size);
		auto spectrum = PNG_IMAGE_PIXEL_CHANNELS(image.format);
		switch (spectrum) {
			case 4:
				convert_pixels<false, 4>(cimg_buffer.get(), buffer.get(), image.height * image.width);
				break;

			case 3:
				convert_pixels<false, 3>(cimg_buffer.get(), buffer.get(), image.height * image.width);
				break;

			case 2:
				convert_pixels<false, 2>(cimg_buffer.get(), buffer.get(), image.height * image.width);
				break;

			case 1:
			case 0:
				break;

			default:
				return (ERROR);
		}
		auto og_cimg = cimg_library::CImg<unsigned char>{cimg_buffer.get(), image.width, image.height, 1, spectrum, true};
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

		auto resized = og_cimg.get_resize(w, h, -100, -100, 5);
		out.resize(out_size);
		resized.save_bmp("test.bmp");
		switch (spectrum) {
			case 4:
				convert_pixels<true, 4>(cimg_buffer.get(), resized.data(), image.height * image.width);
				break;

			case 3:
				convert_pixels<true, 3>(cimg_buffer.get(), resized.data(), image.height * image.width);
				break;

			case 2:
				convert_pixels<true, 2>(cimg_buffer.get(), resized.data(), image.height * image.width);
				break;

			case 1:
			case 0:
				break;

			default:
				return (ERROR);
		}
		if (!png_image_write_to_memory(
			&image,
			out.data(),
			&out_size,
			0,
			cimg_buffer.get(),
			w * spectrum,
			nullptr
		))
			return (ERROR);
		out.resize(out_size);
		content = std::move(out);
		return (RESIZED);
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

		dpp::sticker to_add;
		auto         resize_result = image_process(sticker_data);

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
		if (resize_result == ImageProcessResult::RESIZED)
			ret.content.append(" (note : the image was resized due to being too large)");
	}
	co_return command::response::edit(ret);
}
