#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <FSEL/Button.hpp>
#include <FSEL/TextInput.hpp>
#include <FlowUi/Public/Flow.hpp>

namespace {

void requireStatus(FlowUi::Status status) {
	if (!status) throw FlowUi::FlowUiException(status.error());
}

struct DemoState {
	std::string text = "Hello from the main window";
	std::string status = "Type some text, then open a window.";
	std::vector<FlowUi::WindowId> textWindows{};
	uint64_t nextWindowNumber = 1;
	bool createWindowRequested = false;
};

inline constexpr auto request_text_window = FlowUi::UiAction(
	"example.open-text-window", [](DemoState& state) { state.createWindowRequested = true; });

Clay_TextElementConfig textStyle(
	uint16_t size,
	Clay_Color color = FlowUi::Flow_Color("#e8edf7ff")) {
	Clay_TextElementConfig style{};
	style.textColor = color;
	style.fontSize = size;
	style.wrapMode = CLAY_TEXT_WRAP_WORDS;
	style.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	style.fontId = 0;
	return style;
}

Clay_ElementDeclaration pageStyle() {
	Clay_ElementDeclaration page{};
	page.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	page.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	page.layout.padding = CLAY_PADDING_ALL(24);
	page.layout.childGap = 16;
	page.backgroundColor = FlowUi::Flow_Color("#171b24ff");
	return page;
}

void drawMainWindow(FlowUi::App& app, DemoState& state) {
	FlowUi::UiManager& ui = app.ui(app.mainWindowId());
	const Clay_TextElementConfig heading = textStyle(24);
	const Clay_TextElementConfig helper = textStyle(14, FlowUi::Flow_Color("#aeb8ccff"));

	CLAY(ui.toClaySID("example/main/page"), pageStyle()) {
		CLAY_TEXT(ui.toClayString("Multi-window text demo"), CLAY_TEXT_CONFIG(heading));

		ui.createElement(FlowUi::FSEL::kTextInput, "example/main/text-input")
			.setParameters(FlowUi::FSEL::TextInputParameters{
				.value = &state.text,
				.maxBytes = 4096,
				.idleOverrides = {.backgroundColor = FlowUi::Flow_Color("#f3f5f8ff")},
				.sizing =
					Clay_Sizing{
						.width = CLAY_SIZING_GROW(0),
						.height = CLAY_SIZING_FIT(0),
					},
			})
			.draw();

		ui.createElement(FlowUi::FSEL::kButton, "example/main/open-window")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate =
					FlowUi::ActionCall{ui.actions().uiActions().make(request_text_window, state)},
				.text = "Open text window",
				.idleOverrides =
					{
						.backgroundColor = FlowUi::Flow_Color("#78a9ffff"),
						.labelColor = FlowUi::Flow_Color("#101522ff"),
						.borderColor = FlowUi::Flow_Color("#a9c7ffff"),
					},
				.contentMode = FlowUi::FSEL::ButtonContentMode::TextOnly,
			})
			.draw();

		CLAY_TEXT(ui.toClayString(state.status), CLAY_TEXT_CONFIG(helper));
	}
}

void drawTextWindow(FlowUi::App& app, FlowUi::WindowId window, const DemoState& state) {
	FlowUi::UiManager& ui = app.ui(window);
	const Clay_TextElementConfig heading = textStyle(18, FlowUi::Flow_Color("#aeb8ccff"));
	const Clay_TextElementConfig content = textStyle(28);
	const std::string_view visibleText = state.text.empty()
		? std::string_view("(the input is empty)")
		: std::string_view(state.text);

	CLAY(ui.toClaySID("example/text-window/page"), pageStyle()) {
		CLAY_TEXT(ui.toClayString("Text from the main window:"), CLAY_TEXT_CONFIG(heading));
		CLAY_TEXT(ui.toClayString(visibleText), CLAY_TEXT_CONFIG(content));
	}
}

void closeRequestedTextWindows(FlowUi::App& app, DemoState& state) {
	for (size_t index = 0; index < state.textWindows.size();) {
		const FlowUi::WindowId window = state.textWindows[index];
		if (!app.shouldClose(window)) {
			++index;
			continue;
		}
		requireStatus(app.destroyWindow(window));
		state.textWindows.erase(state.textWindows.begin() + static_cast<std::ptrdiff_t>(index));
	}
}

void createRequestedTextWindow(FlowUi::App& app, DemoState& state) {
	if (!state.createWindowRequested) return;
	state.createWindowRequested = false;

	FlowUi::WindowConfig config{};
	config.width = 560;
	config.height = 260;
	config.title = "FlowUi text window " + std::to_string(state.nextWindowNumber++);

	try {
		auto created = app.createWindow(config);
		if (!created) {
			state.status = std::string("Could not create the window: ") +
				std::string(FlowUi::errorName(created.error().code));
			return;
		}
		const FlowUi::WindowId window = created.value();
		state.textWindows.push_back(window);
		state.status = "Opened " + config.title + ".";
	} catch (const std::exception& error) {
		state.status = std::string("Could not create the window: ") + error.what();
	}
}

} // namespace

int main() {
	try {
		FlowUi::AppConfig config{};
		config.window.title = "FlowUi multi-window example";
		config.window.width = 680;
		config.window.height = 360;
		config.ui.fontAtlasSize = 512;

		FlowUi::App app = FlowUi::makeApplication(config);
		DemoState state{};

		while (true) {
			requireStatus(app.pollEvents());
			if (app.shouldClose(app.mainWindowId())) break;

			closeRequestedTextWindows(app, state);

			requireStatus(app.beginFrame(app.mainWindowId()));
			drawMainWindow(app, state);
			requireStatus(app.endFrame(app.mainWindowId()));
			requireStatus(app.drawFrame(app.mainWindowId()));

			createRequestedTextWindow(app, state);

			for (const FlowUi::WindowId window : state.textWindows) {
				requireStatus(app.beginFrame(window));
				drawTextWindow(app, window, state);
				requireStatus(app.endFrame(window));
				requireStatus(app.drawFrame(window));
			}
		}

		for (const FlowUi::WindowId window : state.textWindows) {
			if (app.hasWindow(window)) requireStatus(app.destroyWindow(window));
		}
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "FlowUi example failed: " << error.what() << '\n';
		return 1;
	}
}
