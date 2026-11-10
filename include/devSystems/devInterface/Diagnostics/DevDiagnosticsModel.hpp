#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsReporting.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems {
struct DevInterfaceState;

/* Persistent display predicates. Zero range endpoints mean unbounded. */
struct DevDiagnosticsFilters {
	std::string search{};
	uint64_t activity_view = 0, grouping = 0, order = 0;
	uint64_t window_mode = 0, window = 0, source_mode = 0, source = 0;
	uint64_t first_tick = 0, last_tick = 0, first_frame = 0, last_frame = 0;
	uint64_t ongoing_ticks = 120, recent_ticks = 600, histogram_ticks = 600;
	uint32_t domain_mask = UINT32_MAX, impact_mask = UINT32_MAX, resolution_mask = UINT32_MAX;
	[[nodiscard]] bool operator==(const DevDiagnosticsFilters&) const noexcept = default;
};

/* One stable selector identity with filtered occurrence starts and outcome distribution. */
struct DevDiagnosticsGroup {
	std::vector<uint64_t> occurrences{};
	std::array<uint64_t, 16> outcomes{};
	FlowUiError error{};
	DevErrorGroupKey key{};
	uint64_t identity = 0, count = 0, first_tick = 0, last_tick = 0;
	uint64_t first_timestamp_ns = 0, last_timestamp_ns = 0, latest = 0, first = 0;
	uint64_t window_count = 0, subject_count = 0, recent_count = 0;
	DevErrorGroupMode mode = DevErrorGroupMode::Contract;
	bool ongoing = false;
};

/* Compact occurrence browser row, sorted before bounded UI pagination. */
struct DevDiagnosticsHistoryRow {
	uint64_t id = 0, first_tick = 0, window = 0, frame = 0;
	ErrorResolution resolution = ErrorResolution::None;
	bool evidence_available = false;
};

/* Fixed tick histogram bin; missing coverage is distinct from an empty bin. */
struct DevDiagnosticsBin {
	uint64_t first_tick = 0, last_tick = 0, count = 0;
	bool selected = false, incomplete = false;
};

/* Retained-key registry preserves widget identity while a group is hidden by filters. */
struct DevDiagnosticsIdentity {
	DevErrorGroupKey key{};
	uint64_t identity = 0;
	DevErrorGroupMode mode = DevErrorGroupMode::Contract;
};

/* Commands are dispatched at the interface safe point, outside UI drawing. */
enum class DevDiagnosticsCommand : uint8_t {
	None,
	Refresh,
	ClearSelection,
	ApplyCaptureLevel,
	ArmTrigger,
	RemoveTrigger,
	MoveTriggerUp,
	MoveTriggerDown,
	Freeze,
	FreezeSelected,
	Resume,
	ExportSelected,
	ExportRetained,
	OpenPerformance,
	OpenMemory,
	Back
};

/* Owned shared view. Selection and evidence focus never depend on a vector index. */
struct DevDiagnosticsState {
	DevDiagnosticsInvestigation owned{};
	DevDiagnosticsSnapshot snapshot{};
	std::optional<DevErrorOccurrence> selected{};
	std::optional<DevErrorAdviceEvidenceRef> focused_advice{};
	std::optional<RetainedMemoryEvent> navigation_memory_event{};
	std::optional<MemorySampleSegment> navigation_memory_checkpoint{};
	std::vector<DevDiagnosticsGroup> groups{};
	std::vector<DevDiagnosticsIdentity> group_identities{};
	std::vector<DevDiagnosticsHistoryRow> history{};
	std::array<DevDiagnosticsBin, 32> bins{};
	DevDiagnosticsFilters filters{}, applied_filters{};
	DevErrorReportingConfig live_reporting_config{};
	DevErrorConfig live_capture_config{};
	std::array<bool, 256> domain_expanded = [] {
		std::array<bool, 256> value{};
		value.fill(true);
		return value;
	}();
	std::array<bool, 10> inspector_expanded{true,  true,  true, true,  false,
											false, false, true, false, false};
	std::optional<DevErrorGroupKey> selected_group{};
	DevErrorTrigger trigger{};
	std::string export_path = "flowui-diagnostics";
	std::string message{}, navigation_message{};
	uint64_t selected_id = 0, focus_kind = 0, focus_sequence = 0;
	uint64_t inspector_scroll_to = UINT64_MAX;
	uint64_t selected_mode = 0, capture_level = 0, command_index = 0;
	uint64_t last_refresh_ns = 0, refreshed_revision = 0, model_revision = 0, refresh_count = 0;
	uint64_t next_identity = 1, total_groups = 0, history_page = 0, selected_bin = UINT64_MAX;
	uint64_t anchor_tick = 0, return_tab = 0;
	float selector_scroll = 0, workbench_scroll = 0, inspector_scroll = 0;
	DevDiagnosticsCommand pending = DevDiagnosticsCommand::None;
	bool filters_open = false, show_trigger = false, show_export = false;
	bool dirty = true, refresh_order = true, order_changed = false;
	bool follow_latest = false, frozen = false, selected_only_freeze = false;
	bool frozen_details_limited = false, message_is_error = false;
	bool selected_outside_filters = false, selected_evicted = false, newer_available = false;
	bool return_available = false, initialized = false;
};

