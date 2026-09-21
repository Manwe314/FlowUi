#include "FlowUi.hpp"
#include "FlowUi/Resources.hpp"
#include <iostream>
#if defined(_MSC_VER)
#include <crtdbg.h>
#include <cstdlib>
#endif

int main(int argument_count, char**) {
#if defined(_MSC_VER)
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
	FlowUi::ResourceConfig config;
	config.use_build_directory = false;
	const auto font = FlowUi::locate_resource("fonts/Inter.arfont", config);
	const auto shader = FlowUi::locate_resource("shaders/flowui_ui_solid.vert.spv", config);
	if (!font || !shader || !FlowUi::read_file_bytes(*font))
		return 1;
	// Exercise both function imports and Clay's public data import.
	FlowUi::App application;
	if (argument_count > 1) {
		FlowUi::AppConfig app_config;
		app_config.resources = config;
		if (argument_count > 2) {
			const auto runtime_font = FlowUi::locate_resource("fonts/runtime.ttf", config);
			if (!runtime_font) return 3;
			app_config.ui.defaultFontFamily.faces.front().path = *runtime_font;
		}
		app_config.window.width = 320;
		app_config.window.height = 240;
		app_config.vk.enableValidation = true;
		app_config.errors.policy.defaultFont = FlowUi::DefaultFontFailurePolicy::FailAppImmediately;
		application = FlowUi::makeApplication(app_config);
		for (unsigned frame = 0; frame < 4; ++frame) {
			if (!application.beginFrame() || !application.endFrame() || !application.drawFrame())
				return 2;
		}
	}
	const auto layout = CLAY_LAYOUT_DEFAULT;
	std::cout << FlowUi::path_to_utf8(*font) << '\n';
	return layout.childGap == 0u && Clay_MinMemorySize() > 0u ? 0 : 1;
}
