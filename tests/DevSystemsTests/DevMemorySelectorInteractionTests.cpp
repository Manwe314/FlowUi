#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Memory/Selector/DevMemorySelector.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
constexpr auto selector_id = Global<kDevMemorySelector>("memory.selector.verification");
constexpr auto inspect_id = Global<kDevNode>("memory.shared.inspect.verification");
[[nodiscard]] FlowElementID node_id(DevMemoryNodeId node) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = selector_id.value}, DevNode::definitionId,
		Keyed("memory-node", static_cast<uint64_t>(node)).token);
}
[[nodiscard]] FlowElementID part_id(FlowElementID owner, FlowDefinitionID definition,
									uint64_t token) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(owner, definition, token);
}
} // namespace
int main() {
	for (const auto& node : memory_selector_nodes) {
		assert(memory_node(static_cast<uint64_t>(node.id)) == &node);
		assert(node.parent == DevMemoryNodeId::None ||
			   memory_node(static_cast<uint64_t>(node.parent)));
		assert(node.parent == DevMemoryNodeId::None ||
			   memory_node(static_cast<uint64_t>(node.parent)) < &node);
		assert(node.parent == DevMemoryNodeId::None ||
			   node.application_wide ==
				   memory_node(static_cast<uint64_t>(node.parent))->application_wide);
	}
	AppConfig config{};
	config.window.title = "Memory selector verification";
	config.window.width = 600;
	config.window.height = 1400;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	auto& selection = session.memory_selection;
	const auto draw = [&](FlowElementID target = {}, int event = 0, FlowElementID extra = {},
						  bool inspect = false) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseDown[0] = event == 1;
		input.mouseX = -10;
		input.mouseY = -10;
		for (const auto control : {target, extra}) {
			if (!control)
				continue;
			const auto clay_id = app.ui().toClayEID(control).id;
			interaction.hoveredElementIds.emplace_back(clay_id);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(clay_id);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(clay_id);
		}
		Clay_ElementDeclaration root{};
		root.layout.sizing = {.width = CLAY_SIZING_FIXED(280), .height = CLAY_SIZING_FIXED(520)};
		root.clip = {.horizontal = true, .vertical = true};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		CLAY(CLAY_ID("memory-selector-root"), root) {
			if (inspect) {
				app.ui()
					.createElement(kDevNode, inspect_id)
					.setParameters(DevNodeParameters{.debugName = "Inspect shared row",
													 .interfaceState = &session,
													 .app = &app,
													 .kind = kDevInterfaceFlowNodeKind,
													 .selectionKey = 456})
					.draw();
			} else
				app.ui()
					.createElement(kDevMemorySelector, selector_id)
					.setParameters({&app, &session})
					.draw();
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto has_label = [&](std::string_view label) {
		const auto& tree = app.ui().devTreeSnapshot();
		return std::ranges::any_of(
			tree.clay.nodes, [&](const auto& node) { return tree.string(node.text) == label; });
	};
	const auto click = [&](FlowElementID target, FlowElementID extra = {}) {
		draw(target, 1, extra);
		draw(target, 2, extra);
		draw();
	};
	draw();
	draw();
	assert(selection.selected_node == 0 && selection.window_scope == 0);
	assert(!has_label("Shared") && has_label("Window-scoped"));
	assert(has_label("Overview · All Windows") && has_label("CPU Storage") &&
		   has_label("Managers"));
	// The clipped column must bound the selector while the tree overflows independently.
	const auto tree_id =
		part_id(FlowElementID{.value = selector_id.value}, DevMemorySelector::definitionId,
				LocalElementName{"memory-tree-scroll"}.token);
	const auto scope_id =
		part_id(FlowElementID{.value = selector_id.value}, DevMemorySelector::definitionId,
				LocalElementName{"scope-control"}.token);
	const auto tree_clay_id = app.ui().toClayEID(tree_id);
	const auto scope_clay_id = app.ui().toClayEID(scope_id);
	const auto scroll_before = Clay_GetScrollContainerData(tree_clay_id);
	assert(scroll_before.found);
	assert(scroll_before.contentDimensions.height > scroll_before.scrollContainerDimensions.height);
	const auto scope_before = Clay_GetElementData(scope_clay_id).boundingBox;
	const auto row_before =
		Clay_GetElementData(app.ui().toClayEID(node_id(DevMemoryNodeId::CpuStorage))).boundingBox;
	const auto viewport = Clay_GetElementData(tree_clay_id).boundingBox;
	assert(viewport.height == 408);
	auto* native_window = static_cast<GLFWwindow*>(app.nativeWindowHandle());
	const auto cursor_callback = glfwSetCursorPosCallback(native_window, nullptr);
	glfwSetCursorPosCallback(native_window, cursor_callback);
	const auto scroll_callback = glfwSetScrollCallback(native_window, nullptr);
	glfwSetScrollCallback(native_window, scroll_callback);
	assert(cursor_callback && scroll_callback);
	glfwSetCursorPos(native_window, viewport.x + 100, viewport.y + 20);
	cursor_callback(native_window, viewport.x + 100, viewport.y + 20);
	scroll_callback(native_window, 0, -3);
	draw();
	assert(Clay_GetScrollContainerData(tree_clay_id).scrollPosition->y < 0);
	const auto row_after =
		Clay_GetElementData(app.ui().toClayEID(node_id(DevMemoryNodeId::CpuStorage))).boundingBox;
	assert(row_after.y < row_before.y);
	assert(Clay_GetElementData(scope_clay_id).boundingBox.y == scope_before.y);
	// Wheel over the pinned scope must not move the tree.
	const float offset_before_scope_wheel =
		Clay_GetScrollContainerData(tree_clay_id).scrollPosition->y;
	glfwSetCursorPos(native_window, scope_before.x + 100, scope_before.y + 10);
	cursor_callback(native_window, scope_before.x + 100, scope_before.y + 10);
	scroll_callback(native_window, 0, -3);
	draw();
	assert(Clay_GetScrollContainerData(tree_clay_id).scrollPosition->y ==
		   offset_before_scope_wheel);
	*Clay_GetScrollContainerData(tree_clay_id).scrollPosition = {};
	draw();
	const auto storage_id = node_id(DevMemoryNodeId::SharedCpuStorage);
	click(storage_id);
	assert(selection.selected_node == static_cast<uint64_t>(DevMemoryNodeId::SharedCpuStorage));
	click(storage_id);
	assert(selection.selected_node == 0);
	click(node_id(DevMemoryNodeId::PersistentPool));
	assert(selection.selected_node == static_cast<uint64_t>(DevMemoryNodeId::PersistentPool));
	const auto disclosure =
		part_id(storage_id, DevNode::definitionId, LocalElementName{"disclosure"}.token);
	click(storage_id, disclosure);
	assert(!selection.expanded[static_cast<size_t>(DevMemoryNodeId::SharedCpuStorage)]);
	assert(selection.selected_node == static_cast<uint64_t>(DevMemoryNodeId::PersistentPool));
	assert(!has_label("Persistent Pool") && has_label("Selected below"));
	click(storage_id, disclosure);
	assert(has_label("Persistent Pool"));
	const auto combo_id =
		part_id(FlowElementID{.value = selector_id.value}, FSEL::ComboBox::definitionId,
				LocalElementName{"window-scope"}.token);
	click(combo_id);
	const auto option_id =
		part_id(combo_id, FSEL::ComboBox::definitionId, Keyed("option", MainWindowId).token);
	click(option_id);
	assert(selection.window_scope == MainWindowId);
	assert(selection.selected_node == static_cast<uint64_t>(DevMemoryNodeId::PersistentPool));
	assert(has_label("Application-wide · window scope does not apply"));
	click(node_id(DevMemoryNodeId::ProcessMemory));
	assert(has_label("Application-wide · window scope does not apply"));
	click(node_id(DevMemoryNodeId::ProcessMemory));
	assert(has_label("Overview · Window " + std::to_string(MainWindowId)));
	// A release without a matching press cannot select; releasing elsewhere cancels.
	draw(storage_id, 2);
	draw();
	assert(selection.selected_node == 0);
	draw(storage_id, 1);
	draw();
	draw();
	draw(storage_id, 2);
	draw();
	assert(selection.selected_node == 0);
	// Expansion and selection survive a hidden/recreated selector.
	click(node_id(DevMemoryNodeId::Managers));
	draw({}, 0, {}, true);
	draw();
	assert(selection.selected_node == static_cast<uint64_t>(DevMemoryNodeId::Managers));
	// Refresh a live popup with another identically titled window, then remove it.
	click(combo_id);
	const auto secondary = app.createWindow(config.window.title, 320, 240);
	assert(secondary);
	draw();
	assert(has_label(config.window.title + " · " + std::to_string(*secondary)));
	const auto secondary_option =
		part_id(combo_id, FSEL::ComboBox::definitionId, Keyed("option", *secondary).token);
	click(secondary_option);
	assert(selection.window_scope == *secondary);
	assert(app.destroyWindow(*secondary));
	draw();
	assert(selection.window_scope == 0);
	assert(selection.selected_node == static_cast<uint64_t>(DevMemoryNodeId::Managers));
	selection.window_scope = UINT64_MAX;
	draw();
	assert(selection.window_scope == 0);
	assert(selection.selected_node == static_cast<uint64_t>(DevMemoryNodeId::Managers));
	selection.selected_node = memory_heap_node_base + UINT32_MAX;
	draw();
	assert(selection.selected_node == 0);
	// Every predefined parent and leaf supports selection and deselection.
	selection.expanded.fill(true);
	draw();
	for (const auto& node : memory_selector_nodes) {
		assert(has_label(node.label));
		click(node_id(node.id));
		assert(selection.selected_node == static_cast<uint64_t>(node.id));
		click(node_id(node.id));
		assert(selection.selected_node == 0);
	}
	// The same retained row keeps Inspect's existing controller-backed toggle.
	draw({}, 0, {}, true);
	draw(FlowElementID{.value = inspect_id.value}, 1, {}, true);
	draw(FlowElementID{.value = inspect_id.value}, 2, {}, true);
	assert(session.inspectSelectedNodeKey == 456);
	draw(FlowElementID{.value = inspect_id.value}, 1, {}, true);
	draw(FlowElementID{.value = inspect_id.value}, 2, {}, true);
	assert(session.inspectSelectedNodeKey == 0 && session.inspectSelectedNodeKind == 0);
	assert(selection.selected_node == 0);
	std::cout << "Memory selector interaction verification passed\n";
}
