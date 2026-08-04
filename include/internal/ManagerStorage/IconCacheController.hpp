#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"

struct plutosvg_document;
using plutosvg_document_t = plutosvg_document;
struct plutovg_surface;
using plutovg_surface_t = plutovg_surface;

namespace FlowUi::detail::manager_storage {

struct IconDocumentRecord {
	storage::BlobHandle source{};
	plutosvg_document_t* document = nullptr;
	float intrinsicWidth = 0, intrinsicHeight = 0;
};

struct IconSurfaceOwner {
	plutovg_surface_t* surface = nullptr;
	IconSurfaceOwner() = default;
	explicit IconSurfaceOwner(plutovg_surface_t* value) : surface(value) {}
	IconSurfaceOwner(const IconSurfaceOwner&) = delete;
	IconSurfaceOwner& operator=(const IconSurfaceOwner&) = delete;
	IconSurfaceOwner(IconSurfaceOwner&& other) noexcept : surface(other.surface) { other.surface = nullptr; }
	IconSurfaceOwner& operator=(IconSurfaceOwner&& other) noexcept;
	~IconSurfaceOwner();
};

struct IconTransientRasterResult {
	IconSurfaceOwner owner{};
	const uint8_t* rgbaPixels = nullptr;
	uint32_t width = 0, height = 0, strideBytes = 0, requestedWidth = 0, requestedHeight = 0;
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
	IconCacheController(storage::IStorageSystem& storageSystem, const IconManagerConfig& config);
	~IconCacheController() noexcept;

	storage::IStorageSystem* storage = nullptr;
	storage::SamplerHandle atlasSampler{};
	uint32_t atlasSize = 0, atlasPadding = 1, sizeReuseTolerance = 8, maxAtlasPages = 10, frameCounter = 0;
	std::unordered_map<std::string, IconDocumentRecord> documentsByKey{};
	std::unordered_map<std::string, TextureHandle> requestTextureByKey{};
	std::unordered_map<uint64_t, std::string> requestedKeyByTexture{};
	std::unordered_map<IconVariantKey, IconVariantEntry, IconVariantKeyHash> variantsByKeyAndSize{};
	std::vector<IconAtlasPage> atlasPages{};
	std::vector<IconRetiredRegion> retiredRegions{};
};

} // namespace FlowUi::detail::manager_storage
