#include "TestHarness.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <FlowUi/Flow.hpp>

namespace {

using SeenIDs = std::vector<FlowUi::FlowElementID>;

struct ProbeParameters {
	SeenIDs* seen = nullptr;
	bool throwFromBuild = false;
};

struct ProbeElement {
	using Parameters = ProbeParameters;
	using BuildContext = FlowUi::ElementBuildContext<ProbeElement>;

	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/scope/probe");

	static void buildElement(BuildContext& context) {
		if (context.params.seen) context.params.seen->push_back(context.id);
		if (context.params.throwFromBuild) {
			throw std::runtime_error("intentional scope restoration probe");
		}
	}
};

inline constexpr ProbeElement kProbe{};
inline constexpr FlowUi::GlobalFlowID kGlobalProbe =
	FlowUi::Global<kProbe>("scope-probe");

struct NestedDrawParameters {
	SeenIDs* seen = nullptr;
};

struct NestedDrawElement {
	using Parameters = NestedDrawParameters;
	using BuildContext = FlowUi::ElementBuildContext<NestedDrawElement>;

	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/scope/nested-draw");

	static void buildElement(BuildContext& context) {
		if (context.params.seen) context.params.seen->push_back(context.id);
		context.uiManager.createElement(kProbe, "child")
			.setParameters(ProbeParameters{.seen = context.params.seen})
			.draw();
	}
};

inline constexpr NestedDrawElement kNestedDraw{};

struct RecursiveParameters {
	SeenIDs* seen = nullptr;
	uint32_t remainingDepth = 0;
};

struct RecursiveElement {
	using Parameters = RecursiveParameters;
	using BuildContext = FlowUi::ElementBuildContext<RecursiveElement>;

	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/scope/recursive");

	static void buildElement(BuildContext& context) {
		if (context.params.seen) context.params.seen->push_back(context.id);
		if (context.params.remainingDepth == 0) return;
		context.uiManager.createElement(RecursiveElement{}, "next")
			.setParameters(RecursiveParameters{
				.seen = context.params.seen,
				.remainingDepth = context.params.remainingDepth - 1,
			})
			.draw();
	}
};

inline constexpr RecursiveElement kRecursive{};

struct ConstructedParameters {
	SeenIDs* seen = nullptr;
};

struct ConstructedElement {
	using Parameters = ConstructedParameters;
	using BuildContext = FlowUi::ElementBuildContext<ConstructedElement>;

	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/scope/constructed");

	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		if (context.params.seen) context.params.seen->push_back(context.id);
		return {};
	}
};

inline constexpr ConstructedElement kConstructed{};

constexpr FlowUi::FlowElementID localID(
	FlowUi::FlowElementID parent,
	FlowUi::FlowDefinitionID definition,
	FlowUi::LocalElementName name) {
	return FlowUi::detail::element_id::resolveLocal(parent, definition, name.token);
}

constexpr FlowUi::FlowElementID localID(
	FlowUi::FlowElementID parent,
	FlowUi::FlowDefinitionID definition,
	FlowUi::IndexedElementName name) {
	return FlowUi::detail::element_id::resolveLocal(parent, definition, name.token);
}

bool environmentUnavailable(std::string_view message) {
	constexpr std::string_view reasons[] = {
		"Failed to initialize GLFW",
		"Failed to create GLFW window",
		"No Vulkan-capable GPU",
		"Failed to find a suitable Vulkan",
		"Missing required instance extension",
		"does not support descriptor indexing features",
		"does not support dynamic rendering",
	};
	for (const std::string_view reason : reasons) {
		if (message.find(reason) != std::string_view::npos) return true;
	}
	return false;
}

void finishFrame(FlowUi::App& app) {
	app.endFrame();
	app.drawFrame();
}

