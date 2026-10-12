#include "devSystems/devInterface/Memory/Inspector/DevMemoryInspector.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Memory/Analysis/DevMemoryUi.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
namespace FlowUi::devSystems::interface_elements {
namespace {
using namespace memory_ui;
constexpr auto presets = std::to_array<FSEL::ComboBoxOption>({{0, "Cover observed maximum"},
															  {1, "Balanced: one growth"},
															  {2, "Lean: up to two growths"},
															  {3, "Custom"}});
constexpr auto initials = std::to_array<FSEL::ComboBoxOption>(
	{{0, "Use production default"}, {1, "Percentile"}, {2, "Observed maximum"}, {3, "Manual"}});
constexpr auto factors = std::to_array<FSEL::ComboBoxOption>(
	{{0, "Keep default model factor (1.5)"}, {1, "Auto-fit"}, {2, "Manual"}});
constexpr auto evidence_choices = std::to_array<FSEL::ComboBoxOption>(
	{{0, "Retained sampled demand"}, {1, "Lifetime maximum only"}, {2, "Preview: growth study"}});
constexpr auto margins =
	std::to_array<FSEL::ComboBoxOption>({{0, "Percent margin"}, {1, "Absolute margin"}});
constexpr auto weightings = std::to_array<FSEL::ComboBoxOption>(
	{{0, "Equal sampled intervals"}, {1, "Elapsed observation time"}});
constexpr auto preferences =
	std::to_array<FSEL::ComboBoxOption>({{0, "Lower final reserve"}, {1, "Fewer resize events"}});
[[nodiscard]] const MemoryWorkbenchTrack*
inspected_track(const DevMemoryWorkbenchState& state) noexcept {
	if (state.focus) {
		const auto found =
			std::ranges::find(state.tracks, state.focus, &MemoryWorkbenchTrack::identity);
		return found == state.tracks.end() ? nullptr : &*found;
	}
	return state.tracks.empty() ? nullptr : &state.tracks.front();
}
[[nodiscard]] uint64_t metric_value(const MemoryValueSample& value,
									const MemoryTuningTargetDescriptor& target) noexcept {
	if (target.unit != MemoryCapacityUnit::Bytes)
		return value.capacityCount;
	return target.metric == MemoryTuningMetric::BackingAllocatedBytes ? value.backingAllocatedBytes
																	  : value.logicalLiveBytes;
}
void refresh_planner(MemoryAnalysisState& analysis, const DevMemoryWorkbenchState& state,
					 const MemoryWorkbenchTrack& track, const MemoryTuningTargetDescriptor& target,
					 DevMemoryReporting& reporting) {
	analysis.evidence.clear();
	analysis.evidence.reserve(4096);
	analysis.planner.maximum_only = analysis.evidence_mode == 1;
	const auto* saved = selected_memory_dataset(analysis);
	if (analysis.evidence_mode == 2) {
		const uint64_t unit = target.unit == MemoryCapacityUnit::Bytes ? 1024 * 1024 : 1;
		uint64_t sequence = 0;
		for (const uint64_t demand : {2, 4, 5, 9})
			analysis.evidence.emplace_back(
				MemoryDemandObservation{demand * unit, 1, ++sequence, 1, 0});
	} else if (analysis.evidence_mode == 1) {
		const auto live = saved ? saved->live : reporting.read_live();
		for (const auto& source : live.sources) {
			if (source.key.source != target.source ||
				(track.window_scoped && state.window && source.key.window != state.window))
				continue;
			const auto maximum = target.unit == MemoryCapacityUnit::Bytes
									 ? source.sessionPeak.peakLogicalBytes
									 : source.sessionPeak.peakObjectCount;
			analysis.evidence.emplace_back(
				MemoryDemandObservation{maximum, 1, source.measured_at_ns, source.key.window, 0});
		}
	} else if (!saved || !saved->snapshot) {
		const auto append = [&](const MemorySampleSegment& segment) {
			if (analysis.evidence.size() == 4096 || segment.value.source != target.source ||
				(uint16_t(segment.value.flags) & uint16_t(MemorySampleFlag::TuningAggregate)))
				return;
			if (track.window_scoped &&
				(!segment.value.window || (state.window && state.window != segment.value.window)))
				return;
			if (track.included &&
				(track.keys.empty() ||
				 std::ranges::find(track.keys,
								   MemorySourceKey{segment.value.source, segment.value.window}) ==
					 track.keys.end()))
				return;
			const auto begin = state.range_begin_ns
								   ? std::max(segment.beginNs, state.range_begin_ns)
								   : segment.beginNs;
			const auto end =
				state.range_end_ns ? std::min(segment.endNs, state.range_end_ns) : segment.endNs;
			if (end <= begin)
				return;
			const auto field = target.unit != MemoryCapacityUnit::Bytes ? MemoryValueField::Capacity
							   : target.metric == MemoryTuningMetric::BackingAllocatedBytes
								   ? MemoryValueField::Backing
								   : MemoryValueField::LogicalLive;
			if (!(segment.value.value_fields & uint16_t(field)))
				return;
			analysis.evidence.emplace_back(MemoryDemandObservation{
				metric_value(segment.value, target), analysis.planner.weighting ? end - begin : 1,
				begin, segment.value.window, 0});
		};
		if (saved)
			for (const auto& segment : saved->segments)
				append(segment);
		else {
			const bool complete = visit_memory_history(state, reporting, append);
			if (!complete)
				analysis.evidence.clear();
		}
	}
	analysis.plan.reset();
	if (analysis.evidence.empty())
		analysis.planner_message =
			saved && saved->snapshot
				? "Snapshot contains no demand trace. Choose a recording or explicit preview."
				: "No applicable demand observations retained for this target/instance.";
	else {
		const auto calculated = calculate_memory_plan(analysis.evidence, target, analysis.planner);
		if (calculated) {
			analysis.plan = *calculated;
			analysis.planner_message =
				"Local geometric model; production adapter validation pending.";
		} else
			analysis.planner_message = "No valid proposal: check reserve, factor, budget and "
									   "growth objective. Percentiles need a demand distribution.";
	}
	analysis.candidates = {};
	if (analysis.plan) {
		analysis.candidates[0] = analysis.plan;
		const auto fixed =
			replay_memory_plan(analysis.evidence, target, analysis.plan->initial, 1.5);
		if (fixed)
			analysis.candidates[1] = *fixed;
		const auto maximum =
			replay_memory_plan(analysis.evidence, target, analysis.plan->observed_maximum, 1.5);
		if (maximum)
			analysis.candidates[2] = *maximum;
	}
	analysis.evaluated_settings = analysis.planner;
	analysis.evaluated_target = analysis.target;
	analysis.evaluated_subject = track.identity;
	analysis.evaluated_dataset = analysis.dataset;
	analysis.evaluated_evidence_mode = analysis.evidence_mode;
	analysis.last_refresh_ns = memory_now_ns() + 1'000'000'000;
}
void replay_plot(DevMemoryInspector::BuildContext& context, const MemoryAnalysisState& analysis) {
	if (!analysis.plan || analysis.evidence.empty())
		return;
	std::vector<MemoryDemandObservation> observations;
	observations.reserve(analysis.evidence.size());
	const auto instance = analysis.evidence.front().instance;
	for (const auto& value : analysis.evidence)
		if (value.instance == instance)
			observations.emplace_back(value);
	std::ranges::sort(observations, {}, &MemoryDemandObservation::timestamp_ns);
	const auto bucket_count = std::min<size_t>(32, observations.size());
	const auto maximum = std::max<uint64_t>(1, analysis.plan->final_capacity);
	for (size_t channel = 0; channel < 2; ++channel) {
		text(context,
			 channel ? "Modeled capacity · first source instance"
					 : "Sampled demand · first source instance",
			 interface_theme::kTextMuted, 10);
		auto plot = row(56);
		plot.layout.childGap = 0;
		plot.layout.childAlignment.y = CLAY_ALIGN_Y_BOTTOM;
		plot.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(context.clayID(Keyed("replay-plot", channel)), plot) {
			for (size_t bucket = 0; bucket < bucket_count; ++bucket) {
				uint64_t value = 0;
				const auto begin = bucket * observations.size() / bucket_count;
				const auto end = (bucket + 1) * observations.size() / bucket_count;
				for (size_t index = begin; index < end; ++index) {
					uint64_t capacity = analysis.plan->initial;
					if (channel)
						for (const auto& step : analysis.plan->steps)
							if (step.instance == instance &&
								step.timestamp_ns <= observations[index].timestamp_ns)
								capacity = step.after;
					value = std::max(value, channel ? capacity : observations[index].demand);
				}
				auto bar = column();
				bar.layout.sizing = {.width = CLAY_SIZING_GROW(0),
									 .height = CLAY_SIZING_FIXED(
										 std::max(1.0f, float(double(value) / maximum) * 52))};
				bar.backgroundColor = channel ? Flow_Color("#BA8CFF") : Flow_Color("#49B9FF");
				bar.border = {.color = interface_theme::kDepth0Keel, .width = {0, 1, 0, 0, 0}};
				CLAY(context.clayID(Keyed("replay-bucket", channel * 32 + bucket)), bar) {}
			}
		}
	}
}
void reading(DevMemoryInspector::BuildContext& context, std::string_view label, uint64_t value,
			 bool supported, bool bytes, bool decimal) {
	auto line = row(22);
	line.layout.sizing.height = CLAY_SIZING_FIT(22);
	CLAY(context.clayID(Keyed("reading", uint64_t(std::hash<std::string_view>{}(label)))), line) {
		text(context, label, interface_theme::kTextMuted, 10);
		text(context,
			 supported ? bytes
							 ? memory_bytes(value, decimal) + "  (" + std::to_string(value) + " B)"
							 : std::to_string(value)
					   : "Not reported",
			 interface_theme::kTextCanvas, 11);
	}
}
} // namespace
void DevMemoryInspector::buildElement(BuildContext& context) {
	if (!context.params.app || !context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->memory_workbench;
	auto& analysis = state.analysis;
	auto& reporting = context.params.app->devMonitoring().memoryReporting();
	const auto* track = inspected_track(state);
	if (analysis.targets.empty())
		analysis.targets = reporting.tuningTargetSnapshot();
	analysis.target_options.clear();
	analysis.target_options.reserve(analysis.targets.size());
	if (track)
		for (const auto& target : analysis.targets)
			if (std::ranges::find(track->history_sources, target.source) !=
				track->history_sources.end())
				analysis.target_options.emplace_back(
					FSEL::ComboBoxOption{.value = target.id,
										 .text = std::string_view(target.configKey)
													 .substr(target.configKey.find('.') + 1, 24)});
	if (!analysis.target_options.empty() &&
		!std::ranges::any_of(analysis.target_options,
							 [&](const auto& option) { return option.value == analysis.target; }))
		analysis.target = analysis.target_options.front().value;
	const auto target_iterator =
		std::ranges::find(analysis.targets, analysis.target, &MemoryTuningTargetDescriptor::id);
	const auto* target =
		target_iterator != analysis.targets.end() && !analysis.target_options.empty()
			? &*target_iterator
			: nullptr;
	const auto action = analysis.ui_action >= 110 && analysis.ui_action < 200
							? std::exchange(analysis.ui_action, 0)
							: 0;
	if (action == 115)
		analysis.show_quality = !analysis.show_quality;
	if (action == 116)
		analysis.show_planner = !analysis.show_planner;
	if (action == 133)
		analysis.show_review = !analysis.show_review;
	if (action == 132) {
		analysis.staged.clear();
		++analysis.staged_revision;
	}
	if (action == 137 && track)
		context.params.app->setClipboardText(
			track->title + "\n" + memory_track_reading(*track, state.decimal) +
			"\nActive bytes: " + std::to_string(track->value.logicalLiveBytes) +
			"\nBacking bytes: " + std::to_string(track->value.backingAllocatedBytes));
	if (action == 134 && track) {
		state.focus = track->parent_identity;
		state.next_history_ns = 0;
	}
	if ((action == 135 || action == 136) && analysis.plan) {
		uint64_t timestamp = 0;
		if (action == 136 && !analysis.plan->steps.empty())
			timestamp = analysis.plan->steps.front().timestamp_ns;
		else if (!analysis.evidence.empty())
			timestamp =
				std::ranges::max_element(analysis.evidence, {}, &MemoryDemandObservation::demand)
					->timestamp_ns;
		if (analysis.evidence_mode != 2 && timestamp) {
			state.selected_ns = timestamp;
			state.following = false;
			state.next_live_ns = 0;
			state.next_history_ns = 0;
		}
	}
	if (action >= 140 && action <= 142 && analysis.candidates[action - 140]) {
		const auto& candidate = *analysis.candidates[action - 140];
		analysis.planner.preset = 3;
		analysis.planner.initial_mode = 3;
		analysis.planner.manual_initial = candidate.initial;
		analysis.planner.factor_mode = 2;
		analysis.planner.factor = candidate.factor;
		analysis.planner.maximum_growths = candidate.maximum_instance_growths;
	}
	if (analysis.previous_preset != analysis.planner.preset) {
		analysis.previous_preset = analysis.planner.preset;
		if (analysis.planner.preset != 3) {
			analysis.planner.initial_mode = analysis.planner.preset == 0 ? 2 : 1;
			analysis.planner.factor_mode = analysis.planner.preset == 0 ? 0 : 1;
			analysis.planner.percentile = analysis.planner.preset == 2 ? 90 : 95;
			analysis.planner.maximum_growths = analysis.planner.preset;
		}
	}
	if (analysis.planner.factor_mode == 0)
		analysis.planner.factor = 1.5;
	if (track && target &&
		(analysis.planner != analysis.evaluated_settings ||
		 analysis.target != analysis.evaluated_target ||
		 analysis.evaluated_subject != track->identity ||
		 analysis.evaluated_dataset != analysis.dataset ||
		 analysis.evaluated_evidence_mode != analysis.evidence_mode ||
		 memory_now_ns() >= analysis.last_refresh_ns))
		refresh_planner(analysis, state, *track, *target, reporting);
	if ((action == 130 || action == 131) && target) {
		const auto staged = stage_memory_setting(analysis, *target, action == 131, action == 131);
		if (!staged && analysis.planner_message.empty())
			analysis.planner_message = "Proposal could not be staged.";
	}
	auto root = column(8);
	root.layout.sizing.height = CLAY_SIZING_GROW(0);
	root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.layout.padding = {12, 12, 12, 12};
	root.clip = {.horizontal = true, .vertical = true};
	const auto scroll = Clay_GetScrollContainerData(context.clayID());
	if (scroll.found && scroll.scrollPosition)
		root.clip.childOffset = *scroll.scrollPosition;
	CLAY(context.clayID(), root) {
		text(context, track ? track->title : "Memory overview", interface_theme::kTextCanvas, 14);
		const auto* saved = selected_memory_dataset(analysis);
		text(context,
			 saved					? saved->name
			 : state.range_begin_ns ? "Selected range · retained samples"
			 : state.selected_ns	? "At selected time · retained samples"
									: "Live measurements",
			 interface_theme::kAccentSeaGlass);
		text(context,
			 track && !track->window_scoped ? "Application-wide · window modifier does not apply"
			 : state.window					? "Window " + std::to_string(state.window)
											: "All attributed windows",
			 interface_theme::kTextMuted, 10);
		if (!track)
			text(
				context,
				"Choose a scope or track. CPU, GPU and process views overlap and cannot be added.");
		else {
			if (!track->available)
				text(
					context,
					"No supported observation for this subject/time. Live data is not substituted.",
					interface_theme::kStatusAmber);
			else {
				text(context,
					 track->value.logicalLiveBytes ? "Measured reading" : "Idle / measured zero",
					 interface_theme::kTextCanvas, 12);
				const auto fields = track->value.value_fields;
				reading(context, "Active", track->value.logicalLiveBytes,
						fields & uint16_t(MemoryValueField::LogicalLive), true, state.decimal);
				reading(context, "Backing", track->value.backingAllocatedBytes,
						fields & uint16_t(MemoryValueField::Backing), true, state.decimal);
				reading(context, "Reusable", track->value.reusableBytes,
						fields & uint16_t(MemoryValueField::Reusable), true, state.decimal);
				reading(context, "Retired", track->value.retiredBytes,
						fields & uint16_t(MemoryValueField::Retired), true, state.decimal);
				reading(context, "Objects", track->value.objectCount,
						fields & uint16_t(MemoryValueField::Objects), false, state.decimal);
				reading(context, "Capacity count", track->value.capacityCount,
						fields & uint16_t(MemoryValueField::Capacity), false, state.decimal);
			}
			button(context, state, 137, "Copy reading", track->available);
			text(context, "Accounting", interface_theme::kTextCanvas, 12);
			text(context, track->note.empty()
							  ? "Independent accounting view; group totals may overlap."
							  : track->note);
			if (track->included)
				text(context, "Included/overlapping child: do not add this reading to its parent.",
					 interface_theme::kStatusAmber, 10);
			if (track->parent_identity)
				button(context, state, 134, "Inspect parent");
			button(context, state, 115,
				   analysis.show_quality ? "Hide measurement details" : "Measurement details");
			if (analysis.show_quality) {
				text(context, track->estimated ? "Estimated container/resource footprint"
											   : "Producer-reported measurement");
				text(context, "Report tick " + std::to_string(track->sampled_tick) + " · " +
								  std::to_string(track->keys.size()) + " attributed producer(s)");
				text(context, "History contains sampled/held source values. Allocator reset "
							  "boundaries and interval burst peaks are not retained yet.");
				const auto status = saved ? saved->live.status : reporting.status();
				text(context, "Retained segments " + std::to_string(status.segments.retainedCount) +
								  " · overwritten " +
								  std::to_string(status.segments.overwriteCount));
			}
		}
		if (analysis.baseline_dataset && track) {
			const auto baseline = std::ranges::find(analysis.datasets, analysis.baseline_dataset,
													&MemoryDataset::identity);
			if (baseline != analysis.datasets.end()) {
				uint64_t active = 0;
				bool found = false;
				for (const auto& source : baseline->live.sources)
					if (std::ranges::find(track->keys, source.key) != track->keys.end()) {
						active += source.value.logicalLiveBytes;
						found = true;
					}
				text(context, found ? "Baseline active: " + memory_bytes(active, state.decimal)
									: "Baseline subject unavailable");
			}
		}
		button(context, state, 116,
			   analysis.show_planner ? "Initial capacity & growth ▾"
									 : "Initial capacity & growth ▸");
		if (analysis.show_planner) {
			if (!target)
				text(context, "This subject has no mapped startup-capacity target. Select a "
							  "storage/manager track.");
			else {
				choice(context, "planner-target", "Configuration target", analysis.target,
					   analysis.target_options);
				auto public_key = memory_public_config_key(target->configKey);
				for (size_t position = 0; position < public_key.size(); ++position)
					if (public_key[position] == '.') {
						public_key.insert(position + 1, " ");
						++position;
					}
				text(context, public_key, interface_theme::kAccentSeaGlass, 10);
				choice(context, "planner-evidence", "Evidence domain (independent of plot zoom)",
					   analysis.evidence_mode, evidence_choices);
				text(context,
					 analysis.evidence_mode == 2
						 ? "PREVIEW DATA · deterministic demand, not application usage"
					 : analysis.evidence_mode == 1
						 ? "Lifetime maximum: no percentile distribution or chronological guarantee"
						 : "Sampled demand only; short bursts may be missed. Per-allocator epochs "
						   "are unavailable.",
					 interface_theme::kStatusAmber, 10);
				choice(context, "planner-preset", "Optimization preset", analysis.planner.preset,
					   presets);
				choice(context, "planner-initial", "Initial reserve basis",
					   analysis.planner.initial_mode, initials);
				if (analysis.planner.initial_mode == 1)
					number(context, "planner-percentile", "Percentile Pq",
						   analysis.planner.percentile, 1.0, 100.0);
				if (analysis.planner.initial_mode == 3)
					number(context, "planner-manual-initial", "Initial capacity (native units)",
						   analysis.planner.manual_initial, uint64_t{0}, target->maximum);
				choice(context, "planner-weighting", "Distribution weighting",
					   analysis.planner.weighting, weightings);
				choice(context, "planner-margin-mode", "Margin", analysis.planner.margin_mode,
					   margins);
				if (!analysis.planner.margin_mode)
					number(context, "planner-margin", "Margin percent",
						   analysis.planner.margin_percent, 0.0, 1000.0);
				else
					number(context, "planner-margin-absolute", "Additional native units",
						   analysis.planner.absolute_margin, uint64_t{0}, target->maximum);
				choice(context, "planner-factor-mode", "Shared growth factor",
					   analysis.planner.factor_mode, factors);
				if (analysis.planner.factor_mode == 2)
					number(context, "planner-factor", "Manual factor", analysis.planner.factor, 1.1,
						   64.0);
				if (analysis.planner.factor_mode == 1)
					number(context, "planner-factor-limit", "Auto-fit maximum factor",
						   analysis.planner.maximum_factor, 1.1, 64.0);
				number(context, "planner-next-target", "Next growth target (0 = observed maximum)",
					   analysis.planner.next_target, uint64_t{0}, target->maximum);
				number(context, "planner-budget", "Final reserve budget (0 = unconstrained)",
					   analysis.planner.final_budget, uint64_t{0}, target->maximum);
				if (analysis.planner.preset == 3)
					number(context, "planner-growth-count", "Maximum growths per modeled instance",
						   analysis.planner.maximum_growths, uint64_t{0}, uint64_t{64});
				choice(context, "planner-preference", "Candidate preference",
					   analysis.planner.preference, preferences);
				text(
					context,
					"Alignment " + std::to_string(target->alignment) + " · default " +
						memory_target_value(target->productionDefault, target->unit, state.decimal),
					interface_theme::kTextMuted, 10);
				text(context, analysis.planner_message,
					 analysis.plan ? interface_theme::kTextSecondary
								   : interface_theme::kStatusAmber,
					 10);
				if (analysis.plan) {
					const auto& plan = *analysis.plan;
					text(context,
						 "Proposed initial " +
							 memory_target_value(plan.initial, target->unit, state.decimal),
						 interface_theme::kTextCanvas, 12);
					text(context, "Factor " + std::to_string(plan.factor).substr(0, 5) +
									  " · shared AppConfig profile setting");
					text(context, "Initial coverage " +
									  std::to_string(int(plan.initial_coverage * 100)) + "% · " +
									  std::to_string(plan.growth_count) + " modeled resize(s) · " +
									  std::to_string(plan.maximum_instance_growths) +
									  " maximum per sampled source");
					text(context,
						 "Final reserve " +
							 memory_target_value(plan.final_capacity, target->unit, state.decimal));
					replay_plot(context, analysis);
					text(context, "Alternative starting points", interface_theme::kTextCanvas, 12);
					for (size_t candidate = 0; candidate < analysis.candidates.size(); ++candidate)
						if (analysis.candidates[candidate]) {
							const auto& alternative = *analysis.candidates[candidate];
							text(context,
								 memory_target_value(alternative.initial, target->unit,
													 state.decimal) +
									 " · factor " +
									 std::to_string(alternative.factor).substr(0, 5) + " · " +
									 std::to_string(alternative.growth_count) +
									 " resize(s) · final " +
									 memory_target_value(alternative.final_capacity, target->unit,
														 state.decimal),
								 interface_theme::kTextSecondary, 10);
							button(context, state, uint32_t(140 + candidate),
								   candidate == 0	? "Use calculated pair"
								   : candidate == 1 ? "Use factor 1.5 pair"
													: "Use maximum reserve pair");
						}
					text(context, "Projected growth ladder", interface_theme::kTextCanvas, 12);
					for (size_t step = 0; step < plan.ladder.size(); ++step) {
						if (step && !plan.ladder[step])
							break;
						text(context, (step ? "Growth " + std::to_string(step) : "Initial") + "  " +
										  memory_target_value(plan.ladder[step], target->unit,
															  state.decimal) +
										  (step ? " (+" +
													  memory_target_value(
														  plan.ladder[step] - plan.ladder[step - 1],
														  target->unit, state.decimal) +
													  ")"
												: ""));
					}
					if (plan.ladder_overflow)
						text(context, "Further projected growth exceeds target limits.",
							 interface_theme::kStatusAmber, 10);
					text(context,
						 "Later ladder steps are hypothetical minimal-overflow requests, not "
						 "observed events.",
						 interface_theme::kTextMuted, 10);
					button(context, state, 135, "Jump to sampled maximum",
						   analysis.evidence_mode == 0);
					button(context, state, 136, "Jump to first modeled growth",
						   analysis.evidence_mode == 0 && !plan.steps.empty());
					button(context, state, 130, "Add to settings review");
					if (analysis.replace_pending || analysis.shared_factor_pending)
						button(context, state, 131, "Confirm shared settings");
				}
			}
		}
		button(context, state, 133,
			   "Settings review (" + std::to_string(analysis.staged.size()) + ")");
		if (analysis.show_review) {
			text(context, "Draft AppConfig settings", interface_theme::kTextCanvas, 12);
			for (const auto& staged : analysis.staged) {
				text(context, memory_public_config_key(staged.target.configKey),
					 interface_theme::kAccentSeaGlass, 10);
				text(context, (staged.plan.uses_default ? "Use default · " : "Explicit · ") +
								  memory_target_value(staged.plan.initial, staged.target.unit,
													  state.decimal) +
								  " (" + std::to_string(staged.plan.initial) + " native units)");
				text(context,
					 staged.provenance + " · " + std::to_string(staged.plan.growth_count) +
						 " modeled resize(s)",
					 interface_theme::kTextMuted, 10);
			}
			text(context,
				 "One shared growthFactor: " + std::to_string(analysis.shared_factor).substr(0, 5));
			button(context, state, 132, "Clear staged settings", !analysis.staged.empty());
			button(context, state, 120, "Export settings", false);
			button(context, state, 121, "Bake settings", false);
			text(context,
				 "Export / Bake integration pending. Drafts do not change live or startup "
				 "configuration.",
				 interface_theme::kTextMuted, 10);
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
