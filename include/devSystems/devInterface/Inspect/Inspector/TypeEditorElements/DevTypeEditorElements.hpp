#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#include "internal/ElementInstanceKey.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

enum class DevInspectorTab : uint64_t {
	Parameters,
	State,
	Resources,
	Changes,
};

enum class DevEditorRole : uint8_t {
	Parameters,
	State,
	Resources,
	Changes,
};

struct DevEditorBinding {
	DevEditorRole role = DevEditorRole::Parameters;
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	devMode::DevTypeId rootOwnerType = 0u;
	devMode::DevFieldIndex field{};
	std::vector<devMode::DevFieldId> nestedPath{};
	std::string elementName{};
};

struct DevInterfaceInspectorTitle {
	using BuildContext = ElementBuildContext<DevInterfaceInspectorTitle>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.title");
	static constexpr std::string_view debugName = "Inspect Inspector Title";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevInterfaceInspectorIdentityParameters {
	std::string_view instanceName{};
	std::string_view definitionName{};
};

struct DevInterfaceInspectorIdentity {
	using Parameters = DevInterfaceInspectorIdentityParameters;
	using BuildContext = ElementBuildContext<DevInterfaceInspectorIdentity>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.identity");
	static constexpr std::string_view debugName = "Inspect Inspector Element Identity";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevInterfaceInspectorTabsParameters {
	uint64_t* selectedTab = nullptr;
};

struct DevInterfaceInspectorTabs {
	using Parameters = DevInterfaceInspectorTabsParameters;
	using BuildContext = ElementBuildContext<DevInterfaceInspectorTabs>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.tabs");
	static constexpr std::string_view debugName = "Inspect Inspector Tabs";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

/**
 * One semantic field editor. The body is intentionally deferred; the card
 * already owns schema identity so a compound Clay type remains one editor.
 */
struct DevEditorCardParameters {
	devMode::DevSchemaView schema{};
	DevEditorBinding binding{};
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	const void* value = nullptr;
	std::string_view labelOverride{};
	uint16_t depth = 0u;
};

struct DevEditorCardState {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	DevEditorBinding binding{};
	tooling::DevOwnedValue currentValue{};
	std::string fieldName{};
	std::string typeName{};
	devMode::DevTypeId fieldType = 0u;
	// Editor bodies are intentionally lazy. A selected element can expose dozens
	// of deeply nested fields; eagerly constructing every FSEL control can exhaust
	// the developer window's per-frame Clay arena before it presents the selection.
	bool collapsed = true;
	bool hasCurrentValue = false;
	bool editable = false;
	bool dirty = false;
};

struct DevEditorCard {
	using Parameters = DevEditorCardParameters;
	using State = DevEditorCardState;
	using BuildContext = ElementBuildContext<DevEditorCard>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.editor-card");
	static constexpr std::string_view debugName = "Developer Editor Card";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();
	static void buildElement(BuildContext& context);
};

struct DevEditorCardHeaderParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	DevEditorCardState* cardState = nullptr;
	std::string_view quickValue{};
	ActionCall onToggle{};
};

struct DevEditorCardHeaderState {
	bool isArmed = false;
	bool mouseUpObserved = false;
};

struct DevEditorCardHeaderResources {
	TextureRef copyIcon{};
	TextureRef pasteIcon{};
	TextureRef revertIcon{};
	TextureRef expandIcon{};
	TextureRef collapseIcon{};

