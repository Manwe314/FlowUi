#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>

#include "devSystems/devTooling/overlay/DevOverlayCommandBuffer.hpp"
#include "devSystems/devTooling/overlay/DevOverlayTypes.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "internal/Text/TextLayoutService.hpp"

namespace FlowUi::devSystems::tooling {

/** Renderer-only visual inspection sidecar. It does not own selection or input state. */
class DevOverlayService {
public:
	[[nodiscard]] size_t memoryFootprintBytes() const noexcept {
		return textLayoutService_.cacheBytes();
	}

	void generateOverlayCommands(
		const DevOverlaySelectionSpec& selection,
		const DevTreeSnapshot& treeSnapshot,
		float viewportWidth,
		float viewportHeight,
		DevOverlayCommandBuffer& outCommandBuffer,
		const Clay_RenderCommandArray* renderCommands = nullptr) noexcept;

	void buildUiRendererInstances(
		DevOverlayCommandBuffer& commandBuffer,
		const ::FlowUi::detail::manager_storage::FontFrameView& fontView,
		float pointsToPixelsScale,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY,
		float framebufferWidth,
		float framebufferHeight) noexcept;

private:
	::FlowUi::detail::text::TextLayoutService textLayoutService_{};
};

} // namespace FlowUi::devSystems::tooling

#endif
