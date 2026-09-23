#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include <algorithm>
#include <cassert>

using namespace FlowUi;
using namespace FlowUi::devSystems;

int main() {
	DevInterfaceState interface_state{};
	auto& selection = interface_state.performance_selection;
	const TimingZoneDescriptor frame = timing_zones::kWindowFrameTotal;
	CpuTimingRecord cpu{};
	cpu.typeId = frame.typeId;
	cpu.frame.window = MainWindowId;
	cpu.track = 1;
	GpuTimingRecord gpu{};
	gpu.typeId = frame.typeId;
	gpu.frame.window = MainWindowId;
	assert(selection.selector_mode == 0);
	assert(std::ranges::all_of(selection.expanded_categories,
							   [](bool expanded) { return !expanded; }));
	assert(selection.accepts(cpu, frame));
	assert(selection.accepts(gpu, frame));

	// Zero IDs and overlapping window/thread IDs must remain distinct selections.
	const DevPerformanceScope all_windows{};
	const DevPerformanceScope all_threads{0, DevPerformanceScopeKind::Thread};
	const DevPerformanceScope main_window{1, DevPerformanceScopeKind::Window};
	const DevPerformanceScope main_thread_scope{1, DevPerformanceScopeKind::Thread};
	assert(selection.selected_scope == all_windows);
	assert(selection.selected_scope != all_threads);
	selection.selected_scope = all_threads;
	assert(selection.selected_scope != all_windows);
	assert(main_window != main_thread_scope);
	selection.selected_scope = main_window;
	cpu.track = 23;
	assert(selection.accepts(cpu, frame)); // A window scope does not constrain the thread.
	selection.selected_scope = main_thread_scope;
	cpu.frame.window = 8;
	cpu.track = 1;
	assert(selection.accepts(cpu, frame)); // A thread scope replaces the window filter.
	selection.selected_scope = all_threads;
	cpu.frame.window = MainWindowId;

	// All Threads means every real track, not an alias hard-coded to track 1.
	cpu.track = 23;
	assert(selection.accepts(cpu, frame));
	selection.selected_scope = {.id = 1, .kind = DevPerformanceScopeKind::Thread};
	assert(!selection.accepts(cpu, frame));
	cpu.track = 1;
	assert(selection.accepts(cpu, frame));
	assert(selection.accepts(gpu, frame));

	selection.selected_scope = {.id = 8, .kind = DevPerformanceScopeKind::Window};
	assert(!selection.accepts(cpu, frame));
	assert(!selection.accepts(gpu, frame));
	selection.selected_scope = {.id = MainWindowId, .kind = DevPerformanceScopeKind::Window};
	assert(selection.accepts(cpu, frame));
	cpu.frame.window = InvalidWindowId;
	assert(!selection.accepts(cpu, frame));
	selection.selected_scope = {};
	assert(selection.accepts(cpu, frame));

	selection.hardware_domain = 1;
	assert(selection.accepts(cpu, frame));
	assert(!selection.accepts(gpu, frame));
	selection.hardware_domain = 2;
	assert(!selection.accepts(cpu, frame));
	assert(selection.accepts(gpu, frame));
	selection.hardware_domain = 0;

	selection.category_mask ^= timingCategoryBit(TimingCategory::Frame);
	assert(!selection.accepts(cpu, frame));
	assert(!selection.accepts(gpu, frame));
	selection.category_mask ^= timingCategoryBit(TimingCategory::Frame);
	assert(selection.accepts(cpu, frame));
	selection.category_mask = 0;
	assert(!selection.accepts(cpu, frame));
	selection.category_mask = timingCategoryBit(TimingCategory::Frame);

	selection.selected_zone = timing_zones::kElementInvoke.typeId;
	assert(selection.accepts(cpu, frame)); // Scope ignores a retained zone choice.
	selection.selector_mode = 1;
	assert(!selection.accepts(cpu, frame));
	selection.selected_zone = frame.typeId;
	assert(selection.accepts(cpu, frame));
	selection.zone_search = "unrelated search";
	assert(selection.accepts(cpu, frame)); // Search filters navigation, not samples.
	cpu.typeId = timing_zones::kElementInvoke.typeId;
	assert(!selection.accepts(cpu, frame));

	interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Inspect);
	interface_state.selectedWindowId = 99;
	interface_state.searchQuery = "inspect search";
	assert(selection.selected_scope == DevPerformanceScope{});
	assert(selection.zone_search == "unrelated search");
	assert(selection.selected_zone == frame.typeId);

	assert(performance_zone_label("flowui.frame.total") == "FlowUi Frame Total");
	assert(performance_zone_label("flowui.frame.user_build") == "FlowUi Frame User Build");
	assert(performance_zone_label("gpu.ui.cpu") == "GPU UI CPU");
	assert(performance_zone_label("").empty());
	assert(performance_zone_label("..user__zone.") == "User Zone");
	const auto label = performance_zone_label(frame.name);
	assert(performance_zone_matches(frame.name, label, "FLOWUI FRAME"));
	assert(performance_zone_matches(frame.name, label, "frame.total"));
	assert(performance_zone_matches(frame.name, label, ""));
	assert(!performance_zone_matches(frame.name, label, "layout"));

	// Consume real producer records through the same reporting API as the selector.
	DevTiming timing;
	DevGpuTiming gpu_timing(timing);
	DevTimingReporting reporting(timing, gpu_timing);
	auto main_thread = timing.attachCurrentThread("flowui.platform");
	auto& recorder = main_thread.recorder();
	recorder.setFrameContext({MainWindowId, 1}, 1);
	constexpr auto custom_zone =
		makeTimingDescriptor(TimingCategory::User, TimingZoneRole::Work, "application.custom_work",
							 {}, CpuTimingLevel::OnlyFrameTime);
	const auto token = recorder.tryBegin(custom_zone);
	assert(token);
	recorder.end(token);
	reporting.consumeThrough(1);
	const auto tracks = reporting.cpuTrackSnapshot();
	assert(tracks.size() == 1);
	assert(tracks.front().id == recorder.trackId());
	const auto descriptors = reporting.descriptorSnapshot();
	const auto descriptor =
		std::ranges::find(descriptors, custom_zone.typeId, &TimingZoneDescriptor::typeId);
	assert(descriptor != descriptors.end());
	assert(performance_zone_label(descriptor->name) == "Application Custom Work");
	const auto report = reporting.appTickReport(1);
	assert(report);
	assert(report->windows.size() == 1);
	const auto& samples = report->windows.front().frames.front().cpuZones;
	assert(samples.size() == 1);
	DevPerformanceSelection reported_selection{};
	assert(reported_selection.accepts(samples.front(), *descriptor));
	reported_selection.selected_scope = {.id = tracks.front().id,
										 .kind = DevPerformanceScopeKind::Thread};
	assert(reported_selection.accepts(samples.front(), *descriptor));
	reported_selection.selector_mode = 1;
	reported_selection.selected_zone = custom_zone.typeId;
	assert(reported_selection.accepts(samples.front(), *descriptor));
	reported_selection.category_mask = 0;
	assert(!reported_selection.accepts(samples.front(), *descriptor));
	assert(timing.config().enabledCategoryMask == 0xFFFFFFFFu);
	return 0;
}
