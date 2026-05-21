#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace FlowUi {

/** @addtogroup flowui_font_manager
 * @{
 */

/** @brief Stable concrete font id consumed by Clay text configuration. */
using FontId = uint16_t;

/** @brief Stable logical font family id returned by FlowUi::FontManager. */
using FontFamilyId = uint32_t;

/** @brief Font style requested when resolving a family face. */
enum class FontStyle : uint8_t {
	Normal,
	Italic,
};

/**
 * @brief Describes a single concrete font face to register in a font family.
 *
 * A face corresponds to one source file and one style/weight variant that can
 * later be selected during text style resolution.
 */
struct FontFaceCreateInfo {
	/**
	 * @brief Path to the font source file for this face.
	 *
	 * Must refer to a supported font asset such as .arfont or .ttf.
	 */
	std::filesystem::path path{};

	/**
	 * @brief Requested font size in pixels per em for loading or baking.
	 *
	 * Must be greater than zero.
	 */
	float pixelSize = 18.0f;

	/**
	 * @brief CSS-style font weight for this face.
	 *
	 * Common values are 400 for regular and 700 for bold.
	 */
	uint32_t weight = 400;

	/** @brief Style variant represented by this face. */
	FontStyle style = FontStyle::Normal;

	/**
	 * @brief Optional explicit face name.
	 *
	 * If empty, the face name is inferred from the font source metadata or file path.
	 */
	std::string name{};
};

/**
 * @brief Describes a logical font family and its initially available faces.
 *
 * The family name is used for string-based font lookup, while the contained
 * faces provide the concrete style/weight variants available for resolution.
 */
struct FontFamilyCreateInfo {
	/** @brief Logical family name used for font-family lookup. */
	std::string name = "Default";

	/**
	 * @brief Initial set of concrete faces available in this family.
	 *
	 * Each entry typically represents one style/weight variant, such as regular,
	 * bold, or italic.
	 */
	std::vector<FontFaceCreateInfo> faces{
		FontFaceCreateInfo{
			.path = "assets/fonts/Inter.arfont",
			.pixelSize = 18.0f,
			.weight = 400,
			.style = FontStyle::Normal,
		},
	};
};

/** @} */

} // namespace FlowUi
