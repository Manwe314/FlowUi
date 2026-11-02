#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/TextInput.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Changes/DevChangesControls.hpp"
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "devSystems/devInterface/Changes/Inspector/DevChangesInspector.hpp"
#include "devSystems/devInterface/Changes/Selector/DevChangesSelector.hpp"
#include "devSystems/devInterface/Changes/Workbench/DevChangesWorkbench.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::tooling;
using namespace FlowUi::devSystems::interface_elements;
namespace {
constexpr auto selector_id = Global<kDevChangesSelector>("changes.test.selector");
constexpr auto workbench_id = Global<kDevChangesWorkbench>("changes.test.workbench");
constexpr auto inspector_id = Global<kDevChangesInspector>("changes.test.inspector");
constexpr auto controls_id = Global<kDevChangesControls>("changes.test.controls");
template <class Identity>
[[nodiscard]] FlowElementID local(FlowElementID parent, FlowDefinitionID definition,
								  const Identity& identity) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(parent, definition, identity.token);
}
void check_bounds(App& app, FlowElementID surface) {
	const auto& snapshot = app.ui().devTreeSnapshot();
	const auto panel = Clay_GetElementData(app.ui().toClayEID(surface));
	assert(panel.found && panel.boundingBox.width > 0);
	for (const auto& node : snapshot.clay.nodes) {
		auto owner = node.directFlowOwner;
		bool descendant = false;
		while (owner != InvalidFlowNode && owner < snapshot.flow.nodes.size()) {
			if (snapshot.flow.nodes[owner].instance.value == surface.value) {
				descendant = true;
				break;
			}
			owner = snapshot.flow.nodes[owner].parent;
		}
		if (!descendant || hasFlag(node.flags, DevClayNodeFlag::BoundsUnavailable))
			continue;
		if (node.bounds.x < panel.boundingBox.x - 1 ||
			node.bounds.x + node.bounds.width > panel.boundingBox.x + panel.boundingBox.width + 1) {
			std::cerr << "Changes horizontal overflow: " << snapshot.string(node.idString)
					  << " text=" << snapshot.string(node.text) << " x=" << node.bounds.x
					  << " width=" << node.bounds.width << " panel=" << panel.boundingBox.x << ','
					  << panel.boundingBox.width << '\n';
			assert(false && "Changes content fits the resizable panel");
		}
	}
}
void check_assigned_width(App& app, FlowElementID surface, float assigned_width) {
	const auto panel = Clay_GetElementData(app.ui().toClayEID(surface));
	assert(panel.found);
	if (std::abs(panel.boundingBox.width - assigned_width) > 1)
		std::cerr << "Changes panel assigned width=" << assigned_width
				  << " actual root width=" << panel.boundingBox.width << '\n';
	assert(std::abs(panel.boundingBox.width - assigned_width) <= 1);
}
void check_centered_search(App& app, bool placeholder = false) {
	const auto search = local(FlowElementID{selector_id.value}, DevSelectorSearch::definitionId,
							  LocalElementName{"search"});
	const auto input =
		local(search, FSEL::TextInput::definitionId, LocalElementName{"search-input"});
	const auto content = Clay_GetElementData(Clay_ElementId{
		.id = FlowIDToClayID(PartID(FSEL::kTextInput, input, FSEL::TextInput::Parts::content))});
	const auto text = Clay_GetElementData(
		Clay_ElementId{.id = FlowIDToClayID(PartID(FSEL::kTextInput, input,
												   placeholder ? FSEL::TextInput::Parts::placeholder
															   : FSEL::TextInput::Parts::text))});
	assert(content.found && text.found && text.boundingBox.width > 0);
	assert(std::abs(content.boundingBox.x + content.boundingBox.width / 2 - text.boundingBox.x -
					text.boundingBox.width / 2) < 2);
	assert(std::abs(content.boundingBox.y + content.boundingBox.height / 2 - text.boundingBox.y -
					text.boundingBox.height / 2) < 2);
}
void check_centered_policy(App& app) {
	static constexpr FSEL::NumberInput<double, DefinitionID("flowui.dev.changes.policy.factor")>
		growth_input{};
	const auto identity =
		local(FlowElementID{inspector_id.value}, decltype(growth_input)::definitionId,
			  LocalElementName{"policy-growth-input"});
	const auto content = Clay_GetElementData(
		Clay_ElementId{.id = FlowIDToClayID(PartID(growth_input, identity,
												   decltype(growth_input)::Parts::content))});
	const auto text = Clay_GetElementData(Clay_ElementId{
		.id = FlowIDToClayID(PartID(growth_input, identity, decltype(growth_input)::Parts::text))});
	assert(content.found && text.found && text.boundingBox.width > 0);
	assert(std::abs(content.boundingBox.x + content.boundingBox.width / 2 - text.boundingBox.x -
					text.boundingBox.width / 2) < 2);
	assert(std::abs(content.boundingBox.y + content.boundingBox.height / 2 - text.boundingBox.y -
					text.boundingBox.height / 2) < 2);
}
void seed_review(DevChangesState& state) {
	state.initialized = true;
	state.review.valid_disk = true;
	state.review.manifest_path =
		".flowui/changes/a_manifest_with_a_long_application_name.flowchanges";
	state.review.rows.reserve(80);
	for (uint64_t index = 0; index < 80; ++index) {
		DevChangesReviewRow row;
		row.key = "element:definition:1234567890:field:" + std::to_string(index);
		row.target_name = "Application toolbar";
		row.field_path = "layout.padding.field_" + std::to_string(index);
		row.on_disk = "12";
		row.proposal = row.next_value = "24";
		row.origin = "Prior manifest and live session";
		row.has_disk = row.has_proposal = true;
		row.operation = DevChangesOperation::Update;
		row.reason = "Ready: reflected identity, writable field and initializer validated.";
		DevBakeEntry entry;
		entry.definition = DefinitionID("changes.application.fixture");
		entry.fieldId = index + 1;
		entry.fieldPath = row.field_path;
		entry.ownerCppType =
			"application::components::ToolbarParametersWithLongButValidSourceIdentity";
		entry.sourceHeader =
			"project/components/toolbar/AnIntentionallyLongSourceHeaderWithoutWhitespace.hpp";
		entry.cppValue = "Clay_Padding{.left=24,.right=24,.top=24,.bottom=24}";
		entry.valueJson = "24";
		entry.valueTypeName = "uint16_t";
		entry.provenance.note =
			"Retained source metadata must remain readable at the narrowest inspector width.";
		entry.provenance.timestamp = "2026-11-02T12:00:00Z";
		if (index >= 30 && index < 55) {
			row.domain = DevChangesDomain::Themes;
			entry.targetKind = DevBakeTargetKind::Theme;
			entry.themeType = 3456;
			entry.themeVariant = index % 2 ? "dark" : "*";
			row.key = "theme:" + std::to_string(index);
			row.target_name = "FSEL theme";
		} else if (index >= 55) {
			row.domain = DevChangesDomain::Memory;
			row.field_path = "flowui.tuning.input_fields.registry_entries_" + std::to_string(index);
			row.key = changes_memory_key(row.field_path);
			FlowUi::detail::MemoryCapacitySelection selection;
			selection.key = row.field_path;
			selection.value = 128;
			selection.observed_maximum = 112;
			selection.headroom = 16;
			selection.evidence.source_name =
				"Measured workload: navigation and large document editing";
			selection.evidence.begin_ns = 1000;
			selection.evidence.end_ns = 5000;
			selection.evidence.begin_tick = 1;
			selection.evidence.end_tick_exclusive = 300;
			row.disk_memory = row.proposed_memory = selection;
		}
		if (row.domain != DevChangesDomain::Memory)
			row.disk_entry = row.proposed_entry = entry;
		state.review.rows.emplace_back(std::move(row));
	}
	state.review.counts.updates = 80;
	state.review.counts.live_edits = 55;
	state.review.counts.prepared_memory = 25;
	state.review.counts.domains = {30, 25, 25};
	state.review.candidate_json = "{\n  \"manifestVersion\": 2,\n  \"bakedChanges\": [\n";
	for (uint64_t index = 0; index < 80; ++index)
		state.review.candidate_json += "    {\"fieldPath\":\"layout.padding.field_" +
									   std::to_string(index) + "\",\"value\":24},\n";
	state.review.candidate_json += "  ]\n}";
	rebuild_changes_model(state);
}
void verify_safe_point_service(App& app) {
	const auto directory =
		std::filesystem::temp_directory_path() /
		("flowui-changes-interface-" +
		 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);
	const auto manifest = directory / "active.flowchanges";
	app.devTooling().bakePipeline().setManifestPath(manifest);
	DevInterfaceState session;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Changes);
	auto& state = session.changes;
	auto& analysis = session.memory_workbench.analysis;
	analysis.staged.reserve(2);
	for (const auto key : {"flowui.tuning.input_fields.registry_entries",
						   "flowui.tuning.input_fields.rect_override_entries"}) {
		MemorySettingDraft draft;
		draft.target.id = ::FlowUi::memory_capacity_target_id(key);
		draft.target.configKey = key;
		draft.plan.initial = 128;
		draft.plan.observed_maximum = 112;
		draft.selection_policy = "manual";
		draft.recipe.initial_mode = 3;
		draft.recipe.manual_initial = 128;
		draft.export_evidence.kind = "maximum";
		draft.export_evidence.complete = true;
		draft.evidence.emplace_back(MemoryDemandObservation{112, 1, 1, 7, 0});
		analysis.staged.emplace_back(std::move(draft));
	}
	analysis.staged.back().synthetic = true;
	analysis.staged_revision = 7;
	service_changes(app, session, true);
	assert(state.review.valid_disk && !state.review.can_commit && state.review.counts.errors > 0);
	state.selected_key = changes_memory_key(analysis.staged.back().target.configKey);
	assert(selected_changes_row(state) && selected_changes_row(state)->blocking);
	queue_changes_decision(state, DevChangesDecision::OmitProposal);
	service_changes(app, session, true);
	assert(state.review.can_commit && state.review.counts.omitted == 1);
	state.pending = DevChangesCommand::Undo;
	service_changes(app, session, true);
	assert(!state.review.can_commit && state.review.counts.errors > 0);
	state.pending = DevChangesCommand::Redo;
	service_changes(app, session, true);
	assert(state.review.can_commit);
	state.search = "nothing-visible";
	state.domain = 1;
	rebuild_changes_model(state);
	assert(state.visible_nodes.empty());
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(std::filesystem::exists(manifest));
	assert(!state.message_is_error &&
		   state.message.starts_with("Manifest updated; rebuild required"));
	assert(state.review.counts.pending() == 0 && state.review.rebuild_required);
	assert(analysis.staged.size() == 2 && session.unbakedChangeCount == 0);
	std::string valid_manifest;
	{
		std::ifstream input(manifest, std::ios::binary);
		valid_manifest.assign(std::istreambuf_iterator<char>(input), {});
	}
	const auto retained_row_count = state.review.rows.size();
	{
		std::ofstream output(manifest, std::ios::trunc);
		output << '{';
	}
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(!state.review.valid_disk && !state.review.can_commit && state.message_is_error);
	assert(state.review.rows.size() == retained_row_count &&
		   !state.review.disk_manifest.memoryCapacity->targets.empty());
	{
		std::ofstream output(manifest, std::ios::trunc);
		output << valid_manifest;
	}
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(state.review.valid_disk && state.review.counts.pending() == 0);
	analysis.staged.front().plan.initial = 256;
	++analysis.staged_revision;
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(state.review.can_commit);
	{
		std::ofstream output(manifest, std::ios::app);
		output << '\n';
	}
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(state.message_is_error && state.stale);
	assert(analysis.staged.front().plan.initial == 256);
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(state.review.can_commit);
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(!state.message_is_error && state.review.counts.pending() == 0);
	state.selected_key = changes_memory_key(analysis.staged.front().target.configKey);
	queue_changes_decision(state, DevChangesDecision::Remove);
	service_changes(app, session, true);
	assert(state.review.can_commit && state.review.counts.removals == 1);
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(!state.message_is_error && state.review.counts.pending() == 0);
	assert(analysis.staged.front().plan.initial == 256);
	std::filesystem::remove_all(directory);
}
} // namespace

