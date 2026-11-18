#pragma once
#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "FlowUi/Private/resourceService/iconRasterProvider/IconRasterProviderSelection.hpp"
#include "FlowUi/Private/storageSystem/StorageSystemSelection.hpp"
#include "FlowUi/Public/configurationStructs/IconManagerConfig.hpp"
#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"

namespace FlowUi::detail::manager_storage {

struct IconDocumentRecord {
	icon::ActiveIconRasterProvider::Document document;
	storage::BlobHandle source{};
	float intrinsicWidth = 0, intrinsicHeight = 0;
};

struct IconTransientRasterResult {
	Rgba8Image image{};
};

struct IconAtlasRect { uint32_t x = 0, y = 0, w = 0, h = 0; };
struct IconAtlasAllocation {
	uint32_t pageIndex = std::numeric_limits<uint32_t>::max();
	IconAtlasRect paddedRect{}, contentRect{};
};
struct IconVariantKey {
	std::string nameKey{};
	uint32_t requestedWidth = 0, requestedHeight = 0;
	bool operator==(const IconVariantKey&) const noexcept = default;
};
struct IconVariantKeyHash { size_t operator()(const IconVariantKey& key) const noexcept; };
struct IconVariantEntry {
	IconVariantKey key{};
	uint32_t pageIndex = std::numeric_limits<uint32_t>::max();
	TextureHandle texture{};
	IconAtlasRect paddedRect{}, contentRect{};
	float uv0x = 0, uv0y = 0, uv1x = 1, uv1y = 1;
	uint32_t sourceWidth = 0, sourceHeight = 0, lastUsedFrame = 0;
	bool referencedThisFrame = false;
};
struct IconAtlasPage {
	storage::ImageHandle image{};
	storage::ImageViewHandle view{};
	uint32_t width = 0, height = 0;
	uint64_t usedArea = 0;
	uint32_t lastUsedFrame = 0;
	std::vector<IconAtlasRect> freeRects{};
};
struct IconRetiredRegion {
	TextureHandle texture{};
	uint32_t pageIndex = std::numeric_limits<uint32_t>::max();
	IconAtlasRect paddedRect{};
};

class IconCacheController {
public:
	IconCacheController(storage::ActiveStorageSystem& storageSystem,
						const IconManagerConfig& config);
	~IconCacheController() noexcept;

	::FlowUi::detail::MemoryDemandPeaks capacity_demand{};
	storage::ActiveStorageSystem* storage = nullptr;
	storage::SamplerHandle atlasSampler{};
	uint32_t atlasSize = 0, atlasPadding = 1, sizeReuseTolerance = 8, maxAtlasPages = 10, frameCounter = 0;
	[[no_unique_address]] icon::ActiveIconRasterProvider provider{};
	std::unordered_map<std::string, IconDocumentRecord> documentsByKey{};
	std::unordered_map<std::string, TextureHandle> requestTextureByKey{};
	std::unordered_map<uint64_t, std::string> requestedKeyByTexture{};
	std::unordered_map<IconVariantKey, IconVariantEntry, IconVariantKeyHash> variantsByKeyAndSize{};
	std::vector<IconAtlasPage> atlasPages{};
	std::vector<IconRetiredRegion> retiredRegions{};
};

} // namespace FlowUi::detail::manager_storage
