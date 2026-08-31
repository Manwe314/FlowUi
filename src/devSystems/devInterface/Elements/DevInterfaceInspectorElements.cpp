#include "devSystems/devInterface/Elements/DevInterfaceInspectorElements.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>

#include "FSEL/Button.hpp"
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr float kTitleHeight = 36.0f;
inline constexpr float kIdentityHeight = 58.0f;
inline constexpr float kTabsHeight = 38.0f;
inline constexpr LocalElementName kTabCard{"tab"};
inline constexpr LocalElementName kCards{"cards"};
inline constexpr LocalElementName kEditorCard{"editor-card"};
inline constexpr LocalElementName kCardHeader{"header"};
inline constexpr LocalElementName kFieldName{"field-name"};
inline constexpr LocalElementName kSemanticType{"semantic-type"};
inline constexpr LocalElementName kQuickValue{"quick-value"};
inline constexpr LocalElementName kHeaderSpacer{"header-spacer"};
inline constexpr LocalElementName kCollapse{"collapse"};
inline constexpr LocalElementName kCopy{"copy"};
inline constexpr LocalElementName kPaste{"paste"};
inline constexpr LocalElementName kReset{"reset"};
inline constexpr LocalElementName kEditingField{"editing-field"};
inline constexpr LocalElementName kEditor{"editor"};
inline constexpr LocalElementName kNestedCard{"nested-card"};
inline constexpr LocalElementName kTypeName{"type-name"};
inline constexpr LocalElementName kTypeEditor{"type-editor"};

struct TabSpec {
	DevInspectorTab tab;
	std::string_view label;
};

inline constexpr std::array<TabSpec, 4> kTabs{{
	{DevInspectorTab::Parameters, "Parameters"},
	{DevInspectorTab::State, "State"},
	{DevInspectorTab::Resources, "Resources"},
	{DevInspectorTab::Changes, "Changes"},
}};

Clay_TextElementConfig textConfig(
	Clay_Color color,
	uint16_t size,
	Clay_TextAlignment alignment = CLAY_TEXT_ALIGN_LEFT) {
	Clay_TextElementConfig config{};
	config.textColor = color;
	config.fontSize = size;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	config.textAlignment = alignment;
	return config;
}

Clay_ElementDeclaration fixedSection(
	float height,
	Clay_Color background,
	Clay_Padding padding = {}) {
	Clay_ElementDeclaration section{};
	section.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(height),
	};
	section.layout.padding = padding;
	section.backgroundColor = background;
	section.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};
	return section;
}

const devMode::DevFieldSchema* fieldAt(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevFieldIndex index) {
	if (!index || index.value > schema.fields.size()) return nullptr;
	return &schema.fields[index.value - 1u];
}

std::string_view schemaTypeName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type) {
	if (!type) return "Unknown";
	std::string_view name = schema.string(type->displayName);
	if (name.empty()) name = schema.string(type->cppTypeName);
	return name.empty() ? std::string_view{"Unknown"} : name;
}

devMode::DevFieldIndex fieldIndex(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevFieldSchema& field) {
	return devMode::DevFieldIndex{
		static_cast<uint32_t>(&field - schema.fields.data()) + 1u};
}

devMode::DevEditorKind editorKind(
	const devMode::DevFieldSchema* field,
	const devMode::DevTypeSchema* type) {
	if (field && field->editor != devMode::DevEditorKind::None) return field->editor;
	if (!type) return devMode::DevEditorKind::None;
	if (type->kind == devMode::DevTypeKind::Enumeration) {
		return devMode::DevEditorKind::EnumChoice;
	}
	if (type->kind == devMode::DevTypeKind::Optional) {
		return devMode::DevEditorKind::OptionalGroup;
	}
	return type->editor;
}

std::string_view editorName(devMode::DevEditorKind editor) {
	switch (editor) {
	case devMode::DevEditorKind::Toggle: return "Boolean";
	case devMode::DevEditorKind::SignedNumber: return "Signed Number";
	case devMode::DevEditorKind::UnsignedNumber: return "Unsigned Number";
	case devMode::DevEditorKind::FloatingNumber: return "Floating Number";
	case devMode::DevEditorKind::Text: return "Text";
	case devMode::DevEditorKind::EnumChoice: return "Enum";
	case devMode::DevEditorKind::Flags: return "Flags";
	case devMode::DevEditorKind::Color: return "Clay Color";
	case devMode::DevEditorKind::Vector: return "Clay Vector";
	case devMode::DevEditorKind::Spacing: return "Clay Spacing";
	case devMode::DevEditorKind::CornerRadius: return "Clay Corner Radius";
	case devMode::DevEditorKind::Sizing: return "Clay Sizing";
	case devMode::DevEditorKind::SizingAxis: return "Clay Sizing Axis";
	case devMode::DevEditorKind::ObjectGroup: return "Object";
	case devMode::DevEditorKind::OptionalGroup: return "Optional";
	case devMode::DevEditorKind::Sequence: return "Sequence";
	case devMode::DevEditorKind::ActionChoice: return "Action";
	case devMode::DevEditorKind::ResourceChoice: return "Resource";
	case devMode::DevEditorKind::Custom: return "Custom";
	case devMode::DevEditorKind::None: return "Unsupported";
	}
	return "Unsupported";
}

