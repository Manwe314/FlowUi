#include "FlowUi/Private/textLayoutService/TextLayoutValidation.hpp"

#include <cmath>
#include <limits>

#include "FlowUi/Private/textLayoutService/TextAnalysis.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

namespace FlowUi::detail::text {
namespace {

[[nodiscard]] bool fitsRange(uint32_t first, uint32_t count, size_t size) noexcept {
	return first <= size && count <= size - first;
}

[[nodiscard]] bool finiteNonnegative(float value) noexcept {
	return std::isfinite(value) && value >= 0.0f;
}

[[nodiscard]] Status invalidLayout(const Font::FontFaceData& fontFace) noexcept {
	return unexpectedError(
		makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::TextLayoutCompute, fontFace.id));
}

} // namespace

Status finalizeTextLayout(const TextProviderInput& input, const Font::FontFaceData& fontFace,
						  TextLayoutResult& result) noexcept {
	const auto* variant = fontFace.defaultVariant();
	if (!result.success || !variant || variant->glyphs.empty() ||
		!finiteNonnegative(result.measuredWidth) || !finiteNonnegative(result.measuredAdvance) ||
		!std::isfinite(result.lineHeight) || result.lineHeight <= 0.0f ||
		!std::isfinite(result.emPixels) || result.emPixels <= 0.0f || result.lines.empty() ||
		result.lines.size() > std::numeric_limits<uint32_t>::max() ||
		result.glyphs.size() > std::numeric_limits<uint32_t>::max() ||
		result.clusters.size() > std::numeric_limits<uint32_t>::max() ||
		result.caretStops.size() > std::numeric_limits<uint32_t>::max() ||
		(!input.includeGlyphGeometry && !result.glyphs.empty())) {
		return invalidLayout(fontFace);
	}
	if (input.includeGlyphGeometry &&
		(fontFace.atlasWidth == 0 || fontFace.atlasHeight == 0 || fontFace.sourceAtlasWidth == 0 ||
		 fontFace.sourceAtlasHeight == 0 ||
		 uint64_t{fontFace.sourceAtlasX} + fontFace.sourceAtlasWidth > fontFace.atlasWidth ||
		 uint64_t{fontFace.sourceAtlasY} + fontFace.sourceAtlasHeight > fontFace.atlasHeight)) {
		return invalidLayout(fontFace);
	}

	size_t nextLineStart = 0;
	size_t nextGlyph = 0;
	size_t nextCluster = 0;
	size_t nextCaret = 0;
	for (size_t lineIndex = 0; lineIndex < result.lines.size(); ++lineIndex) {
		const auto& line = result.lines[lineIndex];
		const bool finalLine = lineIndex + 1 == result.lines.size();
		size_t contentEnd = line.endByte;
		DecodedCodepoint newline{};
		const bool newlineBoundary =
			!finalLine && decodeNextUtf8(input.text, contentEnd, newline) && newline.value == '\n';
		if (line.startByte != nextLineStart || line.endByte < line.startByte ||
			line.endByte > input.text.size() ||
			(finalLine ? line.endByte != input.text.size() : !newlineBoundary) ||
			input.text.substr(line.startByte, line.endByte - line.startByte).find('\n') !=
				std::string_view::npos ||
			line.firstGlyph != nextGlyph || line.firstCluster != nextCluster ||
			line.firstCaretStop != nextCaret ||
			!fitsRange(line.firstGlyph, line.glyphCount, result.glyphs.size()) ||
			!fitsRange(line.firstCluster, line.clusterCount, result.clusters.size()) ||
			!fitsRange(line.firstCaretStop, line.caretStopCount, result.caretStops.size()) ||
			line.caretStopCount == 0 || !finiteNonnegative(line.measuredWidth) ||
			!finiteNonnegative(line.measuredAdvance) || !std::isfinite(line.baselineY) ||
			!std::isfinite(line.lineHeight) || line.lineHeight <= 0.0f ||
			(!input.includeGlyphGeometry && (line.firstGlyph != 0 || line.glyphCount != 0))) {
			return invalidLayout(fontFace);
		}

		size_t nextByte = line.startByte;
		size_t clusterGlyph = line.firstGlyph;
		float previousClusterEnd = -std::numeric_limits<float>::infinity();
		for (size_t clusterIndex = line.firstCluster;
			 clusterIndex < size_t{line.firstCluster} + line.clusterCount; ++clusterIndex) {
			const auto& cluster = result.clusters[clusterIndex];
			if (cluster.lineIndex != lineIndex || cluster.startByte != nextByte ||
				cluster.startByte >= cluster.endByte || cluster.endByte > contentEnd ||
				cluster.firstGlyph != clusterGlyph ||
				!fitsRange(cluster.firstGlyph, cluster.glyphCount, result.glyphs.size()) ||
				size_t{cluster.firstGlyph} + cluster.glyphCount >
					size_t{line.firstGlyph} + line.glyphCount ||
				!std::isfinite(cluster.xStart) || !std::isfinite(cluster.xEnd) ||
				cluster.xStart < previousClusterEnd || cluster.xEnd < cluster.xStart ||
				(!input.includeGlyphGeometry &&
				 (cluster.firstGlyph != 0 || cluster.glyphCount != 0))) {
				return invalidLayout(fontFace);
			}
			for (size_t glyphIndex = cluster.firstGlyph;
				 glyphIndex < size_t{cluster.firstGlyph} + cluster.glyphCount; ++glyphIndex) {
				const auto& glyph = result.glyphs[glyphIndex];
				if (glyph.lineIndex != lineIndex || glyph.clusterStartByte != cluster.startByte ||
					glyph.clusterEndByte != cluster.endByte) {
					return invalidLayout(fontFace);
				}
			}
			previousClusterEnd = cluster.xEnd;
			nextByte = cluster.endByte;
			clusterGlyph += cluster.glyphCount;
		}
		if (nextByte != contentEnd || clusterGlyph != size_t{line.firstGlyph} + line.glyphCount) {
			return invalidLayout(fontFace);
		}

		size_t previousCaretByte = line.startByte;
		float previousCaretX = -std::numeric_limits<float>::infinity();
		for (size_t caretIndex = line.firstCaretStop;
			 caretIndex < size_t{line.firstCaretStop} + line.caretStopCount; ++caretIndex) {
			const auto& caret = result.caretStops[caretIndex];
			if (caret.lineIndex != lineIndex || caret.byteOffset < previousCaretByte ||
				caret.byteOffset > line.endByte || !std::isfinite(caret.x) ||
				caret.x < previousCaretX ||
				(caretIndex == line.firstCaretStop && caret.byteOffset != line.startByte)) {
				return invalidLayout(fontFace);
			}
			previousCaretByte = caret.byteOffset;
			previousCaretX = caret.x;
		}
		if (previousCaretByte != line.endByte) {
			return invalidLayout(fontFace);
		}
		nextGlyph += line.glyphCount;
		nextCluster += line.clusterCount;
		nextCaret += line.caretStopCount;
		nextLineStart = contentEnd;
	}
	if (nextLineStart != input.text.size() || nextGlyph != result.glyphs.size() ||
		nextCluster != result.clusters.size() || nextCaret != result.caretStops.size()) {
		return invalidLayout(fontFace);
	}

	const float inverseWidth =
		input.includeGlyphGeometry ? 1.0f / static_cast<float>(fontFace.atlasWidth) : 0.0f;
	const float inverseHeight =
		input.includeGlyphGeometry ? 1.0f / static_cast<float>(fontFace.atlasHeight) : 0.0f;
	for (auto& glyph : result.glyphs) {
		if (glyph.glyphIndex >= variant->glyphs.size() || !std::isfinite(glyph.x) ||
			!std::isfinite(glyph.y) || !std::isfinite(glyph.x + glyph.width) ||
			!std::isfinite(glyph.y + glyph.height) || !finiteNonnegative(glyph.width) ||
			!finiteNonnegative(glyph.height)) {
			return invalidLayout(fontFace);
		}
		const auto& source = variant->glyphs[glyph.glyphIndex];
		if (!std::isfinite(source.imageLeft) || !std::isfinite(source.imageRight) ||
			!std::isfinite(source.imageBottom) || !std::isfinite(source.imageTop) ||
			source.imageLeft > source.imageRight || source.imageBottom > source.imageTop) {
			return invalidLayout(fontFace);
		}
		const float atlasX = static_cast<float>(fontFace.sourceAtlasX);
		const float atlasY = static_cast<float>(fontFace.sourceAtlasY);
		const float sourceHeight = static_cast<float>(fontFace.sourceAtlasHeight);
		glyph.u0 = textAtlasCoordinate(atlasX + source.imageLeft, inverseWidth);
		glyph.u1 = textAtlasCoordinate(atlasX + source.imageRight, inverseWidth);
		glyph.v0 = textAtlasCoordinate(atlasY + sourceHeight - source.imageTop, inverseHeight);
		glyph.v1 = textAtlasCoordinate(atlasY + sourceHeight - source.imageBottom, inverseHeight);
	}
	result.atlasLayer = fontFace.atlasLayer;
	result.distanceRangePx = std::isfinite(variant->distanceRange) && variant->distanceRange > 0.0f
								 ? variant->distanceRange
								 : 2.0f;
	return {};
}

} // namespace FlowUi::detail::text
