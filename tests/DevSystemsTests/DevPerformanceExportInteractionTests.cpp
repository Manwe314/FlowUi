#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/Button.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevContentHeader.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "internal/Resources/ExecutableDirectory.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
int main() {
	AppConfig config;
	config.window.title = "Performance CSV export interaction verification";
	config.window.width = 1200;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Performance);
	constexpr auto header_id = Global<kDevContentHeader>("export.test.header");
	const auto export_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = header_id.value}, FSEL::Button::definitionId,
		LocalElementName{"export-capture"}.token);
	const auto draw = [&](int pointer_event = 0) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseX = -10;
		input.mouseY = -10;
		if (pointer_event) {
			const auto clay_id = app.ui().toClayEID(export_id).id;
			interaction.hoveredElementIds.emplace_back(clay_id);
			if (pointer_event == 1) {
				interaction.pressedElementIds.emplace_back(clay_id);
				input.mouseDown[0] = true;
			} else
				interaction.releasedElementIds.emplace_back(clay_id);
		}
		Clay_ElementDeclaration root{};
		root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		CLAY(CLAY_ID("export-verification"), root) {
			app.ui()
				.createElement(kDevContentHeader, header_id)
				.setParameters({&app, &session})
				.draw();
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto has_label = [&](std::string_view label) {
		const auto& tree = app.ui().devTreeSnapshot();
		return std::ranges::any_of(
			tree.clay.nodes, [&](const auto& node) { return tree.string(node.text) == label; });
	};
	const auto click = [&] {
		draw(1);
		draw(2);
		draw();
	};
	draw();
	draw();
	assert(has_label("Export"));
	click();
	assert(session.performance_export_generation == 0 && session.performance_export_path.empty());
	auto& reporting = app.devMonitoring().timingReporting();
	assert(reporting.begin_capture(1) == 1);
	reporting.admit_tick(1, 100);
	reporting.note_tick_boundary(2, 200);
	reporting.stop_capture(2);
	reporting.seal_capture();
	session.performance_timeline.capture_generation = 1;
	draw();
	assert(has_label("Export"));
	click();
	assert(session.performance_export_generation == 1 && has_label("Exported"));
	const auto path = path_from_utf8(session.performance_export_path);
	assert(path.parent_path() == ::FlowUi::detail::executable_directory());
	assert(path.filename().string().starts_with("performance-capture-1-") &&
		   path.extension() == ".csv");
	std::ifstream exported(path, std::ios::binary);
	std::ostringstream contents;
	contents << exported.rdbuf();
	assert(contents.str().starts_with("\"schema_version\",\"record_kind\""));
	assert(contents.str().find("\"app_tick\"") != std::string::npos);
	assert(contents.str().find("\"1\",\"app_tick\",\"1\",\"1\"") != std::string::npos);
	const auto saved_path = session.performance_export_path;
	session.performance_selection.selected_scope = {99, DevPerformanceScopeKind::Window};
	session.performance_timeline.zoom = 10;
	draw();
	assert(has_label("Exported"));
	click();
	assert(session.performance_export_path == saved_path);
	assert(reporting.begin_capture(3) == 2);
	reporting.admit_tick(3, 300);
	reporting.note_tick_boundary(4, 400);
	reporting.stop_capture(4);
	reporting.seal_capture();
	session.performance_timeline.capture_generation = 2;
	draw();
	assert(has_label("Export") && !has_label("Exported"));
	click();
	assert(session.performance_export_generation == 2 && has_label("Exported"));
	assert(session.performance_export_path != saved_path);
	exported.close();
	std::filesystem::remove(path);
	std::filesystem::remove(path_from_utf8(session.performance_export_path));
}