/* Derive groups and histogram from the same owned lightweight dataset and predicates. */
void rebuild_diagnostics_model(DevDiagnosticsState& state);
/* Refresh at most five times per second; never consumes monitoring records. */
void refresh_diagnostics_model(DevDiagnosticsState& state, DevErrorReporting& reporting,
							   uint64_t now_ns, bool force = false);
/* Freeze retained evidence and preserve the viewed investigation, even after eviction.
 * If complete detail exceeds the budget, hold the list and available selected detail. */
[[nodiscard]] Status freeze_diagnostics_model(DevDiagnosticsState& state,
											  DevErrorReporting& reporting,
											  bool selected_only = false,
											  uint64_t byte_budget = 32u * 1024u * 1024u) noexcept;
/* Process capture, freeze, export and navigation commands at the application safe point. */
void service_diagnostics(App& app, DevInterfaceState& interface_state, bool visible);
/* Pin the newest matching retained occurrence for a full group key. */
void select_diagnostics_group(DevDiagnosticsState& state, const DevErrorGroupKey& key,
							  DevErrorGroupMode mode);
/* Pin an exact retained occurrence, disabling automatic following. */
void select_diagnostics_occurrence(DevDiagnosticsState& state, uint64_t occurrence_id);
/* Change evidence focus without changing the occurrence. */
void focus_diagnostics_event(DevDiagnosticsState& state, uint64_t kind, uint64_t sequence) noexcept;
/* Restore display predicates without changing recording configuration or selection. */
void reset_diagnostics_filters(DevDiagnosticsState& state);
/* Resolve an advice evidence target by its captured identities. */
void diagnostics_focus_advice(DevDiagnosticsState& state,
							  const DevErrorAdviceEvidenceRef& evidence);
/* Selected technical report, including a preserved evicted report. */
[[nodiscard]] const DevErrorOccurrence*
diagnostics_selected_occurrence(const DevDiagnosticsState& state) noexcept;
/* Current filtered selection, or null when hidden by filters/retention. */
[[nodiscard]] const DevDiagnosticsGroup*
selected_diagnostics_group(const DevDiagnosticsState& state) noexcept;
/* Descriptor text is owned by the active investigation/snapshot. */
[[nodiscard]] const DevDiagnosticsSource* diagnostics_source(const DevDiagnosticsState& state,
															 uint64_t id) noexcept;
/* Resolve only registered breadcrumb names; unknown identities remain explicit. */
[[nodiscard]] std::string_view diagnostics_breadcrumb_name(const DevDiagnosticsState& state,
														   uint64_t id) noexcept;
/* Resolve captured raw stacks without reading mutable producer storage. */
[[nodiscard]] const DevErrorStackTrace* diagnostics_stack(const DevDiagnosticsState& state,
														  uint64_t id) noexcept;
/* Resolve owned triggered-capture evidence. */
[[nodiscard]] const DevErrorTriggeredCapture* diagnostics_capture(const DevDiagnosticsState& state,
																  uint64_t id) noexcept;
/* Exact grouping identity shared by the model and selectors. */
[[nodiscard]] DevErrorGroupKey diagnostics_group_key(const DevDiagnosticsOccurrenceRow& row,
													 DevErrorGroupMode mode) noexcept;
/* Number of advanced predicates differing from their defaults. */
[[nodiscard]] uint32_t diagnostics_filter_count(const DevDiagnosticsFilters& filters) noexcept;
/* Production domain vocabulary, preserving unknown values. */
[[nodiscard]] std::string diagnostics_domain_name(ErrorDomain domain);
/* Production consequence vocabulary, independent of transport category. */
[[nodiscard]] std::string diagnostics_impact_name(ErrorImpact impact);
/* Recorded outcome vocabulary, including the explicit absence of an outcome. */
[[nodiscard]] std::string diagnostics_resolution_name(ErrorResolution resolution);
/* Occurrence-level collection state never implies resolution of the defect. */
[[nodiscard]] std::string_view diagnostics_collection_name(DevErrorOccurrenceState state) noexcept;
/* Validate the captured generation and exact target before changing another tab. */
[[nodiscard]] Status navigate_diagnostics_performance(App& app, DevInterfaceState& interface_state);
/* Select a retained memory operation/checkpoint, preserving the Diagnostics return context. */
[[nodiscard]] Status navigate_diagnostics_memory(App& app, DevInterfaceState& interface_state);
} // namespace FlowUi::devSystems
#endif
