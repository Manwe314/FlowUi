#include "../demo/DemoElementSchemas.hpp"
#include "FlowUi/Public/Flow.hpp"
#include "devSystems/devAgenticWorkflow/AgenticAction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionRuntime.hpp"
#include "devSystems/devAgenticWorkflow/AgenticService.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInspectSelection.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevInterface.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevInterfaceHeader.hpp"
#include "FlowUi/Private/elementSystem/ElementInvocation.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

namespace dev_notes_fixture {
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace agentic = FlowUi::devSystems::agentic;

/* Intentionally undeclared reachable type used to verify an honest metadata summary. */
struct UnregisteredValue {
	std::array<std::uint32_t, 3> channels{17, 23, 41};
};
struct NestedParameters {
	std::optional<Clay_Color> color = Flow_Color("#18B8A6");
	std::optional<float> radius = 6.0f;
	Clay_Padding padding{8, 12, 16, 20};
};
struct SampleParameters {
	std::string caption =
		"A long application-authored label with enough text to exercise bounded inspectors";
	NestedParameters nested{};
	std::optional<double> winning_value = 10.0;
	std::optional<FontFamilyId> font_family = 0;
	UnregisteredValue unregistered{};
	const void* opaque_pointer = nullptr;
	Clay_Sizing sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(140)};
};
struct SampleState {
	std::uint64_t builds = 0;
	bool enabled = true;
};
struct SampleResources {
	TextureRef icon{};
	SampleResources() = default;
	explicit SampleResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
		interface_icons::registerDevInterfaceIcons(app.icons());
		icon = app.icons().textureRef(interface_icons::kInspectKey);
#endif
	}
};
struct Sample {
	using Parameters = SampleParameters;
	using State = SampleState;
	using Resources = SampleResources;
	using BuildContext = ElementBuildContext<Sample>;
	static constexpr FlowDefinitionID definitionId = DefinitionID("tests.dev-notes.sample");
	static constexpr std::string_view debugName = "Developer notes sample";
	static void buildElement(BuildContext& context) {
		++context.state().builds;
		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing;
		root.layout.padding = context.params.nested.padding;
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = 8;
		root.backgroundColor = context.params.nested.color.value_or(Flow_Color("#182227"));
		root.cornerRadius = CLAY_CORNER_RADIUS(context.params.nested.radius.value_or(0));
		CLAY(context.clayID(), root) {
			Clay_TextElementConfig text{};
			text.fontSize = 14;
			text.textColor = Flow_Color("#FFFFFF");
			text.wrapMode = CLAY_TEXT_WRAP_WORDS;
			CLAY_TEXT(context.uiManager.toClayString(context.params.caption),
					  CLAY_TEXT_CONFIG(text));
			const std::string value =
				"Effective optional value: " + (context.params.winning_value
													? std::to_string(*context.params.winning_value)
													: "Unset");
			CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(text));
			if (context.resources().icon.handle) {
				Clay_ElementDeclaration icon{};
				icon.layout.sizing = {CLAY_SIZING_FIXED(18), CLAY_SIZING_FIXED(18)};
				icon.image.imageData = context.uiManager.imageData(context.resources().icon);
				CLAY(context.clayID("sample-resource"), icon);
			}
		}
	}
};
inline constexpr Sample sample{};
inline constexpr auto sample_identity = Global<sample>("dev-notes.sample");
inline constexpr auto combo_identity = Global<FSEL::kComboBox>("dev-notes.combo");
inline constexpr auto button_identity = Global<FSEL::kButton>("dev-notes.button");

FLOWUI_DEV_SCHEMA(NestedParameters, FLOWUI_DEV_FIELD(NestedParameters, color),
				  FLOWUI_DEV_FIELD(NestedParameters, radius),
				  FLOWUI_DEV_FIELD(NestedParameters, padding))
FLOWUI_DEV_SCHEMA(SampleParameters, FLOWUI_DEV_FIELD(SampleParameters, caption),
				  FLOWUI_DEV_FIELD(SampleParameters, nested),
				  FLOWUI_DEV_FIELD(SampleParameters, winning_value),
				  FLOWUI_DEV_FIELD(SampleParameters, font_family,
								   tooling::DevFieldOptions{}
									   .withEditor(tooling::DevEditorKind::FontChoice)
									   .withChoiceDomain(tooling::DevChoiceDomain::FontFamily)),
				  FLOWUI_DEV_FIELD(SampleParameters, unregistered),
				  FLOWUI_DEV_FIELD(SampleParameters, opaque_pointer),
				  FLOWUI_DEV_FIELD(SampleParameters, sizing))
FLOWUI_DEV_SCHEMA(SampleState, FLOWUI_DEV_FIELD(SampleState, builds),
				  FLOWUI_DEV_FIELD(SampleState, enabled))
