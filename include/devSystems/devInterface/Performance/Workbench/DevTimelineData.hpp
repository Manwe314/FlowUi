#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Performance/DevPerformanceSelection.hpp"
#include <limits>
#include <span>
#include <vector>

namespace FlowUi::devSystems::interface_elements {
inline constexpr size_t timeline_no_parent = std::numeric_limits<size_t>::max();
/** Owned sample identity. GPU identities are scoped to a submission and zone index. */
struct TimelineBlockSlice {
	std::string label{};
	uint64_t start_ns = 0, duration_ns = 0, exclusive_ns = 0;
	uint64_t invocation_id = 0, type_id = 0, app_tick = 0, track = 0;
	WindowFrameKey frame{};
	size_t parent = timeline_no_parent;
	TimingCategory category = TimingCategory::User;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	bool selected = false;
};
/** A retained, owned snapshot; pausing keeps identities and labels alive through eviction. */
struct TimelineSnapshot {
	std::vector<TimelineBlockSlice> blocks{};
	std::vector<TimelineBlockSlice> frames{};
	std::vector<uint64_t> frame_metrics{};
	uint64_t start_ns = 0, end_ns = 0, percentile_ns = 0;
	size_t uncalibrated_gpu_count = 0;
	bool window_milestones = false;
};
/** A rendered group retains every member so clustered zones can be inspected. */
struct TimelineCluster {
	std::vector<size_t> members{};
	uint64_t start_ns = 0, duration_ns = 0;
};
struct TimelineTrackLane {
	std::vector<size_t> blocks{};
	uint64_t track = 0;
	uint32_t depth = 0;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
};
struct TimelineCard {
	std::vector<size_t> roots{};
	uint32_t active_depth = 1;
};
enum class TimelineAction {
	None,
	Pause,
	Previous,
	Next,
	Spike,
	Freeze,
	Zoom,
	Domain,
	Open,
	Close,
	Breadcrumb,
	Depth,
	Center
};
struct TimelineCommand {
	std::vector<size_t> members{};
	TimelineAction action = TimelineAction::None;
	size_t index = 0;
	double value = 0;
};
/** State belongs to the Workbench element, never to the recorder. */
struct DevTimelineState {
	TimelineSnapshot snapshot{};
	std::vector<TimingAppTickReport> retained_reports{};
	std::vector<TimingZoneDescriptor> descriptors{};
	std::vector<TimelineCard> cards{};
	TimelineCommand pending{};
	DevPerformanceSelection selection{};
	uint64_t mutation_sequence = UINT64_MAX, visible_start_ns = 0, visible_duration_ns = 1;
	uint64_t last_seen_tick = 0, origin_ns = 0;
	size_t selected_frame = 0;
	double zoom = 1;
	uint32_t active_depth = 1;
	uint32_t reveal_frames = 0;
	bool paused = false, auto_freeze = false;
};
/** Extract scoped history without dropping nonmatching zones needed for context. */
[[nodiscard]] TimelineSnapshot extract_timeline(std::span<const TimingAppTickReport> reports,
												std::span<const TimingZoneDescriptor> descriptors,
												const DevPerformanceSelection& selection);
/** Partition hierarchy by hardware track and relative depth. Empty roots select macro lanes. */
[[nodiscard]] std::vector<TimelineTrackLane> timeline_lanes(const TimelineSnapshot& snapshot,
															uint32_t depth,
															std::span<const size_t> roots = {});
/** Group adjacent subpixel/micro samples without crossing gaps or selected-zone boundaries. */
[[nodiscard]] std::vector<TimelineCluster> cluster_timeline(const TimelineSnapshot& snapshot,
															std::span<const size_t> blocks,
															uint64_t start_ns, uint64_t duration_ns,
															float width);
/** Saturating timestamp end, also used for malformed/incomplete records. */
[[nodiscard]] uint64_t timeline_end(uint64_t start_ns, uint64_t duration_ns) noexcept;
/** Apply transport/card commands and clamp the viewport to retained history. */
void apply_timeline_command(DevTimelineState& state, DevPerformanceSelection& selection);
/** Recompute bounded viewport after zooming, resizing, or snapshot replacement. */
void clamp_timeline_view(DevTimelineState& state) noexcept;
/** Center the viewport on a timestamp, without unsigned underflow. */
void center_timeline(DevTimelineState& state, uint64_t timestamp) noexcept;
} // namespace FlowUi::devSystems::interface_elements
#endif