std::string_view claySemanticName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type) {
	if (!type) return {};
	std::string_view name = schema.string(type->displayName);
	if (name.empty()) name = schema.string(type->cppTypeName);
	if (name == "Clay_Color") return "Clay Color";
	if (name == "Clay_Padding") return "Clay Spacing";
	if (name == "Clay_BorderWidth") return "Clay Border Width";
	if (name == "Clay_CornerRadius") return "Clay Corner Radius";
	if (name == "Clay_Vector2" || name == "Clay_Dimensions" ||
		name == "Clay_SizingMinMax") return "Clay Vector";
	if (name == "Clay_ChildAlignment") return "Clay Alignment";
	if (name == "Clay_FloatingAttachPoints") return "Clay Attach Points";
	if (name == "Clay_AspectRatioElementConfig") return "Clay Aspect Ratio";
	if (name == "Clay_SizingAxis") return "Clay Sizing Axis";
	if (name == "Clay_Sizing") return "Clay Sizing";
	if (name == "Clay_LayoutConfig") return "Clay Layout";
	if (name == "Clay_TextElementConfig") return "Clay Text Config";
	if (name == "Clay_FloatingElementConfig") return "Clay Floating Config";
	if (name == "Clay_ClipElementConfig") return "Clay Clip Config";
	if (name == "Clay_BorderElementConfig") return "Clay Border Config";
	if (name == "Clay_ElementDeclaration") return "Clay Element Declaration";
	return {};
}

std::string semanticName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevFieldSchema* field,
	const devMode::DevTypeSchema* type) {
	if (const std::string_view clayName = claySemanticName(schema, type);
		!clayName.empty()) return std::string(clayName);
	const devMode::DevEditorKind editor = editorKind(field, type);
	if (editor != devMode::DevEditorKind::OptionalGroup || !type) {
		return std::string(editorName(editor));
	}
	const devMode::DevTypeSchema* valueType = schema.type(type->elementType);
	if (!valueType) return "Optional";
	return std::string("Optional · ") + semanticName(schema, nullptr, valueType);
}

std::string quickValue(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type,
	const void* value) {
	if (!type || !value) return {};
	if (type->kind == devMode::DevTypeKind::Optional) {
		const std::size_t typeIndex = static_cast<std::size_t>(
			type - schema.types.data());
		const devMode::DevTypeOps* operations =
			typeIndex < schema.typeOperations.size()
				? schema.typeOperations[typeIndex] : nullptr;
		if (!operations || !operations->optionalHasValue ||
			!operations->optionalValueAddress) return {};
		if (!operations->optionalHasValue(value)) return "None";
		return quickValue(
			schema, schema.type(type->elementType),
			operations->optionalValueAddress(value));
	}
	const std::string_view name = schemaTypeName(schema, type);
	char buffer[128]{};
	if (name == "Clay_Padding") {
		const auto& padding = *static_cast<const Clay_Padding*>(value);
		std::snprintf(buffer, sizeof(buffer), "L%u R%u T%u B%u",
			padding.left, padding.right, padding.top, padding.bottom);
		return buffer;
	}
	if (name == "Clay_Color") {
		const auto& color = *static_cast<const Clay_Color*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.0f %.0f %.0f %.0f",
			color.r, color.g, color.b, color.a);
		return buffer;
	}
	if (name == "Clay_CornerRadius") {
		const auto& radius = *static_cast<const Clay_CornerRadius*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.0f %.0f %.0f %.0f",
			radius.topLeft, radius.topRight, radius.bottomRight, radius.bottomLeft);
		return buffer;
	}
	if (name == "Clay_Dimensions") {
		const auto& dimensions = *static_cast<const Clay_Dimensions*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f × %.2f",
			dimensions.width, dimensions.height);
		return buffer;
	}
	if (name == "Clay_Vector2") {
		const auto& vector = *static_cast<const Clay_Vector2*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f, %.2f", vector.x, vector.y);
		return buffer;
	}
	if (name == "Clay_SizingMinMax") {
		const auto& range = *static_cast<const Clay_SizingMinMax*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f – %.2f", range.min, range.max);
		return buffer;
	}
	if (type->kind == devMode::DevTypeKind::Enumeration &&
		type->enumeration.values.first <= schema.enumValues.size() &&
		type->enumeration.values.count <=
			schema.enumValues.size() - type->enumeration.values.first) {
		const devMode::DevTypeOps* operations =
			static_cast<std::size_t>(type - schema.types.data()) < schema.typeOperations.size()
				? schema.typeOperations[type - schema.types.data()] : nullptr;
		long double numeric = 0.0L;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			const uint64_t bits = static_cast<uint64_t>(numeric);
			for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
				const devMode::DevEnumValueSchema& option =
					schema.enumValues[type->enumeration.values.first + index];
				if (option.bits == bits) return std::string(schema.string(option.name));
			}
		}
	}
	return {};
}

