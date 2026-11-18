#include "../../TestHarness.hpp"
#include "FlowUi.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "RecordingStorageSystem.hpp"

#include <cstdlib>
#include <iostream>

int main() {
	using namespace FlowUi;
	using Provider = test::RecordingStorageSystem;
	if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
		std::cout << "SKIP: no native display.\n";
		return 77;
	}
	AppConfig configuration;
	configuration.window.width = 320;
	configuration.window.height = 240;
	configuration.vk.enableValidation = true;
	configuration.resources.use_build_directory = false;
	unsigned actionInvocations = 0;
	{
		auto application = test::requireValue(makeApplication(configuration));
		auto& actions = test::requireValue(application.actions()).get();
		auto& themes = test::requireValue(application.themes()).get();
		FLOWUI_CHECK(themes.registerTheme<FlowUiTheme>("recording", FlowUiTheme::dark()));
		FLOWUI_CHECK(themes.getTheme<FlowUiTheme>("recording"));
		const auto action = test::requireValue(actions.appActions().bind(
			AppAction("storage.recording"), [](unsigned* count) { ++*count; }, &actionInvocations));
		FLOWUI_CHECK(actions.invoke(action) == ActionInvocationStatus::Invoked);
		const auto secondary = test::requireValue(application.createWindowLikeMain());
		for (unsigned frameIndex = 0; frameIndex < 4; ++frameIndex) {
			FLOWUI_CHECK(application.pollEvents());
			for (const auto window : {application.mainWindowId(), secondary}) {
				FLOWUI_CHECK(application.beginFrame(window));
				FLOWUI_CHECK(application.endFrame(window));
				FLOWUI_CHECK(application.drawFrame(window));
			}
		}
		FLOWUI_CHECK(application.destroyWindow(secondary));
	}
	const auto& calls = Provider::calls;
	FLOWUI_CHECK(actionInvocations == 1);
	FLOWUI_CHECK(calls.constructions == 1 && calls.destructions == 1);
	FLOWUI_CHECK(test::recordingStorageConstructionCount() == 1);
	FLOWUI_CHECK(calls.initialize == 1 && calls.shutdown >= 1);
	FLOWUI_CHECK(calls.createManagerRecord > 0 && calls.managerRecordData > 0);
	FLOWUI_CHECK(calls.createPersistentRecord > 0 && calls.persistentRecord > 0);
	FLOWUI_CHECK(calls.createBuffer > 0 && calls.createImage > 0);
	FLOWUI_CHECK(calls.publishTexture > 0 && calls.prepareTextureBindings > 0);
	FLOWUI_CHECK(calls.beginFrame >= 8 && calls.sealFrame >= 8);
	FLOWUI_CHECK(calls.noteSubmission > 0 && calls.noteCompleted > 0);
	FLOWUI_CHECK(calls.registerWindow == 2 && calls.unregisterWindow == 2);
	FLOWUI_CHECK(calls.collect > 0);
	std::cout << "Custom storage handled App initialization, action/theme records, GPU resources, "
				 "bindings, "
				 "multi-window frames, submission/completion and teardown.\n";
	return 0;
}
