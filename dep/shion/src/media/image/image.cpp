#include "shion/media/image/image.h"

#include "shion/exception.h"

#include <CImg.h>
#include <png.h>

using namespace shion;

namespace {
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
}

template <>
auto image::copy_data<image::layout::unknown>() const -> std::unique_ptr<std::byte[]> {
	auto ret = std::make_unique_for_overwrite<std::byte[]>(byte_size());

	std::memcpy(ret.get(), data.get(), byte_size());
	return (ret);
}

template <image::layout Layout>
auto image::copy_data() const -> std::unique_ptr<std::byte[]> {
	if (Layout == pixel_layout) {
		return (copy_data<layout::unknown>());
	}
	constexpr bool reverse = (Layout == image::layout::interleaved);

	auto copy = std::make_unique_for_overwrite<std::byte[]>(byte_size());

	switch (spectrum) {
		case 6:
			convert_pixels<reverse, 6>(copy.get(), data.get(), width * height);
			break;

		case 5:
			convert_pixels<reverse, 5>(copy.get(), data.get(), width * height);
			break;

		case 4:
			convert_pixels<reverse, 4>(copy.get(), data.get(), width * height);
			break;

		case 3:
			convert_pixels<reverse, 3>(copy.get(), data.get(), width * height);
			break;

		case 2:
			convert_pixels<reverse, 2>(copy.get(), data.get(), width * height);
			break;

		default:
			throw shion::logic_exception(std::format("image::copy_data(): unhandled spectrum {}", spectrum));
	}
	return (copy);
}

auto image::copy_data(layout with_layout) const -> std::unique_ptr<std::byte[]> {
	switch (with_layout) {
		case layout::unknown:
			return (copy_data<layout::unknown>());
		case layout::interleaved:
			return (copy_data<layout::interleaved>());
		case layout::planar:
			return (copy_data<layout::planar>());
		default:
			throw shion::logic_exception{"unknown pixel layout"};
	}
}

template <image::layout Layout>
void image::convert_layout() requires (Layout != layout::unknown) {
	if (Layout == pixel_layout) {
		return;
	}
	data = copy_data<Layout>();
	pixel_layout = Layout;
}

void image::convert_layout(layout target_layout) {
	switch (target_layout) {
		case layout::interleaved:
			return (convert_layout<layout::interleaved>());
		case layout::planar:
			return (convert_layout<layout::planar>());
		case layout::unknown:
		default:
			throw logic_exception{"unknown pixel layout"};
	}
}

auto image::to_png() const -> std::vector<std::byte> {
	std::vector<std::byte> png_data;
	size_t size = byte_size();
	png_data.resize(size);
	write_png(png_data);
	return (png_data);
}

auto image::write_png(std::span<std::byte> out) const -> size_t {
	png_image img{};

	img.version = PNG_IMAGE_VERSION;
	img.width = static_cast<png_uint_32>(width);
	img.height = static_cast<png_uint_32>(height);
	img.format =
		(color_format == color_fmt::rgb ? PNG_FORMAT_FLAG_COLOR : 0) |
		(pixel_format & pixel_flags::has_alpha ? PNG_FORMAT_FLAG_ALPHA : 0) |
		(pixel_format & pixel_flags::alpha_first ? PNG_FORMAT_FLAG_AFIRST : 0) |
		(pixel_format & pixel_flags::color_reversed ? PNG_FORMAT_FLAG_BGR : 0);
	size_t size = out.size();
	std::unique_ptr<std::byte[]> copy;
	std::byte *ptr = data.get();
	if (pixel_layout == layout::planar) {
		copy = copy_data<layout::interleaved>();
		ptr = copy.get();
	}
	if (auto ret = png_image_write_to_memory(
		&img,
		out.data(),
		&size,
		0,
		ptr,
		static_cast<png_int_32>(stride()),
		nullptr
	); ret == 0) {
		throw error_exception(std::format("failed to write png image to memory: {}", img.message));
	}
	return (size);
}