std::string roleName(DevEditorRole role) {
	switch (role) {
	case DevEditorRole::Parameters: return "Parameters";
	case DevEditorRole::State: return "State";
	case DevEditorRole::Resources: return "Resources";
	case DevEditorRole::Changes: return "Changes";
	}
	return "Unknown";
}

tooling::DevOverrideCommand fieldCommand(
	const devMode::DevSchemaGeneration& schema,
	const DevEditorCardState& card,
	tooling::DevOverrideCommandKind kind,
	tooling::DevOwnedValue value = {}) {
	const devMode::DevFieldSchema* field = fieldAt(schema, card.binding.field);
	tooling::DevOverrideCommand command{};
	command.kind = kind;
	command.element = tooling::DevElementOverrideTarget{
		.definition = card.binding.definition,
		.window = card.binding.window,
		.instance = card.binding.instance,
		.instanceDebugLabel = card.binding.elementName,
		.scope = tooling::DevOverrideScope::ExactInstance,
		.bakeable = true,
	};
	command.field = tooling::DevOverrideFieldKey{
		.ownerType = card.binding.rootOwnerType,
		.field = field ? field->id : 0u,
		.nestedPath = card.binding.nestedPath,
	};
	command.layer = tooling::DevOverrideLayer::LiveInstance;
	command.value = std::move(value);
	return command;
}

const tooling::DevOverrideApply::Record* currentOverride(
	const App& app,
	const devMode::DevSchemaGeneration& schema,
	const DevEditorCardState& card) {
	const tooling::DevOverrideCommand key = fieldCommand(
		schema, card, tooling::DevOverrideCommandKind::ClearElementField);
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	const auto found = std::ranges::find_if(
		records,
		[&key](const tooling::DevOverrideApply::Record& record) {
			return record.target == key.element && record.field == key.field &&
				record.layer == key.layer;
		});
	return found == records.end() ? nullptr : &*found;
}

bool submitEditTransaction(
	App& app,
	DevInterfaceState& state,
	DevInterfaceEditTransaction transaction) {
	tooling::DevChangeSet changes{
		.transaction = state.nextEditTransaction++,
		.commands = transaction.forward,
	};
	if (!changes.commands.empty() &&
		!app.devTooling().overrides().submit(std::move(changes))) return false;
	state.editUndoStack.push_back(std::move(transaction));
	state.editRedoStack.clear();
	return true;
}

constexpr auto kCollapseCard = UiAction(
	"flowui.dev_interface.editor-card.collapse",
	[](bool& collapsed) { collapsed = !collapsed; });

constexpr auto kCopyField = UiAction(
	"flowui.dev_interface.editor-card.copy",
	[](DevEditorCardState& card) {
		if (!card.interfaceState) return;
		DevInterfaceState& state = *card.interfaceState;
		if (!card.hasCurrentValue || !card.currentValue) return;
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Copy " + card.typeName;
		transaction.changesClipboard = true;
		transaction.clipboardBefore = state.editorClipboard;
		state.editorClipboard = DevInterfaceEditorClipboard{
			.type = card.currentValue.type(),
			.typeName = card.typeName,
			.value = card.currentValue,
		};
		transaction.clipboardAfter = state.editorClipboard;
		state.editUndoStack.push_back(std::move(transaction));
		state.editRedoStack.clear();
		state.lastActionMessage = card.typeName + " Values of " +
			card.binding.elementName + "'s " + roleName(card.binding.role) +
			" field " + card.fieldName + " copied";
	});