FLOWUI_DEV_SCHEMA(SampleResources, FLOWUI_DEV_FIELD(SampleResources, icon))

struct Fixture {
	DevInterfaceState session{};
	std::optional<agentic::Json> pending_arguments{};
	agentic::Json completed_state{};
	std::optional<FlowUiError> configuration_error{};
	SampleParameters parameters{};
	std::array<FSEL::ComboBoxOption, 2> options{};
	TextureRef icon{};
	DevInterfaceHeaderResources header_icons{};
	WindowId interface_window = InvalidWindowId, secondary_window = InvalidWindowId;
	std::uint64_t combo_selection = 1, button_activations = 0;
	std::uint64_t button_definition_font_size = 0;
	std::uint64_t configuration_generation = 0, applied_generation = 0;
	std::uint64_t applied_interface_frame = 0, applied_main_frame = 0;
	std::uint64_t rendered_interface_frame = 0, rendered_main_frame = 0;
	float combo_height = 20, requested_icon_size = 80;
	bool icons_enabled = true;
	bool geometry_stress = false;
	bool saved_header_icons = false;
	std::string theme = "default";
};

/* Test-owned resource substitution exercises the real permanent header's text fallback. */
void set_icons(App& app, Fixture& fixture, bool enabled) {
	fixture.icons_enabled = enabled;
	auto invocation = ::FlowUi::detail::element::ElementInvocation<DevInterfaceHeader>::begin(
		app.elements(), app.ui(fixture.interface_window), fixture.interface_window,
		FlowElementID{DevInterfaceHeader::definitionId.value});
	auto& resources = const_cast<DevInterfaceHeaderResources&>(invocation.resources());
	if (!fixture.saved_header_icons) {
		fixture.header_icons = resources;
		fixture.saved_header_icons = true;
	}
	resources = enabled ? fixture.header_icons : DevInterfaceHeaderResources{};
}

void require_status(Status status) {
	if (!status)
		throw FlowUiException(status.error());
}

void draw_application(App& app, WindowId window, Fixture& fixture) {
	auto& ui = app.ui(window);
	Clay_ElementDeclaration root{};
	root.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
	root.layout.padding = CLAY_PADDING_ALL(20);
	root.layout.childGap = 16;
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = Flow_Color("#071A22");
	root.clip = {.horizontal = fixture.geometry_stress, .vertical = fixture.geometry_stress};
	CLAY(ui.toClaySID("dev-notes.application"), root) {
		if (fixture.geometry_stress) {
			// Keep the render target at the application viewport size. Only the captured
			// sample's solved coordinates are large; its preview dimensions remain bounded.
			Clay_ElementDeclaration stress_position{};
			stress_position.layout.sizing = {CLAY_SIZING_FIXED(640), CLAY_SIZING_FIXED(480)};
			stress_position.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			stress_position.floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
			stress_position.floating.offset = {1.2345678e27f, -9.876543e26f};
			stress_position.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
			CLAY(ui.toClaySID("dev-notes.geometry-stress"), stress_position) {
				ui.createElement(sample, sample_identity).setParameters(fixture.parameters).draw();
			}
		} else {
			ui.createElement(sample, sample_identity).setParameters(fixture.parameters).draw();
		}
		fixture.options = {{{1, "A long selected option whose label yields to both icons",
							 fixture.icons_enabled ? fixture.icon : TextureRef{}},
							{2, "Second option with another long popup label",
							 fixture.icons_enabled ? fixture.icon : TextureRef{}}}};
		FSEL::ComboBoxParameters combo{};
		combo.options = fixture.options;
		combo.selectedValue = &fixture.combo_selection;
		combo.iconSize = fixture.requested_icon_size;
		combo.fontSize = 12;
		combo.optionHeight = fixture.combo_height;
		combo.sizing = Clay_Sizing{CLAY_SIZING_FIXED(240), CLAY_SIZING_FIXED(fixture.combo_height)};
		combo.closedIcon = fixture.icons_enabled ? fixture.icon : TextureRef{};
		combo.openIcon = combo.closedIcon;
		ui.createElement(FSEL::kComboBox, combo_identity).setParameters(combo).draw();
		FSEL::ButtonParameters button{};
		button.text = "Application FSEL button";
		button.contentMode = FSEL::ButtonContentMode::IconThenText;
		button.icon = fixture.icons_enabled ? fixture.icon : TextureRef{};
		button.onActivate = ActionCall{app.actions().uiActions().make(
			UiAction("tests.dev-notes.increment", [](std::uint64_t& count) { ++count; }),
			fixture.button_activations)};
		ui.createElement(FSEL::kButton, button_identity).setParameters(button).draw();
	}
}

