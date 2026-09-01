#include "devSystems/devInterface/Inspect/Selector/DevInspectSelector.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cstdio>
#include <vector>

#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr LocalElementName kSelectorTitle{"title"};
inline constexpr LocalElementName kSelectorControls{"controls"};
inline constexpr LocalElementName kSelectorSearch{"search"};
inline constexpr LocalElementName kSelectorForest{"forest"};
inline constexpr LocalElementName kSelectorFooter{"footer"};

struct SelectorBuildData {
	std::span<const FSEL::ComboBoxOption> definitionOptions{};
	uint32_t flowNodeCount = 0u;
	uint32_t clayNodeCount = 0u;
	uint64_t generation = 0u;
	bool clayNodeCountKnown = false;
};

void refreshDefinitionOptions(
	App* app,
	DevInspectSelectorState& cache) {
	const devMode::DevSchemaView schema = app
		? app->devTooling().schemas().view() : devMode::DevSchemaView{};
	const uint64_t generation = schema ? schema->generation : 0u;
	if (cache.definitionSchemaGeneration == generation) return;

	cache.definitionLabels.clear();
	cache.definitionOptions.clear();
	const std::size_t definitionCount = schema ? schema->elements.size() : 0u;
	cache.definitionLabels.reserve(definitionCount + 1u);
	cache.definitionOptions.reserve(definitionCount + 1u);

	cache.definitionLabels.emplace_back("All");
	if (schema) {
		for (const devMode::DevElementSchema& definition : schema->elements) {
			if (!definition.definitionId) continue;
			const std::string_view name = schema->string(definition.displayName);
			cache.definitionLabels.emplace_back(
				name.empty() ? "Unnamed Definition" : name);
		}
	}

	cache.definitionOptions.push_back(FSEL::ComboBoxOption{
		.value = kDevInterfaceAllDefinitions,
		.text = cache.definitionLabels.front(),
	});
	std::size_t labelIndex = 1u;
	if (schema) {
		for (const devMode::DevElementSchema& definition : schema->elements) {
			if (!definition.definitionId) continue;
			cache.definitionOptions.push_back(FSEL::ComboBoxOption{
				.value = definition.definitionId.value,
				.text = cache.definitionLabels[labelIndex++],
			});
		}
	}
	cache.definitionSchemaGeneration = generation;
}

SelectorBuildData selectorBuildData(
	App* app,
	DevInterfaceState& state,
	DevInspectSelectorState& cache) {
	SelectorBuildData data{};
	refreshDefinitionOptions(app, cache);
	data.definitionOptions = cache.definitionOptions;

	const tooling::DevTreeSnapshot* snapshot = nullptr;
	if (app && app->hasWindow(state.selectedWindowId)) {
		snapshot = &app->ui(state.selectedWindowId).devTreeSnapshot();
		data.generation = snapshot->generation;
		data.flowNodeCount = static_cast<uint32_t>(snapshot->flow.nodes.size());
#if FLOW_UI_DEV_CAPTURE_CLAY
		data.clayNodeCount = static_cast<uint32_t>(snapshot->clay.nodes.size());
		data.clayNodeCountKnown = snapshot->generation != 0u;
#endif
	}

	const bool selectedDefinitionExists = std::ranges::any_of(
		data.definitionOptions,
		[&state](const FSEL::ComboBoxOption& option) {
			return option.value == state.inspectDefinitionFilter;
		});
	if (!selectedDefinitionExists) {
		state.inspectDefinitionFilter = kDevInterfaceAllDefinitions;
	}
#if !FLOW_UI_DEV_CAPTURE_CLAY
	state.inspectForest = kDevInterfaceFlowForest;
#else
	if (state.inspectForest > kDevInterfaceClayForest) {
		state.inspectForest = kDevInterfaceFlowForest;
	}
#endif
	return data;
}

} // namespace

void DevInspectSelector::buildElement(BuildContext& context) {
	DevInterfaceState* state = context.params.interfaceState;
	DevInspectSelectorState& cache = context.state();
	SelectorBuildData data{};
	if (state) {
		data = selectorBuildData(context.params.app, *state, cache);
	} else {
		refreshDefinitionOptions(context.params.app, cache);
		data.definitionOptions = cache.definitionOptions;
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;

	CLAY(context.clayID(), root) {
		context.uiManager.createElement(kDevInterfaceSelectorTitle, kSelectorTitle)
			.setDevInternalCapture(true)
			.draw();

		context.uiManager.createElement(kDevInterfaceSelectorControls, kSelectorControls)
			.setParameters(DevInterfaceSelectorControlsParameters{
				.selectedForest = state ? &state->inspectForest : nullptr,
				.selectedDefinition = state ? &state->inspectDefinitionFilter : nullptr,
				.definitionOptions = data.definitionOptions,
#if FLOW_UI_DEV_CAPTURE_CLAY
				.clayForestAvailable = true,
#else
				.clayForestAvailable = false,
#endif
			})
			.setDevInternalCapture(true)
			.draw();

		context.uiManager.createElement(kDevInterfaceSelectorSearch, kSelectorSearch)
			.setParameters(DevInterfaceSelectorSearchParameters{
				.query = state ? &state->searchQuery : nullptr,
			})
			.setDevInternalCapture(true)
			.draw();

		context.uiManager.createElement(kDevInterfaceSelectorForest, kSelectorForest)
			.setParameters(DevInterfaceSelectorForestParameters{
				.app = context.params.app,
				.interfaceState = state,
			})
			.setDevInternalCapture(true)
			.draw();

		context.uiManager.createElement(kDevInterfaceSelectorFooter, kSelectorFooter)
			.setParameters(DevInterfaceSelectorFooterParameters{
				.flowNodeCount = data.flowNodeCount,
				.clayNodeCount = data.clayNodeCount,
				.forestGeneration = data.generation,
				.clayNodeCountKnown = data.clayNodeCountKnown,
			})
			.setDevInternalCapture(true)
			.draw();
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
