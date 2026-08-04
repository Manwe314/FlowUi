#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>

#include "clay.h"
#include "internal/ManagerStorage/FontCatalogController.hpp"

namespace FlowUi::detail {

struct TextLayoutGlyphQuad {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
	int byteStartOffset = 0;
	int byteEndOffset = 0;
};

struct TextLayoutRequest {
	Clay_StringSlice text{};
	const FlowUi::Font::FontFaceData* fontFace = nullptr;
	float pointsToPixelsScale = 96.0f / 72.0f;
	uint16_t fontSize = 0;
	uint16_t letterSpacing = 0;
	float lineOriginX = 0.0f;
	float lineOriginY = 0.0f;
	bool emitGlyphQuads = false;
};

struct TextLayoutResult {
	bool success = false;
	float measuredWidth = 0.0f;
	float measuredAdvance = 0.0f;
	float lineHeight = 0.0f;
	float emPixels = 0.0f;
	uint32_t atlasLayer = 0u;
	float distanceRangePx = 2.0f;
};

inline const FlowUi::Font::FontFaceData* ResolveFontFace(
	const manager_storage::FontFrameView* fontView,
	uint16_t fontId) {
	if (!fontView) return nullptr;
	const FlowUi::Font::FontFaceData* face = fontView->font(static_cast<FontId>(fontId));
	if (!face) face = fontView->font(0);
	return face;
}

inline bool DecodeNextUtf8Codepoint(const Clay_StringSlice& stringSlice, int& byteOffset, uint32_t& outCodepoint) {
	if (!stringSlice.chars || byteOffset >= stringSlice.length) {
		return false;
	}

	const auto* bytes = reinterpret_cast<const uint8_t*>(stringSlice.chars);
	const uint8_t first = bytes[byteOffset];

	if (first < 0x80u) {
		outCodepoint = first;
		byteOffset += 1;
		return true;
	}

	auto continuation = [&](int index) -> uint8_t {
		return (index < stringSlice.length) ? bytes[index] : 0u;
	};

	if ((first & 0xE0u) == 0xC0u && byteOffset + 1 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		if ((c1 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x1Fu) << 6u) | (c1 & 0x3Fu);
			byteOffset += 2;
			return true;
		}
	} else if ((first & 0xF0u) == 0xE0u && byteOffset + 2 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x0Fu) << 12u) | ((c1 & 0x3Fu) << 6u) | (c2 & 0x3Fu);
			byteOffset += 3;
			return true;
		}
	} else if ((first & 0xF8u) == 0xF0u && byteOffset + 3 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		const uint8_t c3 = continuation(byteOffset + 3);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u && (c3 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x07u) << 18u) | ((c1 & 0x3Fu) << 12u) | ((c2 & 0x3Fu) << 6u) | (c3 & 0x3Fu);
			byteOffset += 4;
			return true;
		}
	}

	// Replace malformed UTF-8 byte and continue.
	outCodepoint = 0xFFFDu;
	byteOffset += 1;
	return true;
}

