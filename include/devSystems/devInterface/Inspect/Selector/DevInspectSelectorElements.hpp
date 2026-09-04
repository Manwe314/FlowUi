#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

inline constexpr uint64_t kDevInterfaceFlowForest = 0u;
inline constexpr uint64_t kDevInterfaceClayForest = 1u;
inline constexpr uint64_t kDevInterfaceAllDefinitions = 0u;
inline constexpr uint64_t kDevInterfaceFlowNodeKind = 1u;
inline constexpr uint64_t kDevInterfaceClayNodeKind = 2u;

struct DevInterfaceSelectorTitle {
	using BuildContext = ElementBuildContext<DevInterfaceSelectorTitle>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector.title");
	static constexpr std::string_view debugName = "Inspect Selector Title";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInterfaceSelectorControlsParameters {
	uint64_t* selectedForest = nullptr;
	uint64_t* selectedDefinition = nullptr;
	std::span<const FSEL::ComboBoxOption> definitionOptions{};
	bool clayForestAvailable = false;
};

struct DevInterfaceSelectorControls {
	using Parameters = DevInterfaceSelectorControlsParameters;
	using BuildContext = ElementBuildContext<DevInterfaceSelectorControls>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector.controls");
	static constexpr std::string_view debugName = "Inspect Selector Controls";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInterfaceSelectorSearchParameters {
	std::string* query = nullptr;
};

struct DevInterfaceSelectorSearch {
	using Parameters = DevInterfaceSelectorSearchParameters;
	using BuildContext = ElementBuildContext<DevInterfaceSelectorSearch>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector.search");
	static constexpr std::string_view debugName = "Inspect Selector Search";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

[[nodiscard]] uint64_t stableNodeKey(uint64_t value, uint64_t salt) noexcept;

struct DevNodeParameters {
	DevInterfaceState* interfaceState = nullptr;
	// Optional frame-local output used by the forest after draw() completes.
	// Expansion remains owned by DevNode's retained state.
	bool* expandedOutput = nullptr;
	uint64_t kind = 0u;
	uint64_t selectionKey = 0u;
	uint32_t depth = 0u;
	std::string_view debugName{};
	std::string_view detailText{};
	std::string_view badgeText{};
	Clay_Color badgeColor{};
	bool hasChildren = false;
	bool hasChanges = false;
};

struct DevNodeState {
	bool isExpanded = true;
	bool isArmed = false;
	bool mouseUpObserved = false;
};

struct DevNodeResources {
	TextureRef expandedIcon{};
	TextureRef collapsedIcon{};

	DevNodeResources() = default;
	explicit DevNodeResources(App& app);
};

struct DevNode {
	using Parameters = DevNodeParameters;
	using State = DevNodeState;
	using Resources = DevNodeResources;
	using BuildContext = ElementBuildContext<DevNode>;
	using InteractionContext = ElementInteractionContext<DevNode>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector.node");
	static constexpr std::string_view debugName = "Inspect Tree Node";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();

	static void onHovered(InteractionContext& context);
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void buildElement(BuildContext& context);
};

struct DevInterfaceSelectorForestParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

struct DevInterfaceSelectorForest {
	using Parameters = DevInterfaceSelectorForestParameters;
	using BuildContext = ElementBuildContext<DevInterfaceSelectorForest>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector.forest");
	static constexpr std::string_view debugName = "Inspect Selector Forest";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInterfaceSelectorFooterParameters {
	uint32_t flowNodeCount = 0u;
	uint32_t clayNodeCount = 0u;
	uint64_t forestGeneration = 0u;
	bool clayNodeCountKnown = false;
};

struct DevInterfaceSelectorFooter {
	using Parameters = DevInterfaceSelectorFooterParameters;
	using BuildContext = ElementBuildContext<DevInterfaceSelectorFooter>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector.footer");
	static constexpr std::string_view debugName = "Inspect Selector Footer";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevInterfaceSelectorTitle kDevInterfaceSelectorTitle{};
inline constexpr DevInterfaceSelectorControls kDevInterfaceSelectorControls{};
inline constexpr DevInterfaceSelectorSearch kDevInterfaceSelectorSearch{};
inline constexpr DevNode kDevNode{};
inline constexpr DevInterfaceSelectorForest kDevInterfaceSelectorForest{};
inline constexpr DevInterfaceSelectorFooter kDevInterfaceSelectorFooter{};

static_assert(DrawableFlowElement<DevInterfaceSelectorTitle>);
static_assert(DrawableFlowElement<DevInterfaceSelectorControls>);
static_assert(DrawableFlowElement<DevInterfaceSelectorSearch>);
static_assert(DrawableFlowElement<DevNode>);
static_assert(DrawableFlowElement<DevInterfaceSelectorForest>);
static_assert(DrawableFlowElement<DevInterfaceSelectorFooter>);

} // namespace FlowUi::devSystems::interface_elements

#endif
