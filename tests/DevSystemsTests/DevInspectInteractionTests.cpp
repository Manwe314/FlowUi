#include "devSystems/devTooling/inspect/DevInspectInteractionController.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace FlowUi;
using namespace FlowUi::devSystems::tooling;

#define CHECK(condition)                                                      \
	do {                                                                      \
		if (!(condition)) {                                                   \
			std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; \
			std::exit(1);                                                     \
		}                                                                     \
	} while (false)

namespace {
DevTreeSnapshot fixture(WindowId window = 1u) {
	DevTreeSnapshot snapshot{};
	snapshot.window = window;
	snapshot.stats.complete = true;
	snapshot.flow.nodes.resize(3);
	for (uint32_t index = 0; index < 3; ++index) {
		auto& node = snapshot.flow.nodes[index];
		node.definition = FlowDefinitionID{.value = 100u + index};
		node.instance = ::FlowUi::detail::element::ElementInstanceKey{.value = 200u + index};
		node.flags = DevFlowNodeFlag::Drawn;
#if FLOW_UI_DEV_CAPTURE_CLAY
		node.clayRoot = index;
#endif
	}
#if FLOW_UI_DEV_CAPTURE_CLAY
	snapshot.clay.roots.emplace_back(DevClayRoot{.node = 0u});
	snapshot.clay.nodes.resize(3);
	for (uint32_t index = 0; index < 3; ++index) {
		auto& node = snapshot.clay.nodes[index];
		node.clayId = 10u + index;
		node.bounds = {0, 0, 100, 100};
		node.directFlowOwner = index;
		node.subtreeEnd = index + 1;
		node.parent = index ? 0u : InvalidClayNode;
	}
	snapshot.clay.nodes[0].subtreeEnd = 3;
	snapshot.clay.nodes[1].bounds = {10, 10, 30, 30};
	snapshot.clay.nodes[2].bounds = {60, 10, 30, 30};
#endif
	return snapshot;
}

DevInspectTarget target(const DevTreeSnapshot& snapshot, uint32_t index) {
	return {snapshot.window, snapshot.flow.nodes[index].definition,
			snapshot.flow.nodes[index].instance};
}

void identity_and_configuration() {
	auto snapshot = fixture();
	const auto selected = target(snapshot, 1);
	CHECK(resolve_inspect_target(selected, snapshot).flowNodeIndex == 1u);
	std::swap(snapshot.flow.nodes[1], snapshot.flow.nodes[2]);
	CHECK(resolve_inspect_target(selected, snapshot).flowNodeIndex == 2u);
	snapshot.flow.nodes[2].instance.value = 999u;
	CHECK(!resolve_inspect_target(selected, snapshot).isValid());

	DevInspectInteractionController controller;
	controller.set_interface_window(9u);
	CHECK(controller.mode_flags() == DevOverlayModeFlags::Default);
	controller.toggle_surface(DevOverlayModeFlags::BoxModel);
	controller.toggle_surface(DevOverlayModeFlags::RulersAndDistance);
	CHECK(controller.mode_flags() == DevOverlayModeFlags::None);
	controller.select_primary(target(snapshot, 0));
	controller.clear_selection();
	CHECK(!controller.primary_target());
	CHECK(controller.mode_flags() == DevOverlayModeFlags::None);
	CHECK(!controller.eligible_window(9u));
	CHECK(controller.eligible_window(2u));

	CHECK(controller.pick_domain() == DevInspectPickDomain::Flow);
	controller.set_pick_domain(DevInspectPickDomain::Clay);
	CHECK(controller.pick_domain() == DevInspectPickDomain::Clay);
	controller.toggle_pick_domain();
	CHECK(controller.pick_domain() == DevInspectPickDomain::Flow);
	controller.set_pick_domain(DevInspectPickDomain::Clay);

#if FLOW_UI_DEV_CAPTURE_CLAY
	const DevInspectTarget clay_spec(snapshot.window, snapshot.clay.nodes[1].clayId, 0u, 1u, 9999u);
	const auto resolved_clay = resolve_inspect_target(clay_spec, snapshot);
	CHECK(resolved_clay.isValid());
	CHECK(resolved_clay.kind == DevInspectTargetKind::Clay);
	CHECK(resolved_clay.clayNodeIndex == 1u);
	CHECK(resolved_clay.clayId == snapshot.clay.nodes[1].clayId);
#endif
}

#if FLOW_UI_DEV_CAPTURE_CLAY
void geometry() {
	auto snapshot = fixture();
	CHECK(hit_test_inspect_target(snapshot, 20, 20) == target(snapshot, 1));
	CHECK(hit_test_inspect_target(snapshot, 70, 20) == target(snapshot, 2));
	CHECK(!hit_test_inspect_target(snapshot, 100, 100));
	CHECK(!hit_test_inspect_target(snapshot, std::numeric_limits<float>::quiet_NaN(), 0));
	// Later siblings win overlapping areas, independent of Flow tree ordering.
	snapshot.clay.nodes[2].bounds = {10, 10, 30, 30};
	CHECK(hit_test_inspect_target(snapshot, 20, 20) == target(snapshot, 2));
	snapshot.clay.nodes[2].flags = DevClayNodeFlag::Exiting;
	CHECK(hit_test_inspect_target(snapshot, 20, 20) == target(snapshot, 1));
	snapshot.clay.nodes[2].flags = DevClayNodeFlag::None;
	snapshot.flow.nodes[2].flags |= DevFlowNodeFlag::InternalDev;
	CHECK(!hit_test_inspect_target(snapshot, 20, 20));
	snapshot.flow.nodes[2].flags = DevFlowNodeFlag::Constructed;
	CHECK(hit_test_inspect_target(snapshot, 20, 20) == target(snapshot, 2));
	CHECK(resolve_inspect_target(target(snapshot, 2), snapshot).isValid());
	snapshot.flow.nodes[2].flags = DevFlowNodeFlag::None;
	CHECK(!hit_test_inspect_target(snapshot, 20, 20));
	snapshot.flow.nodes[2].flags = DevFlowNodeFlag::Drawn;
	// Unowned content resolves only through its ancestry, not an obscured sibling.
	snapshot.clay.nodes[2].directFlowOwner = InvalidFlowNode;
	CHECK(hit_test_inspect_target(snapshot, 20, 20) == target(snapshot, 0));
	// In Clay domain, hit testing returns the topmost Clay node directly without requiring a Flow owner.
	const auto clay_hit = hit_test_inspect_target(snapshot, 20, 20, DevInspectPickDomain::Clay);
	CHECK(clay_hit.kind == DevInspectTargetKind::Clay);
	CHECK(clay_hit.clay_index == 2u);
	CHECK(clay_hit.clay_id == snapshot.clay.nodes[2].clayId);
	snapshot.clay.nodes[2].parent = InvalidClayNode;
	CHECK(!hit_test_inspect_target(snapshot, 20, 20));
	// Floating root ordering is independent of node storage order.
	snapshot = fixture();
	snapshot.clay.roots.emplace_back(DevClayRoot{.node = 1u, .zIndex = 5, .paintOrder = 1});
	snapshot.clay.nodes[1].rootIndex = 1;
	snapshot.clay.nodes[2].bounds = {10, 10, 30, 30};
	CHECK(hit_test_inspect_target(snapshot, 20, 20) == target(snapshot, 1));
	// A scroll clip clips only its enabled axis; nested clipping applies to floats.
	snapshot.clay.nodes[0].bounds = {0, 0, 100, 15};
	snapshot.clay.nodes[0].declaration.clip.vertical = true;
	snapshot.clay.nodes[1].clipClayId = 10u;
	snapshot.clay.nodes[2].clipClayId = 10u;
	CHECK(!hit_test_inspect_target(snapshot, 20, 20));
	CHECK(hit_test_inspect_target(snapshot, 20, 12) == target(snapshot, 1));
	snapshot.clay.nodes[1].bounds.y = -10; // Captured bounds already include scrolling.
	CHECK(hit_test_inspect_target(snapshot, 20, 2) == target(snapshot, 1));
	snapshot.clay.nodes[1].flags = DevClayNodeFlag::BoundsUnavailable;
	CHECK(hit_test_inspect_target(snapshot, 20, 2) == target(snapshot, 0));
}

void picking() {
	auto snapshot = fixture();
	DevInspectInteractionController controller;
	controller.set_interface_window(9u);
	DevInspectPointerState pointer{};
	FrameInput input{};
	input.pointerInside = true;
	input.mouseX = 20;
	input.mouseY = 20;
	controller.filter_input(1u, snapshot, input, pointer);
	controller.toggle_primary_pick();
	controller.finish_frame(snapshot, pointer);
	DevOverlaySelectionSpec overlay{};
	CHECK(controller.overlay_selection(snapshot, overlay));
	CHECK(overlay.primaryTarget.instanceKey == snapshot.flow.nodes[1].instance);
	CHECK(!controller.primary_target());
	input.mouseDown[0] = true;
	controller.filter_input(1u, snapshot, input, pointer);
	CHECK(!input.mouseDown[0]);
	controller.finish_frame(snapshot, pointer);
	CHECK(controller.primary_target() == target(snapshot, 1));
	CHECK(!controller.picking());
	// A held press cannot reach the application after the mode has exited.
	input.mouseDown[0] = true;
	controller.filter_input(1u, snapshot, input, pointer);
	CHECK(!input.mouseDown[0]);
	input.mouseDown[0] = false;
	controller.filter_input(1u, snapshot, input, pointer);
	CHECK(!pointer.consume_until_release);
	input.mouseDown[0] = true;
	controller.filter_input(1u, snapshot, input, pointer);
	CHECK(input.mouseDown[0]);
	input.mouseDown[0] = false;
	controller.filter_input(1u, snapshot, input, pointer);

	controller.begin_secondary_pick();
	snapshot.flow.nodes[2].flags = DevFlowNodeFlag::Constructed;
	input.mouseX = 70;
	input.mouseDown[0] = true;
	controller.filter_input(1u, snapshot, input, pointer);
	controller.finish_frame(snapshot, pointer);
	CHECK(controller.primary_target() == target(snapshot, 1));
	CHECK(controller.secondary_target() == target(snapshot, 2));
	CHECK(!controller.picking());
	CHECK(controller.overlay_selection(snapshot, overlay));
	CHECK(overlay.secondaryTarget.instanceKey == snapshot.flow.nodes[2].instance);
	controller.toggle_primary_pick();
	controller.toggle_primary_pick();
	CHECK(!controller.picking());
	CHECK(controller.secondary_target() == target(snapshot, 2));
	controller.select_primary(target(snapshot, 1));
	CHECK(controller.primary_target() == target(snapshot, 1));

	// Clay domain picking
	controller.clear_selection();
	controller.set_pick_domain(DevInspectPickDomain::Clay);
	CHECK(controller.pick_domain() == DevInspectPickDomain::Clay);
	controller.toggle_primary_pick();
	CHECK(controller.picking());
	input.mouseX = 20;
	input.mouseY = 20;
	input.mouseDown[0] = false;
	controller.filter_input(1u, snapshot, input, pointer);
	controller.finish_frame(snapshot, pointer);
	CHECK(controller.overlay_selection(snapshot, overlay));
	CHECK(overlay.primaryTarget.kind == DevInspectTargetKind::Clay);
	CHECK(overlay.primaryTarget.clayNodeIndex == 1u);
	CHECK(overlay.primaryTarget.clayId == snapshot.clay.nodes[1].clayId);

	input.mouseDown[0] = true;
	controller.filter_input(1u, snapshot, input, pointer);
	controller.finish_frame(snapshot, pointer);
	CHECK(!controller.picking());
	CHECK(controller.primary_target().kind == DevInspectTargetKind::Clay);
	CHECK(controller.primary_target().clay_id == snapshot.clay.nodes[1].clayId);
	CHECK(controller.primary_target().clay_index == 1u);

	controller.remove_window(1u);
	CHECK(!controller.primary_target());
	CHECK(!controller.overlay_selection(snapshot, overlay));
}

void exclusion_and_lifecycle() {
	auto snapshot = fixture();
	auto second = fixture(2u);
	auto developer = fixture(9u);
	DevInspectInteractionController controller;
	controller.set_interface_window(9u);
	controller.select_primary(target(snapshot, 1));
	controller.begin_secondary_pick();
	DevInspectPointerState pointer{.observed_release = true};
	FrameInput input{};
	input.pointerInside = true;
	input.mouseX = 70;
	input.mouseY = 20;
	input.mouseDown[0] = true;
	controller.filter_input(2u, second, input, pointer);
	controller.finish_frame(second, pointer);
	CHECK(controller.picking());
	CHECK(!controller.secondary_target());
	CHECK(!input.mouseDown[0]);
	DevInspectPointerState dev_pointer{.observed_release = true};
	input.mouseDown[0] = true;
	controller.filter_input(9u, developer, input, dev_pointer);
	controller.finish_frame(developer, dev_pointer);
	CHECK(input.mouseDown[0]);
	DevOverlaySelectionSpec overlay{};
	CHECK(!controller.overlay_selection(developer, overlay));
	controller.cancel_pick();
	controller.toggle_primary_pick();
	// Candidates disappearing between presented and current geometry cannot commit.
	pointer = {.observed_release = true};
	input.mouseDown[0] = true;
	controller.filter_input(2u, second, input, pointer);
	second.flow.nodes[2].instance.value = 900u;
	controller.finish_frame(second, pointer);
	CHECK(controller.primary_target() == target(snapshot, 1));
	CHECK(controller.picking());
	input.keyDown[256] = true;
	controller.filter_input(2u, second, input, pointer);
	CHECK(!controller.picking());
	CHECK(!input.keyDown[256]);
	controller.toggle_primary_pick();
	controller.remove_window(9u);
	CHECK(!controller.picking());
	CHECK(controller.primary_target() == target(snapshot, 1));
	// Incomplete captures are not mistaken for element deletion.
	snapshot.stats.complete = false;
	snapshot.flow.nodes.clear();
	controller.finish_frame(snapshot, pointer);
	CHECK(controller.primary_target());
	snapshot.stats.complete = true;
	controller.finish_frame(snapshot, pointer);
	CHECK(!controller.primary_target());
}
#endif
} // namespace

int main() {
	identity_and_configuration();
#if FLOW_UI_DEV_CAPTURE_CLAY
	geometry();
	picking();
	exclusion_and_lifecycle();
#else
	DevInspectInteractionController controller;
	controller.set_interface_window(9u);
	controller.toggle_primary_pick();
	CHECK(!controller.picking());
	CHECK(!hit_test_inspect_target(fixture(), 20, 20));
#endif
	std::cout << "Inspect interaction tests passed\n";
}