[[nodiscard]] agentic::Json snapshot(App& app, const Fixture& fixture) {
	int window_width = 0, window_height = 0;
	glfwGetWindowSize(static_cast<GLFWwindow*>(app.nativeWindowHandle(fixture.interface_window)),
					  &window_width, &window_height);
	const auto& timeline = fixture.session.performance_timeline;
	std::array<std::uint64_t, 4> cpu_level_counts{};
	std::uint64_t cpu_block_count = 0, gpu_block_count = 0;
	for (const auto& block : timeline.snapshot.blocks) {
		if (!block.scope_visible)
			continue;
		if (!block.cpu_sample.empty()) {
			++cpu_block_count;
			const auto level =
				block.type_id == timing_zones::kElementInvoke.typeId ? CpuTimingLevel::Balanced
				: block.descriptor.empty() ? CpuTimingLevel::OnlyFrameTime
										   : block.descriptor.front().minimumCpuLevel;
			++cpu_level_counts[std::min(static_cast<std::size_t>(level),
										cpu_level_counts.size() - 1)];
		}
		if (!block.gpu_sample.empty())
			++gpu_block_count;
	}
	auto projected_cpu_levels = agentic::Json::make_array();
	projected_cpu_levels.array.reserve(cpu_level_counts.size());
	for (const auto count : cpu_level_counts)
		projected_cpu_levels.array.emplace_back(agentic::Json::number(count));
	return agentic::Json::make_object(
		{{"interface_window_id", agentic::decimal(fixture.interface_window)},
		 {"main_window_id", agentic::decimal(app.mainWindowId())},
		 {"secondary_window_id", agentic::decimal(fixture.secondary_window)},
		 {"selected_window_id", agentic::decimal(fixture.session.selectedWindowId)},
		 {"selected_instance_id", agentic::decimal(fixture.session.selectedElementId.value)},
		 {"inspect_forest", agentic::Json::number(fixture.session.inspectForest)},
		 {"window_width", agentic::Json::number(static_cast<std::uint64_t>(window_width))},
		 {"window_height", agentic::Json::number(static_cast<std::uint64_t>(window_height))},
		 {"completed_window_frame",
		  agentic::decimal(app.ui(fixture.interface_window).dev_completed_window_frame())},
		 {"completed_main_frame", agentic::decimal(app.ui().dev_completed_window_frame())},
		 {"inspector_width", agentic::Json(fixture.session.inspectorWidth)},
		 {"geometry_stress", fixture.geometry_stress},
		 {"active_tab", agentic::Json::number(fixture.session.activeTab)},
		 {"inspector_tab", agentic::Json::number(fixture.session.inspectInspectorTab)},
		 {"authored_value", fixture.parameters.winning_value
								? agentic::Json(*fixture.parameters.winning_value)
								: agentic::Json{}},
		 {"optional_present", fixture.parameters.winning_value.has_value()},
		 {"recording_cpu_level", agentic::Json::number(static_cast<std::uint64_t>(
									 app.devMonitoring().timing().config().cpuLevel))},
		 {"compiled_cpu_level", agentic::Json::number(FLOWUI_DEV_TIMING_LEVEL)},
		 {"timeline_snapshot_revision", agentic::decimal(timeline.snapshot_revision)},
		 {"timeline_capture_generation", agentic::decimal(timeline.capture_generation)},
		 {"timeline_view_cpu_level",
		  agentic::Json::number(static_cast<std::uint64_t>(timeline.selection.cpu_detail_level))},
		 {"timeline_recorded_min_cpu_level", agentic::Json::number(static_cast<std::uint64_t>(
												 timeline.snapshot.minimum_recorded_cpu_level))},
		 {"timeline_recorded_max_cpu_level", agentic::Json::number(static_cast<std::uint64_t>(
												 timeline.snapshot.maximum_recorded_cpu_level))},
		 {"timeline_cpu_block_count", agentic::decimal(cpu_block_count)},
		 {"timeline_gpu_block_count", agentic::decimal(gpu_block_count)},
		 {"timeline_cpu_level_counts", std::move(projected_cpu_levels)},
		 {"combo_height", agentic::Json(fixture.combo_height)},
		 {"combo_selection", agentic::decimal(fixture.combo_selection)},
		 {"requested_icon_size", agentic::Json(fixture.requested_icon_size)},
		 {"icons_enabled", fixture.icons_enabled},
		 {"theme", fixture.theme},
		 {"sample_instance_id", agentic::decimal(sample_identity.value)},
		 {"combo_instance_id", agentic::decimal(combo_identity.value)},
		 {"button_instance_id", agentic::decimal(button_identity.value)},
		 {"button_activations", agentic::decimal(fixture.button_activations)},
		 {"button_definition_font_size",
		  agentic::Json::number(fixture.button_definition_font_size)},
		 {"undo_count", agentic::decimal(fixture.session.editUndoStack.size())},
		 {"redo_count", agentic::decimal(fixture.session.editRedoStack.size())}});
}

