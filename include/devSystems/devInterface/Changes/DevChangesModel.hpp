#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/bake/DevChangesReview.hpp"
#include "devSystems/devTooling/bake/DevChangesRuntime.hpp"
#include <array>
#include <clay.h>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems {
struct DevInterfaceState;

/* A stable shallow hierarchy; descendant indices always refer to the owned review. */
struct DevChangesNode {
	std::vector<std::size_t> rows{};
	std::string key{}, parent{}, label{};
	uint64_t identity = 0;
	uint16_t depth = 0;
	bool leaf = false, expanded = true, matches = false;
};

/* Deferred commands run outside Clay drawing, at the interface safe point. */
enum class DevChangesCommand : uint8_t {
	None,
	Refresh,
	Bake,
	Decide,
	Undo,
	Redo,
	Edit,
	Back,
	ApplyProposal,
	ApplySaved,
	DiscardLive,
	ResetAuthored,
	UndoLive,
	RedoLive,
	StagePolicy
};

/* Explicit future-startup policy intent, independent of the Memory authoring recipe. */
struct DevChangesPolicyIntent {
	std::optional<bool> enabled{}, runtime_growth{};
	std::optional<float> growth_factor{};
	bool clear_section = false, clear_growth_factor = false, clear_runtime_growth = false;
};
/* One reversible manifest intent change, including shared policy set/clear semantics. */
struct DevChangesDecisionSnapshot {
	std::vector<tooling::DevChangesDecisionEntry> decisions{};
	DevChangesPolicyIntent policies{};
};
/* Lightweight read-only memory facts captured with the reviewed snapshot at a safe point. */
struct DevChangesMemoryObservation {
	std::string key{}, startup{}, demand{}, allocations{};
	uint64_t observed_ns = 0, observed_tick = 0, peak_ns = 0;
};

/* Submitted keyboard targets are retained independently of optional developer tree capture. */
struct DevChangesFocusTarget {
	Clay_ElementId identity{}, viewport{};
	FlowElementID text_input{};
	uint64_t* radio_selection = nullptr;
	uint64_t radio_value = 0;
};

/* Session-owned review shared by all Changes surfaces, including the header. */
struct DevChangesState {
	tooling::DevChangesReviewPlan review{};
	std::vector<DevChangesNode> nodes{};
	std::vector<std::size_t> visible_nodes{};
	std::vector<std::size_t> matching_rows{};
	std::vector<DevChangesFocusTarget> focus_targets{};
	std::vector<tooling::DevChangesDecisionEntry> decisions{};
	std::vector<DevChangesDecisionSnapshot> undo{}, redo{};
	std::vector<DevChangesMemoryObservation> memory_observations{};
	DevChangesPolicyIntent policies{};
	std::vector<tooling::DevChangesRuntimeEdit> runtime_undo{}, runtime_redo{};
	tooling::DevChangesRuntimeEdit pending_runtime{};
	std::vector<std::string> pending_keys{};
	std::string search{}, selected_key{}, queue_scope_key{}, message{}, runtime_message{};
	std::string memory_error{}, applied_search{}, feedback_label{}, runtime_target_label{};
	std::array<bool, 5> inspector_expanded{true, true, true, false, false};
	std::size_t pending_skipped = 0;
	Clay_ElementId registration_viewport{};
	uint64_t domain = 0, status = 0, view = 0, applied_domain = UINT64_MAX;
	uint64_t applied_status = UINT64_MAX, model_revision = 0, refresh_count = 0;
	uint64_t selection_revision = 0;
	uint64_t last_check_ns = 0, focused_identity = 0;
	uint64_t runtime_transaction = 0, runtime_mode = 0;
	uint64_t policy_enabled_choice = 1, policy_runtime_choice = 1, pending_policy = 0;
	uint64_t workbench_page = 0;
	double policy_growth_draft = 1.5;
	float selector_scroll = 0, workbench_scroll = 0, inspector_scroll = 0;
	DevChangesCommand pending = DevChangesCommand::None;
	tooling::DevChangesDecision pending_decision = tooling::DevChangesDecision::IncludeProposal;
	bool initialized = false, stale = false, message_is_error = false;
	bool selected_outside_filter = false, acknowledge_incomplete = false;
	bool return_available = false, keyboard_active = false, was_visible = false;
	bool policy_editor_initialized = false;
	bool workbench_page_changed = false;
	bool policy_growth_valid = true;
	bool model_dirty = false;
	bool runtime_message_is_error = false;
	bool raw_manifest_open = false, output_technical_open = false, group_decisions_open = false;
	bool reveal_selection_pending = false;
};

/* Rebuild only owned display hierarchy/filter indices; never reads files or changes bake scope. */
void rebuild_changes_model(DevChangesState& state);
/* Queue a per-leaf manifest decision, preserving selection and unrelated proposals. */
void queue_changes_decision(DevChangesState& state, tooling::DevChangesDecision decision);
/* Queue eligible descendants of an explicit group; an empty scope means the whole plan. */
void queue_changes_scope_decision(DevChangesState& state, std::string_view scope_key,
								  tooling::DevChangesDecision decision);
/* Filtered queue membership is independent of tree expansion and pinned selection. */
[[nodiscard]] std::vector<std::size_t> changes_queue_rows(const DevChangesState& state);
/* Select a canonical target and reveal its page, retaining the current group when applicable. */
void changes_select_key(DevChangesState& state, std::string_view key);
/* Handle review refresh, atomic commit, manifest undo/redo and authoring navigation. */
void service_changes(App& app, DevInterfaceState& interface_state, bool visible);
/* Exact selected hierarchy node; stable even while hidden by filtering. */
[[nodiscard]] const DevChangesNode* selected_changes_node(const DevChangesState& state) noexcept;
/* Exact selected leaf, or null for an aggregate/overview. */
[[nodiscard]] const tooling::DevChangesReviewRow*
selected_changes_row(const DevChangesState& state) noexcept;
/* Human-readable whole-plan operation totals used by all review surfaces. */
[[nodiscard]] std::string changes_summary(const tooling::DevChangesReviewCounts& counts);
/* Decision vocabulary independent of output operation and validation. */
[[nodiscard]] std::string_view changes_decision_name(tooling::DevChangesDecision decision) noexcept;
/* Canonical memory owner grouping and authoring navigation. */
[[nodiscard]] std::string changes_memory_owner(std::string_view key);
} // namespace FlowUi::devSystems
#endif
