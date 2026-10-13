#pragma once
#include "FSEL/ComboBox.hpp"
#include "FSEL/NumberInput.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchControls.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::interface_elements::memory_ui {
inline constexpr FSEL::NumberInput<double, DefinitionID("flowui.dev.memory.number.double")>
	kDoubleInput{};
inline constexpr FSEL::NumberInput<uint64_t, DefinitionID("flowui.dev.memory.number.capacity")>
	kCapacityInput{};
[[nodiscard]] inline Clay_ElementDeclaration column(uint16_t gap = 6) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.layout.childGap = gap;
	return declaration;
}
[[nodiscard]] inline Clay_ElementDeclaration row(float height = 28) noexcept {
	auto declaration = column();
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	declaration.layout.sizing.height = CLAY_SIZING_FIXED(height);
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	return declaration;
}
template <class Context>
void text(Context& context, std::string_view label,
		  Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 11) {
	Clay_TextElementConfig config{};
	config.fontSize = size;
	config.textColor = color;
	config.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(config));
}
template <class Context>
void button(Context& context, DevMemoryWorkbenchState& state, uint32_t action, std::string label,
			bool enabled = true) {
	context.uiManager.createElement(kDevMemoryControl, Keyed("memory-action", action))
		.setParameters(
			{.state = &state, .label = std::move(label), .action = action, .enabled = enabled})
		.setDevInternalCapture(true)
		.draw();
}
template <class Context>
void choice(Context& context, LocalElementName identity, std::string_view label, uint64_t& value,
			std::span<const FSEL::ComboBoxOption> options) {
	text(context, label, interface_theme::kTextMuted, 10);
	FSEL::ComboBoxParameters parameters{};
	parameters.options = options;
	parameters.selectedValue = &value;
	parameters.fontSize = 11;
	parameters.sizing =
		Clay_Sizing{.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_FIXED(28)};
	context.uiManager.createElement(FSEL::kComboBox, identity)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}
template <class Context, class Number>
void number(Context& context, LocalElementName identity, std::string_view label, Number& value,
			Number minimum, Number maximum,
			std::optional<int16_t> floating_z_index = std::nullopt) {
	text(context, label, interface_theme::kTextMuted, 10);
	FSEL::NumberInputParameters<Number> parameters{};
	parameters.value = &value;
	parameters.minimum = minimum;
	parameters.maximum = maximum;
	parameters.sizing =
		Clay_Sizing{.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_FIXED(28)};
	parameters.fontSize = 11;
	parameters.floatingZIndex = floating_z_index;
	parameters.padding = Clay_Padding{8, 8, 4, 4};
	parameters.centerText = true;
	parameters.stepButtons = FSEL::NumberInputStepButtons::TrailingVertical;
	parameters.stepIconSize = 10;
	if constexpr (std::same_as<Number, double>) {
		parameters.step = .1;
		context.uiManager.createElement(kDoubleInput, identity)
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
	} else
		context.uiManager.createElement(kCapacityInput, identity)
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
}
} // namespace FlowUi::devSystems::interface_elements::memory_ui
#endif
