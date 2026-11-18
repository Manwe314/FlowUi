#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include <cmath>
#include <limits>

namespace FlowUi::detail::font {
namespace {
[[nodiscard]] constexpr bool unicodeScalar(uint32_t codepoint) noexcept {
	return codepoint <= 0x10ffffu && (codepoint < 0xd800u || codepoint > 0xdfffu);
}
} // namespace
Status validateFontCpuAsset(const FontCpuAsset& asset) noexcept {
	const auto invalid = [] {
		return unexpectedError(makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::FontParse));
	};
	const auto byteSize = rgba8ByteSize(asset.atlas.width, asset.atlas.height);
	if (!byteSize.valid || byteSize.value != asset.atlas.pixels.size() || asset.variants.empty() ||
		legacyImageType(asset.fieldKind) == 0u)
		return invalid();
	for (const auto& variant : asset.variants) {
		if (variant.glyphs.empty() ||
			variant.glyphs.size() > std::numeric_limits<uint32_t>::max() ||
			!std::isfinite(variant.emSize) || variant.emSize <= 0.0f ||
			!std::isfinite(variant.fontSizePx) || variant.fontSizePx <= 0.0f ||
			!std::isfinite(variant.distanceRange) || variant.distanceRange <= 0.0f)
			return invalid();
		for (const float metric :
			 {variant.distanceRangeMiddle, variant.ascender, variant.descender, variant.lineHeight,
			  variant.underlineY, variant.underlineThickness}) {
			if (!std::isfinite(metric))
				return invalid();
		}
		for (const auto& glyph : variant.glyphs) {
			if (!unicodeScalar(glyph.codepoint) || glyph.sourceImageIndex != 0u)
				return invalid();
			for (const float metric :
				 {glyph.planeLeft, glyph.planeBottom, glyph.planeRight, glyph.planeTop,
				  glyph.imageLeft, glyph.imageBottom, glyph.imageRight, glyph.imageTop,
				  glyph.advanceX, glyph.advanceY}) {
				if (!std::isfinite(metric))
					return invalid();
			}
			if (glyph.planeRight < glyph.planeLeft || glyph.planeTop < glyph.planeBottom ||
				glyph.imageLeft < 0.0f || glyph.imageBottom < 0.0f ||
				glyph.imageRight < glyph.imageLeft || glyph.imageTop < glyph.imageBottom ||
				glyph.imageRight > static_cast<float>(asset.atlas.width) ||
				glyph.imageTop > static_cast<float>(asset.atlas.height))
				return invalid();
		}
		for (const auto& pair : variant.kerningPairs) {
			if (!unicodeScalar(pair.leftCodepoint) || !unicodeScalar(pair.rightCodepoint) ||
				!std::isfinite(pair.advance))
				return invalid();
		}
	}
	return {};
}
} // namespace FlowUi::detail::font
