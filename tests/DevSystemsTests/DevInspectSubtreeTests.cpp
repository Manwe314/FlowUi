#include <cstdlib>
#include <iostream>

#include <FlowUi/Flow.hpp>
#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"

namespace {
template <typename Value>
void require(const Value& condition, const char* message) {
	if (condition) return;
	std::cerr << message << '\n';
	std::exit(1);
}

// Like Demo, this Flow element delegates layout without emitting its own Clay ID.
struct DelegatingRoot {
	using BuildContext = FlowUi::ElementBuildContext<DelegatingRoot>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests.inspect.delegating_root");
	static void buildElement(BuildContext& context) {
		Clay_ElementDeclaration child{};
		child.layout.sizing = {CLAY_SIZING_FIXED(100), CLAY_SIZING_FIXED(100)};
		CLAY(context.clayID("child"), child);
	}
};
inline constexpr DelegatingRoot delegating_root{};
}

int main() {
	FlowUi::AppConfig config{};
	config.window.title = "Inspector subtree regression";
	config.window.width = 320;
	config.window.height = 240;
	config.vk.enableValidation = false;
	auto app = FlowUi::makeApplication(config);
	auto& ui = app.ui();
	require(app.beginFrame(), "Initial frame must begin");
	ui.createElement(delegating_root, "root").draw();
	require(app.endFrame(), "Initial frame must end");
	require(app.drawFrame(), "Initial frame must render");
	const auto& snapshot = ui.devTreeSnapshot();
	const auto selected = snapshot.flow.nodes.front().instance;
	require(snapshot.flow.nodes.front().clayRoot ==
		FlowUi::devSystems::tooling::InvalidClayNode, "Root must lack an exact Clay root");
	FlowUi::devSystems::DevInterfaceState state{};
	state.selectedWindowId = app.mainWindowId();
	require(app.beginFrame(), "Inspector frame must begin");
	for (const bool select_root : {false, true}) {
		state.selectedElementId = select_root ? FlowUi::FlowElementID{.value = selected.value} : FlowUi::FlowElementID{};
		const auto parent_id = Clay_GetOpenElementId();
		ui.createElement(FlowUi::devSystems::interface_elements::kDevClaySubTree,
			FlowUi::Indexed("subtree", select_root ? 1 : 0))
			.setParameters({.app = &app, .interfaceState = &state}).draw();
		require(Clay_GetOpenElementId() == parent_id,
			"Subtree fallback must close its Clay container before returning");
	}
	require(app.endFrame(), "Inspector frame must finish without crashing");
	require(app.drawFrame(), "Inspector frame must render");
}
