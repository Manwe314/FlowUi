#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOWUI_PUBLIC_VULKAN_INTEROP

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include <vulkan/vulkan.h>

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"
#include "managers/structs/ViewPortManagerStructs.hpp"

namespace FlowUi::devSystems {

using interface_elements::DevPreviewState;
using interface_elements::PreviewSelection;

/**
 * @brief Offscreen Vulkan subpass renderer for the DevPreview viewport scene.
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

	void init(const FlowUi::ViewPortVulkanInterop& interop, VkFormat colorFormat);
	void destroy();

	void record(
		const FlowUi::ViewPortRenderContext& ctx,
		const interface_elements::DevPreviewState& state,
		const interface_elements::PreviewSelection& selection,
		const DevUiReplaySource& replay);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

/**
 * @brief Free function helper to record the DevPreview viewport scene.
 */
void recordDevPreviewViewPort(
	const FlowUi::ViewPortRenderContext& ctx,
	const interface_elements::DevPreviewState& state,
	const interface_elements::PreviewSelection& selection,
	const DevUiReplaySource& replay);

} // namespace FlowUi::devSystems

#endif