[[nodiscard]] Status submit_value(App& app, Fixture& fixture, const agentic::Json* value,
								  bool clear) {
	const auto schema = app.devTooling().schemas().view();
	const auto* element = schema ? schema->findElement(Sample::definitionId) : nullptr;
	if (!element)
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	const auto fields = schema->fieldsOf(element->parametersType);
	const auto field = std::ranges::find_if(fields, [&](const auto& candidate) {
		return schema->string(candidate.name) == "winning_value";
	});
	if (field == fields.end())
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	tooling::DevOverrideCommand command{};
	command.kind = clear ? tooling::DevOverrideCommandKind::ClearElementField
						 : tooling::DevOverrideCommandKind::SetElementField;
	command.element.definition = Sample::definitionId;
	command.element.scope = tooling::DevOverrideScope::Definition;
	command.field = {schema->type(element->parametersType)->id, field->id};
	command.layer = tooling::DevOverrideLayer::LiveDefinition;
	command.preserve_baked_assignment = true;
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	const auto previous = std::ranges::find_if(records, [&](const auto& record) {
		return record.target.definition == command.element.definition &&
			   record.target.scope == tooling::DevOverrideScope::Definition &&
			   record.layer == tooling::DevOverrideLayer::LiveDefinition &&
			   record.field == command.field;
	});
	if (clear && previous == records.end())
		return {};
	if (!clear && app.devTooling().overrides().copyValue(std::optional<double>{value->number_or()},
														 command.value) !=
					  tooling::DevValueOperationStatus::Success)
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	tooling::DevOverrideCommand inverse = command;
	if (previous == records.end()) {
		inverse.kind = tooling::DevOverrideCommandKind::ClearElementField;
		inverse.value.reset();
	} else {
		inverse.kind = tooling::DevOverrideCommandKind::SetElementField;
		inverse.value = previous->value;
	}
	DevInterfaceEditTransaction history{};
	history.description =
		clear ? "Clear fixture optional override" : "Edit fixture optional override";
	history.forward.emplace_back(command);
	history.inverse.emplace_back(std::move(inverse));
	tooling::DevChangeSet changes{};
	changes.transaction = fixture.session.nextEditTransaction++;
	changes.commands.emplace_back(std::move(command));
	if (!app.devTooling().overrides().submit(std::move(changes)))
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	fixture.session.editUndoStack.emplace_back(std::move(history));
	fixture.session.editRedoStack.clear();
	return {};
}

[[nodiscard]] Status submit_button_definition(App& app, Fixture& fixture, std::uint64_t font_size) {
	const auto schema = app.devTooling().schemas().view();
	const auto* element = schema ? schema->findElement(FSEL::Button::definitionId) : nullptr;
	if (!element)
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	const auto fields = schema->fieldsOf(element->parametersType);
	const auto field = std::ranges::find_if(fields, [&](const auto& candidate) {
		return schema->string(candidate.name) == "labelFontSize";
	});
	if (field == fields.end())
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	tooling::DevOverrideCommand command{};
	command.kind = font_size == 0 ? tooling::DevOverrideCommandKind::ClearElementField
								  : tooling::DevOverrideCommandKind::SetElementField;
	command.element.definition = FSEL::Button::definitionId;
	command.element.scope = tooling::DevOverrideScope::Definition;
	command.field = {schema->type(element->parametersType)->id, field->id};
	command.layer = tooling::DevOverrideLayer::LiveDefinition;
	command.preserve_baked_assignment = true;
	if (font_size && app.devTooling().overrides().copyValue(
						 std::optional<std::uint16_t>{static_cast<std::uint16_t>(font_size)},
						 command.value) != tooling::DevValueOperationStatus::Success)
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	tooling::DevChangeSet changes{};
	changes.transaction = fixture.session.nextEditTransaction++;
	changes.commands.emplace_back(std::move(command));
	if (!app.devTooling().overrides().submit(std::move(changes)))
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
	fixture.button_definition_font_size = font_size;
	return {};
}

