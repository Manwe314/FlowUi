#pragma once
#include "DevTimelineLayout.hpp"
#if FLOW_UI_DEV_MODE
#include <memory>

struct VulkanUiRenderer;
namespace FlowUi {
class ViewPortManager;
namespace detail::manager_storage {
struct FontFrameView;
}
namespace devSystems::interface_elements {
/** Per-frame work counters for verification and profiling. */
struct TimelineViewportStats {
	uint64_t visible_pixels = 0, uploaded_bytes = 0;
	size_t surfaces = 0, instances = 0, runs = 0, layout_builds = 0, base_builds = 0;
};
/** Window-owned rendering and submission-aligned input state. */
class DevTimelineController {
public:
	DevTimelineController();
	~DevTimelineController();
	DevTimelineController(const DevTimelineController&) = delete;
	DevTimelineController& operator=(const DevTimelineController&) = delete;
	/** Called after the frame-slot fence; completed serial controls upload retirement. */
	void begin_frame(ViewPortManager& viewports, VulkanUiRenderer& renderer,
					 uint64_t completed_serial);
	/** Resolve final Clay bounds and borrow only this frame's font/descriptor resources. */
	void prepare(const ::FlowUi::detail::manager_storage::FontFrameView& fonts, float font_scale,
				 uint32_t texture_slot);
	/** Promote geometry only after successful queue submission. */
	void submitted(uint64_t serial);
	/** Current frame counters, independent of timing-capture settings. */
	[[nodiscard]] TimelineViewportStats stats() const noexcept;
	/** Disable callbacks and free resources after the hosting window has drained. */
	void destroy_drained();
	/** Own wheel events over a previously submitted macro plot before Clay scrolling. */
	[[nodiscard]] bool owns_scroll(const FrameInput& input) const noexcept;
	void draw(UiManager& manager, Clay_ElementId id, Clay_ElementId clip, DevTimelineState& state,
			  const DevPerformanceSelection& selection, TimelineSurfaceKind kind, size_t card_index,
			  Clay_ElementId column_clip = {});

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
/** Emit one full-height placeholder and one bounded viewport image. */
void timeline_viewport(UiManager& manager, Clay_ElementId id, DevTimelineParameters parameters,
					   TimelineSurfaceKind kind);
} // namespace devSystems::interface_elements
} // namespace FlowUi
#endif
