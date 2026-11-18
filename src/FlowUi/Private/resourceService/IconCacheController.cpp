#include "FlowUi/Private/resourceService/IconCacheController.hpp"

#include <algorithm>
#include <functional>


namespace FlowUi::detail::manager_storage {

size_t IconVariantKeyHash::operator()(const IconVariantKey& key) const noexcept {
	size_t seed = std::hash<std::string>{}(key.nameKey);
	seed ^= std::hash<uint32_t>{}(key.requestedWidth) + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
	seed ^= std::hash<uint32_t>{}(key.requestedHeight) + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
	return seed;
}

IconCacheController::IconCacheController(storage::ActiveStorageSystem& storageSystem,
										 const IconManagerConfig& config)
	: storage(&storageSystem), atlasSize(std::max(1u, config.atlasSize)),
	  atlasPadding(config.atlasPadding), sizeReuseTolerance(std::max(1u, config.sizeBucketStep)),
	  maxAtlasPages(std::max(1u, config.maxAtlasPages)) {
	const storage::StringId name = storageSystem.intern("flowui.icon.atlas.sampler");
	atlasSampler = storageSystem.acquireSampler(storage::SamplerDesc{ .debugName = name,
		.minFilter = storage::FilterMode::Linear, .magFilter = storage::FilterMode::Linear,
		.addressU = storage::AddressMode::ClampToEdge, .addressV = storage::AddressMode::ClampToEdge,
		.addressW = storage::AddressMode::ClampToEdge,});
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
	while (!documentsByKey.empty()) {
		const auto document = documentsByKey.begin();
		const storage::BlobHandle source = document->second.source;
		documentsByKey.erase(document);
		try {
			if (source)
				storage->releaseBlob(source);
		} catch (...) {}
	}
	try { if (atlasSampler) storage->releaseSampler(atlasSampler); } catch (...) {}
}

} // namespace FlowUi::detail::manager_storage