int main() {
	AppConfig config;
	config.window.title = "Changes complete surface verification";
	config.window.width = 1400;
	config.window.height = 900;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Changes);
	auto& state = session.changes;
	seed_review(state);
	float selector_width = 280, inspector_width = 360;
	const auto draw = [&](FlowElementID control = {}, int event = 0, int key = -1) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
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
		auto root = changes_ui::column(0);
		root.layout.sizing = {.width = CLAY_SIZING_FIXED(1400), .height = CLAY_SIZING_FIXED(900)};
		root.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(CLAY_ID("changes-test"), root) {
			app.ui()
				.createElement(kDevChangesControls, controls_id)
				.setParameters({&app, &session})
				.draw();
			auto body = changes_ui::row();
			body.layout.childGap = 0;
			body.layout.sizing.height = CLAY_SIZING_GROW(0);
			CLAY(CLAY_ID("changes-test-columns"), body) {
				auto panel = changes_ui::column(0);
				panel.layout.sizing = {.width = CLAY_SIZING_FIXED(selector_width),
									   .height = CLAY_SIZING_GROW(0)};
				CLAY(CLAY_ID("changes-test-selector"), panel)
				app.ui()
					.createElement(kDevChangesSelector, selector_id)
					.setParameters({&app, &session})
					.draw();
				panel.layout.sizing.width =
					CLAY_SIZING_FIXED(1400 - selector_width - inspector_width);
				CLAY(CLAY_ID("changes-test-workbench"), panel)
				app.ui()
					.createElement(kDevChangesWorkbench, workbench_id)
					.setParameters({&app, &session})
					.draw();
				panel.layout.sizing.width = CLAY_SIZING_FIXED(inspector_width);
				CLAY(CLAY_ID("changes-test-inspector"), panel)
				app.ui()
					.createElement(kDevChangesInspector, inspector_id)
					.setParameters({&app, &session})
					.draw();
			}
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto preview = [&](std::string name) {
		if (std::getenv("FLOWUI_CHANGES_PREVIEW")) {
			std::cout << "Changes preview=" << name << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(700));
		}
	};
	const auto viewport = [&](FlowElementID parent, FlowDefinitionID definition,
							  LocalElementName name) {
		return app.ui().toClayEID(local(parent, definition, name));
	};
	const auto selector_scroll =
		viewport(FlowElementID{selector_id.value}, DevChangesSelector::definitionId, "scroll");
	const auto workbench_scroll =
		viewport(FlowElementID{workbench_id.value}, DevChangesWorkbench::definitionId, "scroll");
	const auto inspector_scroll = viewport(FlowElementID{inspector_id.value},
										   DevChangesInspector::definitionId, "scroll-body");
	const auto wheel = [&](Clay_ElementId identity, float delta) {
		const auto bounds = Clay_GetElementData(identity);
		const auto before = Clay_GetScrollContainerData(identity);
		assert(bounds.found && before.found && before.scrollPosition);
		assert(before.contentDimensions.height > before.scrollContainerDimensions.height + 1);
		auto* window = static_cast<GLFWwindow*>(app.nativeWindowHandle());
		glfwFocusWindow(window);
		glfwSetCursorPos(window, bounds.boundingBox.x + bounds.boundingBox.width / 2,
						 bounds.boundingBox.y + bounds.boundingBox.height / 2);
		draw();
		draw();
		const auto old_position = Clay_GetScrollContainerData(identity).scrollPosition->y;
		const auto callback = glfwSetScrollCallback(window, nullptr);
		assert(callback);
		glfwSetScrollCallback(window, callback);
		callback(window, 0, delta);
		draw();
		const auto after = Clay_GetScrollContainerData(identity);
		if (delta < 0)
			assert(after.scrollPosition->y < old_position - 1);
		else
			assert(after.scrollPosition->y > old_position + 1);
	};
	draw();
	draw();
	check_centered_search(app, true);
	check_centered_policy(app);
	static constexpr FSEL::NumberInput<double, DefinitionID("flowui.dev.changes.policy.factor")>
		growth_input{};
	const auto growth_identity =
		local(FlowElementID{inspector_id.value}, decltype(growth_input)::definitionId,
			  LocalElementName{"policy-growth-input"});
	app.ui().inputFields().requestCaret(growth_identity, CaretRequestKind::SetPrimary);
	assert(app.ui().inputFields().replaceText(growth_identity, "-", true));
	draw();
	draw();
	const auto* invalid_growth = app.ui().elements().getStatePointerConst(
		growth_input, app.ui().windowId(), growth_identity);
	assert(invalid_growth && invalid_growth->textStatus != FSEL::NumericTextStatus::Complete);
	const auto stage_factor =
		local(FlowElementID{inspector_id.value}, diagnostics_ui::Control::definitionId,
			  LocalElementName{"stage-growth"});
	draw(stage_factor, 1);
	draw(stage_factor, 2);
	assert(state.pending == DevChangesCommand::None);
	assert(app.ui().inputFields().replaceText(growth_identity, "1.5", true));
	app.ui().inputFields().requestCaret(growth_identity, CaretRequestKind::ClearAll);
	draw();
	draw();
	preview("overview");
	const auto review_trigger =
		local(FlowElementID{controls_id.value}, diagnostics_ui::Control::definitionId,
			  LocalElementName{"review-trigger"});
	draw(review_trigger, 1);
	draw(review_trigger, 2);
	draw();
	draw();
	assert(state.review_open);
	const auto review_popup = Clay_GetElementData(viewport(
		FlowElementID{controls_id.value}, DevChangesControls::definitionId, "review-popover"));
	assert(review_popup.found && review_popup.boundingBox.width > 100);
	preview("quick-review");
	const auto review_scroll = viewport(FlowElementID{controls_id.value},
										DevChangesControls::definitionId, "review-scroll");
	wheel(review_scroll, -4);
	wheel(review_scroll, 4);
	const auto close_review =
		local(FlowElementID{controls_id.value}, diagnostics_ui::Control::definitionId,
			  LocalElementName{"close-review"});
	draw(close_review, 1);
	draw(close_review, 2);
	draw();
	draw();
	assert(!state.review_open);
	preview("review-closed");
	state.keyboard_active = true;
	state.focused_identity = state.nodes[state.visible_nodes.front()].identity;
	const auto next_identity = state.nodes[state.visible_nodes[1]].identity;
	draw({}, 0, GLFW_KEY_DOWN);
	draw();
	assert(state.focused_identity == next_identity);
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	assert(selected_changes_node(state) && selected_changes_node(state)->identity == next_identity);
	assert(!state.review_open);
	state.selected_key.clear();
	const auto memory_radio = local(FlowElementID{selector_id.value},
									FSEL::RadioChoice::definitionId, Keyed("domain", 3));
	draw(memory_radio, 1);
	draw(memory_radio, 2);
	draw();
	assert(state.domain == 3 && state.review.counts.pending() == 80);
	state.domain = 0;
	rebuild_changes_model(state);
	const auto leaf_node =
		std::ranges::find(state.nodes, state.review.rows.front().key, &DevChangesNode::key);
	assert(leaf_node != state.nodes.end());
	const auto leaf_control =
		local(FlowElementID{selector_id.value}, diagnostics_ui::Control::definitionId,
			  Keyed("node", leaf_node->identity));
	draw();
	draw(leaf_control, 1);
	draw(leaf_control, 2);
	draw();
	assert(selected_changes_row(state) == &state.review.rows.front());
	state.selected_key.clear();
	for (const auto width : {280.0f, 220.0f}) {
		selector_width = width;
		state.search = "layout";
		draw();
		draw();
		check_centered_search(app);
		check_bounds(app, FlowElementID{selector_id.value});
		check_bounds(app, FlowElementID{workbench_id.value});
		check_assigned_width(app, FlowElementID{workbench_id.value},
							 1400 - selector_width - inspector_width);
		const auto selector_bounds =
			Clay_GetElementData(app.ui().toClayEID(FlowElementID{selector_id.value}));
		if (std::abs(selector_bounds.boundingBox.width - selector_width) > 1)
			std::cerr << "Changes selector assigned width=" << selector_width
					  << " actual root width=" << selector_bounds.boundingBox.width << '\n';
		assert(std::abs(selector_bounds.boundingBox.width - selector_width) <= 1);
	}
	state.search.clear();
	selector_width = 280;
	draw();
	draw();
	wheel(selector_scroll, -4);
	const auto selector_position = Clay_GetScrollContainerData(selector_scroll).scrollPosition->y;
	wheel(workbench_scroll, -4);
	assert(Clay_GetScrollContainerData(selector_scroll).scrollPosition->y == selector_position);
	wheel(workbench_scroll, 4);
	wheel(selector_scroll, 4);
	for (const auto width : {280.0f, 360.0f, 450.0f}) {
		inspector_width = width;
		draw();
		draw();
		check_assigned_width(app, FlowElementID{inspector_id.value}, width);
		check_bounds(app, FlowElementID{inspector_id.value});
		check_centered_policy(app);
		preview("policy-" + std::to_string(static_cast<int>(width)));
	}
	state.selected_key = state.review.rows.front().key;
	state.inspector_expanded.fill(true);
	rebuild_changes_model(state);
	for (const auto width : {280.0f, 360.0f, 450.0f}) {
		inspector_width = width;
		draw();
		draw();
		check_assigned_width(app, FlowElementID{inspector_id.value}, width);
		check_bounds(app, FlowElementID{inspector_id.value});
		check_bounds(app, FlowElementID{workbench_id.value});
		check_assigned_width(app, FlowElementID{workbench_id.value},
							 1400 - selector_width - inspector_width);
		preview("inspector-" + std::to_string(static_cast<int>(width)));
	}
	const auto copy_identity = local(
		FlowElementID{inspector_id.value}, diagnostics_ui::Control::definitionId,
		Keyed("copy-detail", ::FlowUi::detail::identity_hash::hashBytes("Canonical identity")));
	const auto copy_bounds = Clay_GetElementData(app.ui().toClayEID(copy_identity));
	const auto inspector_viewport = Clay_GetElementData(inspector_scroll);
	assert(copy_bounds.found && inspector_viewport.found);
	Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y -=
		std::max(0.0f, copy_bounds.boundingBox.y - inspector_viewport.boundingBox.y - 40);
	draw();
	draw(copy_identity, 1);
	draw(copy_identity, 2);
	assert(app.clipboardText() == state.review.rows.front().key);
	Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y = 0;
	draw();
	wheel(inspector_scroll, -4);
	const auto inspector_position = Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y;
	wheel(selector_scroll, -4);
	assert(Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y == inspector_position);
	wheel(inspector_scroll, 4);
	state.search = "no_matching_change";
	draw();
	draw();
	assert(state.selected_outside_filter && selected_changes_row(state));
	assert(state.review.counts.pending() == 80);
	preview("selection-outside-filter");
	state.search.clear();
	state.selected_key.clear();
	state.view = 1;
	draw();
	draw();
	check_bounds(app, FlowElementID{workbench_id.value});
	check_assigned_width(app, FlowElementID{workbench_id.value},
						 1400 - selector_width - inspector_width);
	check_assigned_width(app, FlowElementID{inspector_id.value}, inspector_width);
	check_bounds(app, FlowElementID{inspector_id.value});
	preview("final-manifest");
	state.view = 0;
	state.review.rows.reserve(1500);
	for (uint64_t index = 80; index < 1500; ++index) {
		auto row = state.review.rows.front();
		row.key = "large-list:" + std::to_string(index);
		row.field_path = "large_workload.field_" + std::to_string(index);
		state.review.rows.emplace_back(std::move(row));
	}
	state.review.counts.updates = 1500;
	state.review.counts.domains[0] += 1420;
	state.review.counts.live_edits += 1420;
	rebuild_changes_model(state);
	draw();
	draw();
	assert(app.ui().devTreeSnapshot().clay.nodes.size() < 4000);
	check_bounds(app, FlowElementID{selector_id.value});
	check_bounds(app, FlowElementID{workbench_id.value});
	check_assigned_width(app, FlowElementID{workbench_id.value},
						 1400 - selector_width - inspector_width);
	check_assigned_width(app, FlowElementID{inspector_id.value}, inspector_width);
	check_bounds(app, FlowElementID{inspector_id.value});
	preview("large-list");
	verify_safe_point_service(app);
	std::cout
		<< "Changes independent native scrolling, centered search and responsive bounds verified\n";
}
