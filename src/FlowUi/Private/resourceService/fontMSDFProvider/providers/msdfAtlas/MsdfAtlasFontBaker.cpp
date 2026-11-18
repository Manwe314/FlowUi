#include "FlowUi/Private/resourceService/fontMSDFProvider/FontBakeProfiles.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuBoundary.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/providers/msdfAtlas/MsdfAtlasFontProvider.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#if FLOWUI_CONFIG_DEFAULT_FONT_BAKING
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#endif

namespace FlowUi::detail::font {
#if FLOWUI_CONFIG_DEFAULT_FONT_BAKING
namespace {
[[noreturn]] void bakeError(ErrorCode code = ErrorCode::FontBakeFailed) {
	throw FlowUiException(makeError(code, ErrorSite::FontBake));
}
struct FreetypeDeleter {
	void operator()(msdfgen::FreetypeHandle* handle) const noexcept {
		msdfgen::deinitializeFreetype(handle);
	}
};
struct FontDeleter {
	void operator()(msdfgen::FontHandle* handle) const noexcept { msdfgen::destroyFont(handle); }
};
[[nodiscard]] msdf_atlas::Charset buildCharset(const FontBakeRequest& request,
											   msdfgen::FontHandle* font) {
	msdf_atlas::Charset charset;
	switch (request.charsetMode) {
	case FontCharsetMode::RuntimeAscii:
		return msdf_atlas::Charset::ASCII;
	case FontCharsetMode::OfflineUi: {
		charset = msdf_atlas::Charset::ASCII;
		const auto addSupported = [&](uint32_t codepoint) {
			msdfgen::GlyphIndex glyphIndex;
			if (msdfgen::getGlyphIndex(glyphIndex, font, codepoint) && glyphIndex.getIndex() != 0)
				charset.add(codepoint);
		};
		for (uint32_t codepoint = 0xa0u; codepoint <= 0xffu; ++codepoint)
			addSupported(codepoint);
		for (uint32_t codepoint : offlineUiCodepoints)
			addSupported(codepoint);
		return charset;
	}
	case FontCharsetMode::InlineSpecification:
	case FontCharsetMode::FileSpecification: {
		const std::string specification(request.charsetSpecification);
		if (hasExternalCharsetIncludes(specification))
			bakeError();
		if (!charset.parse(specification.data(), specification.size(), false)) {
			if (request.charsetMode == FontCharsetMode::FileSpecification)
				bakeError();
			std::vector<msdf_atlas::unicode_t> decoded;
			msdf_atlas::utf8Decode(decoded, specification.c_str());
			if (decoded.empty())
				bakeError();
			for (auto codepoint : decoded)
				if (codepoint != 0u)
					charset.add(codepoint);
		}
		return charset;
	}
	}
	bakeError();
}
} // namespace
#endif

Result<FontBakeOutput> MsdfAtlasFontProvider::bake(const FontBakeRequest& request) noexcept {
#if FLOWUI_CONFIG_DEFAULT_FONT_BAKING
	return fontCpuResult(ErrorSite::FontBake, [&]() -> Result<FontBakeOutput> {
		constexpr auto intMaximum = static_cast<uint32_t>(std::numeric_limits<int>::max());
		if (request.sourceBytes.empty() || request.sourceBytes.size() > intMaximum ||
			!std::isfinite(request.pixelSize) || request.pixelSize <= 0.0 ||
			request.pixelSize > intMaximum || !std::isfinite(request.pixelRange) ||
			request.pixelRange <= 0.0 || request.pixelRange > intMaximum ||
			!std::isfinite(request.angleThreshold) || request.angleThreshold < 0.0 ||
			!std::isfinite(request.miterLimit) || request.miterLimit < 0.0 ||
			request.miterLimit > intMaximum || request.workerThreadCount == 0u ||
			request.workerThreadCount > intMaximum ||
			((request.pageWidth == 0u) != (request.pageHeight == 0u)) ||
			request.pageWidth > intMaximum || request.pageHeight > intMaximum)
			bakeError();
		if (request.pageWidth != 0 && !rgba8ByteSize(request.pageWidth, request.pageHeight).valid)
			bakeError(ErrorCode::ImageSizeOverflow);
		std::unique_ptr<msdfgen::FreetypeHandle, FreetypeDeleter> freetype(
			msdfgen::initializeFreetype());
		if (!freetype)
			bakeError(ErrorCode::FontBakeUnavailable);
		std::unique_ptr<msdfgen::FontHandle, FontDeleter> font(msdfgen::loadFontData(
			freetype.get(), reinterpret_cast<const unsigned char*>(request.sourceBytes.data()),
			static_cast<int>(request.sourceBytes.size())));
		if (!font)
			bakeError(ErrorCode::AssetOpenFailed);
		const auto charset = buildCharset(request, font.get());
		if (charset.empty() || charset.size() > intMaximum)
			bakeError();
		std::vector<msdf_atlas::GlyphGeometry> glyphs;
		glyphs.reserve(charset.size());
		msdf_atlas::FontGeometry geometry(&glyphs);
		const std::string faceName(request.faceName);
		if (!faceName.empty())
			geometry.setName(faceName.c_str());
		const int glyphsLoaded = geometry.loadCharset(font.get(), 1.0, charset, false, true);
		if (glyphsLoaded <= 0 || glyphs.empty() || glyphs.size() > intMaximum)
			bakeError();
		unsigned long long glyphSeed = 0;
		for (auto& glyph : glyphs) {
			glyphSeed = glyphSeed * fontColoringMultiplier + fontColoringIncrement;
			glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, request.angleThreshold, glyphSeed);
		}
		msdf_atlas::TightAtlasPacker packer;
		if (request.pageWidth != 0u)
			packer.setDimensions(static_cast<int>(request.pageWidth),
								 static_cast<int>(request.pageHeight));
		else
			packer.setDimensionsConstraint(
				msdf_atlas::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);
		packer.setSpacing(runtimeFontBakeProfile.spacing);
		packer.setScale(request.pixelSize);
		packer.setPixelRange(request.pixelRange);
		packer.setMiterLimit(request.miterLimit);
		packer.setOriginPixelAlignment(false, true);
		const int remaining = packer.pack(glyphs.data(), static_cast<int>(glyphs.size()));
		if (remaining < 0)
			bakeError();
		if (remaining > 0)
			bakeError(ErrorCode::FontAtlasCapacityExceeded);
		int atlasWidth = 0, atlasHeight = 0;
		packer.getDimensions(atlasWidth, atlasHeight);
		if (atlasWidth <= 0 || atlasHeight <= 0)
			bakeError();
		if (request.pageWidth && (static_cast<uint32_t>(atlasWidth) > request.pageWidth ||
								  static_cast<uint32_t>(atlasHeight) > request.pageHeight))
			bakeError(ErrorCode::FontAtlasCapacityExceeded);
		const auto byteSize =
			rgba8ByteSize(static_cast<uint32_t>(atlasWidth), static_cast<uint32_t>(atlasHeight));
		if (!byteSize.valid)
			bakeError(ErrorCode::ImageSizeOverflow);
		msdf_atlas::GeneratorAttributes attributes;
		attributes.config.overlapSupport = true;
		attributes.scanlinePass = true;
		using Generator = msdf_atlas::ImmediateAtlasGenerator<
			float, 4, msdf_atlas::mtsdfGenerator,
			msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;
		Generator generator(atlasWidth, atlasHeight);
		generator.setAttributes(attributes);
		generator.setThreadCount(static_cast<int>(request.workerThreadCount));
		generator.generate(glyphs.data(), glyphs.size());
		auto bitmap =
			static_cast<msdfgen::BitmapConstSection<msdf_atlas::byte, 4>>(generator.atlasStorage());
		bitmap.reorient(msdfgen::Y_DOWNWARD);
		FontBakeOutput output;
		output.requestedCodepointCount = static_cast<uint32_t>(charset.size());
		output.loadedGlyphCount = static_cast<uint32_t>(glyphsLoaded);
		if (request.charsetMode == FontCharsetMode::RuntimeAscii)
			output.asset.metadata = "runtime-msdf";
		output.asset.fieldKind = FontDistanceFieldKind::Mtsdf;
		auto& atlas = output.asset.atlas;
		atlas.width = static_cast<uint32_t>(atlasWidth);
		atlas.height = static_cast<uint32_t>(atlasHeight);
		atlas.pixels.resize(byteSize.value);
		const size_t rowBytes = static_cast<size_t>(atlasWidth) * 4u;
		for (int rowIndex = 0; rowIndex < atlasHeight; ++rowIndex)
			std::memcpy(atlas.pixels.data() + static_cast<size_t>(rowIndex) * rowBytes,
						bitmap(0, rowIndex), rowBytes);
		FontCpuVariant variant;
		variant.weight = request.weight;
		const auto& metrics = geometry.getMetrics();
		// Runtime historically stores pixels-per-em directly; Artery's offline export
		// stores that scale multiplied by source em size. Preserve both profiles.
		variant.fontSizePx = static_cast<float>(
			packer.getScale() *
			(request.charsetMode == FontCharsetMode::RuntimeAscii ? 1.0 : metrics.emSize));
		const auto range = packer.getPixelRange();
		variant.distanceRange = static_cast<float>(range.upper - range.lower);
		variant.distanceRangeMiddle = static_cast<float>(0.5 * (range.lower + range.upper));
		variant.emSize = static_cast<float>(metrics.emSize);
		variant.ascender = static_cast<float>(metrics.ascenderY);
		variant.descender = static_cast<float>(metrics.descenderY);
		variant.lineHeight = static_cast<float>(metrics.lineHeight);
		variant.underlineY = static_cast<float>(metrics.underlineY);
		variant.underlineThickness = static_cast<float>(metrics.underlineThickness);
		variant.name = faceName;
		if (request.charsetMode == FontCharsetMode::RuntimeAscii)
			variant.metadata = "runtime-msdf";
		variant.glyphs.reserve(glyphs.size());
		for (const auto& source : geometry.getGlyphs()) {
			double planeLeft = 0.0, planeBottom = 0.0, planeRight = 0.0, planeTop = 0.0;
			double imageLeft = 0.0, imageBottom = 0.0, imageRight = 0.0, imageTop = 0.0;
			source.getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);
			source.getQuadAtlasBounds(imageLeft, imageBottom, imageRight, imageTop);
			variant.glyphs.emplace_back(
				Font::GlyphData{.codepoint = source.getCodepoint(),
								.sourceImageIndex = 0u,
								.planeLeft = static_cast<float>(planeLeft),
								.planeBottom = static_cast<float>(planeBottom),
								.planeRight = static_cast<float>(planeRight),
								.planeTop = static_cast<float>(planeTop),
								.imageLeft = static_cast<float>(imageLeft),
								.imageBottom = static_cast<float>(imageBottom),
								.imageRight = static_cast<float>(imageRight),
								.imageTop = static_cast<float>(imageTop),
								.advanceX = static_cast<float>(source.getAdvance()),
								.advanceY = 0.0f});
			if (request.charsetMode == FontCharsetMode::RuntimeAscii &&
				source.getCodepoint() == '?')
				variant.fallbackGlyphIndex = static_cast<uint32_t>(variant.glyphs.size() - 1u);
		}
		variant.kerningPairs.reserve(geometry.getKerning().size());
		for (const auto& pair : geometry.getKerning()) {
			const auto* left = geometry.getGlyph(msdfgen::GlyphIndex(pair.first.first));
			const auto* right = geometry.getGlyph(msdfgen::GlyphIndex(pair.first.second));
			if (left && right && left->getCodepoint() != 0u && right->getCodepoint() != 0u)
				variant.kerningPairs.emplace_back(FontKerningPair{
					left->getCodepoint(), right->getCodepoint(), static_cast<float>(pair.second)});
		}
		output.asset.variants.emplace_back(std::move(variant));
		const auto valid = validateFontCpuAsset(output.asset);
		if (!valid)
			return unexpectedError(valid.error());
		return output;
	});
#else
	return unexpectedError(makeError(ErrorCode::FontBakeUnavailable, ErrorSite::FontBake, 0u,
									 request.sourceBytes.size()));
#endif
}
} // namespace FlowUi::detail::font
