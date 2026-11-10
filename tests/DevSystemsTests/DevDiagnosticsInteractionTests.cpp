#ifdef NDEBUG
#undef NDEBUG
#endif

#include "FSEL/RadioChoice.hpp"
#include "FSEL/TextInput.hpp"
#include "FlowUi/Public/Flow.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsControls.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devInterface/Diagnostics/Inspector/DevDiagnosticsInspector.hpp"
#include "devSystems/devInterface/Diagnostics/Selector/DevDiagnosticsSelector.hpp"
#include "devSystems/devInterface/Diagnostics/Workbench/DevDiagnosticsWorkbench.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
constexpr auto selector_id = Global<kDevDiagnosticsSelector>("diagnostics.test.selector");
constexpr auto workbench_id = Global<kDevDiagnosticsWorkbench>("diagnostics.test.workbench");
constexpr auto inspector_id = Global<kDevDiagnosticsInspector>("diagnostics.test.inspector");
constexpr auto controls_id = Global<kDevDiagnosticsControls>("diagnostics.test.controls");
constexpr auto recorded_source = makeDevErrorSource(
	"diagnostics.test.capacity_limit",
	"project/components/storage/AnIntentionallyLongCapturedSourcePathWithoutWhitespace.cpp",
	"application::storage::publish_a_large_document_snapshot_with_exact_revision", 127, 9);
constexpr auto recorded_breadcrumb = makeDevErrorBreadcrumb("diagnostics.test.before_allocation");

[[nodiscard]] FlowElementID local(FlowElementID parent, FlowDefinitionID definition,
								  LocalElementName name) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(parent, definition, name.token);
}
[[nodiscard]] FlowElementID local(FlowElementID parent, FlowDefinitionID definition,
								  IndexedElementName name) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(parent, definition, name.token);
}
[[nodiscard]] uint64_t monotonic_ns() noexcept {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
									 std::chrono::steady_clock::now().time_since_epoch())
									 .count());
}
void check_horizontal_bounds(App& app, FlowElementID surface) {
	const auto& snapshot = app.ui().devTreeSnapshot();
	const auto bounds = Clay_GetElementData(app.ui().toClayEID(surface));
	assert(bounds.found && bounds.boundingBox.width > 0);
	for (const auto& node : snapshot.clay.nodes) {
		auto owner = node.directFlowOwner;
		bool child = false;
		while (owner != tooling::InvalidFlowNode && owner < snapshot.flow.nodes.size()) {
			if (snapshot.flow.nodes[owner].instance.value == surface.value) {
				child = true;
				break;
			}
			owner = snapshot.flow.nodes[owner].parent;
		}
		if (!child || tooling::hasFlag(node.flags, tooling::DevClayNodeFlag::BoundsUnavailable))
			continue;
		if (node.bounds.x < bounds.boundingBox.x - 1 ||
			node.bounds.x + node.bounds.width >
				bounds.boundingBox.x + bounds.boundingBox.width + 1) {
			std::cerr << "Diagnostics overflow: " << snapshot.string(node.idString)
					  << " text=" << snapshot.string(node.text) << " x=" << node.bounds.x
					  << " width=" << node.bounds.width << " panel=" << bounds.boundingBox.x << ','
					  << bounds.boundingBox.width << '\n';
			assert(false && "Diagnostics content stays within its resizable surface");
		}
	}
}
template <class Input>
void check_centered_input(App& app, const Input& element, FlowElementID identity) {
	const auto content = Clay_GetElementData(
		Clay_ElementId{.id = FlowIDToClayID(PartID(element, identity, Input::Parts::content))});
	const auto value = Clay_GetElementData(
		Clay_ElementId{.id = FlowIDToClayID(PartID(element, identity, Input::Parts::text))});
	assert(content.found && value.found && value.boundingBox.width > 0);
	assert(std::abs(content.boundingBox.x + content.boundingBox.width / 2 - value.boundingBox.x -
					value.boundingBox.width / 2) < 2);
	assert(std::abs(content.boundingBox.y + content.boundingBox.height / 2 - value.boundingBox.y -
					value.boundingBox.height / 2) < 2);
}
} // namespace

