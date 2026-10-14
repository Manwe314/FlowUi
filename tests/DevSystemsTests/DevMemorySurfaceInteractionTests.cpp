#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/Button.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryCapacityTool.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryContextControls.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryUi.hpp"
#include "devSystems/devInterface/Memory/Inspector/DevMemoryInspector.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbench.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
int main() {
	AppConfig config;
	config.window.title = "Memory complete surface verification";
	config.window.width = 1400;
	config.window.height = 900;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	session.memory_selection.selected_node = uint64_t(DevMemoryNodeId::PersistentPool);
	constexpr auto workbench_id = Global<kDevMemoryWorkbench>("surface.memory.workbench");
	constexpr auto inspector_id = Global<kDevMemoryInspector>("surface.memory.inspector");
	constexpr auto context_id = Global<kDevMemoryContextControls>("surface.memory.context");
	const auto action_id = [&](FlowElementID parent, uint32_t action) {
		return ::FlowUi::detail::element_id::resolveLocal(parent, DevMemoryControl::definitionId,
														  Keyed("memory-action", action).token);
	};
	const auto draw = [&](FlowElementID control = {}, int event = 0, float inspector_width = 360) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.mouseX = input.mouseY = -1;
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseDown[0] = event == 1;
		if (control.value) {
			const auto identity = app.ui().toClayEID(control).id;
			interaction.hoveredElementIds.emplace_back(identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(identity);
		}
		auto root = memory_ui::column();
		root.layout.sizing = {.width = CLAY_SIZING_FIXED(1400), .height = CLAY_SIZING_FIXED(900)};
		root.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(CLAY_ID("memory-surface-test"), root) {
			app.ui()
				.createElement(kDevMemoryContextControls, context_id)
				.setParameters({&app, &session})
				.draw();
			auto body = memory_ui::row();
			body.layout.sizing.height = CLAY_SIZING_GROW(0);
			CLAY(CLAY_ID("memory-surface-body"), body) {
				auto workbench = memory_ui::column();
				workbench.layout.sizing = {.width = CLAY_SIZING_FIXED(850),
										   .height = CLAY_SIZING_GROW(0)};
				CLAY(CLAY_ID("memory-test-workbench"), workbench)
				app.ui()
					.createElement(kDevMemoryWorkbench, workbench_id)
					.setParameters({&app, &session})
					.draw();
				auto inspector = memory_ui::column();
				inspector.layout.sizing = {.width = CLAY_SIZING_FIXED(inspector_width),
										   .height = CLAY_SIZING_GROW(0)};
				inspector.clip = {.horizontal = true, .vertical = true};
				CLAY(CLAY_ID("memory-test-inspector"), inspector)
				app.ui()
					.createElement(kDevMemoryInspector, inspector_id)
					.setParameters({&app, &session})
					.draw();
			}
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto click = [&](FlowElementID identity) {
		// Exercise clipping realistically: bring Inspector controls into its visible viewport.
		for (int attempt = 0; attempt < 2; ++attempt) {
			const auto control = Clay_GetElementData(app.ui().toClayEID(identity));
			const auto inspector =
				Clay_GetElementData(app.ui().toClayEID(FlowElementID{inspector_id.value}));
			const auto scroll =
				Clay_GetScrollContainerData(app.ui().toClayEID(FlowElementID{inspector_id.value}));
			if (control.found && inspector.found && scroll.found && scroll.scrollPosition &&
				control.boundingBox.x >= inspector.boundingBox.x) {
				const auto bottom = inspector.boundingBox.y + inspector.boundingBox.height;
				if (control.boundingBox.y + control.boundingBox.height > bottom)
					scroll.scrollPosition->y -=
						control.boundingBox.y + control.boundingBox.height - bottom + 16;
				else if (control.boundingBox.y < inspector.boundingBox.y)
					scroll.scrollPosition->y +=
						inspector.boundingBox.y - control.boundingBox.y + 16;
			}
			draw();
		}
		draw(identity, 1);
		draw(identity, 2);
		draw();
	};
	const auto local_control = [&](FlowElementID parent, FlowDefinitionID definition,
								   LocalElementName name) {
		return ::FlowUi::detail::element_id::resolveLocal(parent, definition, name.token);
	};
	const auto check_field_text = [&](const auto& element, FlowElementID identity) {
		using Element = std::remove_cvref_t<decltype(element)>;
		const auto field = Clay_GetElementData(app.ui().toClayEID(identity));
		const auto content = Clay_GetElementData(Clay_ElementId{
			.id = FlowIDToClayID(PartID(element, identity, Element::Parts::content))});
		const auto label = Clay_GetElementData(
			Clay_ElementId{.id = FlowIDToClayID(PartID(element, identity, Element::Parts::text))});
		assert(field.found && content.found && label.found);
		assert(label.boundingBox.y >= field.boundingBox.y &&
			   label.boundingBox.y + label.boundingBox.height <=
				   field.boundingBox.y + field.boundingBox.height);
		assert(label.boundingBox.x >= field.boundingBox.x &&
			   label.boundingBox.x + label.boundingBox.width <=
				   field.boundingBox.x + field.boundingBox.width);
		assert(std::abs(label.boundingBox.y + label.boundingBox.height / 2 -
						(content.boundingBox.y + content.boundingBox.height / 2)) < 1);
		assert(std::abs(label.boundingBox.x + label.boundingBox.width / 2 -
						(content.boundingBox.x + content.boundingBox.width / 2)) < 1);
	};
	draw();
	draw();
	auto& analysis = session.memory_workbench.analysis;
	auto& tool = analysis.capacity_tool;
	assert(tool.editor.evidence_mode == 1 && !tool.target_options.empty());
	assert(std::ranges::any_of(tool.proposals, [](const auto& value) { return value.available; }));
	click(action_id(FlowElementID{inspector_id.value}, 402));
	assert(tool.editor.planner.margin_percent == 0);
	check_field_text(memory_ui::kDoubleInput,
					 local_control(FlowElementID{inspector_id.value},
								   decltype(memory_ui::kDoubleInput)::definitionId,
								   LocalElementName{"capacity-headroom"}));
	click(action_id(FlowElementID{inspector_id.value}, 400));
	assert(!analysis.staged.empty() && tool.rules.size() == 1);
	const auto prepared_count = analysis.staged.size();
	const auto staged_factor = analysis.shared_factor;
	const auto exact_prepared = analysis.staged.front().plan.initial;
	click(action_id(FlowElementID{inspector_id.value}, 408));
	assert(tool.rules.front().locked);
	click(action_id(FlowElementID{inspector_id.value}, 400));
	assert(analysis.staged.front().plan.initial == exact_prepared);
	assert(analysis.shared_factor == staged_factor);
	click(action_id(FlowElementID{context_id.value}, 106));
	assert(analysis.show_recording_settings);
	check_field_text(FSEL::kTextInput,
					 local_control(FlowElementID{context_id.value}, FSEL::TextInput::definitionId,
								   LocalElementName{"capture-name"}));
	check_field_text(memory_ui::kDoubleInput,
					 local_control(FlowElementID{context_id.value},
								   decltype(memory_ui::kDoubleInput)::definitionId,
								   LocalElementName{"record-duration"}));
	click(action_id(FlowElementID{context_id.value}, 101));
	assert(analysis.datasets.size() == 1 && analysis.datasets.front().snapshot);
	assert(!analysis.datasets.front().live.sources.empty());
	tool.editor.evidence_mode = 0;
	tool.editor.dataset = analysis.dataset;
	tool.strategy = 1;
	draw();
	draw();
	assert(std::ranges::none_of(tool.proposals, [](const auto& value) {
		return value.available && !value.locked;
	})); // snapshots cannot supply percentiles
	analysis.dataset = 0;
	click(action_id(FlowElementID{context_id.value}, 100));
	assert(analysis.recording);
	const auto recording_tick = app.devMonitoring().memoryReporting().status().lastConsumedAppTick;
	draw();
	draw();
	assert(app.devMonitoring().memoryReporting().status().lastConsumedAppTick > recording_tick);
	click(action_id(FlowElementID{context_id.value}, 100));
	assert(!analysis.recording && analysis.datasets.size() == 2);
	assert(!analysis.datasets.back().segments.empty());
	assert(analysis.dataset == analysis.datasets.back().identity);
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	click(action_id(FlowElementID{context_id.value}, 104));
	assert(analysis.baseline_dataset == analysis.dataset);
	click(action_id(FlowElementID{context_id.value}, 103));
	assert(analysis.dataset == 0 && analysis.datasets.size() == 1 && !analysis.baseline_dataset);
	tool.editor.evidence_mode = 1;
	tool.strategy = 0;
	tool.show_targets = tool.advanced = true;
	draw();
	tool.show_diagnostics = true;
	draw();
	const auto original = analysis.show_quality;
	const auto header = local_control(FlowElementID{inspector_id.value}, FSEL::Button::definitionId,
									  LocalElementName{"measurement-header"});
	click(header);
	assert(analysis.show_quality != original);
	click(header);
	assert(analysis.show_quality == original);
	for (const auto width : {280.0f, 360.0f, 480.0f}) {
		draw({}, 0, width);
		draw({}, 0, width);
		const auto inspector_scroll =
			Clay_GetScrollContainerData(app.ui().toClayEID(FlowElementID{inspector_id.value}));
		assert(inspector_scroll.found && inspector_scroll.contentDimensions.width <=
											 inspector_scroll.scrollContainerDimensions.width + 1);
	}
	const auto scroll =
		Clay_GetScrollContainerData(app.ui().toClayEID(FlowElementID{inspector_id.value}));
	assert(scroll.found &&
		   scroll.contentDimensions.height > scroll.scrollContainerDimensions.height);
	assert(analysis.staged.size() ==
		   prepared_count); // navigation/capture does not discard settings
	std::cout << "Five-stage Inspector, prepared scope lock, independent recording, snapshot "
				 "percentile rejection and narrow layout verified\n";
}
