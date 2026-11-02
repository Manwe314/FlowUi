#ifdef NDEBUG
#undef NDEBUG
#endif
#include "clay.h"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <span>

using namespace FlowUi;
using namespace FlowUi::devSystems;
namespace {
[[nodiscard]] DevDiagnosticsOccurrenceRow row(uint64_t id, uint64_t tick, uint32_t native = 0,
											  uint64_t source = 7, WindowId window = 1,
											  ErrorCode code = ErrorCode::StorageCapacityExceeded) {
	DevDiagnosticsOccurrenceRow result{};
	result.error = makeError(code, ErrorSite::ResourceAllocatePersistent, id, 0, native);
	result.id = id;
	result.first_app_tick = tick;
	result.last_app_tick = tick + 10;
	result.first_timestamp_ns = tick * 1000;
	result.last_timestamp_ns = (tick + 10) * 1000;
	result.steps.emplace_back(
		DevDiagnosticsStepReference{.context = {.appTick = tick, .frame = {window, tick}},
									.source_id = source,
									.sequence = id * 10});
	return result;
}
void grouping_and_stable_order() {
	DevDiagnosticsState state;
	state.snapshot.status.lastConsumedAppTick = 200;
	state.snapshot.rows.reserve(181);
	for (uint64_t occurrence = 1; occurrence <= 180; ++occurrence) {
		auto entry = row(occurrence, occurrence, static_cast<uint32_t>(occurrence % 2),
						 occurrence % 3 + 7, occurrence % 2 + 1);
		entry.resolution =
			occurrence % 2 ? ErrorResolution::UsedFallback : ErrorResolution::Rejected;
		for (uint64_t step = 1; step <= 8; ++step)
			entry.steps.emplace_back(
				DevDiagnosticsStepReference{.source_id = 99, .sequence = occurrence * 10 + step});
		state.snapshot.rows.emplace_back(std::move(entry));
	}
	rebuild_diagnostics_model(state);
	assert(state.groups.size() == 1 && state.groups.front().count == 180);
	assert(state.groups.front().window_count == 2 && state.groups.front().subject_count == 180);
	assert(state.groups.front().outcomes[static_cast<size_t>(ErrorResolution::UsedFallback)] == 90);
	assert(state.groups.front().last_tick == 180); // Last start, not last appended step.
	const auto identity = state.groups.front().identity;
	select_diagnostics_group(state, state.groups.front().key, DevErrorGroupMode::Contract);
	assert(state.selected_id == 180 && !state.follow_latest);
	state.snapshot.rows.emplace_back(row(181, 199));
	rebuild_diagnostics_model(state);
	assert(state.groups.front().identity == identity && state.selected_id == 180 &&
		   state.newer_available);
	assert(state.history.size() == 181 && state.history.front().id == 181);
	uint64_t start_count = 0;
	for (const auto& bin : state.bins)
		start_count += bin.count;
	assert(start_count == 181);
	state.follow_latest = true;
	rebuild_diagnostics_model(state);
	assert(state.selected_id == 181);
	state.filters.grouping = 1;
	rebuild_diagnostics_model(state);
	assert(state.groups.size() == 2);
	state.filters.grouping = 2;
	rebuild_diagnostics_model(state);
	assert(state.groups.size() == 3);
	for (const auto& group : state.groups)
		assert(group.key.nativeCode == 0 && group.key.sourceId != 99);
	state.filters.grouping = 0;
	rebuild_diagnostics_model(state);
	assert(state.groups.front().identity ==
		   identity); // Group-mode changes preserve full-key identity.
	state.filters.search = "no such retained condition";
	rebuild_diagnostics_model(state);
	assert(state.groups.empty());
	state.filters.search.clear();
	rebuild_diagnostics_model(state);
	assert(state.groups.front().identity ==
		   identity); // Filtering does not replace keyboard focus IDs.
	state.snapshot.rows.emplace_back(row(182, 199, 0, 7, 1, ErrorCode::VulkanDeviceLost));
	rebuild_diagnostics_model(state);
	const auto old_order = state.groups;
	state.snapshot.rows.emplace_back(row(183, 200, 0, 7, 1, ErrorCode::VulkanDeviceLost));
	rebuild_diagnostics_model(state);
	assert(state.groups.front().identity == old_order.front().identity);
}
void timestamp_fallback_and_tick_extremes() {
	DevDiagnosticsState state;
	state.snapshot.status.lastConsumedAppTick = UINT64_MAX;
	state.snapshot.rows = {row(1, UINT64_MAX - 10), row(2, UINT64_MAX)};
	state.snapshot.rows.front().first_timestamp_ns = 1000000;
	state.snapshot.rows.back().first_timestamp_ns = 0;
	rebuild_diagnostics_model(state);
	assert(state.groups.front().latest == 2 && state.groups.front().first == 1);
	assert(state.groups.front().first_timestamp_ns == 0 &&
		   state.groups.front().last_timestamp_ns == 0);
	select_diagnostics_group(state, state.groups.front().key, state.groups.front().mode);
	rebuild_diagnostics_model(state);
	assert(state.history.front().id == 2 && state.bins.back().last_tick == UINT64_MAX);
	uint64_t starts = 0;
	for (const auto& bin : state.bins)
		starts += bin.count;
	assert(starts == 2);
}
void occurrence_predicates_and_activity() {
	DevDiagnosticsState state;
	state.snapshot.status.lastConsumedAppTick = 1000;
	state.snapshot.sources.emplace_back(DevDiagnosticsSource{.name = "Storage allocator",
															 .file = "project/Storage.cpp",
															 .function = "reserve_bytes",
															 .id = 7,
															 .line = 91});
	state.snapshot.sources.emplace_back(DevDiagnosticsSource{.name = "Popup resolver",
															 .file = "project/Popup.cpp",
															 .function = "anchor",
															 .id = 8,
															 .line = 17});
	state.snapshot.rows = {row(10, 100, 0, 7, 1), row(11, 990, 1, 8, 2), row(12, 995, 0, 7, 3)};
	state.filters.search = "Storage.cpp 11";
	rebuild_diagnostics_model(state);
	assert(state.groups.empty());
	state.filters.search = "STORAGE.CPP RESERVE_BYTES";
	rebuild_diagnostics_model(state);
	assert(state.groups.front().count == 2);
	state.filters.window_mode = 1;
	state.filters.window = 2;
	rebuild_diagnostics_model(state);
	assert(state.groups.empty());
	state.filters.search.clear();
	state.filters.source_mode = 1;
	state.filters.source = 7;
	rebuild_diagnostics_model(state);
	assert(state.groups.empty());
	// A later retained step is searchable/filterable; first-step-only summaries lose it.
	state.snapshot.rows[1].steps.emplace_back(DevDiagnosticsStepReference{
		.context = {.appTick = 991, .frame = {2, 991}}, .source_id = 7, .sequence = 111});
	rebuild_diagnostics_model(state);
	assert(state.groups.front().count == 1);
	state.filters.first_frame = 992;
	rebuild_diagnostics_model(state);
	assert(state.groups.empty());
	reset_diagnostics_filters(state);
	state.filters.activity_view = 1;
	rebuild_diagnostics_model(state);
	assert(state.groups.size() == 1 && state.groups.front().count == 3); // Ongoing selects groups.
	state.filters.activity_view = 2;
	rebuild_diagnostics_model(state);
	assert(state.groups.front().count == 2); // Recent selects starts.
	state.snapshot.status.lastConsumedAppTick = 2000;
	rebuild_diagnostics_model(state);
	assert(state.groups.empty());
	state.filters.activity_view = 0;
	state.filters.source_mode = 2;
	rebuild_diagnostics_model(state);
	assert(state.groups.empty());
	state.snapshot.rows.emplace_back(row(13, 1999, 0, 55, InvalidWindowId));
	rebuild_diagnostics_model(state);
	assert(state.groups.front().count == 1);
	state.filters.window_mode = 2;
	rebuild_diagnostics_model(state);
	assert(state.groups.front().count == 1);
	state.filters.search = "0xd";
	rebuild_diagnostics_model(state);
	select_diagnostics_group(state, state.groups.front().key, state.groups.front().mode);
	assert(state.selected_id == 13);
	state.filters.search = "unmatched";
	rebuild_diagnostics_model(state);
	assert(state.groups.empty() && state.selected_outside_filters && state.selected_id == 13);
}
void frozen_and_evicted_investigation() {
	DevErrorConfig capture;
	capture.level = DevErrorCaptureLevel::Causal;
	DevErrorMonitoring monitoring(capture);
	DevErrorReportingConfig config;
	config.retainedOccurrenceCapacity = 2;
	DevErrorReporting reporting(monitoring, config);
	auto attachment = monitoring.attachCurrentThread("diagnostics.model.test");
	constexpr auto source = makeDevErrorSource("diagnostics.model.capacity");
	attachment.recorder().setContext({.appTick = 1, .frame = {1, 1}});
	const auto first = monitoring.recordRaised(
		makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent),
		source);
	reporting.consumeThrough(1);
	DevDiagnosticsState state;
	refresh_diagnostics_model(state, reporting, 1, true);
	assert(state.groups.size() == 1);
	select_diagnostics_group(state, state.groups.front().key, state.groups.front().mode);
	refresh_diagnostics_model(state, reporting, 2, true);
	assert(state.selected && state.selected->id == first);
	auto owned = reporting.diagnostics_investigation();
	assert(owned);
	state.owned = std::move(*owned);
	state.snapshot = state.owned.snapshot;
	state.frozen = true;
	const auto frozen_revision = state.snapshot.revision;
	for (uint64_t tick = 2; tick <= 5; ++tick) {
		attachment.recorder().setContext({.appTick = tick, .frame = {1, tick}});
		assert(monitoring.recordRaised(
			makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent),
			source));
		reporting.consumeThrough(tick);
	}
	assert(!reporting.occurrence(first));
	refresh_diagnostics_model(state, reporting, 300'000'000, true);
	assert(state.snapshot.revision == frozen_revision && state.selected->id == first &&
		   state.anchor_tick == 1);
	state.filters.search = "capacity";
	refresh_diagnostics_model(state, reporting, 300'000'001);
	assert(state.groups.size() == 1 && state.selected->id == first);
	state.frozen = false;
	refresh_diagnostics_model(state, reporting, 400'000'000, true);
	assert(state.selected_evicted && state.selected->id == first && state.selected_id == first);
	assert(state.anchor_tick == 5 && state.groups.front().count == 2);
	monitoring.recordStep(5, DevErrorStepKind::Resolution, {}, ErrorResolution::UsedFallback,
						  source);
	reporting.consumeThrough(5);
	select_diagnostics_group(state, state.groups.front().key, state.groups.front().mode);
	refresh_diagnostics_model(state, reporting, 500'000'000, true);
	assert(!state.selected_evicted && state.selected->resolution == ErrorResolution::UsedFallback);
}

