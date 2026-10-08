#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Performance/DevPerformanceSelection.hpp"
#include <limits>
#include <span>
#include <vector>

namespace FlowUi::devSystems::interface_elements {
inline constexpr size_t timeline_no_parent = std::numeric_limits<size_t>::max();
/** Non-owning measured details, resolved directly from the sealed reporting ring. */
struct TimelineSampleDetails {
	std::string_view source_file{}, source_function{};
	TimingEntityRef entity{};
	uint64_t exclusive_ns = 0, parent_invocation_id = 0;
	uint64_t device_start_ticks = 0, device_duration_ticks = 0;
	uint64_t calibration_id = 0, calibration_deviation_ns = 0;
	uint64_t begin_stage = 0, end_stage = 0;
	double timestamp_period_ns = 0;
	uint32_t source_line = 0, timestamp_valid_bits = 64;
	uint16_t quality_flags = 0;
};
/** Derived interval/identity index; measured detail stays in its original ring slot. */
struct TimelineBlockSlice {
	std::span<const CpuTimingRecord> cpu_sample{};
	std::span<const GpuTimingRecord> gpu_sample{};
	std::span<const TimingZoneDescriptor> descriptor{};
	std::string_view label{}, hierarchy_note{};
	uint64_t start_ns = 0, duration_ns = 0;
	uint64_t invocation_id = 0, type_id = 0, app_tick = 0, track = 0;
	uint64_t device_identity = 0, queue_identity = 0, submission_serial = 0;
	WindowFrameKey frame{};
	size_t parent = timeline_no_parent;
	uint32_t zone_index = UINT32_MAX, parent_zone_index = UINT32_MAX;
	TimingCategory category = TimingCategory::User;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	TimingZoneRole role = TimingZoneRole::Work;
	bool selected = false, scope_visible = true, cpu_clock_aligned = true;
	bool synthetic_tick = false, measured_cadence = false;
	/** Read original CPU/GPU detail while the containing sealed generation is leased. */
	[[nodiscard]] TimelineSampleDetails details() const noexcept;
	/** Recorded exclusive CPU duration; synthetic/GPU groupings have none. */
	[[nodiscard]] uint64_t recorded_exclusive_ns() const noexcept {
		return cpu_sample.empty() ? 0 : cpu_sample.front().exclusiveNs();
	}
};
/** Derived investigation indexes and display labels, never a second timing history. */
struct TimelineSnapshot {
	std::vector<TimelineBlockSlice> blocks{}, frames{}, ticks{};
	std::vector<std::string> labels{};
	std::vector<uint64_t> frame_metrics{};
	std::vector<size_t> tick_samples{}, child_offsets{}, children{};
	uint64_t start_ns = 0, end_ns = 0, percentile_ns = 0;
	size_t uncalibrated_gpu_count = 0;
	bool window_milestones = false, tick_cadence = false;
	TimelineSnapshot() = default;
	TimelineSnapshot(TimelineSnapshot&&) noexcept = default;
	TimelineSnapshot& operator=(TimelineSnapshot&&) noexcept = default;
	/** Copy derived indexes and rebind their owned label views. */
	TimelineSnapshot(const TimelineSnapshot& other);
	TimelineSnapshot& operator=(const TimelineSnapshot& other);
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
	std::vector<TimelineCard> cards{};
	TimelineCommand pending{};
	std::vector<TimelineTrackPreference> track_preferences{};
	uint64_t track_preferences_revision = 0;
	DevPerformanceSelection selection{};
	uint64_t capture_generation = 0, visible_start_ns = 0, visible_duration_ns = 1;
	uint64_t origin_ns = 0, capture_event_ns = 0;
	uint64_t snapshot_revision = 1, next_surface_identity = 1;
	uint64_t minimap_maximum_ns = 0;
	size_t selected_frame = 0;
	size_t inspected_sample = timeline_no_parent;
	float major_height = 260;
	double minimap_zoom = 4;
	bool restore_minor_scroll = false;
	bool track_controls_open = false;
	bool minimap_follow_selection = false;
	double zoom = 4;
	uint32_t active_depth = 1;
	uint32_t reveal_frames = 0;
};
/** Index two chronological sealed-ring segments without copying timing records. */
[[nodiscard]] TimelineSnapshot extract_timeline(const TimingCaptureReadView& capture,
												const DevPerformanceSelection& selection);
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
/** Lock the minimap timing domain to the current selection's retained metrics. */
void reset_timeline_minimap_scale(DevTimelineState& state,
								  const DevPerformanceSelection& selection) noexcept;
/** Center the viewport on a timestamp, without unsigned underflow. */
void center_timeline(DevTimelineState& state, uint64_t timestamp) noexcept;
} // namespace FlowUi::devSystems::interface_elements
#endif
