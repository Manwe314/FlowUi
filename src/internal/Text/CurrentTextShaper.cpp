#include "internal/Text/CurrentTextShaper.hpp"

#include <algorithm>

#include "internal/Text/TextAnalysis.hpp"

namespace FlowUi::detail::text {

TextLayoutResult CurrentTextShaper::shape(
	const TextLayoutRequest& request,
	const Font::FontFaceData& fontFace) const {
	TextLayoutResult result{};
	const Font::FontVariantData* variant = fontFace.defaultVariant();
	if (!variant || variant->glyphs.empty()) return result;

	float emPixels = variant->fontSizePx;
	if (request.fontSize > 0) {
		emPixels = static_cast<float>(request.fontSize) * request.pointsToPixelsScale;
	}
	if (emPixels <= 0.0f) return result;
	if (request.includeGlyphGeometry &&
		(fontFace.atlasWidth == 0 || fontFace.atlasHeight == 0 ||
		 fontFace.sourceAtlasWidth == 0 || fontFace.sourceAtlasHeight == 0)) {
		return result;
	}

	const float emToPixels = emPixels / std::max(variant->emSize, 1.0e-6f);
	const float letterSpacingPx = static_cast<float>(request.letterSpacing);
	const float baselineY = variant->ascender * emToPixels;
	float naturalLineHeight = variant->lineHeight * emToPixels;
	if (naturalLineHeight <= 0.0f) naturalLineHeight = emPixels;
	const float invAtlasWidth = request.includeGlyphGeometry
		? 1.0f / static_cast<float>(fontFace.atlasWidth)
		: 0.0f;
	const float invAtlasHeight = request.includeGlyphGeometry
		? 1.0f / static_cast<float>(fontFace.atlasHeight)
		: 0.0f;
	const float sourceAtlasX = static_cast<float>(fontFace.sourceAtlasX);
	const float sourceAtlasY = static_cast<float>(fontFace.sourceAtlasY);
	const float sourceAtlasHeight = static_cast<float>(fontFace.sourceAtlasHeight);

	result.clusters.reserve(request.text.size());
	result.caretStops.reserve(request.text.size() + 1u);
	if (request.includeGlyphGeometry) result.glyphs.reserve(request.text.size());
	result.caretStops.push_back(TextCaretStop{.byteOffset = 0, .lineIndex = 0, .x = 0.0f});

	uint32_t lineIndex = 0;
	size_t lineStartByte = 0;
	uint32_t lineFirstGlyph = 0;
	uint32_t lineFirstCluster = 0;
	uint32_t lineFirstCaretStop = 0;
	float penX = 0.0f;
	float lineMinX = 0.0f;
	float lineMaxX = 0.0f;
	bool lineHasGlyph = false;
	uint32_t previousCodepoint = 0;
	bool hasPreviousCodepoint = false;

	const auto finishLine = [&](size_t endByte) {
		const float width = lineHasGlyph
			? std::max(0.0f, lineMaxX - lineMinX)
			: std::max(0.0f, penX);
		result.measuredWidth = std::max(result.measuredWidth, width);
		result.measuredAdvance = std::max(result.measuredAdvance, std::max(0.0f, penX));
		result.lines.push_back(TextLayoutLine{
			.startByte = lineStartByte,
			.endByte = endByte,
			.firstGlyph = lineFirstGlyph,
			.glyphCount = static_cast<uint32_t>(result.glyphs.size()) - lineFirstGlyph,
			.firstCluster = lineFirstCluster,
			.clusterCount = static_cast<uint32_t>(result.clusters.size()) - lineFirstCluster,
			.firstCaretStop = lineFirstCaretStop,
			.caretStopCount = static_cast<uint32_t>(result.caretStops.size()) - lineFirstCaretStop,
			.measuredWidth = width,
			.measuredAdvance = std::max(0.0f, penX),
			.baselineY = baselineY,
			.lineHeight = naturalLineHeight,
		});
	};

	size_t byteOffset = 0;
	DecodedCodepoint decoded{};
	while (decodeNextUtf8(request.text, byteOffset, decoded)) {
		const uint32_t firstGlyph = static_cast<uint32_t>(result.glyphs.size());
		const float clusterStartX = penX;
		if (decoded.value == '\n') {
			result.clusters.push_back(TextLayoutCluster{
				.startByte = decoded.startByte,
				.endByte = decoded.endByte,
				.firstGlyph = firstGlyph,
				.glyphCount = 0,
				.lineIndex = lineIndex,
				.xStart = clusterStartX,
				.xEnd = clusterStartX,
			});
			finishLine(decoded.startByte);
			++lineIndex;
			lineStartByte = decoded.endByte;
			lineFirstGlyph = static_cast<uint32_t>(result.glyphs.size());
			lineFirstCluster = static_cast<uint32_t>(result.clusters.size());
			lineFirstCaretStop = static_cast<uint32_t>(result.caretStops.size());
			penX = 0.0f;
			lineMinX = 0.0f;
			lineMaxX = 0.0f;
			lineHasGlyph = false;
			hasPreviousCodepoint = false;
			result.caretStops.push_back(TextCaretStop{
				.byteOffset = decoded.endByte,
				.lineIndex = lineIndex,
				.x = 0.0f,
			});
			continue;
		}

		if (hasPreviousCodepoint) {
			penX += variant->kerningAdvance(previousCodepoint, decoded.value) * emToPixels;
		}
		if (decoded.value == ' ' || decoded.value == '\t' || decoded.value == 0x00A0u) {
			float advance = std::max(variant->emSize * emToPixels * 0.33f, 1.0f);
			const auto spaceGlyph = variant->unicodeToGlyphIndex.find(' ');
			if (spaceGlyph != variant->unicodeToGlyphIndex.end() && spaceGlyph->second < variant->glyphs.size()) {
				advance = variant->glyphs[spaceGlyph->second].advanceX * emToPixels;
			}
			if (decoded.value == '\t') advance *= static_cast<float>(std::max<uint8_t>(1u, request.tabWidth));
			penX += advance + letterSpacingPx;
		} else {
			uint32_t glyphIndex = variant->fallbackGlyphIndex;
			if (const auto glyph = variant->unicodeToGlyphIndex.find(decoded.value);
				glyph != variant->unicodeToGlyphIndex.end()) {
				glyphIndex = glyph->second;
			}
			if (glyphIndex >= variant->glyphs.size()) glyphIndex = 0;
			const Font::GlyphData& glyph = variant->glyphs[glyphIndex];
			const float x0 = penX + glyph.planeLeft * emToPixels;
			const float x1 = penX + glyph.planeRight * emToPixels;
			const float glyphWidth = x1 - x0;
			if (glyphWidth > 0.0f) {
				if (!lineHasGlyph) {
					lineMinX = x0;
					lineMaxX = x1;
					lineHasGlyph = true;
				} else {
					lineMinX = std::min(lineMinX, x0);
					lineMaxX = std::max(lineMaxX, x1);
				}
			}
			if (request.includeGlyphGeometry) {
				const float y0 = baselineY - glyph.planeTop * emToPixels;
				const float y1 = baselineY - glyph.planeBottom * emToPixels;
				const float glyphHeight = y1 - y0;
				if (glyphWidth > 0.0f && glyphHeight > 0.0f) {
					result.glyphs.push_back(TextLayoutGlyph{
						.glyphIndex = glyphIndex,
						.codepoint = decoded.value,
						.clusterStartByte = decoded.startByte,
						.clusterEndByte = decoded.endByte,
						.lineIndex = lineIndex,
						.x = x0,
						.y = y0,
						.width = glyphWidth,
						.height = glyphHeight,
						.u0 = std::clamp((sourceAtlasX + glyph.imageLeft) * invAtlasWidth, 0.0f, 1.0f),
						.v0 = std::clamp((sourceAtlasY + sourceAtlasHeight - glyph.imageTop) * invAtlasHeight, 0.0f, 1.0f),
						.u1 = std::clamp((sourceAtlasX + glyph.imageRight) * invAtlasWidth, 0.0f, 1.0f),
						.v1 = std::clamp((sourceAtlasY + sourceAtlasHeight - glyph.imageBottom) * invAtlasHeight, 0.0f, 1.0f),
					});
				}
			}
			penX += glyph.advanceX * emToPixels + letterSpacingPx;
		}

		result.clusters.push_back(TextLayoutCluster{
			.startByte = decoded.startByte,
			.endByte = decoded.endByte,
			.firstGlyph = firstGlyph,
			.glyphCount = static_cast<uint32_t>(result.glyphs.size()) - firstGlyph,
			.lineIndex = lineIndex,
			.xStart = clusterStartX,
			.xEnd = penX,
		});
		result.caretStops.push_back(TextCaretStop{
			.byteOffset = decoded.endByte,
			.lineIndex = lineIndex,
			.x = penX,
		});
		previousCodepoint = decoded.value;
		hasPreviousCodepoint = true;
	}
	finishLine(request.text.size());

	result.success = true;
	result.lineHeight = naturalLineHeight;
	result.emPixels = emPixels;
	result.atlasLayer = fontFace.atlasLayer;
	if (variant->distanceRange > 0.0f) result.distanceRangePx = variant->distanceRange;
	return result;
}

} // namespace FlowUi::detail::text
