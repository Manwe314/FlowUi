#include "FlowUi/Public/Flow.hpp"
#include "FlowUi/Public/input/Key.hpp"
#include "TestHarness.hpp"
#include <FSEL/Button.hpp>
#include <cstdint>
#include <exception>
#include <iostream>

namespace {
void translatedPointerActivatesButton() {
	FlowUi::AppConfig configuration{};
	configuration.window = {.width = 320, .height = 200, .title = "FlowUi translator integration"};
	configuration.vk.enableValidation = false;
	auto application = FlowUi::makeApplication(configuration);
	std::uint64_t activations = 0;
	for (std::size_t tick = 0; tick < 14; ++tick) {
		FLOWUI_CHECK(application.pollEvents());
		FLOWUI_CHECK(application.beginFrame(application.mainWindowId()));
		auto& ui = application.ui();
		FlowUi::FSEL::ButtonParameters parameters{};
		parameters.sizing = {.width = CLAY_SIZING_FIXED(100), .height = CLAY_SIZING_FIXED(60)};
		parameters.onActivate = FlowUi::ActionCall{
			ui.actions().uiActions().make(FlowUi::UiAction("test.translated-footpad.activate",
														   [](std::uint64_t& count) { ++count; }),
										  activations)};
		ui.createElement(FlowUi::FSEL::kButton, "test/translated-footpad/button")
			.setParameters(parameters)
			.draw();
		FLOWUI_CHECK(application.endFrame());
		FLOWUI_CHECK(application.drawFrame());
	}
	FLOWUI_CHECK(activations == 1);
	FLOWUI_CHECK(!application.ui().getCurrentFrameInput().mouseDown[0]);
}
} // namespace

int main() {
	try {
		FlowUi::test::Runner runner;
		runner.run("selected additional footpad translator activates existing FSEL button and "
				   "cancels unplug",
				   translatedPointerActivatesButton);
		return runner.finish();
	} catch (const FlowUi::FlowUiException& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