int main() {
	AppConfig config;
	config.window.title = "Diagnostics complete surface verification";
	config.window.width = 1400;
	config.window.height = 900;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.dev.monitoring.errors.level = DevErrorCaptureLevel::Causal;
	config.dev.monitoring.errorReporting.retainedOccurrenceCapacity = 1024;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Diagnostics);
	auto& state = session.diagnostics;
	auto& monitoring = app.devMonitoring().errors();
	auto& reporting = app.devMonitoring().errorReporting();
	auto attachment = monitoring.attachCurrentThread("diagnostics-interaction-fixture");
	assert(attachment);
	uint64_t first_id = 0, latest_id = 0;
	for (uint64_t tick = 1; tick <= 3; ++tick) {
		attachment.recorder().setContext({.appTick = tick,
										  .frame = {7, tick},
										  .primaryEntity = 42,
										  .secondaryEntity = 18,
										  .timingTrack = 4});
		monitoring.recordBreadcrumb(recorded_breadcrumb, 999, 123);
		latest_id = monitoring.recordRaised(makeError(ErrorCode::StorageCapacityExceeded,
													  ErrorSite::ResourceAllocatePersistent,
													  UINT64_MAX - tick, 32, 73),
											recorded_source);
		if (!first_id)
			first_id = latest_id;
		monitoring.recordStep(latest_id, DevErrorStepKind::Resolution,
							  makeError(ErrorCode::StorageCapacityExceeded,
										ErrorSite::ResourceAllocatePersistent, UINT64_MAX - tick,
										32, 73),
							  ErrorResolution::GrewCapacity, recorded_source);
		reporting.consumeThrough(tick);
	}
	state.filters.search = std::string(recorded_source.name);
	refresh_diagnostics_model(state, reporting, monotonic_ns(), true);
	assert(state.groups.size() == 1 && state.groups.front().count == 3);
	float inspector_width = 360, workbench_width = 750, selector_width = 280;
	Clay_Vector2 last_native_pointer{};
	float last_native_scroll = 0;
	bool draw_controls = true;
	const auto draw = [&](FlowElementID control = {}, int event = 0, int key = -1,
						  Clay_Vector2 pointer = {-1, -1}) {
		service_diagnostics(app, session, true);
		assert(app.beginFrame());
		const auto& native_input = app.ui().getCurrentFrameInput();
		last_native_pointer = {native_input.mouseX, native_input.mouseY};
		last_native_scroll = native_input.scrollY;
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.mouseX = pointer.x;
		input.mouseY = pointer.y;
		input.windowFocused = true;
		input.mouseDown = {};
		input.keyDown = {};
		input.mouseDown[0] = event == 1;
		if (key >= 0)
			input.keyDown[key] = true;
		if (control.value) {
			const auto identity = app.ui().toClayEID(control).id;
			interaction.hoveredElementIds.emplace_back(identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(identity);
		}
		auto root = diagnostics_ui::column(0);
		root.layout.sizing = {.width = CLAY_SIZING_FIXED(1400), .height = CLAY_SIZING_FIXED(900)};
		root.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(CLAY_ID("diagnostics-test"), root) {
			if (draw_controls)
				app.ui()
					.createElement(kDevDiagnosticsControls, controls_id)
					.setParameters({&app, &session})
					.draw();
			auto body = diagnostics_ui::row();
			body.layout.sizing.height = CLAY_SIZING_GROW(0);
			CLAY(CLAY_ID("diagnostics-test-columns"), body) {
				auto selector = diagnostics_ui::column(0);
				selector.layout.sizing = {.width = CLAY_SIZING_FIXED(selector_width),
										  .height = CLAY_SIZING_GROW(0)};
				selector.clip = {.horizontal = true, .vertical = true};
				CLAY(CLAY_ID("diagnostics-test-selector"), selector)
				app.ui()
					.createElement(kDevDiagnosticsSelector, selector_id)
					.setParameters({&app, &session})
					.draw();
				auto workbench = diagnostics_ui::column(0);
				workbench.layout.sizing = {.width = CLAY_SIZING_FIXED(workbench_width),
										   .height = CLAY_SIZING_GROW(0)};
				workbench.clip = {.horizontal = true, .vertical = true};
				CLAY(CLAY_ID("diagnostics-test-workbench"), workbench)
				app.ui()
					.createElement(kDevDiagnosticsWorkbench, workbench_id)
					.setParameters({&app, &session})
					.draw();
				auto inspector = diagnostics_ui::column(0);
				inspector.layout.sizing = {.width = CLAY_SIZING_FIXED(inspector_width),
										   .height = CLAY_SIZING_GROW(0)};
				inspector.clip = {.horizontal = true, .vertical = true};
				CLAY(CLAY_ID("diagnostics-test-inspector"), inspector)
				app.ui()
					.createElement(kDevDiagnosticsInspector, inspector_id)
					.setParameters({&app, &session})
					.draw();
			}
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto preview = [](std::string_view name) {
		if (std::getenv("FLOWUI_DIAGNOSTICS_PREVIEW")) {
			std::cout << "Diagnostics detail preview=" << name << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	};
	const auto inspector_scroll_id = app.ui().toClayEID(local(
		FlowElementID{inspector_id.value}, DevDiagnosticsInspector::definitionId, "scroll-body"));
	const auto workbench_scroll_id = app.ui().toClayEID(
		local(FlowElementID{workbench_id.value}, DevDiagnosticsWorkbench::definitionId, "scroll"));
	const auto popover_scroll_id = app.ui().toClayEID(local(
		FlowElementID{controls_id.value}, DevDiagnosticsControls::definitionId, "popover-body"));
	const auto click = [&](FlowElementID identity) {
		for (int attempt = 0; attempt < 2; ++attempt) {
			for (const auto scroll_identity :
				 {inspector_scroll_id, workbench_scroll_id, popover_scroll_id}) {
				const auto control = Clay_GetElementData(app.ui().toClayEID(identity));
				const auto viewport = Clay_GetElementData(scroll_identity);
				const auto scroll = Clay_GetScrollContainerData(scroll_identity);
				if (!control.found || !viewport.found || !scroll.found || !scroll.scrollPosition ||
					control.boundingBox.x < viewport.boundingBox.x ||
					control.boundingBox.x >= viewport.boundingBox.x + viewport.boundingBox.width)
					continue;
				if (control.boundingBox.y < viewport.boundingBox.y)
					scroll.scrollPosition->y += viewport.boundingBox.y - control.boundingBox.y + 16;
				if (control.boundingBox.y + control.boundingBox.height >
					viewport.boundingBox.y + viewport.boundingBox.height)
					scroll.scrollPosition->y -= control.boundingBox.y + control.boundingBox.height -
												viewport.boundingBox.y -
												viewport.boundingBox.height + 16;
			}
			draw();
		}
		draw(identity, 1);
		draw(identity, 2);
		draw();
	};
	const auto selector_control = [&](auto name) {
		return local(FlowElementID{selector_id.value}, diagnostics_ui::Control::definitionId, name);
	};
	const auto workbench_control = [&](auto name) {
		return local(FlowElementID{workbench_id.value}, diagnostics_ui::Control::definitionId,
					 name);
	};
	const auto header_control = [&](uint64_t key) {
		return local(FlowElementID{controls_id.value}, FSEL::Button::definitionId,
					 Keyed("diagnostics-control", key));
	};
	const auto wheel = [&](Clay_ElementId viewport_identity, float delta) {
		const auto viewport = Clay_GetElementData(viewport_identity);
		const auto before = Clay_GetScrollContainerData(viewport_identity);
		assert(viewport.found && before.found && before.scrollPosition);
		assert(before.contentDimensions.height > before.scrollContainerDimensions.height + 1);
		const float old_position = before.scrollPosition->y;
		auto* window = static_cast<GLFWwindow*>(app.nativeWindowHandle());
		glfwFocusWindow(window);
		glfwSetCursorPos(window, viewport.boundingBox.x + viewport.boundingBox.width / 2,
						 viewport.boundingBox.y + viewport.boundingBox.height / 2);
		const auto scroll_callback = glfwSetScrollCallback(window, nullptr);
		assert(scroll_callback);
		glfwSetScrollCallback(window, scroll_callback);
		scroll_callback(window, 0, delta);
		draw();
		const auto after = Clay_GetScrollContainerData(viewport_identity);
		assert(after.found && after.scrollPosition);
		if ((delta < 0 && after.scrollPosition->y >= old_position - 1) ||
			(delta > 0 && after.scrollPosition->y <= old_position + 1)) {
			std::cerr << "Wheel viewport=" << viewport_identity.id << " expected delta=" << delta
					  << " input=" << last_native_scroll << " mouse=" << last_native_pointer.x
					  << ',' << last_native_pointer.y << " bounds=" << viewport.boundingBox.x << ','
					  << viewport.boundingBox.y << ',' << viewport.boundingBox.width << ','
					  << viewport.boundingBox.height << " position=" << old_position << " -> "
					  << after.scrollPosition->y << '\n';
		}
		if (delta < 0)
			assert(after.scrollPosition->y < old_position - 1);
		else
			assert(after.scrollPosition->y > old_position + 1);
	};
	draw();
	draw();
	click(selector_control(Keyed("issue", state.groups.front().identity)));
	assert(state.selected_id == latest_id && state.selected && state.selected->id == latest_id);
	assert(!state.follow_latest);
	assert(state.selected->steps.size() == 2);
	const auto hovered_issue = selector_control(Keyed("issue", state.groups.front().identity));
	const auto tooltip_identity =
		local(hovered_issue, diagnostics_ui::Control::definitionId, "tooltip");
	draw(hovered_issue, 0, -1, {200, 450});
	draw(hovered_issue, 0, -1, {200, 450});
	const auto tooltip = Clay_GetElementData(app.ui().toClayEID(tooltip_identity));
	assert(tooltip.found && tooltip.boundingBox.x >= 212);
	assert(tooltip.boundingBox.y + tooltip.boundingBox.height <= 438 + 1);
	preview("selector-tooltip");
	draw(hovered_issue, 0, -1, {1380, 8});
	const auto edge_tooltip = Clay_GetElementData(app.ui().toClayEID(tooltip_identity));
	assert(edge_tooltip.found && edge_tooltip.boundingBox.y >= 20);
	assert(edge_tooltip.boundingBox.x + edge_tooltip.boundingBox.width <= 1392 + 1);
	draw();
	const auto raised_sequence = state.selected->steps.front().sequence;
	click(workbench_control(Keyed("step", raised_sequence)));
	assert(state.focus_kind == 1 && state.focus_sequence == raised_sequence);
	click(local(FlowElementID{inspector_id.value}, DevDiagnosticsCopy::definitionId,
				Keyed("copy", 90000)));
	assert(app.clipboardText() == std::to_string(latest_id));
	click(local(FlowElementID{inspector_id.value}, DevDiagnosticsCopy::definitionId,
				Keyed("copy", 104)));
	assert(app.clipboardText() == recorded_source.location.file);
	const auto source_disclosure =
		local(FlowElementID{inspector_id.value}, FSEL::Button::definitionId,
			  Keyed("section-disclosure", 1));
	click(source_disclosure);
	assert(!state.inspector_expanded[1]);
	click(source_disclosure);
	assert(state.inspector_expanded[1]);
	click(workbench_control(LocalElementName{"first"}));
	if (state.selected_id != first_id) {
		const auto* group = selected_diagnostics_group(state);
		const auto control =
			Clay_GetElementData(app.ui().toClayEID(workbench_control(LocalElementName{"first"})));
		std::cerr << "First retained selection: expected=" << first_id
				  << " actual=" << state.selected_id << " groups=" << state.groups.size()
				  << " history=" << state.history.size() << " first=" << (group ? group->first : 0)
				  << " evicted=" << state.selected_evicted << " control=" << control.found
				  << " bounds=" << control.boundingBox.x << ',' << control.boundingBox.y << ','
				  << control.boundingBox.width << ',' << control.boundingBox.height << '\n';
	}
	assert(state.selected_id == first_id);
	click(workbench_control(LocalElementName{"latest"}));
	assert(state.selected_id == latest_id);
	const auto domain = static_cast<uint64_t>(ErrorDomain::Storage);
	click(selector_control(Keyed("domain", domain)));
	assert(!state.domain_expanded[domain] && state.selected_id == latest_id);
	click(selector_control(Keyed("domain", domain)));
	assert(state.domain_expanded[domain] && state.selected_id == latest_id);
	click(header_control(1));
	assert(state.show_trigger && state.trigger.code == ErrorCode::StorageCapacityExceeded);
	const auto trigger_help = local(FlowElementID{controls_id.value},
									DevDiagnosticsControls::definitionId, Keyed("help", 1));
	draw(trigger_help, 0, -1, {700, 250});
	draw(trigger_help, 0, -1, {700, 250});
	assert(Clay_GetElementData(app.ui().toClayEID(local(FlowElementID{controls_id.value},
														DevDiagnosticsControls::definitionId,
														Keyed("help-tooltip", 1))))
			   .found);
	preview("capture-help");
	draw();
	preview("capture-trigger");
	click(header_control(21));
	assert(reporting.config().triggers.size() == 1);
	click(header_control(21));
	assert(reporting.config().triggers.size() == 1); // An identical matching rule is disabled.
	click(header_control(20));
	assert(state.trigger.matchNativeCode);
	click(header_control(21));
	assert(reporting.config().triggers.size() == 2);
	assert(!reporting.config().triggers.front().matchNativeCode);
	click(header_control(1003));
	assert(reporting.config().triggers.front().matchNativeCode);
	click(header_control(1002));
	assert(reporting.config().triggers.size() == 1 &&
		   !reporting.config().triggers.front().matchNativeCode);
	const auto original_retention = reporting.config().retainedOccurrenceCapacity;
	click(header_control(22));
	assert(!state.show_trigger &&
		   reporting.config().retainedOccurrenceCapacity == original_retention);
	click(header_control(2));
	assert(state.frozen && state.selected_id == latest_id);
	const auto frozen_tick = state.anchor_tick;
	const auto frozen_capture_level = state.snapshot.capture_config.level;
	state.capture_level = static_cast<uint64_t>(DevErrorCaptureLevel::Summary);
	state.pending = DevDiagnosticsCommand::ApplyCaptureLevel;
	draw();
	assert(state.frozen && state.snapshot.capture_config.level == frozen_capture_level);
	assert(state.live_capture_config.level == DevErrorCaptureLevel::Summary);
	assert(state.capture_level == static_cast<uint64_t>(state.live_capture_config.level));
	attachment.recorder().setContext({.appTick = 100});
	assert(monitoring.recordRaised(
		makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent, 5),
		recorded_source));
	reporting.consumeThrough(100);
	draw();
	assert(state.anchor_tick == frozen_tick && state.groups.front().count == 3);
	click(header_control(2));
	assert(!state.frozen && state.selected_id == latest_id);
	refresh_diagnostics_model(state, reporting, monotonic_ns(), true);
	draw();
	draw();
	const auto original_timing = state.selected->timing;
	const auto original_memory = state.selected->memory;
	const auto original_scope = session.performance_selection.selected_scope;
	const auto original_sample = session.performance_timeline.inspected_sample;
	const auto original_memory_time = session.memory_workbench.selected_ns;
	state.selected->timing.state = DevErrorCorrelationState::Available;
	state.selected->timing.capture_generation = UINT64_MAX;
	assert(!navigate_diagnostics_performance(app, session));
	assert(session.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
	assert(session.performance_selection.selected_scope == original_scope);
	assert(session.performance_timeline.inspected_sample == original_sample);
	state.selected->memory.state = DevErrorCorrelationState::Available;
	state.selected->memory.history_generation = UINT64_MAX;
	assert(!navigate_diagnostics_memory(app, session));
	assert(session.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
	assert(session.memory_workbench.selected_ns == original_memory_time);
	state.selected->timing = original_timing;
	state.selected->memory = original_memory;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Performance);
	state.pending = DevDiagnosticsCommand::Back;
	state.return_available = true;
	const auto preserved_selection = state.selected_id;
	draw();
	assert(session.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
	assert(state.selected_id == preserved_selection && state.selected->id == preserved_selection);
	// Exercise actual backend wheel delivery in the same clipped columns as the shell.
	draw();
	draw();
	const auto workbench_top = Clay_GetScrollContainerData(workbench_scroll_id);
	const auto inspector_top = Clay_GetScrollContainerData(inspector_scroll_id);
	assert(workbench_top.found && inspector_top.found);
	workbench_top.scrollPosition->y = inspector_top.scrollPosition->y = 0;
	draw();
	wheel(workbench_scroll_id, -6);
	assert(Clay_GetScrollContainerData(inspector_scroll_id).scrollPosition->y == 0);
	wheel(workbench_scroll_id, 6);
	draw();
	wheel(inspector_scroll_id, -6);
	assert(std::abs(Clay_GetScrollContainerData(workbench_scroll_id).scrollPosition->y) < 1);
	wheel(inspector_scroll_id, 6);
	draw();

	const auto old_search = state.filters.search;
	state.filters.search = "capacity_limit";
	draw();
	draw();
	const auto search =
		local(FlowElementID{selector_id.value}, DevSelectorSearch::definitionId, "search");
	check_centered_input(app, FSEL::kTextInput,
						 local(search, FSEL::TextInput::definitionId, "search-input"));
	click(selector_control(LocalElementName{"show-filters"}));
	assert(state.filters_open);
	draw();
	draw();
	const auto numeric_identity =
		local(FlowElementID{selector_id.value}, decltype(diagnostics_ui::kNumber)::definitionId,
			  "first-tick");
	check_centered_input(app, diagnostics_ui::kNumber, numeric_identity);
	const auto filter_scroll = app.ui().toClayEID(local(
		FlowElementID{selector_id.value}, DevDiagnosticsSelector::definitionId, "filter-scroll"));
	wheel(filter_scroll, -6);
	wheel(filter_scroll, 6);
	click(selector_control(LocalElementName{"close-filters"}));
	assert(!state.filters_open);
	state.filters.search = old_search;
	for (const float width : {220.0f, 280.0f}) {
		selector_width = width;
		draw();
		draw();
		const auto track = Clay_GetElementData(
			app.ui().toClayEID(local(FlowElementID{selector_id.value},
									 DevDiagnosticsSelector::definitionId, "activity-track")));
		assert(track.found);
		const auto clip = Clay_GetElementData(
			app.ui().toClayEID(local(FlowElementID{selector_id.value},
									 DevDiagnosticsSelector::definitionId, "activity-clip")));
		assert(clip.found && track.boundingBox.width <= clip.boundingBox.width + 1);
		for (uint64_t choice = 0; choice < 3; ++choice) {
			const auto identity = local(FlowElementID{selector_id.value},
										FSEL::RadioChoice::definitionId, Keyed("activity", choice));
			const auto radio = Clay_GetElementData(app.ui().toClayEID(identity));
			assert(radio.found && radio.boundingBox.x >= track.boundingBox.x - 1);
			assert(radio.boundingBox.x + radio.boundingBox.width <=
				   track.boundingBox.x + track.boundingBox.width + 1);
			check_horizontal_bounds(app, identity);
		}
	}
	selector_width = 280;
	auto capture_config = monitoring.config();
	capture_config.level = DevErrorCaptureLevel::Causal;
	monitoring.setConfig(capture_config);
	for (uint64_t operation = 1; operation <= 19; ++operation) {
		attachment.recorder().setContext({.appTick = 300, .frame = {7, 300}});
		assert(monitoring.recordRaised(
			makeError(ErrorCode::StorageCapacityExceeded,
					  static_cast<ErrorSite>(static_cast<uint16_t>(ErrorSite::StorageInitialize) +
											 operation - 1)),
			recorded_source));
	}
	reporting.consumeThrough(300);
	refresh_diagnostics_model(state, reporting, monotonic_ns(), true);
	draw();
	draw();
	const auto selector_scroll = app.ui().toClayEID(
		local(FlowElementID{selector_id.value}, DevDiagnosticsSelector::definitionId, "scroll"));
	wheel(selector_scroll, -6);
	wheel(selector_scroll, 6);
	draw();
	for (const auto width : {280.0f, 360.0f, 450.0f}) {
		inspector_width = width;
		workbench_width = width > 400 ? 650 : 750;
		for (const auto identity : {inspector_scroll_id, workbench_scroll_id}) {
			const auto scroll = Clay_GetScrollContainerData(identity);
			if (scroll.found && scroll.scrollPosition)
				scroll.scrollPosition->y = 0;
		}
		state.inspector_scroll = 0;
		state.workbench_scroll = 0;
		draw();
		draw();
		check_horizontal_bounds(app, FlowElementID{inspector_id.value});
		check_horizontal_bounds(app, FlowElementID{workbench_id.value});
		if (std::getenv("FLOWUI_DIAGNOSTICS_PREVIEW")) {
			std::cout << "Diagnostics preview width=" << width << std::endl;
			for (int frame = 0; frame < 90; ++frame) {
				draw();
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
		}
	}
	// Following live occurrences must remain responsive as activity filters remove/recreate rows.
	click(workbench_control(LocalElementName{"follow"}));
	assert(state.follow_latest);
	state.filters.recent_ticks = 1;
	state.filters.ongoing_ticks = 1;
	const uint64_t live_cycles = std::getenv("FLOWUI_DIAGNOSTICS_LIVE_REPRO") ? 300 : 12;
	for (uint64_t cycle = 0; cycle < live_cycles; ++cycle) {
		const uint64_t tick = 2000 + cycle;
		attachment.recorder().setContext({.appTick = tick, .frame = {7, tick}});
		assert(monitoring.recordRaised(
			makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent),
			recorded_source));
		reporting.consumeThrough(tick);
		refresh_diagnostics_model(state, reporting, monotonic_ns(), true);
		draw();
		click(local(FlowElementID{selector_id.value}, FSEL::RadioChoice::definitionId,
					Keyed("activity", cycle % 3)));
		assert(state.follow_latest && state.filters.activity_view == cycle % 3);
		if (std::getenv("FLOWUI_DIAGNOSTICS_LIVE_REPRO"))
			std::cout << "Diagnostics live cycle " << cycle << " groups=" << state.groups.size()
					  << " selected=" << state.selected_id << std::endl;
	}
	// Age every occurrence out of Recent while preserving the followed investigation.
	reporting.consumeThrough(3000);
	refresh_diagnostics_model(state, reporting, monotonic_ns(), true);
	draw();
	click(local(FlowElementID{selector_id.value}, FSEL::RadioChoice::definitionId,
				Keyed("activity", 2)));
	assert(state.groups.empty() && state.selected && state.follow_latest);
	click(local(FlowElementID{selector_id.value}, FSEL::RadioChoice::definitionId,
				Keyed("activity", 0)));
	assert(!state.groups.empty() && state.follow_latest);
	const auto original_refreshes = state.refresh_count;
	click(selector_control(LocalElementName{"refresh-order"}));
	assert(state.refresh_count == original_refreshes + 1 && !state.message_is_error);
	assert(state.message.starts_with("Refreshed live diagnostics"));
	click(selector_control(LocalElementName{"refresh-order"}));
	assert(state.refresh_count == original_refreshes + 2);
	click(header_control(2));
	assert(state.frozen && state.selected);
	const auto held_tick = state.anchor_tick;
	const auto held_selection = state.selected->id;
	click(selector_control(LocalElementName{"refresh-order"}));
	assert(state.refresh_count == original_refreshes + 3 && state.anchor_tick == held_tick);
	assert(state.message.starts_with("Refreshed frozen view") &&
		   state.selected->id == held_selection);
	click(workbench_control(LocalElementName{"deselect"}));
	assert(!state.selected && !state.selected_id && !state.follow_latest && state.frozen);
	click(header_control(2));
	assert(!state.frozen && !state.selected);
	click(header_control(2));
	assert(state.frozen && !state.selected);
	click(header_control(2));
	assert(!state.frozen);
	// Persistent section expansion survives switching the tab's surfaces out of the tree.
	draw_controls = false;
	state.inspector_expanded[4] = true;
	draw();
	assert(state.inspector_expanded[4]);
	std::cout << "Diagnostics selector, report, Inspector, exact clipboard, trigger and freeze "
				 "interactions passed\n";
}