constexpr auto kPasteField = UiAction(
	"flowui.dev_interface.editor-card.paste",
	[](DevEditorCardState& card) {
		if (!card.app || !card.interfaceState) return;
		App& app = *card.app;
		DevInterfaceState& state = *card.interfaceState;
		const devMode::DevSchemaView schema = app.devTooling().schemas().view();
		if (!schema || !card.editable || !state.editorClipboard ||
			state.editorClipboard.type != card.fieldType) return;
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Paste " + card.typeName;
		transaction.forward.push_back(fieldCommand(
			*schema, card, tooling::DevOverrideCommandKind::SetElementField,
			state.editorClipboard.value));
		if (const tooling::DevOverrideApply::Record* prior =
			currentOverride(app, *schema, card)) {
			transaction.inverse.push_back(fieldCommand(
				*schema, card, tooling::DevOverrideCommandKind::SetElementField,
				prior->value));
		} else {
			transaction.inverse.push_back(fieldCommand(
				*schema, card, tooling::DevOverrideCommandKind::ClearElementField));
		}
		if (submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = card.typeName + " Values pasted into " +
				card.binding.elementName + "'s " + roleName(card.binding.role) +
				" field " + card.fieldName;
		}
	});

constexpr auto kResetField = UiAction(
	"flowui.dev_interface.editor-card.reset",
	[](DevEditorCardState& card) {
		if (!card.app || !card.interfaceState) return;
		App& app = *card.app;
		DevInterfaceState& state = *card.interfaceState;
		const devMode::DevSchemaView schema = app.devTooling().schemas().view();
		if (!schema || !card.editable) return;
		const tooling::DevOverrideApply::Record* prior = currentOverride(app, *schema, card);
		if (!prior) return;
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Reset " + card.typeName;
		transaction.forward.push_back(fieldCommand(
			*schema, card, tooling::DevOverrideCommandKind::ClearElementField));
		transaction.inverse.push_back(fieldCommand(
			*schema, card, tooling::DevOverrideCommandKind::SetElementField,
			prior->value));
		if (submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = card.binding.elementName + "'s " +
				roleName(card.binding.role) + " field " + card.fieldName + " reset";
		}
	});

void drawHeaderButton(
	DevEditorCardHeader::BuildContext& context,
	LocalElementName id,
	std::string_view label,
	ActionCall action,
	bool enabled) {
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = action;
	parameters.enabled = enabled;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.text = label;
	parameters.sizing = {
		.width = CLAY_SIZING_FIXED(24),
		.height = CLAY_SIZING_FIXED(24),
	};
	parameters.padding = Clay_Padding{0, 0, 0, 0};
	parameters.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextSecondary;
	parameters.idleOverrides.borderColor = interface_theme::kBorderVisible;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.pressedOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.borderColor = interface_theme::kAccentSeaGlass;
	parameters.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	parameters.disabledOverrides.labelColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
	parameters.labelFontSize = 10;
	context.uiManager.createElement(FSEL::kButton, id)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
}

void drawMessage(
	DevInterfaceInspectorFields::BuildContext& context,
	std::string_view message) {
	Clay_ElementDeclaration empty{};
	empty.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	empty.layout.padding = Clay_Padding{16, 16, 16, 16};
	empty.layout.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	Clay_TextElementConfig style = textConfig(
		interface_theme::kTextMuted, 11, CLAY_TEXT_ALIGN_CENTER);
	style.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY(context.clayID("message"), empty) {
		CLAY_TEXT(context.uiManager.toClayString(message), CLAY_TEXT_CONFIG(style));
	}
}

void drawCard(
	DevInterfaceInspectorFields::BuildContext& context,
	const devMode::DevSchemaView& schema,
	const devMode::DevFieldSchema& field,
	DevEditorRole role,
	devMode::DevTypeId rootOwnerType,
	const void* value,
	std::string_view labelOverride = {},
	uint64_t cardKey = 0u) {
	context.uiManager.createElement(
		kDevEditorCard, Keyed(kEditorCard, cardKey == 0u ? field.id : cardKey))
		.setParameters(DevEditorCardParameters{
			.schema = schema,
			.binding = DevEditorBinding{
				.role = role,
				.definition = context.params.definition,
				.window = context.params.interfaceState
					? context.params.interfaceState->selectedWindowId : InvalidWindowId,
				.instance = context.params.instance,
				.rootOwnerType = rootOwnerType,
				.field = fieldIndex(*schema, field),
				.elementName = std::string(context.params.elementName),
			},
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
			.value = value,
			.labelOverride = labelOverride,
		})
		.setDevInternalCapture(true)
		.draw();
}

} // namespace

void DevInterfaceInspectorTitle::buildElement(BuildContext& context) {
	Clay_ElementDeclaration title = fixedSection(
		kTitleHeight,
		interface_theme::kDepth2Ink,
		Clay_Padding{12, 12, 0, 0});
	title.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	const Clay_TextElementConfig style = textConfig(interface_theme::kTextCanvas, 14);
	CLAY(context.clayID(), title) {
		CLAY_TEXT(context.uiManager.toClayString("Inspector"), CLAY_TEXT_CONFIG(style));
	}
}

