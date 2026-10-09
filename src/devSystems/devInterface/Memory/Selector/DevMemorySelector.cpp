#include "devSystems/devInterface/Memory/Selector/DevMemorySelector.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>

namespace FlowUi::devSystems::interface_elements {
namespace {
using Context = DevMemorySelector::BuildContext;
void draw_text(Context& context, std::string_view label,
			   Clay_Color color = interface_theme::kTextSecondary) {
	Clay_TextElementConfig text{};
	text.textColor = color;
	text.fontSize = 11;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(text));
}
void section(Context& context, LocalElementName identity, std::string_view label) {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
	declaration.layout.padding = Clay_Padding{12, 12, 0, 0};
	declaration.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	declaration.backgroundColor = interface_theme::kDepth2Ink;
	declaration.border = {.color = interface_theme::kBorderPrimary,
						  .width = Clay_BorderWidth{0, 0, 0, 1, 0}};
	CLAY(context.clayID(identity), declaration) {
		draw_text(context, label, interface_theme::kTextCanvas);
	}
}
void update_windows(Context& context, DevMemorySelection& selection) {
	auto windows = context.params.app->devWindowSnapshot();
	std::ranges::sort(windows, {}, &DevWindowInfo::id);
	auto& state = context.state();
	const bool changed = state.window_options.empty() || windows.size() != state.windows.size() ||
						 !std::equal(windows.begin(), windows.end(), state.windows.begin(),
									 [](const auto& left, const auto& right) {
										 return left.id == right.id && left.title == right.title;
									 });
	if (changed) {
		state.windows = std::move(windows);
		state.window_options.clear();
		state.window_labels.clear();
		state.window_labels.reserve(state.windows.size() + 1);
		state.window_options.reserve(state.windows.size() + 1);
		state.window_labels.emplace_back("All Windows");
		for (const auto& window : state.windows)
			state.window_labels.emplace_back(
				window.title.empty() ? "Window " + std::to_string(window.id)
									 : window.title + " · " + std::to_string(window.id));
		state.window_options.emplace_back(
			FSEL::ComboBoxOption{.value = 0, .text = state.window_labels.front()});
		for (std::size_t window_index = 0; window_index < state.windows.size(); ++window_index)
			state.window_options.emplace_back(
				FSEL::ComboBoxOption{.value = state.windows[window_index].id,
									 .text = state.window_labels[window_index + 1]});
	}
	if (selection.window_scope && !std::ranges::any_of(state.windows, [&](const auto& window) {
			return window.id == selection.window_scope;
		})) {
		selection.window_scope = 0;
		context.params.interfaceState->lastActionMessage =
			"Memory window closed; scope changed to All Windows";
	}
}
void draw_node(Context& context, DevMemorySelection& selection, uint64_t identity,
			   std::string_view label, uint32_t depth, bool* expanded, std::string_view badge) {
	context.uiManager.createElement(kDevNode, Keyed("memory-node", identity))
		.setParameters(DevNodeParameters{
			.debugName = label,
			.badgeText = badge,
			.selected_key = &selection.selected_node,
			.expanded = expanded,
			.selectionKey = identity,
			.badgeColor = interface_theme::kDepth3Elevated,
			.depth = depth,
			.hasChildren = expanded != nullptr,
		})
		.setDevInternalCapture(true)
		.draw();
}
[[nodiscard]] std::string_view heap_class(GpuHeapClass classification) noexcept {
	switch (classification) {
	case GpuHeapClass::DedicatedLike:
		return "Dedicated";
	case GpuHeapClass::SharedLike:
		return "Shared";
	case GpuHeapClass::HostLike:
		return "Host";
	default:
		return "Unknown";
	}
}
void draw_tree(Context& context, DevMemorySelection& selection,
			   const std::optional<MemoryEnvironmentSnapshot>& environment) {
	bool application_section = false;
	section(context, LocalElementName{"window-aware-title"}, "Window-scoped");
	for (const auto& node : memory_selector_nodes) {
		if (node.application_wide && !application_section) {
			application_section = true;
			section(context, LocalElementName{"application-title"}, "Application-wide");
		}
		bool visible = true;
		uint32_t depth = 0;
		auto parent = node.parent;
		while (parent != DevMemoryNodeId::None) {
			visible = visible && selection.expanded[static_cast<std::size_t>(parent)];
			++depth;
			parent = memory_node(static_cast<uint64_t>(parent))->parent;
		}
		if (!visible)
			continue;
		const bool children = memory_node_has_children(node.id);
		const bool contains_selection = children &&
										!selection.expanded[static_cast<std::size_t>(node.id)] &&
										memory_node_descends_from(selection.selected_node, node.id);
		const auto badge =
			contains_selection ? std::string_view{"Selected below"} : std::string_view{};
		draw_node(context, selection, static_cast<uint64_t>(node.id), node.label, depth,
				  children ? &selection.expanded[static_cast<std::size_t>(node.id)] : nullptr,
				  badge);
		if (node.id == DevMemoryNodeId::GpuHeaps &&
			selection.expanded[static_cast<std::size_t>(node.id)]) {
			if (environment && environment->gpu.available && !environment->gpu.heaps.empty()) {
				for (const auto& heap : environment->gpu.heaps) {
					const auto label = "Heap " + std::to_string(heap.heapIndex) + " · " +
									   std::string(heap_class(heap.classification));
					draw_node(context, selection, memory_heap_node_base + heap.heapIndex, label, 1,
							  nullptr, {});
				}
			} else
				draw_text(context, "Heap data unavailable", interface_theme::kTextMuted);
		}
	}
}
} // namespace
void DevMemorySelector::buildElement(BuildContext& context) {
	Clay_ElementDeclaration root{};
	// Bound the selector to its clipped column so only the inner tree can overflow.
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	if (!context.params.app || !context.params.interfaceState) {
		CLAY(context.clayID(), root) {
			draw_text(context, "Memory selection unavailable");
		}
		return;
	}
	CLAY(context.clayID(), root) {
		auto& selection = context.params.interfaceState->memory_selection;
		update_windows(context, selection);
		const auto environment =
			context.params.app->devMonitoring().memoryReporting().environmentSnapshot();
		const bool valid_heap =
			selection.selected_node >= memory_heap_node_base && environment &&
			environment->gpu.available &&
			std::ranges::any_of(environment->gpu.heaps, [&](const auto& heap) {
				return memory_heap_node_base + heap.heapIndex == selection.selected_node;
			});
		if (selection.selected_node && !memory_node(selection.selected_node) && !valid_heap) {
			selection.selected_node = 0;
			context.params.interfaceState->lastActionMessage =
				"Memory selection unavailable; returned to Overview";
		}
		section(context, LocalElementName{"scope-title"}, "Window scope");
		Clay_ElementDeclaration controls{};
		controls.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(42)};
		controls.layout.padding = Clay_Padding{8, 8, 7, 7};
		CLAY(context.clayID("scope-control"), controls) {
			FSEL::ComboBoxParameters parameters{};
			parameters.options = context.state().window_options;
			parameters.selectedValue = &selection.window_scope;
			parameters.fontSize = 11;
			parameters.sizing =
				Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
			context.uiManager.createElement(FSEL::kComboBox, LocalElementName{"window-scope"})
				.setParameters(parameters)
				.setDevInternalCapture(true)
				.draw();
		}
		Clay_ElementDeclaration content{};
		content.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		content.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		content.backgroundColor = interface_theme::kDepth0Keel;
		const auto scroll_id = context.clayID("memory-tree-scroll");
		const auto scroll = Clay_GetScrollContainerData(scroll_id);
		content.clip = {.vertical = true,
						.childOffset = scroll.found && scroll.scrollPosition
										   ? *scroll.scrollPosition
										   : Clay_Vector2{}};
		CLAY(scroll_id, content) {
			Clay_ElementDeclaration rows{};
			rows.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
			rows.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			CLAY(context.clayID("rows"), rows) {
				draw_tree(context, selection, environment);
			}
		}
		Clay_ElementDeclaration status{};
		status.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(40)};
		status.layout.padding = Clay_Padding{8, 8, 4, 4};
		status.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
		const std::string label =
			memory_selection_application_wide(selection.selected_node)
				? "Application-wide · window scope does not apply"
				: std::string(selection.selected_node ? "Window scope · " : "Overview · ") +
					  (selection.window_scope ? "Window " + std::to_string(selection.window_scope)
											  : "All Windows");
		CLAY(context.clayID("selection-context"), status) {
			draw_text(context, label);
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
