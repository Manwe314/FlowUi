#pragma once

#include "FlowUi/Private/resourceService/IconCacheController.hpp"
#include "FlowUi/Private/storageSystem/StorageBinding.hpp"
#include "FlowUi/Public/managers/IconManager.hpp"

namespace FlowUi::detail::icon {

/** Internal lifecycle/verification access, with the same ownership preconditions as App. */
struct IconManagerAccess {
	static void initialize(IconManager& manager, storage::StorageBinding& binding,
						   const IconManagerConfig& config) {
		manager.init(binding, config, IconGenerationFailurePolicy::FailRequest,
					 CacheCapacityPolicy::RejectOperation);
	}
	static void destroy(IconManager& manager) noexcept { manager.destroy(); }
	[[nodiscard]] static manager_storage::IconVariantEntry& variant(IconManager& manager,
																	std::string_view key,
																	uint32_t width, uint32_t height,
																	bool exact = false) {
		return manager.ensureVariantForRequest(key, width, height, exact);
	}
	[[nodiscard]] static manager_storage::IconTransientRasterResult
	raster(IconManager& manager, std::string_view key, uint32_t width, uint32_t height) {
		return manager.rasterizeForAtlas(key, width, height);
	}
	static void prepare(IconManager& manager, Clay_RenderCommandArray& commands) {
		manager.prepareFrameTextures(commands, 1.0f, 1.0f);
	}
	static void beginTick(IconManager& manager) { manager.beginAppTick(); }
	[[nodiscard]] static const manager_storage::IconCacheController&
	controller(const IconManager& manager) noexcept {
		return *manager.controller_;
	}
};

} // namespace FlowUi::detail::icon
