#pragma once

#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuBoundary.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontMSDFProvider.hpp"
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace FlowUi::test {

/** Independent CPU fixture provider with an exact, bounded private binary format. */
class DistinctFontMsdfProvider {
public:
	static constexpr uint32_t contractRevision = detail::font::fontMsdfContractRevision;
	static constexpr bool supportsBaking = true;
	static constexpr bool supportsEncoding = true;
	inline static std::atomic_uint64_t loadCalls{0}, bakeCalls{0}, encodeCalls{0};
	inline static std::atomic_bool invalidOutput{false};

	[[nodiscard]] Result<detail::font::FontBakeOutput>
	bake(const detail::font::FontBakeRequest& request) noexcept {
		return detail::font::fontCpuResult(
			ErrorSite::FontBake, [&]() -> Result<detail::font::FontBakeOutput> {
				++bakeCalls;
				if (request.charsetMode != detail::font::FontCharsetMode::RuntimeAscii &&
					request.charsetMode != detail::font::FontCharsetMode::OfflineUi)
					return unexpectedError(
						makeError(ErrorCode::AssetFormatUnsupported, ErrorSite::FontBake));
				if (request.sourceBytes.empty() || !std::isfinite(request.pixelSize) ||
					!(request.pixelSize > 0.0) || !std::isfinite(request.pixelRange) ||
					!(request.pixelRange > 0.0) || !std::isfinite(request.angleThreshold) ||
					request.angleThreshold < 0.0 || !std::isfinite(request.miterLimit) ||
					request.miterLimit < 0.0 || request.workerThreadCount == 0u ||
					((request.pageWidth == 0u) != (request.pageHeight == 0u)))
					return unexpectedError(
						makeError(ErrorCode::FontBakeFailed, ErrorSite::FontBake));
				detail::font::FontBakeOutput output;
				auto& asset = output.asset;
				asset.metadata = "independent synthetic MSDF fixture";
				asset.atlas.width = request.pageWidth ? request.pageWidth : 4u;
				asset.atlas.height = request.pageHeight ? request.pageHeight : 4u;
				const auto byteSize = detail::rgba8ByteSize(asset.atlas.width, asset.atlas.height);
				if (!byteSize.valid || asset.atlas.width < 4u || asset.atlas.height < 4u)
					return unexpectedError(
						makeError(ErrorCode::FontAtlasCapacityExceeded, ErrorSite::FontBake));
				asset.atlas.pixels.resize(byteSize.value, std::byte{0});
				for (uint32_t rowIndex = 0; rowIndex < 4u; ++rowIndex) {
					for (uint32_t columnIndex = 0; columnIndex < 4u; ++columnIndex) {
						const std::byte distance{
							static_cast<unsigned char>((rowIndex == 0u || rowIndex == 3u ||
														columnIndex == 0u || columnIndex == 3u)
														   ? 32
														   : 224)};
						const size_t offset =
							(static_cast<size_t>(rowIndex) * asset.atlas.width + columnIndex) * 4u;
						for (size_t channelIndex = 0; channelIndex < 4u; ++channelIndex)
							asset.atlas.pixels[offset + channelIndex] = distance;
					}
				}
				detail::font::FontCpuVariant variant;
				variant.name = std::string(request.faceName);
				variant.weight = request.weight;
				variant.fontSizePx = static_cast<float>(request.pixelSize);
				variant.distanceRange = static_cast<float>(request.pixelRange);
				variant.emSize = 1.0f;
				variant.ascender = 0.9f;
				variant.descender = -0.3f;
				variant.lineHeight = 1.4f;
				variant.glyphs.reserve(95u);
				const float advance =
					0.8f + static_cast<float>(
							   std::to_integer<unsigned>(request.sourceBytes.front()) % 3u) *
							   0.01f;
				for (uint32_t codepoint = 32u; codepoint < 127u; ++codepoint) {
					variant.glyphs.emplace_back(
						Font::GlyphData{.codepoint = codepoint,
										.planeRight = codepoint == 32u ? 0.0f : 0.7f,
										.planeTop = codepoint == 32u ? 0.0f : 1.0f,
										.imageRight = codepoint == 32u ? 0.0f : 4.0f,
										.imageTop = codepoint == 32u ? 0.0f : 4.0f,
										.advanceX = advance});
				}
				variant.fallbackGlyphIndex = '?' - 32u;
				variant.kerningPairs.emplace_back(detail::font::FontKerningPair{'A', 'V', -0.05f});
				asset.variants.emplace_back(std::move(variant));
				output.requestedCodepointCount = 95u;
				output.loadedGlyphCount = 95u;
				if (invalidOutput)
					asset.variants.front().glyphs.front().sourceImageIndex = 1u;
				return output;
			});
	}

	[[nodiscard]] Result<std::vector<std::byte>>
	encodeBaked(const detail::font::FontCpuAsset& asset) noexcept {
		return detail::font::fontCpuResult(
			ErrorSite::FontParse, [&]() -> Result<std::vector<std::byte>> {
				++encodeCalls;
				const auto valid = detail::font::validateFontCpuAsset(asset);
				if (!valid)
					return unexpectedError(valid.error());
				Writer writer;
				writer.integer(0x31464344u);
				writer.integer(static_cast<uint32_t>(asset.fieldKind));
				writer.integer(asset.atlas.width);
				writer.integer(asset.atlas.height);
				writer.string(asset.metadata);
				writer.array(asset.atlas.pixels);
				writer.count(asset.variants.size());
				for (const auto& variant : asset.variants) {
					writer.string(variant.name);
					writer.string(variant.metadata);
					writer.integer(variant.flags);
					writer.integer(variant.weight);
					writer.integer(variant.fallbackGlyphIndex);
					for (float metric :
						 {variant.fontSizePx, variant.distanceRange, variant.distanceRangeMiddle,
						  variant.emSize, variant.ascender, variant.descender, variant.lineHeight,
						  variant.underlineY, variant.underlineThickness})
						writer.scalar(metric);
					writer.count(variant.glyphs.size());
					for (const auto& glyph : variant.glyphs) {
						writer.integer(glyph.codepoint);
						writer.integer(glyph.sourceImageIndex);
						for (float metric :
							 {glyph.planeLeft, glyph.planeBottom, glyph.planeRight, glyph.planeTop,
							  glyph.imageLeft, glyph.imageBottom, glyph.imageRight, glyph.imageTop,
							  glyph.advanceX, glyph.advanceY})
							writer.scalar(metric);
					}
					writer.count(variant.kerningPairs.size());
					for (const auto& pair : variant.kerningPairs) {
						writer.integer(pair.leftCodepoint);
						writer.integer(pair.rightCodepoint);
						writer.scalar(pair.advance);
					}
				}
				return std::move(writer.bytes);
			});
	}

	[[nodiscard]] Result<detail::font::FontCpuAsset>
	loadBaked(std::span<const std::byte> source) noexcept {
		return detail::font::fontCpuResult(
			ErrorSite::FontParse, [&]() -> Result<detail::font::FontCpuAsset> {
				++loadCalls;
				Reader reader{source};
				if (reader.integer() != 0x31464344u)
					fail();
				detail::font::FontCpuAsset asset;
				const auto fieldKind = reader.integer();
				if (fieldKind > 1u)
					fail();
				asset.fieldKind = static_cast<detail::font::FontDistanceFieldKind>(fieldKind);
				asset.atlas.width = reader.integer();
				asset.atlas.height = reader.integer();
				asset.metadata = reader.string();
				const auto pixels = reader.array();
				asset.atlas.pixels.assign(pixels.begin(), pixels.end());
				const uint32_t variants = reader.count(56u);
				asset.variants.reserve(variants);
				for (uint32_t variantIndex = 0; variantIndex < variants; ++variantIndex) {
					detail::font::FontCpuVariant variant;
					variant.name = reader.string();
					variant.metadata = reader.string();
					variant.flags = reader.integer();
					variant.weight = reader.integer();
					variant.fallbackGlyphIndex = reader.integer();
					for (float* metric :
						 {&variant.fontSizePx, &variant.distanceRange, &variant.distanceRangeMiddle,
						  &variant.emSize, &variant.ascender, &variant.descender,
						  &variant.lineHeight, &variant.underlineY, &variant.underlineThickness})
						*metric = reader.scalar();
					const uint32_t glyphCount = reader.count(48u);
					variant.glyphs.reserve(glyphCount);
					for (uint32_t glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
						Font::GlyphData glyph;
						glyph.codepoint = reader.integer();
						glyph.sourceImageIndex = reader.integer();
						for (float* metric :
							 {&glyph.planeLeft, &glyph.planeBottom, &glyph.planeRight,
							  &glyph.planeTop, &glyph.imageLeft, &glyph.imageBottom,
							  &glyph.imageRight, &glyph.imageTop, &glyph.advanceX, &glyph.advanceY})
							*metric = reader.scalar();
						variant.glyphs.emplace_back(glyph);
					}
					const uint32_t pairCount = reader.count(12u);
					variant.kerningPairs.reserve(pairCount);
					for (uint32_t pairIndex = 0; pairIndex < pairCount; ++pairIndex) {
						const auto left = reader.integer();
						const auto right = reader.integer();
						const auto advance = reader.scalar();
						variant.kerningPairs.emplace_back(
							detail::font::FontKerningPair{left, right, advance});
					}
					asset.variants.emplace_back(std::move(variant));
				}
				if (!reader.bytes.empty())
					fail();
				const auto valid = detail::font::validateFontCpuAsset(asset);
				if (!valid)
					return unexpectedError(valid.error());
				if (invalidOutput)
					asset.variants.front().glyphs.front().sourceImageIndex = 1u;
				return asset;
			});
	}

private:
	[[noreturn]] static void fail() {
		throw FlowUiException(makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::FontParse));
	}
	struct Writer {
		std::vector<std::byte> bytes;
		void integer(uint32_t value) {
			for (unsigned shift = 0; shift < 32; shift += 8)
				bytes.emplace_back(std::byte{static_cast<unsigned char>(value >> shift)});
		}
		void scalar(float value) { integer(std::bit_cast<uint32_t>(value)); }
		void count(size_t value) {
			if (value > std::numeric_limits<uint32_t>::max())
				fail();
			integer(static_cast<uint32_t>(value));
		}
		void array(std::span<const std::byte> value) {
			count(value.size());
			bytes.insert(bytes.end(), value.begin(), value.end());
		}
		void string(const std::string& value) { array(std::as_bytes(std::span(value))); }
	};
	struct Reader {
		std::span<const std::byte> bytes;
		[[nodiscard]] uint32_t integer() {
			if (bytes.size() < 4u)
				fail();
			uint32_t value = 0;
			for (unsigned index = 0; index < 4; ++index)
				value |= std::to_integer<uint32_t>(bytes[index]) << (index * 8u);
			bytes = bytes.subspan(4u);
			return value;
		}
		[[nodiscard]] float scalar() { return std::bit_cast<float>(integer()); }
		[[nodiscard]] uint32_t count(size_t minimumRecordBytes) {
			const auto value = integer();
			if (value > bytes.size() / minimumRecordBytes)
				fail();
			return value;
		}
		[[nodiscard]] std::span<const std::byte> array() {
			const auto length = integer();
			if (length > bytes.size())
				fail();
			const auto value = bytes.first(length);
			bytes = bytes.subspan(length);
			return value;
		}
		[[nodiscard]] std::string string() {
			const auto value = array();
			return value.empty()
					   ? std::string{}
					   : std::string(reinterpret_cast<const char*>(value.data()), value.size());
		}
	};
};
static_assert(detail::font::FontMSDFProvider<DistinctFontMsdfProvider>);
} // namespace FlowUi::test