void freeze_empty_evicted_and_budget_limited_views() {
	DevErrorConfig capture{};
	capture.level = DevErrorCaptureLevel::Causal;
	DevErrorMonitoring monitoring(capture);
	DevErrorReportingConfig config{};
	config.retainedOccurrenceCapacity = 2;
	DevErrorReporting reporting(monitoring, config);
	DevDiagnosticsState state{};
	refresh_diagnostics_model(state, reporting, 1, true);
	assert(freeze_diagnostics_model(state, reporting));
	assert(state.frozen && state.snapshot.rows.empty() && !state.selected);
	state.frozen = false;
	auto attachment = monitoring.attachCurrentThread("diagnostics.freeze.test");
	constexpr auto source = makeDevErrorSource("diagnostics.freeze.capacity");
	attachment.recorder().setContext({.appTick = 1});
	const auto first = monitoring.recordRaised(
		makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent),
		source);
	reporting.consumeThrough(1);
	refresh_diagnostics_model(state, reporting, 2, true);
	select_diagnostics_group(state, state.groups.front().key, state.groups.front().mode);
	refresh_diagnostics_model(state, reporting, 3, true);
	assert(state.selected && state.selected->id == first);
	const std::string advice =
		state.selected->advice.empty() ? "" : std::string(state.selected->advice.front().title);
	uint64_t latest = first;
	for (uint64_t tick = 2; tick <= 4; ++tick) {
		attachment.recorder().setContext({.appTick = tick});
		latest = monitoring.recordRaised(
			makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent),
			source);
		assert(latest);
		reporting.consumeThrough(tick);
	}
	refresh_diagnostics_model(state, reporting, 4, true);
	assert(state.selected_evicted && state.selected->id == first);
	assert(freeze_diagnostics_model(state, reporting));
	assert(state.frozen && !state.selected_only_freeze && !state.frozen_details_limited);
	assert(state.selected && state.selected->id == first && state.selected_evicted);
	assert(state.snapshot.rows.size() == 2);
	assert(std::ranges::none_of(state.snapshot.rows,
								[&](const auto& entry) { return entry.id == first; }));
	assert(select_diagnostics_investigation(state.owned, first));
	if (!advice.empty())
		assert(state.selected->advice.front().title == advice);
	select_diagnostics_occurrence(state, latest);
	assert(state.selected->id == first); // Drawing never replaces the borrowed report mid-build.
	refresh_diagnostics_model(state, reporting, 5);
	assert(state.selected && state.selected->id == latest);
	assert(freeze_diagnostics_model(state, reporting, false, 1));
	assert(state.frozen && state.frozen_details_limited && state.snapshot.rows.size() == 2);
	assert(state.selected && state.selected->id == latest);
	const auto other = std::ranges::find_if(state.snapshot.rows,
											[&](const auto& entry) { return entry.id != latest; });
	assert(other != state.snapshot.rows.end());
	select_diagnostics_occurrence(state, other->id);
	refresh_diagnostics_model(state, reporting, 6);
	assert(!state.selected); // Uncaptured details never display the previous occurrence's report.
}

