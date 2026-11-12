#pragma once

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/input/Key.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Catalogue/DevCatalogueModel.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include <algorithm>

namespace FlowUi::devSystems::interface_elements::catalogue_focus {
/* Bind controls to their actual clipped panel so keyboard traversal can reveal them. */
class ViewportScope {
public:
	ViewportScope(DevCatalogueState& state, Clay_ElementId viewport) noexcept
		: state_(state), previous_(state.registration_viewport) {
		state.registration_viewport = viewport;
	}
	~ViewportScope() { state_.registration_viewport = previous_; }
	ViewportScope(const ViewportScope&) = delete;
	ViewportScope& operator=(const ViewportScope&) = delete;

private:
	DevCatalogueState& state_;
	Clay_ElementId previous_{};
};
/*
 * Retain this frame's submitted target; no captured tree is required.
 * @param pointer_focus Whether pointer presses also assign focus. Disable for containers
 * whose descendants manage their own text or control focus.
 */
template <class Context>
void register_target(Context& context, DevCatalogueState& state, Clay_ElementId identity,
					 FlowElementID text_input = {}, bool enabled = true,
					 bool pointer_focus = true) {
	if (!enabled || !identity.id)
		return;
	const auto found = std::ranges::find(state.focus_targets, identity.id,
										 [](const auto& target) { return target.identity.id; });
	if (found == state.focus_targets.end())
		state.focus_targets.emplace_back(
			DevCatalogueFocusTarget{identity, state.registration_viewport, text_input});
	if (pointer_focus && context.uiManager.getPreviousFramesInteraction().isPressed(identity)) {
		context.uiManager.shortcuts().setFocusedElement(identity);
		if (!text_input.value)
			context.uiManager.inputFields().requestCaret(FlowElementID{},
														 CaretRequestKind::ClearAll);
	}
}
/* Resolve a locally created retained control before registering its keyboard target. */
template <class Context, class Identity>
void register_element(Context& context, DevCatalogueState& state, FlowDefinitionID definition,
					  const Identity& identity, bool enabled = true, bool text_input = false) {
	const auto element =
		::FlowUi::detail::element_id::resolveLocal(context.id, definition, identity.token);
	register_target(context, state, context.uiManager.toClayEID(element),
					text_input ? element : FlowElementID{}, enabled);
}
/* Traverse the previous submitted order, then collect only this frame's visible controls. */
template <class Context>
void begin_frame(Context& context, DevCatalogueState& state) {
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	if (current.windowFocused && current.keyDown[FlowUi::keyCode(FlowUi::Key::Escape)] &&
		!previous.keyDown[FlowUi::keyCode(FlowUi::Key::Escape)] &&
		!context.uiManager.inputFields().hasPrimaryFieldFocus() &&
		!context.uiManager.popups().has_open_popups()) {
		const auto focused = context.uiManager.shortcuts().focusedElement().id;
		if (std::ranges::any_of(state.focus_targets, [focused](const auto& target) {
				return target.identity.id == focused;
			})) {
			state.selected_identity = 0;
			state.selected_missing = false;
			state.dirty = true;
		}
	}
	if (current.windowFocused && current.keyDown[FlowUi::keyCode(FlowUi::Key::Tab)] &&
		!previous.keyDown[FlowUi::keyCode(FlowUi::Key::Tab)] && !state.focus_targets.empty() &&
		!context.uiManager.popups().has_open_popups()) {
		const auto focused = context.uiManager.shortcuts().focusedElement().id;
		const auto found = std::ranges::find(state.focus_targets, focused,
											 [](const auto& target) { return target.identity.id; });
		const bool reverse = current.keyDown[FlowUi::keyCode(FlowUi::Key::LeftShift)] ||
							 current.keyDown[FlowUi::keyCode(FlowUi::Key::RightShift)];
		const auto count = state.focus_targets.size();
		const auto position = found == state.focus_targets.end()
								  ? (reverse ? 0 : count - 1)
								  : static_cast<size_t>(found - state.focus_targets.begin());
		const auto& target = state.focus_targets[(position + (reverse ? count - 1 : 1)) % count];
		context.uiManager.inputFields().requestCaret(FlowElementID{}, CaretRequestKind::ClearAll);
		context.uiManager.shortcuts().setFocusedElement(target.identity);
		if (target.text_input.value)
			context.uiManager.inputFields().requestCaret(target.text_input,
														 CaretRequestKind::SetPrimary);
		if (target.viewport.id) {
			const auto bounds = Clay_GetElementData(target.identity);
			const auto viewport = Clay_GetElementData(target.viewport);
			const auto scroll = Clay_GetScrollContainerData(target.viewport);
			if (bounds.found && viewport.found && scroll.found && scroll.scrollPosition) {
				if (bounds.boundingBox.y < viewport.boundingBox.y)
					scroll.scrollPosition->y += viewport.boundingBox.y - bounds.boundingBox.y;
				else if (bounds.boundingBox.y + bounds.boundingBox.height >
						 viewport.boundingBox.y + viewport.boundingBox.height)
					scroll.scrollPosition->y -= bounds.boundingBox.y + bounds.boundingBox.height -
												viewport.boundingBox.y -
												viewport.boundingBox.height;
			}
		}
	}
	state.focus_targets.clear();
	if (state.focus_targets.capacity() < 128)
		state.focus_targets.reserve(128);
	state.registration_viewport = {};
}
} // namespace FlowUi::devSystems::interface_elements::catalogue_focus
#endif
