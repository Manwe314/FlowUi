#include "devSystems/devInterface/Memory/Analysis/DevMemorySession.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
namespace FlowUi::devSystems::interface_elements {
const MemoryDataset* selected_memory_dataset(const MemoryAnalysisState& state) noexcept {
	const auto found = std::ranges::find(state.datasets, state.dataset, &MemoryDataset::identity);
	return found == state.datasets.end() ? nullptr : &*found;
}
Status save_memory_dataset(MemoryAnalysisState& state, DevMemoryReporting& reporting, bool snapshot,
						   uint64_t begin_ns, uint64_t end_ns) {
	if (state.datasets.size() >= 4) {
		state.message = "Four saved datasets retained. Remove one before saving another.";
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::None));
	}
	MemoryDataset saved;
	saved.live = reporting.read_live();
	saved.end_ns = end_ns ? end_ns : saved.live.status.last_consumed_ns;
	saved.begin_ns = begin_ns							? begin_ns
					 : saved.end_ns > 30'000'000'000ull ? saved.end_ns - 30'000'000'000ull
														: 0;
	saved.identity = state.next_dataset;
	saved.snapshot = snapshot;
	saved.name = state.capture_name + (snapshot ? " · snapshot " : " · recording ") +
				 std::to_string(saved.identity);
	saved.complete = true;
	if (!snapshot) {
		saved.segments.reserve(8192);
		bool limit = false;
		const bool coherent = reporting.visit_history_chunks(saved.live.status, [&](auto records) {
			for (auto segment : records) {
				if (segment.beginNs >= saved.end_ns || segment.endNs <= saved.begin_ns)
					continue;
				if (saved.segments.size() == 8192) {
					limit = true;
					continue;
				}
				segment.beginNs = std::max(segment.beginNs, saved.begin_ns);
				segment.endNs = std::min(segment.endNs, saved.end_ns);
				saved.segments.emplace_back(segment);
			}
		});
		saved.complete = coherent && !limit && saved.live.status.segments.overwriteCount == 0 &&
						 saved.live.status.freeze_owner_count == 0;
		if (saved.segments.empty())
			saved.complete = false;
	}
	state.dataset = saved.identity;
	++state.next_dataset;
	state.message =
		snapshot ? "Instant reading preserved; no replay trace."
		: saved.complete
			? "Recording preserved independently of the live ring."
			: "Partial recording preserved; missing/limited observations are not reconstructed.";
	state.datasets.emplace_back(std::move(saved));
	state.last_refresh_ns = 0;
	return {};
}
void delete_memory_dataset(MemoryAnalysisState& state) {
	std::erase_if(state.datasets,
				  [&](const auto& saved) { return saved.identity == state.dataset; });
	if (state.baseline_dataset == state.dataset)
		state.baseline_dataset = 0;
	state.dataset = 0;
	state.message = "Saved dataset removed; live collection continues.";
}
void update_memory_session(MemoryAnalysisState& state, DevMemoryReporting& reporting) {
	const auto action = std::exchange(state.pending_action, 0);
	if (action == 1) {
		if (!state.recording) {
			if (state.datasets.size() == 4)
				state.message = "Remove a saved dataset before recording.";
			else {
				state.recording = reporting.beginCapture(state.capture_name);
				state.recording_begin_ns = memory_now_ns();
				state.message = state.recording
									? "Recording while live collection continues."
									: "Recording unavailable while collection is paused.";
			}
		} else {
			if (reporting.endCapture(state.recording)) {
				const auto saved = save_memory_dataset(
					state, reporting, false,
					state.include_recent && state.recording_begin_ns > 5'000'000'000ull
						? state.recording_begin_ns - 5'000'000'000ull
						: state.recording_begin_ns);
				if (!saved)
					state.message =
						"Recording ended; save failed. Remove a dataset and save recent history.";
			}
			state.recording = 0;
		}
	}
	if (action == 2) {
		const auto saved = save_memory_dataset(state, reporting, true);
		if (!saved)
			state.message = "Snapshot could not be saved; dataset limit reached.";
	}
	if (action == 3) {
		const auto saved = save_memory_dataset(state, reporting, false);
		if (!saved)
			state.message = "Recent history could not be saved; dataset limit reached.";
	}
	if (action == 4)
		delete_memory_dataset(state);
	if (action == 5)
		state.baseline_dataset = state.dataset;
	if (state.recording && state.recording_seconds > 0 &&
		memory_now_ns() - state.recording_begin_ns >= uint64_t(state.recording_seconds * 1e9)) {
		state.pending_action = 1;
	}
	state.dataset_options.clear();
	state.dataset_options.reserve(5);
	state.dataset_options.emplace_back(FSEL::ComboBoxOption{.value = 0, .text = "Live + recent"});
	for (const auto& saved : state.datasets)
		state.dataset_options.emplace_back(
			FSEL::ComboBoxOption{.value = saved.identity, .text = saved.name});
}
Status stage_memory_setting(MemoryAnalysisState& state, const MemoryTuningTargetDescriptor& target,
							bool confirm_replace, bool confirm_factor) {
	if (!state.plan)
		return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::None));
	const auto existing = std::ranges::find(state.staged, target.id,
											[](const auto& setting) { return setting.target.id; });
	if (existing != state.staged.end() && !confirm_replace) {
		state.replace_pending = true;
		state.planner_message =
			"This configuration key already has a proposal. Confirm replacement.";
		return unexpectedError(makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::None));
	}
	const bool factor_changed =
		!state.staged.empty() && std::abs(state.plan->factor - state.shared_factor) > .000001;
	if (factor_changed && !confirm_factor) {
		state.shared_factor_pending = true;
		state.planner_message =
			"Shared growthFactor changes every staged target. Review and confirm.";
		return unexpectedError(makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::None));
	}
	std::vector<MemoryPlan> reevaluated;
	reevaluated.reserve(state.staged.size());
	for (const auto& staged : state.staged) {
		if (staged.target.id == target.id) {
			reevaluated.emplace_back(*state.plan);
			continue;
		}
		auto result = replay_memory_plan(staged.evidence, staged.target, staged.plan.initial,
										 state.plan->factor);
		if (!result || result->maximum_instance_growths > staged.maximum_growths ||
			(staged.final_budget && result->final_capacity > staged.final_budget)) {
			state.planner_message =
				"Shared factor conflicts with a staged target's growth/memory objective.";
			return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
		}
		reevaluated.emplace_back(std::move(*result));
	}
	for (size_t index = 0; index < reevaluated.size(); ++index)
		state.staged[index].plan = std::move(reevaluated[index]);
	MemorySettingDraft draft;
	draft.target = target;
	draft.evidence = state.evidence;
	draft.plan = *state.plan;
	draft.maximum_growths =
		state.planner.preset == 3 ? state.planner.maximum_growths : state.planner.preset;
	draft.final_budget = state.planner.final_budget;
	draft.provenance = state.evidence_mode == 2			? "Preview data"
					   : selected_memory_dataset(state) ? selected_memory_dataset(state)->name
														: "Retained live samples";
	draft.complete = selected_memory_dataset(state) && selected_memory_dataset(state)->complete;
	if (existing != state.staged.end())
		*existing = std::move(draft);
	else
		state.staged.emplace_back(std::move(draft));
	state.shared_factor = state.plan->factor;
	state.replace_pending = state.shared_factor_pending = false;
	state.show_review = true;
	state.planner_message = "Staged locally; Export / Bake backend is not connected.";
	++state.staged_revision;
	return {};
}
} // namespace FlowUi::devSystems::interface_elements
#endif