void DevInterfaceInspectorIdentity::buildElement(BuildContext& context) {
	Clay_ElementDeclaration identity = fixedSection(
		kIdentityHeight,
		interface_theme::kDepth1Panel,
		Clay_Padding{12, 12, 8, 8});
	identity.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	identity.layout.childGap = 3;
	identity.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	const Clay_TextElementConfig instanceStyle = textConfig(interface_theme::kTextCanvas, 13);
	const Clay_TextElementConfig definitionStyle = textConfig(interface_theme::kTextMuted, 10);
	CLAY(context.clayID(), identity) {
		CLAY_TEXT(
			context.uiManager.toClayString(context.params.instanceName),
			CLAY_TEXT_CONFIG(instanceStyle));
		CLAY_TEXT(
			context.uiManager.toClayString(context.params.definitionName),
			CLAY_TEXT_CONFIG(definitionStyle));
	}
}

void DevInterfaceInspectorTabs::buildElement(BuildContext& context) {
	if (context.params.selectedTab &&
		*context.params.selectedTab > static_cast<uint64_t>(DevInspectorTab::Changes)) {
		*context.params.selectedTab = static_cast<uint64_t>(DevInspectorTab::Parameters);
	}
	Clay_ElementDeclaration strip = fixedSection(
		kTabsHeight, interface_theme::kDepth2Ink);
	strip.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	CLAY(context.clayID(), strip) {
		for (const TabSpec& tab : kTabs) {
			const uint64_t value = static_cast<uint64_t>(tab.tab);
			const bool selected = context.params.selectedTab &&
				*context.params.selectedTab == value;
			FSEL::RadioChoiceParameters parameters{};
			parameters.choiceValue = value;
			parameters.selectedValue = context.params.selectedTab;
			parameters.style.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			parameters.style.padding = Clay_Padding{5, 5, 0, 0};
			parameters.style.childAlignment = {
				.x = CLAY_ALIGN_X_CENTER,
				.y = CLAY_ALIGN_Y_CENTER,
			};
			parameters.style.borderWidth = Clay_BorderWidth{0, 0, 0, 2, 0};
			parameters.style.cornerRadius = CLAY_CORNER_RADIUS(0);
			parameters.style.idleOverrides.backgroundColor = interface_theme::kDepth2Ink;
			parameters.style.idleOverrides.borderColor = interface_theme::kDepth2Ink;
			parameters.style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
			parameters.style.hoveredOverrides.borderColor = interface_theme::kHoverSurface;
			parameters.style.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
			parameters.style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
			parameters.style.selectedOverrides.backgroundColor = interface_theme::kDepth1Panel;
			parameters.style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;

			context.uiManager.createElement(FSEL::kRadioChoice, Keyed(kTabCard, value))
				.setParameters(std::move(parameters))
				.setDevInternalCapture(true)
				.construct();
			const Clay_TextElementConfig labelStyle = textConfig(
				selected ? interface_theme::kTextCanvas : interface_theme::kTextSecondary,
				10,
				CLAY_TEXT_ALIGN_CENTER);
			CLAY_TEXT(
				context.uiManager.toClayString(tab.label),
				CLAY_TEXT_CONFIG(labelStyle));
			context.uiManager.drawConstructed();
		}
	}
}

void DevEditorCard::buildElement(BuildContext& context) {
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* field = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* type = field && schema
		? schema->type(field->valueType) : nullptr;
	std::string_view fieldName = context.params.labelOverride;
	if (fieldName.empty() && field && schema) fieldName = schema->string(field->displayName);
	if (fieldName.empty() && field && schema) fieldName = schema->string(field->name);
	if (fieldName.empty()) fieldName = "Unnamed Field";

	DevEditorCardState& state = context.state();
	state.app = context.params.app;
	state.interfaceState = context.params.interfaceState;
	state.binding = context.params.binding;
	state.fieldName = std::string(fieldName);
	state.typeName = schema ? std::string(schemaTypeName(*schema, type)) : "Unknown";
	state.fieldType = type ? type->id : 0u;
	state.editable = field &&
		context.params.binding.role == DevEditorRole::Parameters &&
		field->effectiveEdit == devMode::DevEditCapability::Editable;
	state.currentValue.reset();
	state.hasCurrentValue = false;
	if (schema && field && type && context.params.value &&
		devSystems::tooling::DevOwnedValue::copyFrom(
			*schema, field->valueType, context.params.value, state.currentValue) ==
			devMode::DevValueOperationStatus::Success) {
		state.hasCurrentValue = true;
	}
	const std::string collapsedValue = state.collapsed && schema
		? quickValue(*schema, type, state.currentValue.data()) : std::string{};

	Clay_ElementDeclaration card{};
	card.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	card.backgroundColor = interface_theme::kDepth2Ink;
	card.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	CLAY(context.clayID(), card) {
		context.uiManager.createElement(kDevEditorCardHeader, kCardHeader)
			.setParameters(DevEditorCardHeaderParameters{
				.app = context.params.app,
				.interfaceState = context.params.interfaceState,
				.cardState = &state,
				.quickValue = collapsedValue,
			})
			.setDevInternalCapture(true)
			.draw();
		if (!state.collapsed && field && type) {
			context.uiManager.createElement(kDevEditingField, kEditingField)
				.setParameters(DevEditingFieldParameters{
					.schema = context.params.schema,
					.binding = context.params.binding,
					.app = context.params.app,
					.interfaceState = context.params.interfaceState,
					.value = context.params.value,
					.depth = context.params.depth,
				})
				.setDevInternalCapture(true)
				.draw();
		}
	}
}

