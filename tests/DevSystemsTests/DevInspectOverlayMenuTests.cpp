#include <cstdlib>
#include <iostream>

#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevInspectOverlayMenu.hpp"
#include "devSystems/devTooling/DevTooling.hpp"

namespace {
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;

constexpr auto menu_id = Global<kDevInspectOverlayMenu>("tests.inspect.overlay-menu");

template <typename Value>
void require(const Value& value, const char* message) {
	if (value)
		return;
	std::cerr << message << '\n';
	std::exit(1);
}

enum class PointerEvent { None, Press, Release };

/** Exercise real retained FSEL button callbacks without desktop focus/timing dependencies. */
[[nodiscard]] bool draw_frame(App& app, FlowElementID control, PointerEvent event) {
	require(app.beginFrame(), "Menu frame must begin");
	auto& ui = app.ui();
	// These are mutable manager-owned test inputs exposed through const observer APIs.
	// Inject the completed interaction snapshot consumed by the normal builder hooks.
	auto& interaction = const_cast<InteractionSnapshot&>(ui.getPreviousFramesInteraction());
	interaction = {};
	auto& input = const_cast<FrameInput&>(ui.getCurrentFrameInput());
	input.mouseDown[0] = event == PointerEvent::Press;
	if (control) {
		const uint32_t clay_id = ui.toClayEID(control).id;
		interaction.hoveredElementIds.emplace_back(clay_id);
		if (event == PointerEvent::Press)
			interaction.pressedElementIds.emplace_back(clay_id);
		if (event == PointerEvent::Release)
			interaction.releasedElementIds.emplace_back(clay_id);
	}
	ui.createElement(kDevInspectOverlayMenu, menu_id).setParameters({.app = &app}).draw();
	require(app.endFrame(), "Menu frame must finish");
	const auto popup = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = menu_id.value}, DevInspectOverlayMenu::definitionId,
		LocalElementName{"popup"}.token);
	bool popup_present = false;
	for (const auto& node : ui.devTreeSnapshot().clay.nodes) {
		if (node.clayId == ui.toClayEID(popup).id && node.bounds.width > 0 &&
			node.bounds.height > 0)
			popup_present = true;
	}
	require(app.drawFrame(), "Menu frame must render");
	return popup_present;
}
} // namespace

int main() {
	AppConfig config{};
	config.window.title = "Inspect overlay menu regression";
	config.window.width = 480;
	config.window.height = 560;
	config.vk.enableValidation = false;
	auto app = makeApplication(config);
	const auto trigger = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = menu_id.value}, FSEL::Button::definitionId,
		LocalElementName{"trigger"}.token);
	require(!draw_frame(app, {}, PointerEvent::None), "Popup starts closed");
	require(!draw_frame(app, trigger, PointerEvent::Press), "Press alone must not open popup");
	require(draw_frame(app, trigger, PointerEvent::Release),
			"Releasing Overlay must open the popup in the same build");
	require(draw_frame(app, {}, PointerEvent::None), "Popup must remain open on the next frame");

	const auto checkbox = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = menu_id.value}, FSEL::Checkbox::definitionId,
		Keyed("surface-check", 0).token);
	require(draw_frame(app, checkbox, PointerEvent::Press), "Checkbox press keeps popup open");
	require(draw_frame(app, checkbox, PointerEvent::Release), "Checkbox toggle keeps popup open");
	require(!tooling::hasFlag(app.devTooling().inspect_interaction().mode_flags(),
							  tooling::DevOverlayModeFlags::BoxModel),
			"Checkbox must change authoritative overlay flags");

	const auto flow_btn = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = menu_id.value}, FSEL::Button::definitionId,
		LocalElementName{"pick-domain-flow"}.token);
	const auto clay_btn = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = menu_id.value}, FSEL::Button::definitionId,
		LocalElementName{"pick-domain-clay"}.token);

	require(app.devTooling().inspect_interaction().pick_domain() ==
				tooling::DevInspectPickDomain::Flow,
			"Initial domain must be Flow");
	require(draw_frame(app, clay_btn, PointerEvent::Press), "Clay domain press keeps popup open");
	require(draw_frame(app, clay_btn, PointerEvent::Release), "Clay domain release keeps popup open");
	require(app.devTooling().inspect_interaction().pick_domain() ==
				tooling::DevInspectPickDomain::Clay,
			"Activating Clay domain button must switch pick domain to Clay");

	require(draw_frame(app, flow_btn, PointerEvent::Press), "Flow domain press keeps popup open");
	require(draw_frame(app, flow_btn, PointerEvent::Release), "Flow domain release keeps popup open");
	require(app.devTooling().inspect_interaction().pick_domain() ==
				tooling::DevInspectPickDomain::Flow,
			"Activating Flow domain button must switch pick domain to Flow");

	require(draw_frame(app, trigger, PointerEvent::Press), "Trigger press preserves open popup");
	require(!draw_frame(app, trigger, PointerEvent::Release), "Trigger release closes popup");
	require(!draw_frame(app, trigger, PointerEvent::Press),
			"Closed popup stays closed until release");
	require(draw_frame(app, trigger, PointerEvent::Release), "Popup can reopen after closing");
	require(draw_frame(app, {}, PointerEvent::None), "Reopened popup remains visible");
	std::cout << "Overlay menu open, toggle, close, and reopen checks passed\n";
}