	DevEditorCardHeaderResources() = default;
	explicit DevEditorCardHeaderResources(App& app);
};

struct DevEditorCardHeader {
	using Parameters = DevEditorCardHeaderParameters;
	using State = DevEditorCardHeaderState;
	using Resources = DevEditorCardHeaderResources;
	using BuildContext = ElementBuildContext<DevEditorCardHeader>;
	using InteractionContext = ElementInteractionContext<DevEditorCardHeader>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.editor-card.header");
	static constexpr std::string_view debugName = "Developer Editor Card Header";
	static constexpr bool isDevInternal = true;
	struct Parts {
		static constexpr FlowElementPart collapse = Part("collapse");
		static constexpr FlowElementPart copy = Part("copy");
		static constexpr FlowElementPart paste = Part("paste");
		static constexpr FlowElementPart reset = Part("reset");
	};
	static void onHovered(InteractionContext& context);
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void buildElement(BuildContext& context);
};

struct DevEditingFieldParameters {
	devMode::DevSchemaView schema{};
	DevEditorBinding binding{};
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	const void* value = nullptr;
	uint16_t depth = 0u;
};

struct DevEditingField {
	using Parameters = DevEditingFieldParameters;
	using BuildContext = ElementBuildContext<DevEditingField>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.editing-field");
	static constexpr std::string_view debugName = "Developer Editing Field";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevEditorParameters {
	devMode::DevSchemaView schema{};
	DevEditorBinding binding{};
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	const void* value = nullptr;
	bool showFieldName = true;
};

struct DevEditor {
	using Parameters = DevEditorParameters;
	using BuildContext = ElementBuildContext<DevEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.editor");
	static constexpr std::string_view debugName = "Developer Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevTypeEditorParameters {
	devMode::DevSchemaView schema{};
	DevEditorBinding binding{};
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	const void* value = nullptr;
};

struct DevTypeEditorState {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	DevEditorBinding binding{};
	tooling::DevOwnedValue currentValue{};
	devMode::DevTypeId type = 0u;
	devMode::DevTypeIndex typeIndex{};
	devMode::DevTypeIndex operationType{};
	void* editValue = nullptr;
	std::string fieldName{};
	std::string draft{};
	std::vector<std::string> choiceLabels{};
	std::vector<FSEL::ComboBoxOption> choiceOptions{};
	devMode::DevTypeId choiceType = 0u;
	uint64_t enumSelection = 0u;
	devMode::DevTypeId dragValueType = 0u;
	int dragSigned = 0;
	unsigned int dragUnsigned = 0u;
	float dragFloat = 0.0f;
	uint64_t semanticMode = std::numeric_limits<uint64_t>::max();
	float semanticValueA = 0.0f;
	float semanticValueB = 0.0f;
	double semanticSliderValue = 0.0;
	std::array<double, 4> semanticChannels{};
	std::string semanticText{};
	std::size_t sequenceActionIndex = 0u;
	bool draftInitialized = false;
	bool draftValid = true;
	bool editable = false;
	bool colorExpanded = false;
	bool previewActive = false;
};

struct DevTypeEditor {
	using Parameters = DevTypeEditorParameters;
	using State = DevTypeEditorState;
	using BuildContext = ElementBuildContext<DevTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor");
	static constexpr std::string_view debugName = "Developer Type Editor";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();
	static void buildElement(BuildContext& context);
};

struct DevInterfaceInspectorFieldsParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	FlowDefinitionID definition{};
	::FlowUi::detail::element::ElementInstanceKey instance{};
	std::string_view elementName{};
};

struct DevInterfaceInspectorFields {
	using Parameters = DevInterfaceInspectorFieldsParameters;
	using BuildContext = ElementBuildContext<DevInterfaceInspectorFields>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.fields");
	static constexpr std::string_view debugName = "Inspect Inspector Fields";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

inline constexpr DevInterfaceInspectorTitle kDevInterfaceInspectorTitle{};
inline constexpr DevInterfaceInspectorIdentity kDevInterfaceInspectorIdentity{};
inline constexpr DevInterfaceInspectorTabs kDevInterfaceInspectorTabs{};
inline constexpr DevEditorCard kDevEditorCard{};
inline constexpr DevEditorCardHeader kDevEditorCardHeader{};
inline constexpr DevEditingField kDevEditingField{};
inline constexpr DevEditor kDevEditor{};
inline constexpr DevTypeEditor kDevTypeEditor{};
inline constexpr DevInterfaceInspectorFields kDevInterfaceInspectorFields{};

static_assert(DrawableFlowElement<DevInterfaceInspectorTitle>);
static_assert(DrawableFlowElement<DevInterfaceInspectorIdentity>);
static_assert(DrawableFlowElement<DevInterfaceInspectorTabs>);
static_assert(DrawableFlowElement<DevEditorCard>);
static_assert(DrawableFlowElement<DevEditorCardHeader>);
static_assert(DrawableFlowElement<DevEditingField>);
static_assert(DrawableFlowElement<DevEditor>);
static_assert(DrawableFlowElement<DevTypeEditor>);
static_assert(DrawableFlowElement<DevInterfaceInspectorFields>);

} // namespace FlowUi::devSystems::interface_elements

#endif