void DevEditorCardHeader::buildElement(BuildContext& context) {
	DevEditorCardState* card = context.params.cardState;
	if (!card) return;
	App* app = context.params.app;
	DevInterfaceState* interfaceState = context.params.interfaceState;
	ActionCall collapseAction{};
	ActionCall copyAction{};
	ActionCall pasteAction{};
	ActionCall resetAction{};
	bool canPaste = false;
	bool canReset = false;
	if (app && interfaceState) {
		auto& actions = app->actions().uiActions();
		collapseAction = ActionCall{actions.make(kCollapseCard, card->collapsed)};
		copyAction = ActionCall{actions.make(kCopyField, *card)};
		pasteAction = ActionCall{actions.make(kPasteField, *card)};
		resetAction = ActionCall{actions.make(kResetField, *card)};
		canPaste = card->editable && interfaceState->editorClipboard &&
			interfaceState->editorClipboard.type == card->fieldType;
		const devMode::DevSchemaView schema = app->devTooling().schemas().view();
		canReset = card->editable && schema && currentOverride(*app, *schema, *card);
	}

	Clay_ElementDeclaration header{};
	header.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(38),
	};
	header.layout.padding = Clay_Padding{9, 7, 0, 0};
	header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	header.layout.childGap = 5;
	header.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	header.backgroundColor = interface_theme::kDepth2Ink;
	CLAY(context.clayID(), header) {
		const Clay_TextElementConfig nameStyle = textConfig(interface_theme::kTextCanvas, 11);
		CLAY_TEXT(
			context.uiManager.toClayString(card->fieldName),
			CLAY_TEXT_CONFIG(nameStyle));
		if (card->collapsed && !context.params.quickValue.empty()) {
			const Clay_TextElementConfig quickStyle = textConfig(interface_theme::kTextMuted, 9);
			CLAY(context.clayID(kQuickValue), Clay_ElementDeclaration{
				.layout = {.sizing = {
					.width = CLAY_SIZING_FIT(0),
					.height = CLAY_SIZING_FIT(0),
				}},
			}) {
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.quickValue),
					CLAY_TEXT_CONFIG(quickStyle));
			}
		}
		Clay_ElementDeclaration spacer{};
		spacer.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(1),
		};
		CLAY(context.clayID(kHeaderSpacer), spacer);
		drawHeaderButton(context, kCollapse, card->collapsed ? "↓" : "↑", collapseAction, true);
		drawHeaderButton(context, kCopy, "C", copyAction, card->hasCurrentValue);
		drawHeaderButton(context, kPaste, "P", pasteAction, canPaste);
		drawHeaderButton(context, kReset, "R", resetAction, canReset);
	}
}

