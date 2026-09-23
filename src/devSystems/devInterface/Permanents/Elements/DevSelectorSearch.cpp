#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"
namespace FlowUi::devSystems::interface_elements {
void DevSelectorSearch::buildElement(BuildContext& context) {
	Clay_ElementDeclaration search{};
	search.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(46)};
	search.layout.padding = Clay_Padding{8, 8, 8, 8};
	search.backgroundColor = interface_theme::kDepth1Panel;
	search.border = {.color = interface_theme::kBorderPrimary,
					 .width = Clay_BorderWidth{0, 0, 0, 1, 0}};
	search.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};

	CLAY(context.clayID(), search) {
		FSEL::TextInputParameters input{};
		input.value = context.params.query;
		input.placeholder = context.params.placeholder;
		input.enabled = context.params.query != nullptr;
		input.sizing = Clay_Sizing{
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(30),
		};
		input.padding = Clay_Padding{8, 8, 5, 5};
		input.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
		input.cornerRadius = CLAY_CORNER_RADIUS(3);
		input.fontSize = 12;
		input.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
		input.idleOverrides.textColor = interface_theme::kTextCanvas;
		input.idleOverrides.placeholderColor = interface_theme::kTextMuted;
		input.idleOverrides.borderColor = interface_theme::kBorderVisible;
		input.hoveredOverrides.backgroundColor = interface_theme::kDepth3Elevated;
		input.hoveredOverrides.textColor = interface_theme::kTextCanvas;
		input.hoveredOverrides.placeholderColor = interface_theme::kTextSecondary;
		input.hoveredOverrides.borderColor = interface_theme::kTextMuted;
		input.focusedOverrides.backgroundColor = interface_theme::kDepth3Elevated;
		input.focusedOverrides.textColor = interface_theme::kTextCanvas;
		input.focusedOverrides.placeholderColor = interface_theme::kTextMuted;
		input.focusedOverrides.borderColor = interface_theme::kAccentCurrent;
		input.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
		input.disabledOverrides.textColor = interface_theme::kTextMuted;
		input.disabledOverrides.placeholderColor = interface_theme::kTextMuted;
		input.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
		input.caret.color = interface_theme::kAccentCurrent;
		input.caret.selectionBoxColor = interface_theme::kSelectedRow;

		context.uiManager.createElement(FSEL::kTextInput, LocalElementName{"search-input"})
			.setParameters(std::move(input))
			.setDevInternalCapture(true)
			.draw();
	}
}

} // namespace FlowUi::devSystems::interface_elements
#endif
