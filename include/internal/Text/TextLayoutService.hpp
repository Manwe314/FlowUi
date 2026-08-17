#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "internal/Text/CurrentTextShaper.hpp"

namespace FlowUi::detail::text {

struct TextLayoutCacheKey {
	uint64_t textHash = 0;
	size_t textLength = 0;
	uintptr_t fontCatalogIdentity = 0;
	uint64_t fontGeneration = 0;
	FontId fontId = 0;
	uint32_t pointsToPixelsScaleBits = 0;
	uint16_t fontSize = 0;
	uint16_t letterSpacing = 0;
	uint8_t tabWidth = 4;
	bool includeGlyphGeometry = false;

	friend bool operator==(const TextLayoutCacheKey&, const TextLayoutCacheKey&) = default;
};

class TextLayoutService {
public:
	static constexpr size_t MaxCacheEntries = 256;
	static constexpr size_t MaxCacheBytes = 4u * 1024u * 1024u;
	static constexpr size_t MaxCachedTextBytes = 64u * 1024u;

	/** Result references remain valid until a later call mutates this service. */
	[[nodiscard]] const TextLayoutResult& layout(const TextLayoutRequest& request);
	void clear() noexcept;

	[[nodiscard]] size_t cacheEntryCount() const noexcept { return cache_.size(); }
	[[nodiscard]] size_t cacheBytes() const noexcept { return cacheBytes_; }

private:
	struct CacheEntry {
		TextLayoutCacheKey key{};
		std::string text{};
		TextLayoutResult result{};
		uint64_t lastUse = 0;
		size_t byteCost = 0;
	};

	[[nodiscard]] static TextLayoutCacheKey makeKey(const TextLayoutRequest& request) noexcept;
	[[nodiscard]] static size_t resultByteCost(
		std::string_view text,
		const TextLayoutResult& result) noexcept;
	void evictFor(size_t incomingBytes);

	CurrentTextShaper shaper_{};
	std::vector<CacheEntry> cache_{};
	TextLayoutResult uncachedResult_{};
	uint64_t useSequence_ = 0;
	size_t cacheBytes_ = 0;
	uintptr_t observedFontCatalogIdentity_ = 0;
	uint64_t observedFontGeneration_ = 0;
	bool hasObservedFontCatalog_ = false;
};

[[nodiscard]] const Font::FontFaceData* resolveFontFace(
	const manager_storage::FontFrameView* fontView,
	FontId fontId) noexcept;

} // namespace FlowUi::detail::text
