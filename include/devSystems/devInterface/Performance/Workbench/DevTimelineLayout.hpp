#pragma once
#include "devSystems/devInterface/Performance/Workbench/DevTimelineControls.hpp"
#if FLOW_UI_DEV_MODE
#include <optional>

namespace FlowUi::devSystems::interface_elements {
enum class TimelineSurfaceKind { Minimap, Macro, Card };
/** Owned projected geometry. Coordinates are logical content units, never timestamps. */
struct TimelineDisplayItem {
	std::string label{};
	std::string detail{};
	TimelineCommand command{};
	Clay_BoundingBox bounds{};
	Clay_Color color{};
	bool selected = false;
	bool screen_space = false;
	bool focused_root = false;
	float text_offset_x = 0;
};
struct TimelineLayout {
	std::vector<TimelineDisplayItem> items{};
	uint64_t revision = 0, start_ns = 0, duration_ns = 1;
	uint64_t root_duration_ns = 1;
	float width = 0, height = 0;
	float label_width = 0, ruler_height = 20;
	TimelineSurfaceKind kind = TimelineSurfaceKind::Macro;
};
/** Clip before subtracting unsigned timestamps; retain at least one logical pixel. */
[[nodiscard]] std::optional<Clay_BoundingBox>
timeline_project_interval(uint64_t sample_start, uint64_t sample_duration, uint64_t view_start,
						  uint64_t view_duration, float width, float top, float height) noexcept;
/** Reverse draw-order picking handles overlapping intervals and excludes decorations. */
[[nodiscard]] size_t timeline_hit(const TimelineLayout& layout, float horizontal,
								  float vertical) noexcept;
/** Format rich details only for the hovered item against its retained revision. */
[[nodiscard]] std::string timeline_item_detail(const DevTimelineState& state,
											   const TimelineLayout& layout, size_t item_index);
/** Build one body, including all lane labels, ruler and selectable geometry. */
[[nodiscard]] TimelineLayout
build_timeline_layout(const DevTimelineState& state, const DevPerformanceSelection& selection,
					  TimelineSurfaceKind kind, size_t card_index, float width,
					  const std::vector<TimelineTrackLane>* cached_lanes = nullptr);
} // namespace FlowUi::devSystems::interface_elements
#endif