void DevEditingField::buildElement(BuildContext& context) {
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* parentField = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* type = parentField && schema
		? schema->type(parentField->valueType) : nullptr;
	const bool arbitraryStruct = type && type->kind == devMode::DevTypeKind::Object &&
		editorKind(parentField, type) == devMode::DevEditorKind::ObjectGroup &&
		!schema->fieldsOf(parentField->valueType).empty();
	const uint16_t inset = context.params.depth == 0u ? 8u
		: context.params.depth == 1u ? 4u : 0u;

	Clay_ElementDeclaration field{};
	field.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	field.layout.padding = Clay_Padding{inset, inset, inset, inset};
	field.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	field.layout.childGap = 6;
	field.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), field) {
		if (!schema || !parentField || !type || !arbitraryStruct) {
			context.uiManager.createElement(kDevEditor, kEditor)
				.setParameters(DevEditorParameters{
					.schema = context.params.schema,
					.binding = context.params.binding,
					.app = context.params.app,
					.interfaceState = context.params.interfaceState,
					.value = context.params.value,
					.showFieldName = false,
				})
				.setDevInternalCapture(true)
				.draw();
		} else {
			for (const devMode::DevFieldSchema& child : schema->fieldsOf(parentField->valueType)) {
				const devMode::DevTypeSchema* childType = schema->type(child.valueType);
				const void* childValue = nullptr;
				if (context.params.value && child.operations < schema->fieldOperations.size()) {
					const devMode::DevFieldOps* operations = schema->fieldOperations[child.operations];
					if (operations && operations->constAddress) {
						childValue = operations->constAddress(context.params.value);
					}
				}
				DevEditorBinding childBinding = context.params.binding;
				childBinding.nestedPath.push_back(parentField->id);
				childBinding.field = fieldIndex(*schema, child);
				const bool nestedStruct = childType &&
					childType->kind == devMode::DevTypeKind::Object &&
					editorKind(&child, childType) == devMode::DevEditorKind::ObjectGroup &&
					!schema->fieldsOf(child.valueType).empty();
				if (nestedStruct) {
					context.uiManager.createElement(kDevEditorCard, Keyed(kNestedCard, child.id))
						.setParameters(DevEditorCardParameters{
							.schema = context.params.schema,
							.binding = std::move(childBinding),
							.app = context.params.app,
							.interfaceState = context.params.interfaceState,
							.value = childValue,
							.depth = static_cast<uint16_t>(context.params.depth + 1u),
						})
						.setDevInternalCapture(true)
						.draw();
				} else {
					context.uiManager.createElement(kDevEditor, Keyed(kEditor, child.id))
						.setParameters(DevEditorParameters{
							.schema = context.params.schema,
							.binding = std::move(childBinding),
							.app = context.params.app,
							.interfaceState = context.params.interfaceState,
							.value = childValue,
							.showFieldName = true,
						})
						.setDevInternalCapture(true)
						.draw();
				}
			}
		}
	}
}

void DevEditor::buildElement(BuildContext& context) {
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* field = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* type = field && schema
		? schema->type(field->valueType) : nullptr;
	std::string_view fieldName = field && schema ? schema->string(field->displayName) : std::string_view{};
	if (fieldName.empty() && field && schema) fieldName = schema->string(field->name);
	const std::string_view typeName = schema
		? schemaTypeName(*schema, type) : std::string_view{"Unknown"};
	const std::string typeLabel = std::string(typeName) + ":";

	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(32),
	};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childGap = 7;
	row.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	CLAY(context.clayID(), row) {
		if (context.params.showFieldName && !fieldName.empty()) {
			const Clay_TextElementConfig nameStyle = textConfig(interface_theme::kTextSecondary, 10);
			CLAY_TEXT(context.uiManager.toClayString(fieldName), CLAY_TEXT_CONFIG(nameStyle));
		}
		const Clay_TextElementConfig typeStyle = textConfig(interface_theme::kAccentSeaGlass, 9);
		CLAY_TEXT(context.uiManager.toClayString(typeLabel), CLAY_TEXT_CONFIG(typeStyle));
		context.uiManager.createElement(kDevTypeEditor, kTypeEditor)
			.setParameters(DevTypeEditorParameters{
				.schema = context.params.schema,
				.binding = context.params.binding,
				.app = context.params.app,
				.interfaceState = context.params.interfaceState,
				.value = context.params.value,
			})
			.setDevInternalCapture(true)
			.draw();
	}
}

void DevTypeEditor::buildElement(BuildContext& context) {
	Clay_ElementDeclaration stub{};
	stub.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(26),
	};
	stub.layout.padding = Clay_Padding{7, 7, 0, 0};
	stub.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	stub.backgroundColor = interface_theme::kDepth3Elevated;
	stub.cornerRadius = CLAY_CORNER_RADIUS(3);
	stub.border = {
		.color = interface_theme::kBorderVisible,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	const Clay_TextElementConfig style = textConfig(interface_theme::kTextMuted, 9);
	CLAY(context.clayID(), stub) {
		CLAY_TEXT(context.uiManager.toClayString("Type editor pending"), CLAY_TEXT_CONFIG(style));
	}
}

