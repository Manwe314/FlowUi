#pragma once
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/managers/UiManager.hpp"
#include <memory>
struct VulkanUiRenderer;
namespace FlowUi {
namespace detail::manager_storage {
struct FontFrameView;
}
class ViewPortManager;
namespace devSystems::interface_elements {
/* Window-owned chart resource and submission-aligned input controller. */
class DevMemoryHistoryController {
public:
	DevMemoryHistoryController();
	~DevMemoryHistoryController();
	/* Reset visibility after the hosting frame fence. */
	void begin_frame(ViewPortManager& viewports, VulkanUiRenderer& renderer,
					 uint64_t completed_serial);
	/* Bind one chart, process gestures and emit the viewport image. */
	void draw(UiManager& manager, Clay_ElementId image, DevMemoryWorkbenchState& state,
			  DevMemoryReporting& reporting);
	/* Resolve final layout bounds before viewport callbacks run. */
	void prepare(const ::FlowUi::detail::manager_storage::FontFrameView& fonts, float font_scale,
				 uint32_t texture_slot);
	/* Promote pending geometry only after successful submission. */
	void submitted(uint64_t serial);
	/* Clear callbacks and release freeze before window resources are destroyed. */
	void destroy_drained();
	/* Estimated owned CPU presentation capacity; GPU targets are reported separately. */
	[[nodiscard]] uint64_t capacity_bytes() const noexcept;
	/* Consume plot scrolling before Clay's enclosing scroll containers. */
	[[nodiscard]] bool owns_scroll(const FrameInput& input) const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace devSystems::interface_elements
} // namespace FlowUi
#endif