[[nodiscard]] Status configure(App& app, Fixture& fixture, const agentic::Json& arguments) {
	if (const auto* stress = arguments.find("geometry_stress")) {
		fixture.geometry_stress = stress->bool_or();
		fixture.parameters.sizing =
			fixture.geometry_stress
				? Clay_Sizing{CLAY_SIZING_FIXED(345678), CLAY_SIZING_FIXED(456789)}
				: Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(140)};
		fixture.parameters.nested.padding = fixture.geometry_stress
												? Clay_Padding{65535, 65534, 65533, 65532}
												: Clay_Padding{8, 12, 16, 20};
	}
	if (const auto* width = arguments.find("inspector_width"))
		fixture.session.inspectorWidth = static_cast<float>(width->number_or());
	if (const auto* width = arguments.find("window_width"))
		glfwSetWindowSize(
			static_cast<GLFWwindow*>(app.nativeWindowHandle(fixture.interface_window)),
			static_cast<int>(width->number_or()), 820);
	if (const auto* height = arguments.find("combo_height"))
		fixture.combo_height = static_cast<float>(height->number_or());
	if (const auto* size = arguments.find("requested_icon_size"))
		fixture.requested_icon_size = static_cast<float>(size->number_or());
	if (const auto* icons = arguments.find("icons_enabled"))
		set_icons(app, fixture, icons->bool_or());
	if (const auto* value = arguments.find("authored_value"))
		fixture.parameters.winning_value = value->number_or();
	if (const auto* present = arguments.find("optional_present")) {
		if (!present->bool_or())
			fixture.parameters.winning_value.reset();
		else if (!fixture.parameters.winning_value)
			fixture.parameters.winning_value = 10.0;
	}
	if (const auto* theme = arguments.find("theme")) {
		fixture.theme = theme->string_or();
		const auto changed = app.themes().setActiveVariant<FSEL::FSELTheme>(fixture.theme);
		if (!changed)
			return changed;
	}
	if (const auto* tab = arguments.find("active_tab")) {
		constexpr std::array<std::string_view, 6> names{"inspect",	   "performance", "memory",
														"diagnostics", "changes",	  "catalogue"};
		fixture.session.activeTab =
			static_cast<std::uint64_t>(std::ranges::find(names, tab->string_or()) - names.begin());
	}
	if (const auto* tab = arguments.find("inspector_tab")) {
		constexpr std::array<std::string_view, 4> names{"parameters", "state", "resources",
														"changes"};
		fixture.session.inspectInspectorTab =
			static_cast<std::uint64_t>(std::ranges::find(names, tab->string_or()) - names.begin());
	}
	if (const auto* selection = arguments.find("selection")) {
		fixture.session.inspectForest = kDevInterfaceFlowForest;
		fixture.session.inspectDefinitionFilter = 0;
		if (selection->string_or() == "theme") {
			fixture.session.inspectForest = kDevInterfaceThemeForest;
			fixture.session.inspect_theme_type = ::FlowUi::detail::typeHash<FSEL::FSELTheme>();
			fixture.session.inspect_theme_variant = "default";
			fixture.session.inspect_theme_all_variants = false;
			fixture.session.inspect_theme_group = 0;
		} else if (selection->string_or() == "clay") {
#if FLOW_UI_DEV_CAPTURE_CLAY
			fixture.session.inspectForest = kDevInterfaceClayForest;
			const auto& tree = app.ui(fixture.session.selectedWindowId).devTreeSnapshot();
			const auto node = std::ranges::find_if(tree.flow.nodes, [](const auto& candidate) {
				return candidate.definition == Sample::definitionId;
			});
			if (node != tree.flow.nodes.end() && node->clayRoot < tree.clay.nodes.size()) {
				const auto& clay = tree.clay.nodes[node->clayRoot];
				apply_inspect_selection(
					app, fixture.session, kDevInterfaceClayNodeKind,
					stableNodeKey((static_cast<std::uint64_t>(clay.rootIndex) << 32u) |
									  node->clayRoot,
								  clay.clayId),
					false);
			}
#endif
		} else {
			const auto identity = selection->string_or() == "sample"   ? sample_identity.value
								  : selection->string_or() == "combo"  ? combo_identity.value
								  : selection->string_or() == "button" ? button_identity.value
																	   : 0;
			apply_inspect_selection(app, fixture.session, kDevInterfaceFlowNodeKind, identity,
									false);
		}
	}
	if (const auto* font_size = arguments.find("button_definition_font_size")) {
		const auto changed = submit_button_definition(app, fixture, static_cast<std::uint64_t>(font_size->number_or()));
		if (!changed)
			return changed;
	}
	if (const auto* value = arguments.find("live_value"))
		return submit_value(app, fixture, value, false);
	if (const auto* clear = arguments.find("clear_override"); clear && clear->bool_or())
		return submit_value(app, fixture, nullptr, true);
	return {};
}

