#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogLease.hpp"
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

/** One snapshot of every override state represented by an editor card. */
struct DevOverridePresence {
	bool exactLive = false;
	bool descendantLive = false;
	bool instanceLive = false;
	bool definitionLive = false;
	bool preview = false;
	std::size_t descendantCount = 0u;

	[[nodiscard]] bool hasCommittedChange() const noexcept {
		return exactLive || descendantLive;
	}
};

enum class DevQuickStatus : std::uint8_t { Normal, Missing, Warning, Unavailable };

struct DevQuickView {
	enum class Kind : std::uint8_t { Text, Color, Sizing, Spacing, Status };
	Kind kind = Kind::Text;
	std::string primary{};
	std::string secondary{};
	std::optional<Clay_Color> swatch{};
	std::optional<TextureRef> thumbnail{};
	DevQuickStatus status = DevQuickStatus::Normal;
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
	DevOverridePresence overridePresence{};
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
	DevQuickView quickView{};
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

enum class DevNineSplitMode : std::uint8_t { Numeric, Radio };

struct DevNineSplitCell {
	std::string_view label{};
	unsigned int* numericValue = nullptr;
	float* floatingValue = nullptr;
	std::uint64_t choiceValue = 0u;
	ActionCall onPreview{};
	ActionCall onCommit{};
	ActionCall onCancel{};
	bool occupied = false;
};

struct DevNineSplitEditorParameters {
	DevNineSplitMode mode = DevNineSplitMode::Numeric;
	std::array<DevNineSplitCell, 9> cells{};
	std::uint64_t* selectedValue = nullptr;
	ActionCall onSelected{};
	ActionCall centerAction{};
	std::string_view centerLabel{"Link"};
	bool centerSelected = false;
	bool enabled = true;
};

struct DevNineSplitEditorResources {
	TextureRef linkIcon{};

