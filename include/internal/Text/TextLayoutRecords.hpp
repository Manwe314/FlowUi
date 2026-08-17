#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "managers/structs/FontManagerStructs.hpp"

namespace FlowUi::detail::manager_storage { struct FontFrameView; }

namespace FlowUi::detail::text {

struct TextLayoutGlyph {
	uint32_t glyphIndex = 0;
	uint32_t codepoint = 0;
	size_t clusterStartByte = 0;
	size_t clusterEndByte = 0;
	uint32_t lineIndex = 0;
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
};

struct TextLayoutCluster {
	size_t startByte = 0;
	size_t endByte = 0;
	uint32_t firstGlyph = 0;
	uint32_t glyphCount = 0;
	uint32_t lineIndex = 0;
	float xStart = 0.0f;
	float xEnd = 0.0f;
};

struct TextLayoutLine {
	size_t startByte = 0;
	size_t endByte = 0;
	uint32_t firstGlyph = 0;
	uint32_t glyphCount = 0;
	uint32_t firstCluster = 0;
	uint32_t clusterCount = 0;
	uint32_t firstCaretStop = 0;
	uint32_t caretStopCount = 0;
	float measuredWidth = 0.0f;
	float measuredAdvance = 0.0f;
	float baselineY = 0.0f;
	float lineHeight = 0.0f;
};

struct TextCaretStop {
	size_t byteOffset = 0;
	uint32_t lineIndex = 0;
	float x = 0.0f;
};

struct TextLayoutRequest {
	std::string_view text{};
	const manager_storage::FontFrameView* fontView = nullptr;
	FontId fontId = 0;
	float pointsToPixelsScale = 96.0f / 72.0f;
	uint16_t fontSize = 0;
	uint16_t letterSpacing = 0;
	uint8_t tabWidth = 4;
	bool includeGlyphGeometry = false;
};

struct TextLayoutResult {
	bool success = false;
	float measuredWidth = 0.0f;
	float measuredAdvance = 0.0f;
	float lineHeight = 0.0f;
	float emPixels = 0.0f;
	uint32_t atlasLayer = 0;
	float distanceRangePx = 2.0f;
	std::vector<TextLayoutGlyph> glyphs{};
	std::vector<TextLayoutCluster> clusters{};
	std::vector<TextLayoutLine> lines{};
	std::vector<TextCaretStop> caretStops{};
};

} // namespace FlowUi::detail::text