[[nodiscard]] agentic::Json argument_schema() {
	using Json = agentic::Json;
	return Json::make_object(
		{{"type", "object"},
		 {"additionalProperties", false},
		 {"minProperties", Json::number(1)},
		 {"properties",
		  Json::make_object(
			  {{"inspector_width",
				Json::make_object(
					{{"type", "number"}, {"minimum", Json(240.0)}, {"maximum", Json(560.0)}})},
			   {"window_width",
				Json::make_object(
					{{"type", "number"}, {"minimum", Json(800.0)}, {"maximum", Json(2000.0)}})},
			   {"selection",
				Json::make_object({{"type", "string"},
								   {"enum", Json::make_array({"sample", "combo", "button", "clay",
															  "theme", "none"})}})},
			   {"inspector_tab",
				Json::make_object(
					{{"type", "string"},
					 {"enum", Json::make_array({"parameters", "state", "resources", "changes"})}})},
			   {"active_tab",
				Json::make_object(
					{{"type", "string"},
					 {"enum", Json::make_array({"inspect", "performance", "memory", "diagnostics",
												"changes", "catalogue"})}})},
			   {"authored_value", Json::make_object({{"type", "number"}})},
			   {"optional_present", Json::make_object({{"type", "boolean"}})},
			   {"geometry_stress", Json::make_object({{"type", "boolean"}})},
			   {"live_value", Json::make_object({{"type", "number"}})},
			   {"clear_override", Json::make_object({{"type", "boolean"}})},
			   {"button_definition_font_size",
				Json::make_object({{"type", "integer"},
								   {"minimum", Json::number(0)},
								   {"maximum", Json::number(64)},
								   {"not", Json::make_object({{"minimum", Json::number(1)},
															  {"maximum", Json::number(7)}})}})},
			   {"theme", Json::make_object({{"type", "string"},
											{"enum", Json::make_array({"default", "hostile"})}})},
			   {"combo_height",
				Json::make_object(
					{{"type", "number"},
					 {"enum", Json::make_array({Json(20.0), Json(24.0), Json(28.0)})}})},
			   {"requested_icon_size",
				Json::make_object(
					{{"type", "number"}, {"minimum", Json(0.0)}, {"maximum", Json(128.0)}})},
			   {"icons_enabled", Json::make_object({{"type", "boolean"}})}})}});
}

[[nodiscard]] Result<agentic::Json> decode_arguments(const agentic::Json& arguments) {
	if (arguments.kind != agentic::Json::Kind::object || arguments.object.empty())
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
	const auto schema = argument_schema();
	const auto* properties = schema.find("properties");
	for (const auto& [key, value] : arguments.object) {
		const auto* property = properties->find(key);
		if (!property)
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
		const auto type = property->find("type")->string_or();
		if (((type == "number" || type == "integer") &&
			 (value.kind != agentic::Json::Kind::number || !std::isfinite(value.number_or()))) ||
			(type == "string" && value.kind != agentic::Json::Kind::string) ||
			(type == "boolean" && value.kind != agentic::Json::Kind::boolean))
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
		if ((type == "integer" && std::floor(value.number_or()) != value.number_or()) ||
			(key == "button_definition_font_size" && value.number_or() > 0 &&
			 value.number_or() < 8))
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
		if (const auto* minimum = property->find("minimum");
			minimum && value.number_or() < minimum->number_or())
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
		if (const auto* maximum = property->find("maximum");
			maximum && value.number_or() > maximum->number_or())
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
		if (const auto* choices = property->find("enum");
			choices && !std::ranges::any_of(choices->array, [&](const auto& choice) {
				return type == "string" ? value.string_or() == choice.string_or()
										: value.number_or() == choice.number_or();
			}))
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
	}
	return arguments;
}

[[nodiscard]] Status register_action(App& app, Fixture& fixture) {
	auto runtime = app.agentic_workflow().interaction_runtime();
	if (!runtime)
		return unexpectedError(runtime.error());
	agentic::AgenticActionDeclaration declaration;
	declaration.action_id = "dev_notes.configure";
	declaration.description = "Configure the real developer notes fixture, application values and "
							  "window sizes; acknowledge three subsequent completed frames.";
	declaration.argument_schema = argument_schema();
	declaration.result_schema = agentic::Json::make_object({{"type", "object"}});
	declaration.asynchronous = true;
	agentic::AgenticActionAdapter adapter;
	adapter.decode_arguments = decode_arguments;
	adapter.available = [&fixture](const App& application) -> Result<bool> {
		return application.hasWindow(fixture.interface_window);
	};
	adapter.invoke =
		[&fixture](App&, const agentic::AgenticActionInvocation&,
				   const agentic::Json& arguments) -> Result<agentic::AgenticActionReceipt> {
		// The runtime dispatches under its frame admission leases. Native window
		// operations and captured-tree reads belong to the outer loop between frames.
		fixture.pending_arguments = arguments;
		fixture.configuration_error.reset();
		++fixture.configuration_generation;
		agentic::AgenticActionReceipt receipt;
		receipt.completion_token = agentic::Json::make_object(
			{{"configuration_generation", agentic::decimal(fixture.configuration_generation)}});
		return receipt;
	};
	adapter.poll_completion = [&fixture](App&, const agentic::AgenticActionReceipt& previous)
		-> Result<agentic::AgenticActionReceipt> {
		auto receipt = previous;
		const auto requested_generation =
			previous.completion_token.find("configuration_generation")->uint64_or();
		if (fixture.applied_generation < requested_generation)
			return receipt;
		if (fixture.configuration_error) {
			receipt.completed = true;
			receipt.reason = "fixture_configuration_failed";
			return receipt;
		}
		if (fixture.rendered_interface_frame < fixture.applied_interface_frame + 3 ||
			fixture.rendered_main_frame < fixture.applied_main_frame + 3)
			return receipt;
		receipt.result = fixture.completed_state;
		receipt.completed = true;
		receipt.completion_succeeded = true;
		return receipt;
	};
	return (*runtime)->actions().register_action(std::move(declaration), std::move(adapter));
}