	DevNineSplitEditorResources() = default;
	explicit DevNineSplitEditorResources(App& app);
};

struct DevNineSplitEditor {
	using Parameters = DevNineSplitEditorParameters;
	using Resources = DevNineSplitEditorResources;
	using BuildContext = ElementBuildContext<DevNineSplitEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.nine-split");
	static constexpr std::string_view debugName = "Developer Nine Split Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevCatalogueChoiceParameters {
	std::span<const FSEL::ComboBoxOption> options{};
	std::uint64_t* selectedValue = nullptr;
	ActionCall onChanged{};
	std::string_view placeholder{"Search catalogue..."};
	bool enabled = true;
	bool searchable = true;
};

struct DevCatalogueChoiceState {
	std::string search{};
	std::vector<std::string> filteredLabels{};
	std::vector<FSEL::ComboBoxOption> filteredOptions{};
};

struct DevCatalogueChoice {
	using Parameters = DevCatalogueChoiceParameters;
	using State = DevCatalogueChoiceState;
	using BuildContext = ElementBuildContext<DevCatalogueChoice>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.catalogue-choice");
	static constexpr std::string_view debugName = "Developer Catalogue Choice";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();
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
	std::vector<std::string> choiceKeys{};
	std::vector<std::uint8_t> choiceResourceDomains{};
	std::vector<std::uint64_t> choiceIdentities{};
	devMode::DevTypeId choiceType = 0u;
	devMode::DevChoiceDomain choiceDomain = devMode::DevChoiceDomain::None;
	std::uint64_t choiceRevision = std::numeric_limits<std::uint64_t>::max();
	std::uint64_t choiceCurrentIdentity = std::numeric_limits<std::uint64_t>::max();
	uint64_t enumSelection = 0u;
	uint64_t resourceSelection = 0u;
	uint64_t resourceFitSelection = 0u;
	uint64_t resourceSamplingSelection = 0u;
	devMode::DevTypeId dragValueType = 0u;
	int dragSigned = 0;
	unsigned int dragUnsigned = 0u;
	float dragFloat = 0.0f;
	uint64_t semanticMode = std::numeric_limits<uint64_t>::max();
	float semanticValueA = 0.0f;
	float semanticValueB = 0.0f;
	double semanticSliderValue = 0.0;
	std::array<float, 4> semanticChannels{};
	std::array<float, 4> semanticCorners{};
	std::array<unsigned int, 5> semanticUnsigned{};
	std::array<std::uint64_t, 2> semanticSelections{};
	std::string semanticText{};
	std::string localDiagnostic{};
	devMode::DevCatalogLease catalogLease{};
	std::size_t sequenceActionIndex = 0u;
	bool draftInitialized = false;
	bool draftValid = true;
	bool editable = false;
	bool resourceExpanded = false;
	bool resourceTintEnabled = false;
	bool semanticLinked = true;
	bool previewActive = false;
};

struct DevOptionalTypeEditorParameters {
	bool present = false;
	bool enabled = true;
	ActionCall onToggle{};
};

struct DevOptionalTypeEditor {
	using Parameters = DevOptionalTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevOptionalTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.optional");
	static constexpr std::string_view debugName = "Optional Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevColorTypeEditorParameters {
	DevTypeEditorState* editor = nullptr;
};

struct DevColorTypeEditorState {
	bool popupOpen = false;
};

struct DevColorTypeEditor {
	using Parameters = DevColorTypeEditorParameters;
	using State = DevColorTypeEditorState;
	using BuildContext = ElementBuildContext<DevColorTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.color");
	static constexpr std::string_view debugName = "Color Type Editor";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();
	struct Parts {
		static constexpr FlowElementPart swatch = Part("swatch");
	};
	static void buildElement(BuildContext& context);
};

struct DevPaddingTypeEditorParameters {
	DevTypeEditorState* editor = nullptr;
};

struct DevPaddingTypeEditor {
	using Parameters = DevPaddingTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevPaddingTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.padding");
	static constexpr std::string_view debugName = "Padding Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevBorderWidthTypeEditor {
	using Parameters = DevPaddingTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevBorderWidthTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.border-width");
	static constexpr std::string_view debugName = "Border Width Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevCornerRadiusTypeEditor {
	using Parameters = DevPaddingTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevCornerRadiusTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.corner-radius");
	static constexpr std::string_view debugName = "Corner Radius Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevAttachmentPointsTypeEditor {
	using Parameters = DevPaddingTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevAttachmentPointsTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.attachment-points");
	static constexpr std::string_view debugName = "Attachment Points Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevPrimitiveTypeEditorParameters {
	DevTypeEditorState* editor = nullptr;
};

struct DevBooleanTypeEditor {
	using Parameters = DevPrimitiveTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevBooleanTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.boolean");
	static constexpr std::string_view debugName = "Boolean Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevNumericTypeEditor {
	using Parameters = DevPrimitiveTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevNumericTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.numeric");
	static constexpr std::string_view debugName = "Numeric Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevTextTypeEditor {
	using Parameters = DevPrimitiveTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevTextTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.text");
	static constexpr std::string_view debugName = "Text Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

struct DevEnumTypeEditor {
	using Parameters = DevPrimitiveTypeEditorParameters;
	using BuildContext = ElementBuildContext<DevEnumTypeEditor>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.type-editor.enum");
	static constexpr std::string_view debugName = "Enum Type Editor";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
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

struct DevChangesHeaderParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	std::string_view elementName{};
};

struct DevChangesHeaderState {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	std::string elementName{};
};

/** Shared precedence legend and batch controls for the Inspector's Changes tab. */
struct DevChangesHeader {
	using Parameters = DevChangesHeaderParameters;
	using State = DevChangesHeaderState;
	using BuildContext = ElementBuildContext<DevChangesHeader>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.changes.header");
	static constexpr std::string_view debugName = "Developer Changes Header";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();
	static void buildElement(BuildContext& context);
};

struct DevChangeCardParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	std::string_view elementName{};
	tooling::DevOverrideFieldKey field{};
	devMode::DevFieldIndex fieldIndex{};
	std::string_view fieldName{};
	std::string_view typeName{};
	std::string_view authoredValue{};
	std::string_view bakedValue{};
};

struct DevChangeCardState {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	std::string elementName{};
	tooling::DevOverrideFieldKey field{};
	devMode::DevFieldIndex fieldIndex{};
	std::string fieldName{};
	std::string typeName{};
	std::string authoredValue{};
	std::string bakedValue{};
};

/** One compact, vertically composed provenance/action row in the Changes tab. */
struct DevChangeCard {
	using Parameters = DevChangeCardParameters;
	using State = DevChangeCardState;
	using BuildContext = ElementBuildContext<DevChangeCard>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector.change-card");
	static constexpr std::string_view debugName = "Developer Change Card";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();
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
inline constexpr DevNineSplitEditor kDevNineSplitEditor{};
inline constexpr DevCatalogueChoice kDevCatalogueChoice{};
inline constexpr DevOptionalTypeEditor kDevOptionalTypeEditor{};
inline constexpr DevColorTypeEditor kDevColorTypeEditor{};
inline constexpr DevPaddingTypeEditor kDevPaddingTypeEditor{};
inline constexpr DevBorderWidthTypeEditor kDevBorderWidthTypeEditor{};
inline constexpr DevCornerRadiusTypeEditor kDevCornerRadiusTypeEditor{};
inline constexpr DevAttachmentPointsTypeEditor kDevAttachmentPointsTypeEditor{};
inline constexpr DevBooleanTypeEditor kDevBooleanTypeEditor{};
inline constexpr DevNumericTypeEditor kDevNumericTypeEditor{};
inline constexpr DevTextTypeEditor kDevTextTypeEditor{};
inline constexpr DevEnumTypeEditor kDevEnumTypeEditor{};
inline constexpr DevTypeEditor kDevTypeEditor{};
inline constexpr DevChangesHeader kDevChangesHeader{};
inline constexpr DevChangeCard kDevChangeCard{};
inline constexpr DevInterfaceInspectorFields kDevInterfaceInspectorFields{};

static_assert(DrawableFlowElement<DevInterfaceInspectorTitle>);
static_assert(DrawableFlowElement<DevInterfaceInspectorIdentity>);
static_assert(DrawableFlowElement<DevInterfaceInspectorTabs>);
static_assert(DrawableFlowElement<DevEditorCard>);
static_assert(DrawableFlowElement<DevEditorCardHeader>);
static_assert(DrawableFlowElement<DevEditingField>);
static_assert(DrawableFlowElement<DevEditor>);
static_assert(DrawableFlowElement<DevNineSplitEditor>);
static_assert(DrawableFlowElement<DevCatalogueChoice>);
static_assert(DrawableFlowElement<DevOptionalTypeEditor>);
static_assert(DrawableFlowElement<DevColorTypeEditor>);
static_assert(DrawableFlowElement<DevPaddingTypeEditor>);
static_assert(DrawableFlowElement<DevBorderWidthTypeEditor>);
static_assert(DrawableFlowElement<DevCornerRadiusTypeEditor>);
static_assert(DrawableFlowElement<DevAttachmentPointsTypeEditor>);
static_assert(DrawableFlowElement<DevBooleanTypeEditor>);
static_assert(DrawableFlowElement<DevNumericTypeEditor>);
static_assert(DrawableFlowElement<DevTextTypeEditor>);
static_assert(DrawableFlowElement<DevEnumTypeEditor>);
static_assert(DrawableFlowElement<DevTypeEditor>);
static_assert(DrawableFlowElement<DevChangesHeader>);
static_assert(DrawableFlowElement<DevChangeCard>);
static_assert(DrawableFlowElement<DevInterfaceInspectorFields>);

} // namespace FlowUi::devSystems::interface_elements

#endif
