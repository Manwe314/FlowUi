#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuBoundary.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/providers/msdfAtlas/MsdfAtlasFontProvider.hpp"
#include <algorithm>
#include <artery-font/std-artery-font.h>
#include <cstring>
#include <limits>
#include <memory>
#include "stb_image.h"

namespace FlowUi::detail::font {
namespace {
using ArteryFont = artery_font::StdArteryFont<float>;
static_assert(legacyImageType(FontDistanceFieldKind::Msdf) == artery_font::IMAGE_MSDF);
static_assert(legacyImageType(FontDistanceFieldKind::Mtsdf) == artery_font::IMAGE_MTSDF);
[[noreturn]] void invalidFont(ErrorCode code = ErrorCode::AssetPayloadInvalid) {
	throw FlowUiException(makeError(code, ErrorSite::FontParse));
}
struct BakedFontMemoryReader {
	std::span<const std::byte> remaining;
};
[[nodiscard]] int readArteryMemory(void* destination, int length, void* context) noexcept {
	if (length <= 0)
		return 0;
	auto& reader = *static_cast<BakedFontMemoryReader*>(context);
	const size_t byteCount = std::min(static_cast<size_t>(length), reader.remaining.size());
	if (byteCount != 0)
		std::memcpy(destination, reader.remaining.data(), byteCount);
	reader.remaining = reader.remaining.subspan(byteCount);
	return static_cast<int>(byteCount);
}
[[nodiscard]] bool supportedEncoding(artery_font::ImageEncoding encoding) noexcept {
	return encoding == artery_font::IMAGE_PNG || encoding == artery_font::IMAGE_RAW_BINARY;
}
[[nodiscard]] int pickAtlasImageIndex(const ArteryFont& font) noexcept {
	if (font.variants.length() > 0) {
		const auto preferredType = font.variants[0].imageType;
		for (int index = 0; index < font.images.length(); ++index) {
			if (font.images[index].imageType == preferredType &&
				supportedEncoding(font.images[index].encoding))
				return index;
		}
	}
	for (int index = 0; index < font.images.length(); ++index) {
		if (supportedEncoding(font.images[index].encoding))
			return index;
	}
	return -1;
}
[[nodiscard]] std::string sourceString(const artery_font::StdString& text) {
	return std::string(static_cast<const char*>(text), static_cast<size_t>(text.length()));
}
struct StbiImageDeleter {
	void operator()(stbi_uc* pixels) const noexcept { stbi_image_free(pixels); }
};
[[nodiscard]] Rgba8Image decodeImage(const ArteryFont::Image& image) {
	const auto byteSize = rgba8ByteSize(image.width, image.height);
	if (!byteSize.valid)
		invalidFont(ErrorCode::ImageSizeOverflow);
	const auto* bytes = static_cast<const unsigned char*>(image.data);
	const size_t sourceByteCount = static_cast<size_t>(image.data.length());
	Rgba8Image result{.width = image.width, .height = image.height};
	if (image.encoding == artery_font::IMAGE_PNG) {
		if (sourceByteCount == 0 ||
			sourceByteCount > static_cast<size_t>(std::numeric_limits<int>::max()))
			invalidFont();
		int width = 0, height = 0, channels = 0;
		std::unique_ptr<stbi_uc, StbiImageDeleter> pixels(stbi_load_from_memory(
			bytes, static_cast<int>(sourceByteCount), &width, &height, &channels, 4));
		if (!pixels)
			invalidFont(ErrorCode::ImageDecodeFailed);
		if (width <= 0 || height <= 0 || static_cast<uint32_t>(width) != image.width ||
			static_cast<uint32_t>(height) != image.height)
			invalidFont();
		if (channels != 3 && channels != 4)
			invalidFont(ErrorCode::AssetFormatUnsupported);
		result.pixels.resize(byteSize.value);
		std::memcpy(result.pixels.data(), pixels.get(), byteSize.value);
	} else if (image.encoding == artery_font::IMAGE_RAW_BINARY) {
		if (image.pixelFormat != artery_font::PIXEL_UNSIGNED8 ||
			(image.channels != 3u && image.channels != 4u))
			invalidFont(ErrorCode::AssetFormatUnsupported);
		if (static_cast<size_t>(image.width) > std::numeric_limits<size_t>::max() / image.channels)
			invalidFont(ErrorCode::ImageSizeOverflow);
		const size_t pixelRowBytes = static_cast<size_t>(image.width) * image.channels;
		const size_t rowBytes =
			image.rawBinaryFormat.rowLength ? image.rawBinaryFormat.rowLength : pixelRowBytes;
		if (rowBytes < pixelRowBytes ||
			image.height > std::numeric_limits<size_t>::max() / rowBytes ||
			sourceByteCount < rowBytes * image.height)
			invalidFont();
		if (image.rawBinaryFormat.orientation != artery_font::ORIENTATION_TOP_DOWN &&
			image.rawBinaryFormat.orientation != artery_font::ORIENTATION_BOTTOM_UP)
			invalidFont();
		result.pixels.resize(byteSize.value);
		const bool topDown = image.rawBinaryFormat.orientation == artery_font::ORIENTATION_TOP_DOWN;
		for (uint32_t rowIndex = 0; rowIndex < image.height; ++rowIndex) {
			const uint32_t sourceRow = topDown ? rowIndex : image.height - rowIndex - 1u;
			const unsigned char* source = bytes + static_cast<size_t>(sourceRow) * rowBytes;
			std::byte* destination =
				result.pixels.data() + static_cast<size_t>(rowIndex) * image.width * 4u;
			for (uint32_t columnIndex = 0; columnIndex < image.width; ++columnIndex) {
				for (size_t channelIndex = 0; channelIndex < 3; ++channelIndex)
					destination[columnIndex * 4u + channelIndex] =
						std::byte{source[columnIndex * image.channels + channelIndex]};
				destination[columnIndex * 4u + 3u] = image.channels == 4u
														 ? std::byte{source[columnIndex * 4u + 3u]}
														 : std::byte{255};
			}
		}
	} else
		invalidFont(ErrorCode::AssetFormatUnsupported);
	return result;
}
struct BakedFontMemoryWriter {
	std::vector<std::byte> bytes;
	FlowUiError failure{};
};
[[nodiscard]] int writeArteryMemory(const void* source, int length, void* context) noexcept {
	auto& writer = *static_cast<BakedFontMemoryWriter*>(context);
	if (length < 0 || writer.failure)
		return 0;
	try {
		const auto* begin = static_cast<const std::byte*>(source);
		writer.bytes.insert(writer.bytes.end(), begin, begin + length);
		return length;
	} catch (const std::bad_alloc&) {
		writer.failure = makeError(ErrorCode::AllocationFailed, ErrorSite::FontParse);
	} catch (...) {
		writer.failure = makeError(ErrorCode::ForeignExceptionObserved, ErrorSite::FontParse);
	}
	return 0;
}
[[nodiscard]] int checkedCount(size_t count) {
	if (count > static_cast<size_t>(std::numeric_limits<int>::max()))
		invalidFont(ErrorCode::ImageSizeOverflow);
	return static_cast<int>(count);
}
[[nodiscard]] artery_font::StdString arteryString(const std::string& text) {
	return artery_font::StdString(text.data(), checkedCount(text.size()));
}
} // namespace

Result<FontCpuAsset>
MsdfAtlasFontProvider::loadBaked(std::span<const std::byte> bakedBytes) noexcept {
	return fontCpuResult(ErrorSite::FontParse, [&]() -> Result<FontCpuAsset> {
		ArteryFont decoded{};
		BakedFontMemoryReader reader{bakedBytes};
		if (bakedBytes.empty() || !artery_font::decode<readArteryMemory>(decoded, &reader))
			invalidFont(ErrorCode::AssetReadFailed);
		if (decoded.variants.length() <= 0 || decoded.images.length() <= 0)
			invalidFont();
		const int imageIndex = pickAtlasImageIndex(decoded);
		if (imageIndex < 0)
			invalidFont(ErrorCode::AssetFormatUnsupported);
		const auto& image = decoded.images[imageIndex];
		if (image.imageType != artery_font::IMAGE_MSDF &&
			image.imageType != artery_font::IMAGE_MTSDF)
			invalidFont(ErrorCode::AssetFormatUnsupported);
		FontCpuAsset asset;
		asset.atlas = decodeImage(image);
		asset.fieldKind = image.imageType == artery_font::IMAGE_MSDF ? FontDistanceFieldKind::Msdf
																	 : FontDistanceFieldKind::Mtsdf;
		asset.metadata = sourceString(decoded.metadata);
		asset.variants.reserve(static_cast<size_t>(decoded.variants.length()));
		for (int variantIndex = 0; variantIndex < decoded.variants.length(); ++variantIndex) {
			const auto& source = decoded.variants[variantIndex];
			if (source.codepointType != artery_font::CP_UNICODE ||
				source.imageType != image.imageType)
				invalidFont(ErrorCode::AssetFormatUnsupported);
			FontCpuVariant variant;
			variant.flags = source.flags;
			variant.weight = source.weight;
			variant.fallbackGlyphIndex = source.fallbackGlyph;
			variant.fontSizePx = source.metrics.fontSize;
			variant.distanceRange = source.metrics.distanceRange;
			variant.distanceRangeMiddle = source.metrics.distanceRangeMiddle;
			variant.emSize = source.metrics.emSize;
			variant.ascender = source.metrics.ascender;
			variant.descender = source.metrics.descender;
			variant.lineHeight = source.metrics.lineHeight;
			variant.underlineY = source.metrics.underlineY;
			variant.underlineThickness = source.metrics.underlineThickness;
			variant.name = sourceString(source.name);
			variant.metadata = sourceString(source.metadata);
			variant.glyphs.reserve(static_cast<size_t>(source.glyphs.length()));
			for (int glyphIndex = 0; glyphIndex < source.glyphs.length(); ++glyphIndex) {
				const auto& glyph = source.glyphs[glyphIndex];
				if (glyph.image != static_cast<uint32_t>(imageIndex))
					invalidFont(ErrorCode::AssetFormatUnsupported);
				variant.glyphs.emplace_back(Font::GlyphData{.codepoint = glyph.codepoint,
															.sourceImageIndex = 0u,
															.planeLeft = glyph.planeBounds.l,
															.planeBottom = glyph.planeBounds.b,
															.planeRight = glyph.planeBounds.r,
															.planeTop = glyph.planeBounds.t,
															.imageLeft = glyph.imageBounds.l,
															.imageBottom = glyph.imageBounds.b,
															.imageRight = glyph.imageBounds.r,
															.imageTop = glyph.imageBounds.t,
															.advanceX = glyph.advance.h,
															.advanceY = glyph.advance.v});
			}
			variant.kerningPairs.reserve(static_cast<size_t>(source.kernPairs.length()));
			for (int pairIndex = 0; pairIndex < source.kernPairs.length(); ++pairIndex) {
				const auto& pair = source.kernPairs[pairIndex];
				variant.kerningPairs.emplace_back(
					FontKerningPair{pair.codepoint1, pair.codepoint2, pair.advance.h});
			}
			asset.variants.emplace_back(std::move(variant));
		}
		const auto valid = validateFontCpuAsset(asset);
		if (!valid)
			return unexpectedError(valid.error());
		return asset;
	});
}

Result<std::vector<std::byte>>
MsdfAtlasFontProvider::encodeBaked(const FontCpuAsset& asset) noexcept {
	return fontCpuResult(ErrorSite::FontParse, [&]() -> Result<std::vector<std::byte>> {
		const auto valid = validateFontCpuAsset(asset);
		if (!valid)
			return unexpectedError(valid.error());
		ArteryFont encoded{};
		encoded.metadataFormat = artery_font::METADATA_PLAINTEXT;
		encoded.metadata = arteryString(asset.metadata);
		encoded.variants =
			artery_font::StdList<ArteryFont::Variant>(checkedCount(asset.variants.size()));
		const auto imageType =
			static_cast<artery_font::ImageType>(legacyImageType(asset.fieldKind));
		for (size_t variantIndex = 0; variantIndex < asset.variants.size(); ++variantIndex) {
			const auto& source = asset.variants[variantIndex];
			auto& destination = encoded.variants[static_cast<int>(variantIndex)];
			destination = ArteryFont::Variant{};
			destination.flags = source.flags;
			destination.weight = source.weight;
			destination.codepointType = artery_font::CP_UNICODE;
			destination.imageType = imageType;
			destination.fallbackGlyph = source.fallbackGlyphIndex;
			destination.metrics.fontSize = source.fontSizePx;
			destination.metrics.distanceRange = source.distanceRange;
			destination.metrics.distanceRangeMiddle = source.distanceRangeMiddle;
			destination.metrics.emSize = source.emSize;
			destination.metrics.ascender = source.ascender;
			destination.metrics.descender = source.descender;
			destination.metrics.lineHeight = source.lineHeight;
			destination.metrics.underlineY = source.underlineY;
			destination.metrics.underlineThickness = source.underlineThickness;
			destination.name = arteryString(source.name);
			destination.metadata = arteryString(source.metadata);
			destination.glyphs =
				artery_font::StdList<artery_font::Glyph<float>>(checkedCount(source.glyphs.size()));
			for (size_t glyphIndex = 0; glyphIndex < source.glyphs.size(); ++glyphIndex) {
				const auto& glyph = source.glyphs[glyphIndex];
				auto& target = destination.glyphs[static_cast<int>(glyphIndex)];
				target = artery_font::Glyph<float>{};
				target.codepoint = glyph.codepoint;
				target.image = 0u;
				target.planeBounds = {glyph.planeLeft, glyph.planeBottom, glyph.planeRight,
									  glyph.planeTop};
				target.imageBounds = {glyph.imageLeft, glyph.imageBottom, glyph.imageRight,
									  glyph.imageTop};
				target.advance = {glyph.advanceX, glyph.advanceY};
			}
			destination.kernPairs = artery_font::StdList<artery_font::KernPair<float>>(
				checkedCount(source.kerningPairs.size()));
			for (size_t pairIndex = 0; pairIndex < source.kerningPairs.size(); ++pairIndex) {
				const auto& pair = source.kerningPairs[pairIndex];
				destination.kernPairs[static_cast<int>(pairIndex)] = {
					pair.leftCodepoint, pair.rightCodepoint, {pair.advance, 0.0f}};
			}
		}
		encoded.images = artery_font::StdList<ArteryFont::Image>(1);
		auto& image = encoded.images[0];
		image = ArteryFont::Image{};
		image.width = asset.atlas.width;
		image.height = asset.atlas.height;
		image.channels = 4u;
		image.pixelFormat = artery_font::PIXEL_UNSIGNED8;
		image.imageType = imageType;
		image.encoding = artery_font::IMAGE_RAW_BINARY;
		if (image.width > std::numeric_limits<uint32_t>::max() / 4u)
			invalidFont(ErrorCode::ImageSizeOverflow);
		image.rawBinaryFormat.rowLength = image.width * 4u;
		image.rawBinaryFormat.orientation = artery_font::ORIENTATION_TOP_DOWN;
		image.data = artery_font::StdByteArray(checkedCount(asset.atlas.pixels.size()));
		std::memcpy(static_cast<unsigned char*>(image.data), asset.atlas.pixels.data(),
					asset.atlas.pixels.size());
		BakedFontMemoryWriter writer;
		if (!artery_font::encode<writeArteryMemory>(encoded, &writer)) {
			return unexpectedError(
				writer.failure ? writer.failure
							   : makeError(ErrorCode::AssetReadFailed, ErrorSite::FontParse));
		}
		if (writer.failure)
			return unexpectedError(writer.failure);
		return std::move(writer.bytes);
	});
}
} // namespace FlowUi::detail::font
