#include "devSystems/devTooling/DevTooling.hpp"

#if FLOW_UI_DEV_MODE

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif
#include "managers/ThemeManager.hpp"

namespace FlowUi::devSystems {

DevTooling::DevTooling() noexcept
	: overrides_(schemas_), bakePipeline_(schemas_, overrides_) {
#if defined(FLOWUI_BAKE_MANIFEST_PATH)
	try { bakePipeline_.setManifestPath(FLOWUI_BAKE_MANIFEST_PATH); } catch (...) {}
#endif
}
DevTooling::~DevTooling() = default;

void DevTooling::setOverlaySelection(
	WindowId window,
	const tooling::DevOverlaySelectionSpec& selection) noexcept {
	if (window == InvalidWindowId) return;
	try {
		for (WindowOverlaySelection& entry : overlaySelections_) {
			if (entry.window == window) {
				entry.selection = selection;
				return;
			}
		}
		overlaySelections_.push_back(WindowOverlaySelection{window, selection});
	} catch (...) {}
}

void DevTooling::clearOverlaySelection(WindowId window) noexcept {
	for (auto it = overlaySelections_.begin(); it != overlaySelections_.end(); ++it) {
		if (it->window == window) {
			overlaySelections_.erase(it);
			return;
		}
	}
}

bool DevTooling::overlaySelection(
	WindowId window,
	tooling::DevOverlaySelectionSpec& outSelection) const noexcept {
	for (const WindowOverlaySelection& entry : overlaySelections_) {
		if (entry.window == window) {
			outSelection = entry.selection;
			return true;
		}
	}
	return false;
}

tooling::DevCommandResult DevTooling::bakeActiveEdits() noexcept {
	return bakePipeline_.bake();
}

tooling::DevBakeStatusSnapshot DevTooling::queryBakeStatus() const noexcept {
	return bakePipeline_.queryStatus();
}

std::vector<tooling::DevBakeDiffEntry> DevTooling::queryBakeDiff() const noexcept {
	return bakePipeline_.queryDiff();
}

void DevTooling::commitAtSafePoint(
	ThemeManager& themes,
	DevTimingRecorder* timing) noexcept {
	overrides_.commitAtSafePoint(themes, timing);
}

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void DevTooling::appendDevMemorySamples(MemorySampleSink& sink) const noexcept {
	try {
		DevContainerMemoryAccumulator memory{};
		const std::size_t bytes = overrides_.memoryFootprintBytes() +
			overlays_.memoryFootprintBytes() +
			overlaySelections_.capacity() * sizeof(WindowOverlaySelection);
		memory.liveBytes = bytes;
		memory.capacityBytes = bytes;
		memory.objectCount = overrides_.stats().activeElementOverrides +
			overrides_.stats().activeThemeOverrides + overlaySelections_.size();
		memory.capacityCount = memory.objectCount;
		appendManagerSample(sink, memory_sources::kDevTooling.id, memory);
	} catch (...) {}
}
#endif

} // namespace FlowUi::devSystems

#endif
