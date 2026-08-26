#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/WindowId.hpp"
#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#include "devSystems/devTooling/overlay/DevOverlayService.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace FlowUi { class ThemeManager; }

namespace FlowUi::devSystems {

class DevTimingRecorder;
class MemorySampleSink;

/** Development-only shell for live and offline developer-tool surfaces. */
class DevTooling {
public:
	DevTooling() noexcept;
	~DevTooling();

	DevTooling(const DevTooling&) = delete;
	DevTooling& operator=(const DevTooling&) = delete;
	DevTooling(DevTooling&&) = delete;
	DevTooling& operator=(DevTooling&&) = delete;

	[[nodiscard]] devMode::DevSchemaRegistry& schemas() noexcept { return schemas_; }
	[[nodiscard]] const devMode::DevSchemaRegistry& schemas() const noexcept { return schemas_; }
	[[nodiscard]] tooling::DevOverrideEngine& overrides() noexcept { return overrides_; }
	[[nodiscard]] const tooling::DevOverrideEngine& overrides() const noexcept { return overrides_; }
	[[nodiscard]] tooling::DevBakePipeline& bakePipeline() noexcept { return bakePipeline_; }
	[[nodiscard]] const tooling::DevBakePipeline& bakePipeline() const noexcept {
		return bakePipeline_;
	}
	[[nodiscard]] tooling::DevOverlayService& overlays() noexcept { return overlays_; }
	[[nodiscard]] const tooling::DevOverlayService& overlays() const noexcept { return overlays_; }

	/** Attachment surface for a future interface or picking controller. */
	void setOverlaySelection(
		WindowId window,
		const tooling::DevOverlaySelectionSpec& selection) noexcept;
	void clearOverlaySelection(WindowId window) noexcept;
	[[nodiscard]] bool overlaySelection(
		WindowId window,
		tooling::DevOverlaySelectionSpec& outSelection) const noexcept;

	/** Persist the current bakeable live edits to the active .flowchanges manifest. */
	tooling::DevCommandResult bakeActiveEdits() noexcept;
	[[nodiscard]] tooling::DevBakeStatusSnapshot queryBakeStatus() const noexcept;
	[[nodiscard]] std::vector<tooling::DevBakeDiffEntry> queryBakeDiff() const noexcept;

	void commitAtSafePoint(
		ThemeManager& themes,
		DevTimingRecorder* timing = nullptr) noexcept;
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(MemorySampleSink& sink) const noexcept;
#endif

private:
	struct WindowOverlaySelection {
		WindowId window = InvalidWindowId;
		tooling::DevOverlaySelectionSpec selection{};
	};

	devMode::DevSchemaRegistry schemas_{};
	tooling::DevOverrideEngine overrides_;
	tooling::DevBakePipeline bakePipeline_;
	tooling::DevOverlayService overlays_{};
	std::vector<WindowOverlaySelection> overlaySelections_{};
};

} // namespace FlowUi::devSystems

#endif