void DevInterfaceInspectorFields::buildElement(BuildContext& context) {
	App* app = context.params.app;
	DevInterfaceState* state = context.params.interfaceState;
	if (state &&
		state->inspectInspectorTab > static_cast<uint64_t>(DevInspectorTab::Changes)) {
		state->inspectInspectorTab = static_cast<uint64_t>(DevInspectorTab::Parameters);
	}
	const DevInspectorTab tab = state
		? static_cast<DevInspectorTab>(state->inspectInspectorTab)
		: DevInspectorTab::Parameters;
	const devMode::DevSchemaView schema = app
		? app->devTooling().schemas().view() : devMode::DevSchemaView{};
	const devMode::DevElementSchema* element = schema
		? schema->findElement(context.params.definition) : nullptr;

	devMode::DevTypeIndex rootType{};
	std::string_view emptyMessage{};
	if (element) {
		switch (tab) {
		case DevInspectorTab::Parameters:
			rootType = element->parametersType;
			emptyMessage = "This element has no parameter fields";
			break;
		case DevInspectorTab::State:
			rootType = element->stateType;
			emptyMessage = "This element does not have state";
			break;
		case DevInspectorTab::Resources:
			rootType = element->resourcesType;
			emptyMessage = "This element does not have resources";
			break;
		case DevInspectorTab::Changes:
			break;
		}
	}
	const std::span<const devMode::DevFieldSchema> fields = schema && rootType
		? schema->fieldsOf(rootType) : std::span<const devMode::DevFieldSchema>{};
	const devMode::DevTypeSchema* rootSchema = schema
		? schema->type(rootType) : nullptr;
	const devMode::DevTypeId rootOwnerType = rootSchema ? rootSchema->id : 0u;
	const tooling::DevElementCaptureSnapshot* capture = app && state
		? &app->devTooling().overrides().elementSnapshot(state->selectedWindowId)
		: nullptr;
	const tooling::DevCapturedElement* capturedElement = nullptr;
	if (capture) {
		const auto found = std::ranges::find_if(
			capture->elements,
			[&context](const tooling::DevCapturedElement& captured) {
				return captured.definition == context.params.definition &&
					captured.instance == context.params.instance;
			});
		if (found != capture->elements.end()) capturedElement = &*found;
	}
	const auto capturedValue = [&](const devMode::DevFieldSchema& field) -> const void* {
		if (!schema || !capture || !capturedElement) return nullptr;
		const devMode::DevFieldIndex wanted = fieldIndex(*schema, field);
		const std::size_t begin = capturedElement->firstField;
		const std::size_t end = std::min<std::size_t>(
			capture->fields.size(), begin + capturedElement->fieldCount);
		for (std::size_t index = begin; index < end; ++index) {
			if (capture->fields[index].field == wanted) {
				return capture->fields[index].value.data();
			}
		}
		return nullptr;
	};
	const std::vector<tooling::DevBakeDiffEntry> changes =
		app && tab == DevInspectorTab::Changes
			? app->devTooling().queryBakeDiff()
			: std::vector<tooling::DevBakeDiffEntry>{};

	Clay_ElementDeclaration surface{};
	surface.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	surface.backgroundColor = interface_theme::kDepth1Panel;
	const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(context.clayID());
	surface.clip = {
		.vertical = true,
		.childOffset = scroll.found && scroll.scrollPosition
			? *scroll.scrollPosition : Clay_Vector2{},
	};

	CLAY(context.clayID(), surface) {
		if (!app || !state) {
			drawMessage(context, "Inspector data is unavailable");
		} else if (!schema || !element) {
			drawMessage(context, "No schema exists for this element");
		} else {
			Clay_ElementDeclaration cards{};
			cards.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			cards.layout.padding = Clay_Padding{9, 9, 9, 9};
			cards.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			cards.layout.childGap = 7;
			CLAY(context.clayID(kCards), cards) {
				if (tab == DevInspectorTab::Changes) {
					bool drewChange = false;
					uint64_t changeIndex = 1u;
					for (const tooling::DevBakeDiffEntry& change : changes) {
						if (change.targetKind != tooling::DevBakeTargetKind::Element ||
							change.definition != context.params.definition ||
							(change.instance && change.instance != context.params.instance)) continue;
						const auto found = std::ranges::find_if(
							schema->fields,
							[&change](const devMode::DevFieldSchema& field) {
								return field.id == change.fieldId;
							});
						if (found == schema->fields.end()) continue;
						drawCard(
							context, schema, *found, DevEditorRole::Changes,
							rootOwnerType, nullptr, change.fieldPath,
							found->id ^ (changeIndex++ * 0x9e3779b97f4a7c15ull));
						drewChange = true;
					}
					if (!drewChange) drawMessage(context, "This element has no changes");
				} else if (!rootType || fields.empty()) {
					drawMessage(context, emptyMessage);
				} else {
					const DevEditorRole role = tab == DevInspectorTab::Parameters
						? DevEditorRole::Parameters
						: tab == DevInspectorTab::State
							? DevEditorRole::State : DevEditorRole::Resources;
					for (const devMode::DevFieldSchema& field : fields) {
						drawCard(
							context, schema, field, role, rootOwnerType,
							tab == DevInspectorTab::Parameters
								? capturedValue(field) : nullptr);
					}
				}
			}
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