void testScopeLifecycle() {
	FlowUi::AppConfig config{};
	config.window.title = "FlowUi element scope lifecycle";
	config.window.width = 320;
	config.window.height = 240;
	config.vk.enableValidation = false;
	FlowUi::App app = FlowUi::makeApplication(config);
	FlowUi::UiManager& ui = app.ui();

	SeenIDs seen;
	app.beginFrame();

	ui.createElement(kNestedDraw, "parent")
		.setParameters(NestedDrawParameters{.seen = &seen})
		.draw();
	const FlowUi::FlowElementID nestedParent =
		localID(FlowUi::RootFlowScopeID, NestedDrawElement::definitionId, "parent");
	FLOWUI_CHECK(seen.size() == 2);
	FLOWUI_CHECK(seen[0] == nestedParent);
	FLOWUI_CHECK(seen[1] == localID(nestedParent, ProbeElement::definitionId, "child"));

	seen.clear();
	ui.createElement(kProbe)
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	ui.createElement(kProbe)
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	auto positionalIDs = FlowUi::IndexedIDs("row", 4);
	ui.createElement(kProbe, positionalIDs.next())
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	ui.createElement(kProbe, positionalIDs.next())
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	FLOWUI_CHECK(seen.size() == 4);
	FLOWUI_CHECK(seen[0] != seen[1]);
	FLOWUI_CHECK(seen[2] == localID(
		FlowUi::RootFlowScopeID,
		ProbeElement::definitionId,
		FlowUi::Indexed("row", 4)));
	FLOWUI_CHECK(seen[3] == localID(
		FlowUi::RootFlowScopeID,
		ProbeElement::definitionId,
		FlowUi::Indexed("row", 5)));

	seen.clear();
	ui.createElement(kRecursive, "recursive")
		.setParameters(RecursiveParameters{.seen = &seen, .remainingDepth = 2})
		.draw();
	const FlowUi::FlowElementID recursive0 =
		localID(FlowUi::RootFlowScopeID, RecursiveElement::definitionId, "recursive");
	const FlowUi::FlowElementID recursive1 =
		localID(recursive0, RecursiveElement::definitionId, "next");
	const FlowUi::FlowElementID recursive2 =
		localID(recursive1, RecursiveElement::definitionId, "next");
	FLOWUI_CHECK(seen.size() == 3);
	FLOWUI_CHECK(seen[0] == recursive0);
	FLOWUI_CHECK(seen[1] == recursive1);
	FLOWUI_CHECK(seen[2] == recursive2);

	seen.clear();
	FLOWUI_CHECK_THROWS(
		ui.createElement(kProbe, "throws")
			.setParameters(ProbeParameters{.seen = &seen, .throwFromBuild = true})
			.draw());
	ui.createElement(kProbe, "after-throw")
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	FLOWUI_CHECK(seen.size() == 2);
	FLOWUI_CHECK(seen[1] ==
		localID(FlowUi::RootFlowScopeID, ProbeElement::definitionId, "after-throw"));

	seen.clear();
	ui.createElement(kProbe, "skipped")
		.setParameters(ProbeParameters{.seen = &seen})
		.draw(FlowUi::ElementDrawOptions::SkipBuildCallback);
	ui.createElement(kProbe, "after-skip")
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	FLOWUI_CHECK(seen.size() == 1);
	FLOWUI_CHECK(seen[0] ==
		localID(FlowUi::RootFlowScopeID, ProbeElement::definitionId, "after-skip"));

	seen.clear();
	ui.createElement(kConstructed, "panel")
		.setParameters(ConstructedParameters{.seen = &seen})
		.construct();
	auto capturedChild = ui.createElement(kProbe, "temporary")
		.setParameters(ProbeParameters{.seen = &seen})
		.withID("captured-child");
	ui.createElement(kProbe, "direct-child")
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	ui.createElement(kProbe, kGlobalProbe)
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	ui.drawConstructed();
	capturedChild.draw();
	ui.createElement(kProbe, "top-level-after-construct")
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	const FlowUi::FlowElementID panel =
		localID(FlowUi::RootFlowScopeID, ConstructedElement::definitionId, "panel");
	FLOWUI_CHECK(seen.size() == 5);
	FLOWUI_CHECK(seen[0] == panel);
	FLOWUI_CHECK(seen[1] == localID(panel, ProbeElement::definitionId, "direct-child"));
	FLOWUI_CHECK(seen[2].value == kGlobalProbe.value);
	FLOWUI_CHECK(seen[3] == localID(panel, ProbeElement::definitionId, "captured-child"));
	FLOWUI_CHECK(seen[4] == localID(
		FlowUi::RootFlowScopeID,
		ProbeElement::definitionId,
		"top-level-after-construct"));

	ui.createElement(kConstructed, "auto-closed").construct();
	finishFrame(app);

	seen.clear();
	app.beginFrame();
	ui.createElement(kProbe, "next-frame")
		.setParameters(ProbeParameters{.seen = &seen})
		.draw();
	FLOWUI_CHECK(seen.size() == 1);
	FLOWUI_CHECK(seen[0] ==
		localID(FlowUi::RootFlowScopeID, ProbeElement::definitionId, "next-frame"));
	finishFrame(app);
}

} // namespace

int main() {
	try {
		testScopeLifecycle();
		return 0;
	} catch (const std::exception& error) {
		if (environmentUnavailable(error.what())) {
			std::cerr << "Element scope lifecycle validation unavailable: "
				<< error.what() << '\n';
			return 77;
		}
		std::cerr << "Element scope lifecycle validation failed: "
			<< error.what() << '\n';
		return 1;
	}
}
