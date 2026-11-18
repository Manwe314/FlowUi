#include "FlowUi/Private/textLayoutService/TextLayoutService.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/textLayoutService/TextLayoutValidation.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

namespace FlowUi::detail::text {

namespace {

[[nodiscard]] constexpr uint64_t hashText(std::string_view text) noexcept {
	uint64_t hash = 1469598103934665603ull;
	for (const unsigned char value : text) {
		hash ^= value;
		hash *= 1099511628211ull;
	}
	return hash;
}

static_assert(hashText("") == 1469598103934665603ull);
static_assert(hashText("A") == 4953232625885864614ull);

} // namespace

const Font::FontFaceData* resolveFontFace(
	const manager_storage::FontFrameView* fontView,
	FontId fontId) noexcept {
	if (!fontView) return nullptr;
	const Font::FontFaceData* face = fontView->font(fontId);
	if (!face) face = fontView->font(0);
	return face;
}

TextLayoutCacheKey TextLayoutService::makeKey(const TextLayoutRequest& request) noexcept {
	return TextLayoutCacheKey{
		.textHash = hashText(request.text),
		.textLength = request.text.size(),
		.fontCatalogIdentity = request.fontView
			? reinterpret_cast<uintptr_t>(request.fontView->fonts)
			: uintptr_t{0},
		.fontGeneration = request.fontView ? request.fontView->publicationRevision : 0,
		.fontId = request.fontId,
		.pointsToPixelsScaleBits = std::bit_cast<uint32_t>(request.pointsToPixelsScale),
		.fontSize = request.fontSize,
		.letterSpacing = request.letterSpacing,
		.tabWidth = request.tabWidth,
		.includeGlyphGeometry = request.includeGlyphGeometry,
	};
}

size_t TextLayoutService::resultByteCost(
	std::string_view text,
	const TextLayoutResult& result) noexcept {
	const auto addProduct = [](size_t current, size_t count, size_t width) constexpr noexcept {
		if (count > (std::numeric_limits<size_t>::max() - current) / width) {
			return std::numeric_limits<size_t>::max();
		}
		return current + count * width;
	};
	static_assert(addProduct(10, 2, 3) == 16);
	static_assert(addProduct(std::numeric_limits<size_t>::max(), 1, 1) ==
				  std::numeric_limits<size_t>::max());
	size_t total = text.size();
	total = addProduct(total, result.glyphs.size(), sizeof(TextLayoutGlyph));
	total = addProduct(total, result.clusters.size(), sizeof(TextLayoutCluster));
	total = addProduct(total, result.lines.size(), sizeof(TextLayoutLine));
	total = addProduct(total, result.caretStops.size(), sizeof(TextCaretStop));
	return total;
}

void TextLayoutService::evictFor(size_t incomingBytes) {
	while (!cache_.empty() &&
		(cache_.size() >= MaxCacheEntries || incomingBytes > MaxCacheBytes - std::min(cacheBytes_, MaxCacheBytes))) {
		const auto oldest = std::min_element(cache_.begin(), cache_.end(), [](const CacheEntry& a, const CacheEntry& b) {
			return a.lastUse < b.lastUse;
		});
		cacheBytes_ -= oldest->byteCost;
		FLOWUI_MEMORY_DEMAND_RECORD(
			capacity_demand_, FlowUi::detail::InitialCapacityTarget::ui_text_cache, cache_.size());
		cache_.erase(oldest);
	}
}

const TextLayoutResult& TextLayoutService::layout(const TextLayoutRequest& request) {
	const TextLayoutCacheKey key = makeKey(request);
	if (request.fontView && (!hasObservedFontCatalog_ ||
		key.fontCatalogIdentity != observedFontCatalogIdentity_ ||
		key.fontGeneration != observedFontGeneration_)) {
		clear();
		observedFontCatalogIdentity_ = key.fontCatalogIdentity;
		observedFontGeneration_ = key.fontGeneration;
		hasObservedFontCatalog_ = true;
	}
	for (CacheEntry& entry : cache_) {
		if (entry.key == key && entry.text == request.text) {
			entry.lastUse = ++useSequence_;
			return entry.result;
		}
	}

	const Font::FontFaceData* fontFace = resolveFontFace(request.fontView, request.fontId);
	uncachedResult_ = {};
	if (!fontFace)
		return uncachedResult_;
	(void)layout(
		TextProviderInput{
			.text = request.text,
			.pointsToPixelsScale = request.pointsToPixelsScale,
			.fontSize = request.fontSize,
			.letterSpacing = request.letterSpacing,
			.tabWidth = request.tabWidth,
			.includeGlyphGeometry = request.includeGlyphGeometry,
		},
		*fontFace);
	if (!uncachedResult_.success)
		return uncachedResult_;
	if (request.text.size() > MaxCachedTextBytes) return uncachedResult_;
	const size_t byteCost = resultByteCost(request.text, uncachedResult_);
	if (byteCost > MaxCacheBytes) return uncachedResult_;

	evictFor(byteCost);
	cache_.push_back(CacheEntry{
		.key = key,
		.text = std::string(request.text),
		.result = std::move(uncachedResult_),
		.lastUse = ++useSequence_,
		.byteCost = byteCost,
	});
	cacheBytes_ += byteCost;
	return cache_.back().result;
}

const TextLayoutResult& TextLayoutService::layout(const TextProviderInput& input,
												  const Font::FontFaceData& fontFace) {
	uncachedResult_ = {};
	auto computed = provider_.layout(input, fontFace);
	if (!computed) {
		auto error = computed.error();
		const auto* variant = fontFace.defaultVariant();
		const bool unusableFace =
			!variant || variant->glyphs.empty() || !std::isfinite(variant->emSize) ||
			!std::isfinite(variant->ascender) || !std::isfinite(variant->lineHeight);
		const float emPixels = variant ? textEmPixels(input, variant->fontSizePx) : 0.0f;
		const bool unusableScale =
			!std::isfinite(emPixels) || emPixels <= 0.0f ||
			(input.fontSize > 0 &&
			 (!std::isfinite(input.pointsToPixelsScale) || input.pointsToPixelsScale <= 0.0f));
		if (error.site == ErrorSite::TextLayoutCompute &&
			((error.code == ErrorCode::AssetPayloadInvalid && unusableFace) ||
			 (error.code == ErrorCode::RendererConfigurationInvalid && unusableScale))) {
			return uncachedResult_;
		}
		if (error.site == ErrorSite::TextLayoutCompute)
			error.subject = fontFace.id;
		throw FlowUiException(error);
	}
	const auto finalized = finalizeTextLayout(input, fontFace, *computed);
	if (!finalized)
		throw FlowUiException(finalized.error());
	uncachedResult_ = std::move(*computed);
	return uncachedResult_;
}

void TextLayoutService::clear() noexcept {
	FLOWUI_MEMORY_DEMAND_RECORD(
		capacity_demand_, FlowUi::detail::InitialCapacityTarget::ui_text_cache, cache_.size());
	cache_.clear();
	uncachedResult_ = {};
	cacheBytes_ = 0;
	useSequence_ = 0;
	observedFontCatalogIdentity_ = 0;
	observedFontGeneration_ = 0;
	hasObservedFontCatalog_ = false;
}

} // namespace FlowUi::detail::text
