#ifndef SHION_MEDIA_IMAGE_H_
#define SHION_MEDIA_IMAGE_H_

#include <memory>
#include <span>
#include <cstddef>
#include <vector>

#include "shion/shion.h"

#include "shion/utils/flexible_ptr.h"
#include "shion/utils/bit_mask.h"
#include "shion/utils/misc.h"

namespace shion {

struct SHION_EXPORT image {
	enum class layout {
		/**
			* @brief Unknown layout. Usually means image not loaded / errored
			*/
		unknown,

		/**
			* @brief By pixel. e.g. RGBRGBRGB
			*/
		interleaved,

		/**
			* @brief By plane. e.g. RRRGGGBBB.
			*/
		planar
	};

	enum class color_fmt {
		unknown,
		grayscale,
		rgb,
		yuv420
	};

	enum class pixel_flags {
		none = 0,
		has_alpha = 1 << 0,
		alpha_first = 1 << 1,
		color_reversed = 1 << 2
	};

	/**
		* @brief Width of the image
		*/
	size_t width = 0;

	/**
		* @brief Height of the image.
		*/
	size_t height = 0;

	/**
		* @brief Size in bits of a pixel component (a single channel).
		*/
	size_t component_bit_size = 0;

	/**
		* @brief Number of channels in the image.
		*/
	size_t spectrum = 0;

	/**
		* @brief Pixel layout of the image.
		*
		* @see layout
		*/
	layout pixel_layout = layout::unknown;

	/**
		* @brief Color format of the image.
		*
		* @see color_fmt
		*/
	color_fmt color_format = color_fmt::unknown;

	/**
		* @brief Pixel flags of the image.
		*
		* @see pixel_flags
		*/
	bit_mask<pixel_flags> pixel_format = pixel_flags::none;

	/**
		* @brief Raw data in bytes of the image.
		*/
	flexible_ptr<std::byte[]> data = {};

	/**
		* @brief Get the number of bytes in one row of the image
		*/
	constexpr auto stride() const noexcept -> std::size_t;

	/**
		* @brief Get the size in bytes of a pixel
		*
		* @return `(component_bit_size * spectrum) / CHAR_BIT` rounded up
		*/
	constexpr auto pixel_byte_size() const noexcept -> std::size_t;

	/**
		* @brief Get the size of the image in bytes.
		*/
	constexpr auto byte_size() const noexcept -> std::size_t;

	template <layout Layout>
	void convert_layout() requires (Layout != layout::unknown);

	void convert_layout(layout target_layout);

	template <layout Layout = layout::unknown>
	auto copy_data() const -> std::unique_ptr<std::byte[]>;

	auto copy_data(layout with_layout) const -> std::unique_ptr<std::byte[]>;

	void resize(size_t width, size_t height, bool keep_ratio = false);

	bool truncate(size_t max_width, size_t max_height);

	auto write_png(std::span<std::byte> bytes) const -> size_t;

	std::vector<std::byte> to_png() const;

	static auto from_png(std::span<std::byte const> image_data) -> image;

	static auto png_info(std::span<std::byte const> image_data) -> image;
};

constexpr auto image::stride() const noexcept -> std::size_t {
	return (pixel_byte_size() * width);
}

constexpr auto image::pixel_byte_size() const noexcept -> std::size_t {
	return (((component_bit_size * spectrum) + (CHAR_BIT - 1)) / CHAR_BIT);
}

constexpr auto image::byte_size() const noexcept -> std::size_t {
	return (stride() * height);
}

}

#endif
