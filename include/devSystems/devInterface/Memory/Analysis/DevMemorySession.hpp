#pragma once
#include "devSystems/devInterface/Memory/Analysis/DevMemoryPlanner.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/ComboBox.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Immutable window-owned dataset, bounded independently of reporting's live ring. */
struct MemoryDataset {
	MemoryLiveRead live{};
	std::vector<MemorySampleSegment> segments{};
	std::string name{};
	uint64_t identity = 0, begin_ns = 0, end_ns = 0;
	bool snapshot = false, complete = false;
};
/** One staged public configuration target and the evidence used to reevaluate a shared factor. */
struct MemorySettingDraft {
	std::vector<MemoryDemandObservation> evidence{};
	MemoryTuningTargetDescriptor target{};
	MemoryPlan plan{};
	std::string provenance{};
	uint64_t maximum_growths = 0, final_budget = 0;
	bool complete = false;
};
/** Surface-owned dataset, planner and settings-review state. No production configuration writes. */
struct MemoryAnalysisState {
	std::vector<MemoryDataset> datasets{};
	std::vector<FSEL::ComboBoxOption> dataset_options{};
	std::vector<MemorySettingDraft> staged{};
	std::vector<MemoryDemandObservation> evidence{};
	std::vector<FSEL::ComboBoxOption> target_options{};
	std::vector<MemoryTuningTargetDescriptor> targets{};
	std::optional<MemoryPlan> plan{};
	std::array<std::optional<MemoryPlan>, 3> candidates{};
	MemoryPlannerSettings planner{}, evaluated_settings{};
	std::string message{}, planner_message{}, capture_name{"Memory workload"};
	uint64_t dataset = 0, previous_dataset = 0, next_dataset = 1;
	uint64_t recording = 0, recording_begin_ns = 0, last_refresh_ns = 0, evidence_revision = 0;
	uint64_t target = 0, evaluated_target = 0, evidence_mode = 0, previous_preset = UINT64_MAX;
	uint64_t ui_action = 0, evaluated_subject = UINT64_MAX, evaluated_dataset = UINT64_MAX,
			 evaluated_evidence_mode = UINT64_MAX;
	uint64_t pending_action = 0, staged_revision = 0, baseline_dataset = 0;
	double recording_seconds = 0, shared_factor = 1.5;
	bool show_recording_settings = false, show_quality = false, show_planner = true,
		 show_review = false;
	bool include_recent = true, record_shortcut = false;
	bool replace_pending = false, shared_factor_pending = false;
};
/** Find the selected immutable dataset; null selects live reporting. */
[[nodiscard]] const MemoryDataset*
selected_memory_dataset(const MemoryAnalysisState& state) noexcept;
/** Update recording/dataset transitions and rebuild stable combobox options. */
void update_memory_session(MemoryAnalysisState& state, DevMemoryReporting& reporting);
/** Preserve recent history or one coherent instant reading; records are explicitly bounded. */
[[nodiscard]] Status save_memory_dataset(MemoryAnalysisState& state, DevMemoryReporting& reporting,
										 bool snapshot, uint64_t begin_ns = 0, uint64_t end_ns = 0);
/** Remove selected saved data, preserving live collection and staged settings. */
void delete_memory_dataset(MemoryAnalysisState& state);
/** Confirm one common factor and reevaluate every staged target before changing it. */
[[nodiscard]] Status stage_memory_setting(MemoryAnalysisState& state,
										  const MemoryTuningTargetDescriptor& target,
										  bool confirm_replace = false,
										  bool confirm_factor = false);
} // namespace FlowUi::devSystems::interface_elements
#endif
