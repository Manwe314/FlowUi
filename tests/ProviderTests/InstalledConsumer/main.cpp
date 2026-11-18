#include "FlowUi.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/IconManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

/** Compile ordinary application templates with no selected-provider declarations available. */
[[nodiscard]] bool exerciseTemplates(FlowUi::ActionManager& actions, FlowUi::ThemeManager& themes,
									 unsigned& invocationCount) {
	const auto action = actions.appActions().bind(
		FlowUi::AppAction("providers.installed"), [](unsigned* count) { ++*count; },
		&invocationCount);
	if (!action ||
		!themes.registerTheme<FlowUi::FlowUiTheme>("installed", FlowUi::FlowUiTheme::dark()))
		return false;
	return themes.getTheme<FlowUi::FlowUiTheme>("installed") &&
		   themes.getActiveTheme<FlowUi::FlowUiTheme>() &&
		   actions.invoke(*action) == FlowUi::ActionInvocationStatus::Invoked &&
		   invocationCount == 1;
}

int main(int argumentCount, char** arguments) {
	bool live = false;
	std::string_view fontPath;
	for (int argumentIndex = 1; argumentIndex < argumentCount; ++argumentIndex) {
		const std::string_view argument{arguments[argumentIndex]};
		if (argument == "--live")
			live = true;
		else if (argument == "--font" && argumentIndex + 1 < argumentCount)
			fontPath = arguments[++argumentIndex];
		else
			return 1;
	}
	if (!live) {
		FlowUi::App empty;
		std::cout
			<< "Installed public action/theme templates linked without private provider headers.\n";
		return 0;
	}
	std::vector<std::byte> bakedFont;
	if (!fontPath.empty()) {
		std::ifstream input(fontPath.data(), std::ios::binary | std::ios::ate);
		if (!input || input.tellg() <= 0)
			return 2;
		bakedFont.resize(static_cast<size_t>(input.tellg()));
		input.seekg(0);
		input.read(reinterpret_cast<char*>(bakedFont.data()),
				   static_cast<std::streamsize>(bakedFont.size()));
		if (!input.good())
			return 2;
	}
	FlowUi::AppConfig configuration;
	configuration.window.width = 400;
	configuration.window.height = 260;
	configuration.resources.use_build_directory = false;
	if (!bakedFont.empty()) {
		configuration.ui.defaultFontFamily.name = "Installed provider font";
		configuration.ui.defaultFontFamily.faces = {
			FlowUi::FontFaceCreateInfo{.baked_data = bakedFont}};
	}
	unsigned invocationCount = 0;
	auto created = FlowUi::makeApplication(configuration);
	if (!created) {
		std::cerr << "Application initialization failed: "
				  << FlowUi::errorName(created.error().code) << " at "
				  << FlowUi::errorSiteName(created.error().site) << '\n';
		return 3;
	}
	auto application = std::move(*created);
	auto actions = application.actions();
	auto themes = application.themes();
	if (!actions || !themes || !exerciseTemplates(actions->get(), themes->get(), invocationCount))
		return 4;
	FlowUi::TextureRef icon;
#if FLOWUI_INCLUDE_ICON_MANAGER
	auto icons = application.icons();
	if (!icons)
		return 5;
	const auto registered = icons->get().registerSvg(
		"installed.svg",
		R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="#1177dd"/></svg>)");
	if (!registered || !*registered)
		return 5;
	auto reference = icons->get().textureRef("installed.svg");
	if (!reference)
		return 5;
	icon = *reference;
#endif
	for (unsigned frameIndex = 0; frameIndex < 3; ++frameIndex) {
		if (!application.pollEvents() || !application.beginFrame())
			return 6;
		auto ui = application.ui();
		if (!ui)
			return 6;
		CLAY(CLAY_ID("Installed provider surface"),
			 {.layout = {
				  .sizing = {.width = CLAY_SIZING_FIXED(360), .height = CLAY_SIZING_FIXED(220)},
				  .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
			CLAY_TEXT(
				CLAY_STRING("Installed providers AV"),
				CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = 0, .fontSize = 18}));
#if FLOWUI_INCLUDE_ICON_MANAGER
			CLAY(CLAY_ID("Installed provider icon"),
				 {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(64),
										.height = CLAY_SIZING_FIXED(32)}},
				  .image = {.imageData = ui->get().imageData(icon)}}) {}
#endif
		}
		if (!application.endFrame() || !application.drawFrame())
			return 6;
	}
	std::cout << "Relocated installed application exercised action/theme templates, text/icon "
				 "rendering and three Vulkan frames.\n";
	return 0;
}