void render(App& app, Fixture& fixture) {
	if (fixture.pending_arguments) {
		const auto arguments = std::move(*fixture.pending_arguments);
		fixture.pending_arguments.reset();
		const auto changed = configure(app, fixture, arguments);
		if (!changed)
			fixture.configuration_error = changed.error();
		fixture.applied_generation = fixture.configuration_generation;
		fixture.applied_interface_frame =
			app.ui(fixture.interface_window).dev_completed_window_frame();
		fixture.applied_main_frame = app.ui().dev_completed_window_frame();
	}
	require_status(app.beginFrame());
	draw_application(app, app.mainWindowId(), fixture);
	require_status(app.endFrame());
	require_status(app.drawFrame());
	fixture.rendered_interface_frame =
		app.ui(fixture.interface_window).dev_completed_window_frame();
	fixture.rendered_main_frame = app.ui().dev_completed_window_frame();
	fixture.completed_state = snapshot(app, fixture);
}

[[nodiscard]] bool contained(const Clay_BoundingBox& inner,
							 const Clay_BoundingBox& outer) noexcept {
	return inner.x >= outer.x - 0.1f && inner.y >= outer.y - 0.1f &&
		   inner.x + inner.width <= outer.x + outer.width + 0.1f &&
		   inner.y + inner.height <= outer.y + outer.height + 0.1f;
}