png_image read_png_header(std::span<std::byte const> image_data) {
	png_image img{};
	img.version = PNG_IMAGE_VERSION;
	if (auto ret = png_image_begin_read_from_memory(&img, image_data.data(), image_data.size()); ret == 0) {
		throw error_exception{std::format("could not read png image header: {}", img.message)};
	}
	return (img);
}

image image_from_png(const png_image &png) {
	return {
		.width = png.width,
		.height = png.height,
		.component_bit_size = PNG_IMAGE_PIXEL_COMPONENT_SIZE(png.format) * 8,
		.spectrum = PNG_IMAGE_PIXEL_CHANNELS(png.format),
		.pixel_layout = image::layout::interleaved,
		.color_format = (png.format & PNG_FORMAT_FLAG_COLOR ? image::color_fmt::rgb : image::color_fmt::grayscale),
		.pixel_format =
			bit_if(image::pixel_flags::has_alpha, png.format & PNG_FORMAT_FLAG_ALPHA) |
			bit_if(image::pixel_flags::alpha_first, png.format & PNG_FORMAT_FLAG_AFIRST) |
			bit_if(image::pixel_flags::color_reversed, png.format & PNG_FORMAT_FLAG_BGR)
	};
}

auto image::png_info(std::span<std::byte const> image_data) -> image {
	auto png = read_png_header(image_data);
	auto image = image_from_png(png);
	png_image_finish_read(&png, 0, 0, 0, 0);
	return (image);
}

auto image::from_png(std::span<std::byte const> image_data) -> image {
	png_image png = read_png_header(image_data);
	image image = image_from_png(png);

	image.data = std::make_unique_for_overwrite<std::byte[]>(PNG_IMAGE_SIZE(png));
	if (auto ret = png_image_finish_read(&png, 0, image.data.get(), static_cast<png_int_32>(image.stride()), nullptr); ret == 0) {
		throw error_exception{"could not read png image data"};
	}
	return image;
}

void image::resize(size_t w, size_t h, bool keep_ratio) {
	using byte_u = std::underlying_type_t<std::byte>;
	std::unique_ptr<std::byte[]> copy;
	std::byte *data_ptr = data.get();
	layout old_layout = pixel_layout;
	if (pixel_layout != layout::planar) {
		copy = copy_data<layout::planar>();
		data_ptr = copy.get();
	}
	auto og_cimg = cimg_library::CImg<byte_u>{reinterpret_cast<byte_u*>(data_ptr), static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1, static_cast<unsigned int>(spectrum), true};

	if (keep_ratio)
	{
		float wf = static_cast<float>(width);
		float hf = static_cast<float>(height);
		float max_height_f = static_cast<float>(w);
		float max_width_f = static_cast<float>(h);
		size_t wd = (w > width ? w - width : width - w);
		size_t hd = (h > height ? h - height : height - h);


		if (wd > hd)
		{
			hf = (hf / wf) * max_width_f;
			wf = max_width_f;
		}
		else if (hd > wd)
		{
			wf = (hf / wf) * max_height_f;
			hf = max_height_f;
		}
		w = static_cast<uint32_t>(floorf(wf));
		h = static_cast<uint32_t>(floorf(hf));
	}
	size_t prev_size = byte_size();
	width  = w;
	height = h;
	if (prev_size < byte_size())
		data = std::make_unique_for_overwrite<std::byte[]>(byte_size());
	auto resized = og_cimg.get_resize(static_cast<int>(w), static_cast<int>(h), -100, -100, 5);
	std::memcpy(data.get(), resized.data(), resized.size());
	pixel_layout = layout::planar;
	if (old_layout != layout::planar) {
		convert_layout(old_layout);
	}
}

bool image::truncate(size_t w, size_t h) {
	if (width <= w && height <= h)
		return (false);
	resize(w, h, true);
	return (true);
}
