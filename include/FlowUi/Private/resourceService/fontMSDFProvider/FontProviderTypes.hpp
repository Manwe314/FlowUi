#pragma once
#include "FlowUi/Private/resourceService/providerTypes/Rgba8Image.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace FlowUi::detail::font {
using ::FlowUi::detail::Rgba8Image;
/** Owned, single-atlas MSDF samples and source metrics; image rows are tight and top-down.
 * Atlas bounds use bottom-origin coordinates. No identity or GPU ownership crosses this boundary.
 */
enum class FontDistanceFieldKind : uint8_t {
	Msdf,
	Mtsdf,
};

struct FontKerningPair {
	uint32_t leftCodepoint = 0;
	uint32_t rightCodepoint = 0;
	float advance = 0.0f;
};

struct FontCpuVariant {
	std::vector<Font::GlyphData> glyphs{};
	std::vector<FontKerningPair> kerningPairs{};
	std::string name{};
	std::string metadata{};
	uint32_t flags = 0;
	uint32_t weight = 0;
	uint32_t fallbackGlyphIndex = 0;
	float fontSizePx = 0.0f;
	float distanceRange = 0.0f;
	float distanceRangeMiddle = 0.0f;
	float emSize = 0.0f;
	float ascender = 0.0f;
	float descender = 0.0f;
	float lineHeight = 0.0f;
	float underlineY = 0.0f;
	float underlineThickness = 0.0f;
};

struct FontCpuAsset {
	std::vector<FontCpuVariant> variants{};
	Rgba8Image atlas{};
	std::string metadata{};
	FontDistanceFieldKind fieldKind = FontDistanceFieldKind::Mtsdf;
};

struct FontBakeOutput {
	FontCpuAsset asset{};
	uint32_t requestedCodepointCount = 0;
	uint32_t loadedGlyphCount = 0;
};

enum class FontCharsetMode : uint8_t {
	RuntimeAscii,
	OfflineUi,
	InlineSpecification,
	FileSpecification,
};

struct FontBakeRequest {
	std::span<const std::byte> sourceBytes{};
	std::string_view faceName{};
	std::string_view charsetSpecification{};
	double pixelSize = 18.0;
	double pixelRange = 6.0;
	double angleThreshold = 3.0;
	double miterLimit = 1.0;
	uint32_t pageWidth = 0;
	uint32_t pageHeight = 0;
	uint32_t workerThreadCount = 1;
	uint32_t weight = 400;
	FontCharsetMode charsetMode = FontCharsetMode::RuntimeAscii;
};
/** Legacy renderer format values, independent of any font serialization library. */
[[nodiscard]] constexpr uint32_t legacyImageType(FontDistanceFieldKind kind) noexcept {
	switch (kind) {
	case FontDistanceFieldKind::Msdf:
		return 6u;
	case FontDistanceFieldKind::Mtsdf:
		return 7u;
	}
	return 0u;
}
} // namespace FlowUi::detail::font