[[nodiscard]] bool verify_combo_geometry(App& app) {
	const auto& tree = app.ui().devTreeSnapshot();
	const auto root = std::ranges::find_if(tree.clay.nodes, [&](const auto& node) {
		return node.clayId == app.ui().toClayEID(FlowElementID{combo_identity.value}).id;
	});
	if (root == tree.clay.nodes.end())
		return false;
	for (const auto& node : tree.clay.nodes) {
		const auto name = tree.string(node.idString);
		if ((name.ends_with("selected-icon") || name.ends_with("disclosure-icon")) &&
			!contained(node.bounds, root->bounds)) {
			std::cerr << "Combo icon outside trigger: " << name << " icon=" << node.bounds.x << ','
					  << node.bounds.y << ',' << node.bounds.width << ',' << node.bounds.height
					  << " trigger=" << root->bounds.x << ',' << root->bounds.y << ','
					  << root->bounds.width << ',' << root->bounds.height << '\n';
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool self_test(App& app, Fixture& fixture) {
	for (unsigned frame = 0; frame < 4; ++frame)
		render(app, fixture);
	for (const float height : {20.0f, 24.0f, 28.0f}) {
		for (const bool icons : {false, true}) {
			fixture.combo_height = height;
			set_icons(app, fixture, icons);
			for (unsigned frame = 0; frame < 3; ++frame)
				render(app, fixture);
			if (!verify_combo_geometry(app))
				return false;
		}
	}
	for (const float width : {240.0f, 320.0f, 560.0f}) {
		const auto request = agentic::Json::make_object(
			{{"inspector_width", agentic::Json(width)}, {"selection", "sample"}});
		require_status(configure(app, fixture, request));
		for (unsigned frame = 0; frame < 3; ++frame)
			render(app, fixture);
		if (fixture.session.inspectorWidth != width)
			return false;
	}
	require_status(configure(app, fixture,
							 agentic::Json::make_object({{"authored_value", agentic::Json(11.0)},
														 {"live_value", agentic::Json(30.0)},
														 {"theme", "hostile"}})));
	for (unsigned frame = 0; frame < 3; ++frame)
		render(app, fixture);
	if (*fixture.parameters.winning_value != 11.0 || fixture.theme != "hostile")
		return false;
	fixture.pending_arguments = agentic::Json::make_object({{"window_width", agentic::Json(1100.0)},
															{"optional_present", false},
															{"clear_override", true}});
	++fixture.configuration_generation;
	for (unsigned frame = 0; frame < 3; ++frame)
		render(app, fixture);
	if (fixture.configuration_error ||
		fixture.applied_generation != fixture.configuration_generation ||
		fixture.parameters.winning_value ||
		fixture.completed_state.find("window_width")->uint64_or() != 1100)
		return false;
	set_inspect_window(app, fixture.session, fixture.secondary_window);
	if (fixture.session.selectedElementId ||
		app.devTooling().inspect_interaction().primary_target())
		return false;
	require_status(app.destroyWindow(fixture.secondary_window));
	fixture.secondary_window = InvalidWindowId;
	synchronize_inspect_window(app, fixture.session, fixture.interface_window);
	return fixture.session.selectedWindowId == app.mainWindowId();
}
} // namespace dev_notes_fixture

int main(int argument_count, char** arguments) {
	using namespace dev_notes_fixture;
	try {
		Fixture fixture;
		bool native_test = false;
		std::filesystem::path store;
		for (int argument_index = 1; argument_index < argument_count; ++argument_index) {
			const std::string_view argument(arguments[argument_index]);
			if (argument == "--self-test")
				native_test = true;
			else if (argument == "--agentic-store" && argument_index + 1 < argument_count)
				store = arguments[++argument_index];
			else {
				std::cerr
					<< "Usage: DevInterfaceNotesFixture [--agentic-store PATH] [--self-test]\n";
				return 2;
			}
		}
		AppConfig config;
		config.window.title = "Developer Notes Application A";
		config.window.width = 680;
		config.window.height = 520;
		config.vk.enableValidation = false;
		config.dev.enabled = true;
		config.dev.excludeInternalDevElementsFromCapture = false;
		config.dev.monitoring.timing.cpuLevel = CpuTimingLevel::Deep;
		config.agentic_workflow.application_id = "flowui.dev-notes.fixture";
		config.agentic_workflow.application_revision = "1";
		config.agentic_workflow.enabled = !native_test;
		config.agentic_workflow.local_attachment_enabled = !native_test;
		config.agentic_workflow.visual_capture_enabled = true;
		config.agentic_workflow.input_control_enabled = !native_test;
		config.agentic_workflow.action_control_enabled = !native_test;
		config.agentic_workflow.store_path = store;
		config.agentic_workflow.limits.maximum_images = 64;
		App app = makeApplication(config);
		if (!store.empty())
			app.devTooling().bakePipeline().setManifestPath(store / "fixture.flowchanges");
		require_status(
			app.themes().registerTheme<FSEL::FSELTheme>("default", FSEL::FSELTheme{}, true));
		auto hostile = FSEL::FSELTheme{};
		hostile.buttonTheme.idle.backgroundColor = Flow_Color("#E05050");
		hostile.buttonTheme.padding = CLAY_PADDING_ALL(28);
		hostile.buttonTheme.labelFontSize = 25;
		hostile.comboBoxTheme.iconSize = 80;
		hostile.comboBoxTheme.triggerPadding = CLAY_PADDING_ALL(18);
		require_status(app.themes().registerTheme<FSEL::FSELTheme>("hostile", hostile, false));
#if FLOWUI_INCLUDE_ICON_MANAGER
		interface_icons::registerDevInterfaceIcons(app.icons());
		fixture.icon = app.icons().textureRef(interface_icons::kInspectKey);
#endif
		const auto secondary = app.createWindow(
			WindowConfigOverrides{
				.width = 680, .height = 520, .title = "Developer Notes Application B"},
			[&fixture, &app](UiManager&, WindowId window) {
				draw_application(app, window, fixture);
			});
		if (!secondary)
			throw FlowUiException(secondary.error());
		fixture.secondary_window = *secondary;
		const auto developer = app.createWindow(
			WindowConfigOverrides{
				.width = 1280, .height = 820, .title = "Developer Notes Interface"},
			kDevInterface,
			std::function<void(ElementBuilder<interface_elements::DevInterface>&, WindowId)>{
				[&fixture, &app](auto& builder, WindowId window) {
					builder.setParameters(
						DevInterfaceParameters{&app, &fixture.session, window, app.mainWindowId()});
				}});
		if (!developer)
			throw FlowUiException(developer.error());
		fixture.interface_window = *developer;
		fixture.session.selectedWindowId = app.mainWindowId();
		fixture.session.validated_window_id = app.mainWindowId();
		fixture.session.interface_window_id = fixture.interface_window;
		if (native_test) {
			const bool passed = self_test(app, fixture);
			std::cout << "Developer notes native controls: " << (passed ? "PASS" : "FAIL") << '\n';
			return passed ? 0 : 1;
		}
		require_status(register_action(app, fixture));
		while (!app.shouldClose()) {
			render(app, fixture);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		return 0;
	} catch (const std::exception& exception) {
		std::cerr << "Developer notes fixture: " << exception.what() << '\n';
		return 1;
	}
}
