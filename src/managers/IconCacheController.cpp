#include "internal/ManagerStorage/IconCacheController.hpp"

#include <algorithm>
#include <functional>

#include <plutosvg.h>

namespace FlowUi::detail::manager_storage {

IconSurfaceOwner& IconSurfaceOwner::operator=(IconSurfaceOwner&& other) noexcept {
	if (this == &other) return *this;
	if (surface) plutovg_surface_destroy(surface);
	surface = other.surface;
	other.surface = nullptr;
	return *this;
}
IconSurfaceOwner::~IconSurfaceOwner() { if (surface) plutovg_surface_destroy(surface); }

size_t IconVariantKeyHash::operator()(const IconVariantKey& key) const noexcept {
	size_t seed = std::hash<std::string>{}(key.nameKey);
	seed ^= std::hash<uint32_t>{}(key.requestedWidth) + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
	seed ^= std::hash<uint32_t>{}(key.requestedHeight) + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
	return seed;
}

IconCacheController::IconCacheController(
	storage::IStorageSystem& storageSystem,
	const IconManagerConfig& config)
	: storage(&storageSystem), atlasSize(std::max(1u, config.atlasSize)),
	  atlasPadding(config.atlasPadding), sizeReuseTolerance(std::max(1u, config.sizeBucketStep)),
	  maxAtlasPages(std::max(1u, config.maxAtlasPages)) {
	const storage::StringId name = storageSystem.intern("flowui.icon.atlas.sampler");
	atlasSampler = storageSystem.acquireSampler(storage::SamplerDesc{
		.minFilter = storage::FilterMode::Linear, .magFilter = storage::FilterMode::Linear,
		.addressU = storage::AddressMode::ClampToEdge, .addressV = storage::AddressMode::ClampToEdge,
		.addressW = storage::AddressMode::ClampToEdge, .debugName = name,
	});
}

IconCacheController::~IconCacheController() noexcept {
	if (!storage) return;
	for (auto& [_, variant] : variantsByKeyAndSize) {
		try { if (variant.texture) storage->releaseAnonymousTexture(variant.texture); } catch (...) {}
	}
	for (auto& [key, _] : requestTextureByKey) {
		try { (void)storage->removeTexture({storage::ResourceDomain::Icon, storage->intern(key), InvalidWindowId}); }
		catch (...) {}
	}
	for (IconAtlasPage& page : atlasPages) {
		try { if (page.view) storage->releaseImageView(page.view); } catch (...) {}
		try { if (page.image) storage->releaseImage(page.image); } catch (...) {}
	}
	for (auto& [_, document] : documentsByKey) {
		if (document.document) plutosvg_document_destroy(document.document);
		try { if (document.source) storage->releaseBlob(document.source); } catch (...) {}
	}
	try { if (atlasSampler) storage->releaseSampler(atlasSampler); } catch (...) {}
}

} // namespace FlowUi::detail::manager_storage
