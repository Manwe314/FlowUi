#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Memory/Analysis/DevMemorySession.hpp"
#include "devSystems/devInterface/Memory/Selector/DevMemorySelection.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include <array>
#include <string>
#include <vector>
namespace FlowUi::devSystems::interface_elements {
enum class MemoryTrackRecipe { Container, Pool, Arena, ActiveOnly, Independent, Budget, Resource };
/** Owned distribution row; source keys are diagnostic identities, never tuning totals. */
struct MemoryWorkbenchTrack {
	MemoryValueSample value{};
	std::string title{}, note{};
	std::vector<MemorySourceKey> keys{};
	std::vector<MemorySourceId> history_sources{};
	uint64_t padding_bytes = 0, largest_block_bytes = 0, reference_bytes = 0;
	uint64_t identity = 0, parent_identity = 0, sampled_tick = 0;
	MemoryTrackRecipe recipe = MemoryTrackRecipe::Container;
	bool available = false, included = false, estimated = false, window_scoped = false;
};
/** Pixel envelope of one actual source, never a sum of independent maxima. */
struct MemoryHistoryColumn {
	uint64_t active_first = 0, active_last = 0, backing_first = 0, backing_last = 0;
	uint64_t active_min = UINT64_MAX, active_max = 0, backing_min = UINT64_MAX, backing_max = 0;
	uint64_t epoch = 0, sequence = 0, first_ns = 0, last_ns = 0;
	MemorySampleFlag flags = MemorySampleFlag::None;
	uint16_t value_fields = 0;
	bool available = false, mixed_epochs = false, gap = false;
};
/** Exact endpoint evidence; invalid endpoints are never replaced with invented zeros. */
struct MemoryHistoryRangeSummary {
	uint64_t duration_ns = 0, active_before = 0, active_after = 0;
	uint64_t backing_before = 0, backing_after = 0;
	uint64_t before_epoch = 0, after_epoch = 0, covered_until_ns = 0;
	uint16_t before_fields = 0, after_fields = 0;
	bool valid = false, incomplete = false;
};
/** Persistent lane configuration. UINT64_MAX follows the workbench selector. */
struct MemoryHistoryLaneSettings {
	MemorySourceKey selected_key{};
	uint64_t scope = UINT64_MAX, source_index = 0, metric = 0;
	uint64_t resolved_scope = UINT64_MAX, applied_source_index = UINT64_MAX;
};
struct MemoryHistoryLane {
	std::string label{};
	MemorySourceKey key{};
	std::vector<MemoryHistoryColumn> columns{}, overview{};
	std::vector<MemoryOperationRecord> events{};
	MemoryHistoryRangeSummary range{};
	MemoryValueSample point{};
	uint64_t point_ns = 0;
	uint64_t metric = 0;
	uint64_t scale = 1;
	bool independent = false, point_available = false;
};
/** Persistent presentation state; inspection is independent of live collection. */
struct DevMemoryWorkbenchState {
	MemoryAnalysisState analysis{};
	std::vector<MemoryWorkbenchTrack> tracks{};
	std::vector<MemoryHistoryLane> lanes{}, all_lanes{};
	std::optional<MemoryDataset> held_data{};
	std::vector<RetainedMemoryEvent> held_events{};
	std::array<MemoryHistoryLaneSettings, 3> lane_settings{};
	std::array<std::vector<MemoryHistoryLane>, 3> lane_sources{};
	std::array<std::vector<FSEL::ComboBoxOption>, 3> source_options{};
	std::array<uint64_t, 3> history_scales{1, 1, 1}, scale_changed_ns{};
	uint64_t lane_count = 1;
	bool history_settings_open = false, history_window_open = false;
	std::string message{}, tooltip{}, viewport_error{}, live_summary{};
	uint64_t scope = UINT64_MAX, window = UINT64_MAX, focus = 0;
	uint64_t scale_bytes = 1, history_scale_bytes = 1, shrink_since_ns = 0;
	uint64_t next_live_ns = 0, next_history_ns = 0, history_reads = 0, layout_revision = 0;
	uint64_t view_begin_ns = 0, view_end_ns = 0, duration_ns = 30'000'000'000ull;
	uint64_t hovered_ns = 0, inspected_lane = 0;
	uint64_t selected_ns = 0, history_revision = 0, range_begin_ns = 0, range_end_ns = 0;
	bool decimal = false, expanded = false, following = true, fit_pending = false,
		 externally_paused = false;
	uint64_t units_choice = 0;
	uint64_t retained_begin_ns = 0, retained_end_ns = 0;
	uint64_t duration_choice = 2, previous_duration_choice = 2;
	uint64_t history_settings_key = UINT64_MAX;
	uint32_t pending_action = 0;
};
/** Byte formatting shared by ruler, tracks and history. */
[[nodiscard]] std::string memory_bytes(uint64_t bytes, bool decimal);
/** Readings use producer-supported metrics, never unsupported zeros. */
[[nodiscard]] std::string memory_track_reading(const MemoryWorkbenchTrack& track, bool decimal);
/** Monotonic clock used for bounded presentation refresh. */
[[nodiscard]] uint64_t memory_now_ns() noexcept;
/** Resolve the complete selector catalogue against coherent live inputs. */
void resolve_memory_tracks(DevMemoryWorkbenchState& state, const DevMemorySelection& selection,
						   const MemoryLiveRead& read);
/** Resolve source-level historical rows; detail that was not retained remains unavailable. */
void resolve_memory_history_tracks(DevMemoryWorkbenchState& state, DevMemoryReporting& reporting,
								   uint64_t timestamp_ns);
/** Build bounded per-source envelopes directly from retained storage, without raw ring copies. */
void build_memory_history(DevMemoryWorkbenchState& state, DevMemoryReporting& reporting,
						  size_t columns = 512);
/** Preserve a bounded inspection interval without pausing global collection. */
void hold_memory_history(DevMemoryWorkbenchState& state, DevMemoryReporting& reporting);
/** Read selected immutable/live evidence; returns false on concurrent eviction. */
[[nodiscard]] bool
visit_memory_history(const DevMemoryWorkbenchState& state, DevMemoryReporting& reporting,
					 const std::function<void(const MemorySampleSegment&)>& visitor);
/** Move a held view within known retained bounds, without changing selected evidence. */
void pan_memory_view(DevMemoryWorkbenchState& state, int direction) noexcept;
/** Zoom time around a normalized plot anchor and clamp to available history. */
void zoom_memory_view(DevMemoryWorkbenchState& state, double factor, double anchor = .5) noexcept;
/** Fit a readable byte axis with overflow-safe headroom. */
[[nodiscard]] uint64_t memory_axis_max(uint64_t extent) noexcept;
} // namespace FlowUi::devSystems::interface_elements
#endif