void clay_collision_pruning_preserves_retained_tail() {
	auto* previous_context = Clay_GetCurrentContext();
	std::vector<std::byte> storage(Clay_MinMemorySize());
	const auto arena = Clay_CreateArenaWithCapacityAndMemory(storage.size(), storage.data());
	const auto* context = Clay_Initialize(arena, {640, 480}, {});
	assert(context);
	const auto bucket_count = static_cast<uint32_t>(Clay_GetMaxElementCount());
	const auto root = Clay_GetElementId(CLAY_STRING("Clay__RootContainer"));
	const auto bucket = static_cast<uint32_t>((uint64_t{root.id} + 1) % bucket_count);
	const auto base = bucket ? bucket : bucket_count;
	const std::array<Clay_ElementId, 4> initial{{{.id = base},
												 {.id = base + bucket_count},
												 {.id = base + 2 * bucket_count},
												 {.id = base + 3 * bucket_count}}};
	const auto render = [](std::span<const Clay_ElementId> identities) {
		Clay_BeginLayout();
		Clay_ElementDeclaration leaf{};
		leaf.layout.sizing = {.width = CLAY_SIZING_FIXED(32), .height = CLAY_SIZING_FIXED(12)};
		leaf.backgroundColor = {255, 255, 255, 255};
		for (const auto identity : identities) {
			CLAY(identity, leaf) {}
		}
		const auto commands = Clay_EndLayout(0.016f);
		assert(commands.length >= static_cast<int32_t>(identities.size()));
	};
	render(initial);
	assert(Clay_GetElementData(initial.back()).found);
	const std::array<Clay_ElementId, 2> retained{initial.front(), initial.back()};
	render(retained);
	// Following occurrences and changing filters remove adjacent IDs in a collision bucket.
	// The retained tail must survive before removed slots are reused; the old bug fails here.
	assert(Clay_GetElementData(initial.back()).found);
	assert(!Clay_GetElementData(initial[1]).found && !Clay_GetElementData(initial[2]).found);
	const std::array<Clay_ElementId, 3> reused{
		initial.front(), Clay_ElementId{.id = base + 4 * bucket_count}, initial.back()};
	render(reused);
	assert(Clay_GetElementData(reused[1]).found && Clay_GetElementData(initial.back()).found);
	Clay_SetCurrentContext(previous_context);
}
} // namespace
int main() {
	grouping_and_stable_order();
	timestamp_fallback_and_tick_extremes();
	occurrence_predicates_and_activity();
	frozen_and_evicted_investigation();
	freeze_empty_evicted_and_budget_limited_views();
	clay_collision_pruning_preserves_retained_tail();
	std::cout << "Diagnostics model grouping, filtering, pinning and frozen retention verified\n";
}
