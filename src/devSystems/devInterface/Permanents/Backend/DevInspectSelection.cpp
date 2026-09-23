#include "devSystems/devInterface/Permanents/Backend/DevInspectSelection.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/App.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/UiManager.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include <algorithm>
#include <cctype>

namespace FlowUi::devSystems {
using namespace interface_elements;
namespace {
void assign_selection(DevInterfaceState& state, uint64_t node_kind, uint64_t node_key) noexcept {
	state.inspectSelectedNodeKind = node_key ? node_kind : 0u;
	state.inspectSelectedNodeKey = node_key;
	state.selectedElementId = node_kind == 1u ? FlowElementID{.value = node_key} : FlowElementID{};
}
} // namespace

void apply_inspect_selection(App& app, DevInterfaceState& state, uint64_t node_kind,
							 uint64_t node_key, bool toggle) noexcept {
	if (toggle && state.inspectSelectedNodeKind == node_kind &&
		state.inspectSelectedNodeKey == node_key)
		node_key = 0u;
	assign_selection(state, node_kind, node_key);
	auto& controller = app.devTooling().inspect_interaction();
	tooling::DevInspectTarget target{};
	if (node_kind == kDevInterfaceFlowNodeKind && node_key && app.hasWindow(state.selectedWindowId)) {
		const auto& snapshot = app.ui(state.selectedWindowId).devTreeSnapshot();
		for (const auto& node : snapshot.flow.nodes) {
			if (node.instance.value == node_key) {
				target.window = state.selectedWindowId;
				target.kind = tooling::DevInspectTargetKind::Flow;
				target.definition = node.definition;
				target.instance = node.instance;
				target.selection_key = node_key;
				break;
			}
		}
	} else if (node_kind == kDevInterfaceClayNodeKind && node_key && app.hasWindow(state.selectedWindowId)) {
#if FLOW_UI_DEV_CAPTURE_CLAY
		const auto& snapshot = app.ui(state.selectedWindowId).devTreeSnapshot();
		for (uint32_t nodeIndex = 0; nodeIndex < snapshot.clay.nodes.size(); ++nodeIndex) {
			const auto& node = snapshot.clay.nodes[nodeIndex];
			const uint64_t key = stableNodeKey(
				(static_cast<uint64_t>(node.rootIndex) << 32u) | nodeIndex, node.clayId);
			if (key == node_key) {
				target.window = state.selectedWindowId;
				target.kind = tooling::DevInspectTargetKind::Clay;
				target.clay_id = node.clayId;
				target.clay_root = node.rootIndex;
				target.clay_index = nodeIndex;
				target.selection_key = node_key;
				if (node.directFlowOwner < snapshot.flow.nodes.size()) {
					target.definition = snapshot.flow.nodes[node.directFlowOwner].definition;
					target.instance = snapshot.flow.nodes[node.directFlowOwner].instance;
				}
				break;
			}
		}
#endif
	}
	if (target)
		controller.select_primary(target);
	else
		controller.clear_selection();
	state.inspect_selection_revision = controller.selection_revision();
}

void synchronize_inspect_selection(App& app, DevInterfaceState& state) {
	auto& controller = app.devTooling().inspect_interaction();
	if (state.activeTab != static_cast<uint64_t>(DevInterfaceTab::Inspect))
		controller.cancel_pick();
	if (state.inspect_selection_revision == controller.selection_revision())
		return;
	state.inspect_selection_revision = controller.selection_revision();
	const auto target = controller.primary_target();
	if (!target) {
		assign_selection(state, 0u, 0u);
		return;
	}

	state.selectedWindowId = target.window;
	state.inspect_reveal_frames = 2u;

	if (target.kind == tooling::DevInspectTargetKind::Clay) {
		state.inspectForest = kDevInterfaceClayForest;
		assign_selection(state, kDevInterfaceClayNodeKind, target.selection_key);
		state.lastActionMessage = "Clay element selected in application window";
	} else {
		state.inspectForest = kDevInterfaceFlowForest;
		assign_selection(state, kDevInterfaceFlowNodeKind, target.instance.value);
		if (state.inspectDefinitionFilter != target.definition.value)
			state.inspectDefinitionFilter = 0u;
		if (app.hasWindow(target.window)) {
			const auto& snapshot = app.ui(target.window).devTreeSnapshot();
			const auto resolved = tooling::resolve_inspect_target(target, snapshot);
			if (resolved.isValid() && resolved.flowNodeIndex < snapshot.flow.nodes.size()) {
				const auto& node = snapshot.flow.nodes[resolved.flowNodeIndex];
				auto name = snapshot.string(node.debugName);
				if (name.empty())
					name = snapshot.string(node.definitionName);
				const auto match =
					std::search(name.begin(), name.end(), state.searchQuery.begin(),
								state.searchQuery.end(), [](unsigned char left, unsigned char right) {
									return std::tolower(left) == std::tolower(right);
								});
				if (!state.searchQuery.empty() && match == name.end())
					state.searchQuery.clear();
			}
		}
		state.lastActionMessage = "Flow element selected in application window";
	}
}
} // namespace FlowUi::devSystems
#endif
