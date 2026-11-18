#include "FlowUi.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include <iostream>
#include <utility>

/** Instantiate public record templates without the library's private provider header. */
[[nodiscard]] bool exerciseTemplates(FlowUi::ActionManager& actions, FlowUi::ThemeManager& themes) {
	const auto action = actions.appActions().bind(FlowUi::AppAction("storage.installed"), [] {});
	if (!action ||
		!themes.registerTheme<FlowUi::FlowUiTheme>("installed", FlowUi::FlowUiTheme::dark()))
		return false;
	return themes.getTheme<FlowUi::FlowUiTheme>("installed") &&
		   themes.getActiveTheme<FlowUi::FlowUiTheme>() &&
		   actions.invoke(*action) == FlowUi::ActionInvocationStatus::Invoked;
}

int main(int argumentCount, char**) {
	FlowUi::App application;
	if (argumentCount > 1) {
		FlowUi::AppConfig configuration;
		configuration.window.width = 320;
		configuration.window.height = 240;
		configuration.vk.enableValidation = true;
		configuration.resources.use_build_directory = false;
		auto created = FlowUi::makeApplication(configuration);
		if (!created)
			return 1;
		application = std::move(*created);
		auto actions = application.actions();
		auto themes = application.themes();
		if (!actions || !themes || !exerciseTemplates(actions->get(), themes->get()))
			return 2;
		for (unsigned frameIndex = 0; frameIndex < 3; ++frameIndex) {
			if (!application.beginFrame() || !application.endFrame() || !application.drawFrame())
				return 3;
		}
	}
	std::cout << "Installed consumer linked public action/theme templates without private storage "
				 "header.\n";
	return 0;
}