template <typename EmitGlyphFn>
inline TextLayoutResult LayoutTextLine(const TextLayoutRequest& request, EmitGlyphFn&& emitGlyph) {
	TextLayoutResult result{};

	if (!request.fontFace) {
		return result;
	}

	const FlowUi::Font::FontVariantData* variant = request.fontFace->defaultVariant();
	if (!variant || variant->glyphs.empty()) {
		return result;
	}

	float emPixels = variant->fontSizePx;
	if (request.fontSize > 0) {
		emPixels = static_cast<float>(request.fontSize) * request.pointsToPixelsScale;
	}
	if (emPixels <= 0.0f) {
		return result;
	}

	if (request.emitGlyphQuads && (request.fontFace->atlasWidth == 0 || request.fontFace->atlasHeight == 0)) {
		return result;
	}
	if (request.emitGlyphQuads &&
		(request.fontFace->sourceAtlasWidth == 0 || request.fontFace->sourceAtlasHeight == 0)) {
		return result;
	}

	const float emToPixels = emPixels / std::max(variant->emSize, 1.0e-6f);
	const float letterSpacingPx = static_cast<float>(request.letterSpacing);
	const float baselineY = request.lineOriginY + variant->ascender * emToPixels;
	const float invAtlasWidth = request.emitGlyphQuads
		? (1.0f / static_cast<float>(request.fontFace->atlasWidth))
		: 0.0f;
	const float invAtlasHeight = request.emitGlyphQuads
		? (1.0f / static_cast<float>(request.fontFace->atlasHeight))
		: 0.0f;
	const float sourceAtlasX = request.emitGlyphQuads
		? static_cast<float>(request.fontFace->sourceAtlasX)
		: 0.0f;
	const float sourceAtlasY = request.emitGlyphQuads
		? static_cast<float>(request.fontFace->sourceAtlasY)
		: 0.0f;
	const float sourceAtlasHeight = request.emitGlyphQuads
		? static_cast<float>(request.fontFace->sourceAtlasHeight)
		: 0.0f;

	float penX = request.lineOriginX;
	float lineStartX = request.lineOriginX;
	float lineMinX = request.lineOriginX;
	float lineMaxX = request.lineOriginX;
	bool lineHasGlyph = false;
	float measuredWidth = 0.0f;
	float measuredAdvance = 0.0f;

	uint32_t previousCodepoint = 0;
	bool hasPreviousCodepoint = false;
	int byteOffset = 0;

	while (byteOffset < request.text.length) {
		const int codepointStartByteOffset = byteOffset;
		uint32_t codepoint = 0;
		if (!DecodeNextUtf8Codepoint(request.text, byteOffset, codepoint)) {
			break;
		}
		const int codepointEndByteOffset = byteOffset;

		if (codepoint == '\n') {
			const float lineWidth = lineHasGlyph
				? std::max(0.0f, lineMaxX - lineMinX)
				: std::max(0.0f, penX - lineStartX);
			measuredWidth = std::max(measuredWidth, lineWidth);
			measuredAdvance = std::max(measuredAdvance, std::max(0.0f, penX - lineStartX));

			penX = request.lineOriginX;
			lineStartX = request.lineOriginX;
			lineMinX = request.lineOriginX;
			lineMaxX = request.lineOriginX;
			lineHasGlyph = false;
			hasPreviousCodepoint = false;
			continue;
		}

		if (hasPreviousCodepoint) {
			penX += variant->kerningAdvance(previousCodepoint, codepoint) * emToPixels;
		}

		// Match renderer + measurement text layout: treat spacing codepoints as advance-only.
		if (codepoint == ' ' || codepoint == '\t' || codepoint == 0x00A0u) {
			float whitespaceAdvance = std::max(variant->emSize * emToPixels * 0.33f, 1.0f);
			const auto spaceGlyphIt = variant->unicodeToGlyphIndex.find(' ');
			if (spaceGlyphIt != variant->unicodeToGlyphIndex.end() && spaceGlyphIt->second < variant->glyphs.size()) {
				whitespaceAdvance = variant->glyphs[spaceGlyphIt->second].advanceX * emToPixels;
			}
			if (codepoint == '\t') {
				whitespaceAdvance *= 4.0f;
			}
			penX += whitespaceAdvance + letterSpacingPx;
			previousCodepoint = codepoint;
			hasPreviousCodepoint = true;
			continue;
		}

		uint32_t glyphIndex = variant->fallbackGlyphIndex;
		const auto glyphIt = variant->unicodeToGlyphIndex.find(codepoint);
		if (glyphIt != variant->unicodeToGlyphIndex.end()) {
			glyphIndex = glyphIt->second;
		}
		if (glyphIndex >= variant->glyphs.size()) {
			glyphIndex = 0;
		}

		const FlowUi::Font::GlyphData& glyph = variant->glyphs[glyphIndex];
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

		if (request.emitGlyphQuads) {
			const float y0 = baselineY - glyph.planeTop * emToPixels;
			const float y1 = baselineY - glyph.planeBottom * emToPixels;
			const float glyphHeight = y1 - y0;
			if (glyphWidth > 0.0f && glyphHeight > 0.0f) {
				const float u0 = (sourceAtlasX + glyph.imageLeft) * invAtlasWidth;
				const float u1 = (sourceAtlasX + glyph.imageRight) * invAtlasWidth;
				const float v0 = (sourceAtlasY + (sourceAtlasHeight - glyph.imageTop)) * invAtlasHeight;
				const float v1 = (sourceAtlasY + (sourceAtlasHeight - glyph.imageBottom)) * invAtlasHeight;
				emitGlyph(TextLayoutGlyphQuad{
					.x = x0,
					.y = y0,
					.w = glyphWidth,
					.h = glyphHeight,
					.u0 = std::clamp(u0, 0.0f, 1.0f),
					.v0 = std::clamp(v0, 0.0f, 1.0f),
					.u1 = std::clamp(u1, 0.0f, 1.0f),
					.v1 = std::clamp(v1, 0.0f, 1.0f),
					.byteStartOffset = codepointStartByteOffset,
					.byteEndOffset = codepointEndByteOffset,
				});
			}
		}

		penX += glyph.advanceX * emToPixels + letterSpacingPx;

		previousCodepoint = codepoint;
		hasPreviousCodepoint = true;
	}

	const float finalLineWidth = lineHasGlyph
		? std::max(0.0f, lineMaxX - lineMinX)
		: std::max(0.0f, penX - lineStartX);
	measuredWidth = std::max(measuredWidth, finalLineWidth);
	measuredAdvance = std::max(measuredAdvance, std::max(0.0f, penX - lineStartX));

	float naturalLineHeight = variant->lineHeight * emToPixels;
	if (naturalLineHeight <= 0.0f) {
		naturalLineHeight = emPixels;
	}

	result.success = true;
	result.measuredWidth = measuredWidth;
	result.measuredAdvance = measuredAdvance;
	result.lineHeight = naturalLineHeight;
	result.emPixels = emPixels;
	result.atlasLayer = request.fontFace->atlasLayer;
	if (variant->distanceRange > 0.0f) {
		result.distanceRangePx = variant->distanceRange;
	}
	return result;
}

} // namespace FlowUi::detail
