#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOWUI_PUBLIC_VULKAN_INTEROP

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include <vulkan/vulkan.h>

#include "FlowUi/Public/App.hpp"
#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"
#include "FlowUi/Public/interop/vulkan/Viewport.hpp"

namespace FlowUi::devSystems {

using interface_elements::DevPreviewState;
using interface_elements::PreviewSelection;

/*
 * Offscreen Vulkan subpass renderer for the DevPreview viewport scene.
 *
 * Handles recording of viewport background, grid lines, captured element geometry,
 * sidecar overlays, and ruler measurement tools directly into the ViewPort
 * offscreen command buffer context.
 */
class DevPreviewViewPortRenderer {
public:
	DevPreviewViewPortRenderer();
	~DevPreviewViewPortRenderer();

	DevPreviewViewPortRenderer(const DevPreviewViewPortRenderer&) = delete;
	DevPreviewViewPortRenderer& operator=(const DevPreviewViewPortRenderer&) = delete;

	void record(
		const FlowUi::ViewPortRenderContext& ctx,
		interface_elements::DevPreviewState& state,
		const DevUiReplaySource& replay);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace FlowUi::devSystems

#endif
