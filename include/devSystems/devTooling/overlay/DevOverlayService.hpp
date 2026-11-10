#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>

#include "devSystems/devTooling/overlay/DevOverlayCommandBuffer.hpp"
#include "devSystems/devTooling/overlay/DevOverlayTypes.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
namespace FlowUi::detail::manager_storage { struct FontFrameView; }
#include "FlowUi/Private/textLayoutService/TextLayoutService.hpp"

namespace FlowUi::devSystems::tooling {

/** Renderer-only visual inspection sidecar. It does not own selection or input state. */
class DevOverlayService {
public:
	/** @brief Return retained overlay text-layout cache bytes. */
	[[nodiscard]] size_t memoryFootprintBytes() const noexcept {
		return textLayoutService_.cacheBytes();
	}

	/** @brief Generate visual-only primitives from a snapshot-local selection and viewport extent; output is caller-owned and optional renderCommands are borrowed for this call. */
	void generateOverlayCommands(
		const DevOverlaySelectionSpec& selection,
		const DevTreeSnapshot& treeSnapshot,
		float viewportWidth,
		float viewportHeight,
		DevOverlayCommandBuffer& outCommandBuffer,
		const Clay_RenderCommandArray* renderCommands = nullptr) noexcept;

	/** @brief Convert overlay primitives to contiguous draw instances using borrowed font-frame data and the supplied layout/framebuffer scales. */
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
