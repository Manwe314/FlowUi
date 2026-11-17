#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <FSEL/Button.hpp>
#include <FSEL/TextInput.hpp>
#include <FlowUi/Public/Flow.hpp>

namespace {

bool checkStatus(const FlowUi::Status& status) {
	if (status)
		return true;
	std::cerr << "FlowUi example failed: " << FlowUi::errorName(status.error().code) << '\n';
	return false;
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

template <FlowUi::FlowElement Element>
void drawElement(FlowUi::UiManager& ui, const Element& element,
				 FlowUi::LocalElementName name, FlowUi::ParametersOf<Element> parameters) {
	ui.createElement(element, name).setParameters(std::move(parameters)).draw();
}

void drawMainWindow(FlowUi::UiManager& ui, DemoState& state) {
	const auto& actions = ui.actions();
	const Clay_TextElementConfig heading = textStyle(24);
	const Clay_TextElementConfig helper = textStyle(14, FlowUi::Flow_Color("#aeb8ccff"));

	const auto pageId = ui.toClaySID("example/main/page");
	const auto headingText = ui.toClayString("Multi-window text demo");
	const auto statusText = ui.toClayString(state.status);
	if (!pageId || !headingText || !statusText)
		return;
	CLAY(*pageId, pageStyle()) {
		CLAY_TEXT(*headingText, CLAY_TEXT_CONFIG(heading));

		drawElement(ui, FlowUi::FSEL::kTextInput, "example/main/text-input", FlowUi::FSEL::TextInputParameters{
				.value = &state.text,
				.maxBytes = 4096,
				.idleOverrides = {.backgroundColor = FlowUi::Flow_Color("#f3f5f8ff")},
				.sizing =
					Clay_Sizing{
						.width = CLAY_SIZING_GROW(0),
						.height = CLAY_SIZING_FIT(0),
					},
			});

		drawElement(ui, FlowUi::FSEL::kButton, "example/main/open-window", FlowUi::FSEL::ButtonParameters{
				.onActivate =
					FlowUi::ActionCall{actions.uiActions().make(request_text_window, state)},
				.text = "Open text window",
				.idleOverrides =
					{
						.backgroundColor = FlowUi::Flow_Color("#78a9ffff"),
						.labelColor = FlowUi::Flow_Color("#101522ff"),
						.borderColor = FlowUi::Flow_Color("#a9c7ffff"),
					},
				.contentMode = FlowUi::FSEL::ButtonContentMode::TextOnly,
			});

		CLAY_TEXT(*statusText, CLAY_TEXT_CONFIG(helper));
	}
}

void drawTextWindow(FlowUi::UiManager& ui, const DemoState& state) {
	const Clay_TextElementConfig heading = textStyle(18, FlowUi::Flow_Color("#aeb8ccff"));
	const Clay_TextElementConfig content = textStyle(28);
	const std::string_view visibleText = state.text.empty()
		? std::string_view("(the input is empty)")
		: std::string_view(state.text);

	const auto pageId = ui.toClaySID("example/text-window/page");
	const auto headingText = ui.toClayString("Text from the main window:");
	const auto bodyText = ui.toClayString(visibleText);
	if (!pageId || !headingText || !bodyText)
		return;
	CLAY(*pageId, pageStyle()) {
		CLAY_TEXT(*headingText, CLAY_TEXT_CONFIG(heading));
		CLAY_TEXT(*bodyText, CLAY_TEXT_CONFIG(content));
	}
}

FlowUi::Status closeRequestedTextWindows(FlowUi::App& app, DemoState& state) {
	for (size_t index = 0; index < state.textWindows.size();) {
		const FlowUi::WindowId window = state.textWindows[index];
		const auto closed = app.shouldClose(window);
		if (!closed)
			return FlowUi::unexpectedError(closed.error());
		if (!*closed) {
			++index;
			continue;
		}
		const auto destroyed = app.destroyWindow(window);
		if (!destroyed)
			return destroyed;
		state.textWindows.erase(state.textWindows.begin() + static_cast<std::ptrdiff_t>(index));
	}
	return {};
}

void createRequestedTextWindow(FlowUi::App& app, DemoState& state) {
	if (!state.createWindowRequested) return;
	state.createWindowRequested = false;

	FlowUi::WindowConfig config{};
	config.width = 560;
	config.height = 260;
	config.title = "FlowUi text window " + std::to_string(state.nextWindowNumber++);

	auto created = app.createWindow(config);
	if (!created) {
		state.status = std::string("Could not create the window: ") +
			std::string(FlowUi::errorName(created.error().code));
		return;
	}
	state.textWindows.push_back(*created);
	state.status = "Opened " + config.title + ".";
}

} // namespace

int main() {
	FlowUi::AppConfig config{};
	config.window.title = "FlowUi multi-window example";
	config.window.width = 680;
	config.window.height = 360;
	config.ui.fontAtlasSize = 512;

	auto createdApp = FlowUi::makeApplication(config);
	if (!createdApp) {
		std::cerr << "Could not initialize FlowUi: " << FlowUi::errorName(createdApp.error().code) << '\n';
		return 1;
	}
	FlowUi::App app = std::move(*createdApp);
	DemoState state{};

	while (true) {
		if (!checkStatus(app.pollEvents()))
			return 1;
		const auto closed = app.shouldClose();
		if (!closed) {
			app.reportError(closed.error());
			return 1;
		}
		if (*closed)
			break;
		if (!checkStatus(closeRequestedTextWindows(app, state)))
			return 1;
		if (!checkStatus(app.runFrame(app.mainWindowId(),
			[&](FlowUi::UiManager& ui, FlowUi::WindowId) { drawMainWindow(ui, state); })))
			return 1;

		createRequestedTextWindow(app, state);
		for (const FlowUi::WindowId window : state.textWindows) {
			if (!checkStatus(app.runFrame(window,
				[&](FlowUi::UiManager& ui, FlowUi::WindowId) { drawTextWindow(ui, state); })))
				return 1;
		}
	}

	for (const FlowUi::WindowId window : state.textWindows) {
		if (app.hasWindow(window) && !checkStatus(app.destroyWindow(window)))
			return 1;
	}
	return 0;
}
