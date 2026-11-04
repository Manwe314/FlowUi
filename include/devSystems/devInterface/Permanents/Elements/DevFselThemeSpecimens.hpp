#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/Button.hpp"
#include "FSEL/Checkbox.hpp"
#include "FSEL/Label.hpp"
#include "FSEL/ProgressBar.hpp"
#include "FSEL/Slider.hpp"
#include "FSEL/Switch.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Owned backing data shared by the theme editor and Catalogue specimen provider. */
struct FselThemeSpecimenState {
	std::string sample_text = "The quick brown fox";
	uint64_t preview_layout = 0;
	double sample_value = .42;
	bool sample_checked = true, sample_switch = false;
};
inline constexpr auto kFselThemeToggleSample =
	UiAction("flowui.dev_interface.theme.toggle-sample", [](bool& value) { value = !value; });
/** Reusable built-in FSEL component scene; callers choose passive or interactive scope. */
template <class Context, class SpecimenState>
void draw_fsel_theme_specimens(Context& context, App& app, SpecimenState& sandbox) {
	using diagnostics_ui::row;

	if (sandbox.preview_layout < 2 || sandbox.preview_layout == 2 || sandbox.preview_layout == 3)
		CLAY(context.clayID("buttons"), row()) {
			for (uint32_t specimen = 0; specimen < 2; ++specimen) {
				if (sandbox.preview_layout >= 2 && sandbox.preview_layout != specimen + 2)
					continue;
				FSEL::ButtonParameters parameters{};
				parameters.text = specimen ? "Disabled" : "Interactive button";
				parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
				parameters.enabled = specimen == 0;
				parameters.onActivate = ActionCall{
					app.actions().uiActions().make(kFselThemeToggleSample, sandbox.sample_checked)};
				context.uiManager
					.createElement(FSEL::kButton,
								   RuntimeName("sample-button-" + std::to_string(specimen)))
					.setParameters(parameters)
					.setDevInternalCapture(true)
					.draw();
			}
		}
	if (sandbox.preview_layout < 2 || sandbox.preview_layout == 4 || sandbox.preview_layout == 5)
		CLAY(context.clayID("toggles"), row()) {
			if (sandbox.preview_layout < 2 || sandbox.preview_layout == 4) {
				FSEL::CheckboxParameters checkbox{};
				checkbox.isChecked = sandbox.sample_checked;
				checkbox.onToggle = ActionCall{
					app.actions().uiActions().make(kFselThemeToggleSample, sandbox.sample_checked)};
				context.uiManager.createElement(FSEL::kCheckbox, "sample-checkbox")
					.setParameters(checkbox)
					.setDevInternalCapture(true)
					.draw();
			}
			if (sandbox.preview_layout < 2 || sandbox.preview_layout == 5) {
				FSEL::SwitchParameters toggle{};
				toggle.isOn = sandbox.sample_switch;
				toggle.onToggle = ActionCall{
					app.actions().uiActions().make(kFselThemeToggleSample, sandbox.sample_switch)};
				context.uiManager.createElement(FSEL::kSwitch, "sample-switch")
					.setParameters(toggle)
					.setDevInternalCapture(true)
					.draw();
			}
		}
	if (sandbox.preview_layout < 2 || sandbox.preview_layout == 6) {
		FSEL::SliderParameters slider{};
		slider.value = &sandbox.sample_value;
		slider.length = 260;
		context.uiManager.createElement(FSEL::kSlider, "sample-slider")
			.setParameters(slider)
			.setDevInternalCapture(true)
			.draw();
	}
	if (sandbox.preview_layout < 2 || sandbox.preview_layout == 7) {
		FSEL::ProgressBarParameters progress{};
		progress.value = sandbox.sample_value;
		progress.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(8)};
		context.uiManager.createElement(FSEL::kProgressBar, "sample-progress")
			.setParameters(progress)
			.setDevInternalCapture(true)
			.draw();
	}
	if (sandbox.preview_layout < 2 || sandbox.preview_layout == 8) {
		FSEL::TextInputParameters input{};
		input.enabled = !context.uiManager.isDevPassivePreview();
		input.fontSize = 12;
		input.padding = Clay_Padding{8, 8, 4, 4};
		input.value = &sandbox.sample_text;
		input.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34)};
		context.uiManager.createElement(FSEL::kTextInput, "sample-input")
			.setParameters(input)
			.setDevInternalCapture(true)
			.draw();
	}
	if (sandbox.preview_layout < 2 || sandbox.preview_layout == 9) {
		FSEL::LabelParameters label{};
		label.text = sandbox.sample_text;
		context.uiManager.createElement(FSEL::kLabel, "sample-label")
			.setParameters(label)
			.setDevInternalCapture(true)
			.draw();
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
