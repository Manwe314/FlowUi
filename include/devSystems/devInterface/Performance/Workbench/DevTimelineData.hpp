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
	std::string source_file{}, source_function{}, hierarchy_note{};
	uint64_t start_ns = 0, duration_ns = 0, exclusive_ns = 0;
	uint64_t invocation_id = 0, type_id = 0, app_tick = 0, track = 0;
	WindowFrameKey frame{};
	size_t parent = timeline_no_parent;
	TimingCategory category = TimingCategory::User;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	TimingZoneRole role = TimingZoneRole::Work;
	bool selected = false;
	bool scope_visible = true;
	bool cpu_clock_aligned = true;
	bool synthetic_tick = false;
	bool measured_cadence = false;
	uint64_t device_start_ticks = 0, device_duration_ticks = 0;
	uint64_t device_identity = 0, queue_identity = 0;
	TimingEntityRef entity{};
	uint64_t parent_invocation_id = 0, submission_serial = 0;
	uint64_t calibration_id = 0, calibration_deviation_ns = 0;
	uint64_t begin_stage = 0, end_stage = 0;
	uint32_t source_line = 0, zone_index = UINT32_MAX, parent_zone_index = UINT32_MAX;
	double timestamp_period_ns = 0;
	uint32_t timestamp_valid_bits = 64;
	uint16_t quality_flags = 0;
};
/** A retained, owned snapshot; pausing keeps identities and labels alive through eviction. */
struct TimelineSnapshot {
	std::vector<TimelineBlockSlice> blocks{};
	std::vector<TimelineBlockSlice> frames{};
	std::vector<uint64_t> frame_metrics{};
	uint64_t start_ns = 0, end_ns = 0, percentile_ns = 0;
	size_t uncalibrated_gpu_count = 0;
	bool window_milestones = false;
	bool tick_cadence = false;
	std::vector<TimelineBlockSlice> ticks{};
	std::vector<size_t> tick_samples{};
	std::vector<size_t> child_offsets{}, children{};
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
	uint64_t window = 0;
};
/** Stable semantic row identity, independent of overlap sibling lanes. */
struct TimelineTrackKey {
	uint64_t device = 0, track = 0, window = 0;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	bool contextual = false;
	[[nodiscard]] bool operator==(const TimelineTrackKey&) const noexcept = default;
};
/** Session-persistent presentation settings; capture configuration is unaffected. */
struct TimelineTrackPreference {
	TimelineTrackKey key{};
	bool hidden = false, pinned = false;
};
struct TimelineCard {
	std::vector<size_t> roots{};
	uint32_t active_depth = 256;
	uint64_t surface_identity = 0;
	uint64_t visible_start_ns = 0, visible_duration_ns = 0;
	double zoom = 1;
	float scroll_y = 0;
};
enum class TimelineAction {
	None,
	Pause,
	Previous,
	Next,
	Spike,
	Zoom,
	Domain,
	Open,
	Close,
	Breadcrumb,
	Depth,
	Center,
	Inspect,
	Refresh,
	FitSelected,
	MinimapZoom,
	TrackHide,
	TrackPin,
	TrackMove,
	TrackControls
};
struct TimelineCommand {
	std::vector<size_t> members{};
	TimelineAction action = TimelineAction::None;
	size_t index = 0;
	double value = 0;
	uint64_t revision = 0;
};
/** Interface-owned session shared by the Workbench and Inspector, independent of recording. */
struct DevTimelineState {
	TimelineSnapshot snapshot{};
	std::vector<TimingAppTickReport> retained_reports{};
	std::vector<TimingZoneDescriptor> descriptors{};
	std::vector<TimelineCard> cards{};
	TimelineCommand pending{};
	std::vector<TimelineTrackPreference> track_preferences{};
	uint64_t track_preferences_revision = 0;
	DevPerformanceSelection selection{};
	uint64_t mutation_sequence = UINT64_MAX, visible_start_ns = 0, visible_duration_ns = 1;
	uint64_t origin_ns = 0, last_snapshot_refresh_ns = 0;
	uint64_t snapshot_revision = 1, next_surface_identity = 1;
	size_t selected_frame = 0;
	size_t inspected_sample = timeline_no_parent;
	float major_height = 260;
	double minimap_zoom = 4;
	bool refresh_requested = false;
	bool restore_minor_scroll = false;
	bool track_controls_open = false;
	bool minimap_follow_selection = false;
	double zoom = 4;
	uint32_t active_depth = 1;
	uint32_t reveal_frames = 0;
	bool paused = false;
};
/** Extract scoped history without dropping nonmatching zones needed for context. */
[[nodiscard]] TimelineSnapshot extract_timeline(std::span<const TimingAppTickReport> reports,
												std::span<const TimingZoneDescriptor> descriptors,
												const DevPerformanceSelection& selection);
/** Partition hierarchy by hardware track and relative depth. Empty roots select macro lanes. */
[[nodiscard]] std::vector<TimelineTrackLane> timeline_lanes(const TimelineSnapshot& snapshot,
															uint32_t depth,
															std::span<const size_t> roots = {});
/** Select flat semantic roots and allocate sibling lanes only for genuine overlaps. */
[[nodiscard]] std::vector<TimelineTrackLane>
timeline_major_lanes(const TimelineSnapshot& snapshot, const DevPerformanceSelection& selection,
					 std::span<const TimelineTrackPreference> preferences = {});
/** Recover a semantic row key from its retained geometry/data projection. */
[[nodiscard]] TimelineTrackKey timeline_track_key(const TimelineSnapshot& snapshot,
												  const TimelineTrackLane& lane) noexcept;
/** Group adjacent subpixel/micro samples without crossing gaps or selected-zone boundaries. */
[[nodiscard]] std::vector<TimelineCluster>
cluster_timeline(const TimelineSnapshot& snapshot, std::span<const size_t> blocks,
				 uint64_t start_ns, uint64_t duration_ns, float width,
				 size_t inspected_sample = timeline_no_parent,
				 std::span<const size_t> protected_roots = {});
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
