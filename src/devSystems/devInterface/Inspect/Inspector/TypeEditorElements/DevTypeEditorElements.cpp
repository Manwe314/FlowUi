#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "FSEL/Button.hpp"
#include "FSEL/ComboBox.hpp"
#include "FSEL/DragValue.hpp"
#include "FSEL/RadioChoice.hpp"
#include "FSEL/Slider.hpp"
#include "FSEL/Switch.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/UiManager.hpp"
#include "managers/ImageManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr float kTitleHeight = 36.0f;
inline constexpr float kIdentityHeight = 68.0f;
inline constexpr float kTabsHeight = 38.0f;
inline constexpr LocalElementName kTabCard{"tab"};
inline constexpr LocalElementName kCards{"cards"};
inline constexpr LocalElementName kEditorCard{"editor-card"};
inline constexpr LocalElementName kCardHeader{"header"};
inline constexpr LocalElementName kFieldName{"field-name"};
inline constexpr LocalElementName kSemanticType{"semantic-type"};
inline constexpr LocalElementName kQuickValue{"quick-value"};
inline constexpr LocalElementName kHeaderSpacer{"header-spacer"};
inline constexpr LocalElementName kEditingField{"editing-field"};
inline constexpr LocalElementName kEditor{"editor"};
inline constexpr LocalElementName kNestedCard{"nested-card"};
inline constexpr LocalElementName kTypeName{"type-name"};
inline constexpr LocalElementName kTypeEditor{"type-editor"};
inline constexpr LocalElementName kEditorControl{"control"};
inline constexpr LocalElementName kEditorMessage{"editor-message"};
inline constexpr LocalElementName kSemanticChild{"semantic-child"};
inline constexpr LocalElementName kSequenceUp{"sequence-up"};
inline constexpr LocalElementName kSequenceDown{"sequence-down"};
inline constexpr LocalElementName kSequenceRemove{"sequence-remove"};
inline constexpr uint16_t kMaximumEditorDepth = 8u;
inline constexpr std::size_t kMaximumVisibleSequenceItems = 64u;
inline constexpr uint16_t kTypeControlFontSize = 12u;
inline constexpr uint16_t kTypeControlHorizontalPadding = 8u;
inline constexpr uint16_t kTypeControlVerticalPadding = 6u;
inline constexpr float kTypeControlHeight =
	static_cast<float>(kTypeControlFontSize) * 1.5f +
	static_cast<float>(kTypeControlVerticalPadding * 2u) + 2.0f;
inline constexpr uint64_t kColorSemanticMode =
	std::numeric_limits<uint64_t>::max() - 1u;

inline constexpr Clay_Padding kTypeControlPadding{
	kTypeControlHorizontalPadding,
	kTypeControlHorizontalPadding,
	kTypeControlVerticalPadding,
	kTypeControlVerticalPadding,
};

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

inline constexpr std::array<FSEL::ComboBoxOption, 4> kSizingAxisOptions{{
	{.value = CLAY__SIZING_TYPE_FIT, .text = "Fit"},
	{.value = CLAY__SIZING_TYPE_GROW, .text = "Grow"},
	{.value = CLAY__SIZING_TYPE_PERCENT, .text = "Percent"},
	{.value = CLAY__SIZING_TYPE_FIXED, .text = "Fixed"},
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

std::string normalizedTypeName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type) {
	if (!type) return "Unknown";
	if (type->kind == devMode::DevTypeKind::Optional) {
		return "Optional " + normalizedTypeName(schema, schema.type(type->elementType));
	}
	switch (type->kind) {
	case devMode::DevTypeKind::Boolean: return "Boolean";
	case devMode::DevTypeKind::SignedInteger: return "Integer";
	case devMode::DevTypeKind::UnsignedInteger: return "Unsigned Integer";
	case devMode::DevTypeKind::FloatingPoint: return "Float";
	case devMode::DevTypeKind::Text: return "Text";
	case devMode::DevTypeKind::Sequence:
		return "Sequence of " + normalizedTypeName(schema, schema.type(type->elementType));
	default: break;
	}
	std::string name(schemaTypeName(schema, type));
	if (name.starts_with("std::")) name.erase(0, 5);
	if (name.starts_with("FlowUi::")) name.erase(0, 8);
	return name;
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
		if (!operations->optionalHasValue(value)) return "NullOpt";
		return quickValue(
			schema, schema.type(type->elementType),
			operations->optionalValueAddress(value));
	}
	const std::string_view name = schemaTypeName(schema, type);
	char buffer[128]{};
	if (name == "Clay_Padding") {
		const auto& padding = *static_cast<const Clay_Padding*>(value);
		if (padding.left == padding.right && padding.left == padding.top &&
			padding.left == padding.bottom) {
			std::snprintf(buffer, sizeof(buffer), "%u all", padding.left);
			return buffer;
		}
		std::snprintf(buffer, sizeof(buffer), "L%u R%u T%u B%u",
			padding.left, padding.right, padding.top, padding.bottom);
		return buffer;
	}
	if (name == "Clay_BorderWidth") {
		const auto& width = *static_cast<const Clay_BorderWidth*>(value);
		if (width.left == width.right && width.left == width.top &&
			width.left == width.bottom) {
			std::snprintf(buffer, sizeof(buffer), "%u all · Between %u",
				width.left, width.betweenChildren);
		} else {
			std::snprintf(buffer, sizeof(buffer), "L%u R%u T%u B%u · Between %u",
				width.left, width.right, width.top, width.bottom, width.betweenChildren);
		}
		return buffer;
	}
	if (name == "Clay_Color") {
		const auto& color = *static_cast<const Clay_Color*>(value);
		std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
			static_cast<unsigned>(std::clamp(color.r, 0.0f, 255.0f)),
			static_cast<unsigned>(std::clamp(color.g, 0.0f, 255.0f)),
			static_cast<unsigned>(std::clamp(color.b, 0.0f, 255.0f)),
			static_cast<unsigned>(std::clamp(color.a, 0.0f, 255.0f)));
		return buffer;
	}
	if (type->kind == devMode::DevTypeKind::Boolean) {
		const std::size_t typeIndex = static_cast<std::size_t>(type - schema.types.data());
		const devMode::DevTypeOps* operations = typeIndex < schema.typeOperations.size()
			? schema.typeOperations[typeIndex] : nullptr;
		long double numeric = 0.0L;
		return operations && operations->numericValue && operations->numericValue(value, numeric)
			? (numeric == 0.0L ? "Off" : "On") : std::string{};
	}
	if (name == "Clay_CornerRadius") {
		const auto& radius = *static_cast<const Clay_CornerRadius*>(value);
		if (radius.topLeft == radius.topRight && radius.topLeft == radius.bottomRight &&
			radius.topLeft == radius.bottomLeft) {
			std::snprintf(buffer, sizeof(buffer), "%.0f all", radius.topLeft);
		} else {
			std::snprintf(buffer, sizeof(buffer), "TL%.0f TR%.0f BR%.0f BL%.0f",
				radius.topLeft, radius.topRight, radius.bottomRight, radius.bottomLeft);
		}
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
	if (name == "Clay_SizingAxis") {
		const auto& axis = *static_cast<const Clay_SizingAxis*>(value);
		switch (axis.type) {
		case CLAY__SIZING_TYPE_FIT:
			std::snprintf(buffer, sizeof(buffer), "Fit %.0f–%.0f",
				axis.size.minMax.min, axis.size.minMax.max);
			break;
		case CLAY__SIZING_TYPE_GROW:
			std::snprintf(buffer, sizeof(buffer), "Grow %.0f–%.0f",
				axis.size.minMax.min, axis.size.minMax.max);
			break;
		case CLAY__SIZING_TYPE_PERCENT:
			std::snprintf(buffer, sizeof(buffer), "%.0f%%", axis.size.percent * 100.0f);
			break;
		case CLAY__SIZING_TYPE_FIXED:
			std::snprintf(buffer, sizeof(buffer), "Fixed %.0f px", axis.size.minMax.min);
			break;
		}
		return buffer;
	}
	if (name == "Clay_Sizing") {
		const auto& sizing = *static_cast<const Clay_Sizing*>(value);
		auto mode = [](Clay__SizingType type) -> std::string_view {
			switch (type) {
			case CLAY__SIZING_TYPE_FIT: return "Fit";
			case CLAY__SIZING_TYPE_GROW: return "Grow";
			case CLAY__SIZING_TYPE_PERCENT: return "%";
			case CLAY__SIZING_TYPE_FIXED: return "Fixed";
			}
			return "?";
		};
		std::snprintf(buffer, sizeof(buffer), "W %.*s · H %.*s",
			static_cast<int>(mode(sizing.width.type).size()), mode(sizing.width.type).data(),
			static_cast<int>(mode(sizing.height.type).size()), mode(sizing.height.type).data());
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

tooling::DevOverrideCommand editorFieldCommand(
	const devMode::DevSchemaGeneration& schema,
	const DevTypeEditorState& editor,
	tooling::DevOverrideCommandKind kind,
	tooling::DevOwnedValue value = {}) {
	const devMode::DevFieldSchema* field = fieldAt(schema, editor.binding.field);
	tooling::DevOverrideCommand command{};
	command.kind = kind;
	command.element = tooling::DevElementOverrideTarget{
		.definition = editor.binding.definition,
		.window = editor.binding.window,
		.instance = editor.binding.instance,
		.instanceDebugLabel = editor.binding.elementName,
		.scope = tooling::DevOverrideScope::ExactInstance,
		.bakeable = true,
	};
	command.field = tooling::DevOverrideFieldKey{
		.ownerType = editor.binding.rootOwnerType,
		.field = field ? field->id : 0u,
		.nestedPath = editor.binding.nestedPath,
	};
	command.layer = tooling::DevOverrideLayer::LiveInstance;
	command.value = std::move(value);
	return command;
}

bool updateEditorPreview(DevTypeEditorState& editor) {
	if (!editor.app || !editor.interfaceState || !editor.editable ||
		!editor.currentValue) return false;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	if (!schema) return false;
	tooling::DevChangeSet changes{};
	changes.transaction = editor.interfaceState->nextEditTransaction++;
	changes.commands.push_back(editorFieldCommand(
		*schema,
		editor,
		editor.previewActive
			? tooling::DevOverrideCommandKind::UpdateBatchDrag
			: tooling::DevOverrideCommandKind::BeginBatchDrag,
		editor.currentValue));
	if (!editor.app->devTooling().overrides().submit(std::move(changes))) return false;
	editor.previewActive = true;
	return true;
}

void cancelEditorPreview(DevTypeEditorState& editor) {
	if (!editor.previewActive || !editor.app || !editor.interfaceState) return;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	if (!schema) return;
	tooling::DevOverrideCommand clear = editorFieldCommand(
		*schema, editor, tooling::DevOverrideCommandKind::ClearElementField);
	clear.layer = tooling::DevOverrideLayer::EphemeralPreview;
	tooling::DevChangeSet changes{};
	changes.transaction = editor.interfaceState->nextEditTransaction++;
	changes.commands.push_back(std::move(clear));
	if (editor.app->devTooling().overrides().submit(std::move(changes))) {
		editor.previewActive = false;
	}
}

const tooling::DevOverrideApply::Record* currentEditorOverride(
	const App& app,
	const devMode::DevSchemaGeneration& schema,
	const DevTypeEditorState& editor) {
	const tooling::DevOverrideCommand key = editorFieldCommand(
		schema, editor, tooling::DevOverrideCommandKind::ClearElementField);
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	const auto found = std::ranges::find_if(records,
		[&key](const tooling::DevOverrideApply::Record& record) {
			return record.target == key.element && record.field == key.field &&
				record.layer == key.layer;
		});
	return found == records.end() ? nullptr : &*found;
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

bool cardHasOverride(
	const App& app,
	const devMode::DevSchemaGeneration& schema,
	const DevEditorCardState& card) {
	const tooling::DevOverrideCommand key = fieldCommand(
		schema, card, tooling::DevOverrideCommandKind::ClearElementField);
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	return std::ranges::any_of(records,
		[&](const tooling::DevOverrideApply::Record& record) {
			if (!(record.target == key.element) || record.layer != key.layer) return false;
			if (record.field.ownerType != key.field.ownerType) return false;
			if (record.field.nestedPath.size() < key.field.nestedPath.size()) return false;
			if (!std::equal(key.field.nestedPath.begin(), key.field.nestedPath.end(),
				record.field.nestedPath.begin())) return false;
			if (record.field.nestedPath.size() == key.field.nestedPath.size()) {
				return record.field.field == key.field.field;
			}
			return record.field.nestedPath[key.field.nestedPath.size()] == key.field.field;
		});
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

bool commitEditorValue(DevTypeEditorState& editor, std::string description) {
	if (!editor.app || !editor.interfaceState || !editor.editable ||
		!editor.currentValue) return false;
	App& app = *editor.app;
	DevInterfaceState& state = *editor.interfaceState;
	const devMode::DevSchemaView schema = app.devTooling().schemas().view();
	if (!schema) return false;
	DevInterfaceEditTransaction transaction{};
	transaction.description = std::move(description);
	transaction.forward.push_back(editorFieldCommand(
		*schema, editor,
		editor.previewActive
			? tooling::DevOverrideCommandKind::EndBatchDrag
			: tooling::DevOverrideCommandKind::SetElementField,
		editor.currentValue));
	if (const tooling::DevOverrideApply::Record* prior =
		currentEditorOverride(app, *schema, editor)) {
		transaction.inverse.push_back(editorFieldCommand(
			*schema, editor, tooling::DevOverrideCommandKind::SetElementField,
			prior->value));
	} else {
		transaction.inverse.push_back(editorFieldCommand(
			*schema, editor, tooling::DevOverrideCommandKind::ClearElementField));
	}
	if (!submitEditTransaction(app, state, std::move(transaction))) return false;
	editor.previewActive = false;
	state.lastActionMessage = editor.binding.elementName + " · " +
		editor.fieldName + " updated";
	return true;
}

const devMode::DevTypeOps* typeOperations(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevTypeIndex index) {
	return index.value < schema.typeOperations.size()
		? schema.typeOperations[index.value] : nullptr;
}

std::string numericDraft(
	const devMode::DevTypeSchema& type,
	const devMode::DevTypeOps* operations,
	const void* value) {
	long double numeric = 0.0L;
	if (!operations || !operations->numericValue ||
		!operations->numericValue(value, numeric)) return {};
	std::ostringstream stream;
	if (type.kind == devMode::DevTypeKind::FloatingPoint) {
		stream << std::setprecision(std::numeric_limits<long double>::digits10) << numeric;
	} else {
		stream << std::fixed << std::setprecision(0) << numeric;
	}
	return stream.str();
}

bool parseNumericDraft(std::string_view draft, long double& value) {
	if (draft.empty()) return false;
	std::string storage(draft);
	char* end = nullptr;
	errno = 0;
	value = std::strtold(storage.c_str(), &end);
	return errno != ERANGE && end == storage.c_str() + storage.size() &&
		std::isfinite(value);
}

bool applyEditorDraft(DevTypeEditorState& editor) {
	if (!editor.app || !editor.currentValue || !editor.editValue) return false;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	const devMode::DevTypeSchema* type = schema ? schema->type(editor.operationType) : nullptr;
	const devMode::DevTypeOps* operations = schema
		? typeOperations(*schema, editor.operationType) : nullptr;
	if (!type || !operations) return false;
	devMode::DevValueOperationStatus status = devMode::DevValueOperationStatus::Unsupported;
	if (type->kind == devMode::DevTypeKind::Text && operations->assignTextValue) {
		status = operations->assignTextValue(editor.editValue, editor.draft);
	} else if (operations->assignNumericValue) {
		long double candidate = 0.0L;
		if (parseNumericDraft(editor.draft, candidate)) {
			status = operations->assignNumericValue(editor.editValue, candidate);
		}
	}
	editor.draftValid = status == devMode::DevValueOperationStatus::Success;
	return editor.draftValid;
}

constexpr auto kPreviewEditorDraft = UiAction(
	"flowui.dev_interface.type-editor.preview-draft",
	[](DevTypeEditorState& editor) {
		if (applyEditorDraft(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitEditorDraft = UiAction(
	"flowui.dev_interface.type-editor.commit-draft",
	[](DevTypeEditorState& editor) {
		if (applyEditorDraft(editor)) {
			(void)commitEditorValue(editor, "Edit " + editor.fieldName);
		} else {
			cancelEditorPreview(editor);
		}
	});

bool applyEditorDragValue(DevTypeEditorState& editor) {
	if (!editor.app || !editor.currentValue || !editor.editValue) return false;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	const devMode::DevTypeSchema* type = schema ? schema->type(editor.operationType) : nullptr;
	const devMode::DevTypeOps* operations = schema
		? typeOperations(*schema, editor.operationType) : nullptr;
	if (!type || !operations || !operations->assignNumericValue) return false;

	long double candidate = 0.0L;
	switch (type->kind) {
	case devMode::DevTypeKind::SignedInteger:
		candidate = static_cast<long double>(editor.dragSigned);
		break;
	case devMode::DevTypeKind::UnsignedInteger:
		candidate = static_cast<long double>(editor.dragUnsigned);
		break;
	case devMode::DevTypeKind::FloatingPoint:
		candidate = static_cast<long double>(editor.dragFloat);
		break;
	default:
		return false;
	}
	return operations->assignNumericValue(editor.editValue, candidate) ==
		devMode::DevValueOperationStatus::Success;
}

constexpr auto kPreviewEditorDragValue = UiAction(
	"flowui.dev_interface.type-editor.preview-drag-value",
	[](DevTypeEditorState& editor) {
		if (applyEditorDragValue(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitEditorDragValue = UiAction(
	"flowui.dev_interface.type-editor.commit-drag-value",
	[](DevTypeEditorState& editor) {
		if (applyEditorDragValue(editor)) {
			(void)commitEditorValue(editor, "Edit " + editor.fieldName);
		}
	});

constexpr auto kCancelEditorDragValue = UiAction(
	"flowui.dev_interface.type-editor.cancel-drag-value",
	[](DevTypeEditorState& editor) {
		(void)applyEditorDragValue(editor);
		cancelEditorPreview(editor);
	});

constexpr auto kToggleEditorValue = UiAction(
	"flowui.dev_interface.type-editor.toggle",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeOps* operations = schema
			? typeOperations(*schema, editor.operationType) : nullptr;
		long double current = 0.0L;
		if (!operations || !operations->numericValue || !operations->assignNumericValue ||
			!operations->numericValue(editor.editValue, current)) return;
		if (operations->assignNumericValue(editor.editValue, current == 0.0L ? 1.0L : 0.0L) ==
			devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Toggle " + editor.fieldName);
		}
	});

constexpr auto kChooseEditorEnum = UiAction(
	"flowui.dev_interface.type-editor.choose-enum",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeOps* operations = schema
			? typeOperations(*schema, editor.operationType) : nullptr;
		if (operations && operations->assignNumericValue &&
			operations->assignNumericValue(editor.editValue,
				static_cast<long double>(editor.enumSelection)) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Choose " + editor.fieldName);
		}
	});

constexpr auto kToggleEditorFlag = UiAction(
	"flowui.dev_interface.type-editor.toggle-flag",
	[](DevTypeEditorState& editor, uint64_t mask) {
		if (!editor.app || !editor.currentValue) return;
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.operationType) : nullptr;
		long double current = 0.0L;
		if (!operations || !operations->numericValue || !operations->assignNumericValue ||
			!operations->numericValue(editor.editValue, current)) return;
		const uint64_t replacement = static_cast<uint64_t>(current) ^ mask;
		if (operations->assignNumericValue(editor.editValue, static_cast<long double>(replacement)) ==
			devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Toggle flag in " + editor.fieldName);
		}
	});

constexpr auto kToggleOptional = UiAction(
	"flowui.dev_interface.type-editor.toggle-optional",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeOps* operations = schema
			? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (!operations || !operations->optionalHasValue || !operations->setOptionalPresence) return;
		const bool present = operations->optionalHasValue(editor.currentValue.data());
		if (operations->setOptionalPresence(editor.currentValue.data(), !present) ==
			devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, present ? "Clear " + editor.fieldName
				: "Enable " + editor.fieldName);
		}
	});

constexpr auto kChooseSizingAxisMode = UiAction(
	"flowui.dev_interface.type-editor.sizing-axis-mode",
	[](DevTypeEditorState& editor) {
		if (!editor.currentValue) return;
		auto& axis = *static_cast<Clay_SizingAxis*>(editor.currentValue.data());
		const auto type = static_cast<Clay__SizingType>(editor.enumSelection);
		if (axis.type == type) return;
		switch (type) {
		case CLAY__SIZING_TYPE_FIT: axis = CLAY_SIZING_FIT(0); break;
		case CLAY__SIZING_TYPE_GROW: axis = CLAY_SIZING_GROW(0); break;
		case CLAY__SIZING_TYPE_PERCENT: axis = CLAY_SIZING_PERCENT(0.5f); break;
		case CLAY__SIZING_TYPE_FIXED: axis = CLAY_SIZING_FIXED(100.0f); break;
		}
		editor.semanticMode = std::numeric_limits<uint64_t>::max();
		(void)commitEditorValue(editor, "Change sizing mode for " + editor.fieldName);
	});

bool applySizingAxisValues(DevTypeEditorState& editor) {
	if (!editor.currentValue || !editor.editValue) return false;
	auto& axis = *static_cast<Clay_SizingAxis*>(editor.editValue);
	switch (axis.type) {
	case CLAY__SIZING_TYPE_PERCENT:
		editor.semanticSliderValue = std::clamp(
			editor.semanticSliderValue, 0.0, 100.0);
		axis.size.percent = static_cast<float>(editor.semanticSliderValue / 100.0);
		break;
	case CLAY__SIZING_TYPE_FIXED:
		editor.semanticValueA = std::max(editor.semanticValueA, 0.0f);
		axis.size.minMax = {editor.semanticValueA, editor.semanticValueA};
		break;
	case CLAY__SIZING_TYPE_FIT:
	case CLAY__SIZING_TYPE_GROW:
		editor.semanticValueA = std::max(editor.semanticValueA, 0.0f);
		editor.semanticValueB = std::max(editor.semanticValueB, editor.semanticValueA);
		axis.size.minMax = {editor.semanticValueA, editor.semanticValueB};
		break;
	}
	return true;
}

constexpr auto kPreviewSizingAxisValues = UiAction(
	"flowui.dev_interface.type-editor.preview-sizing-axis-values",
	[](DevTypeEditorState& editor) {
		if (applySizingAxisValues(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitSizingAxisValues = UiAction(
	"flowui.dev_interface.type-editor.sizing-axis-values",
	[](DevTypeEditorState& editor) {
		if (!applySizingAxisValues(editor)) return;
		(void)commitEditorValue(editor, "Edit sizing for " + editor.fieldName);
	});

constexpr auto kCancelSizingAxisValues = UiAction(
	"flowui.dev_interface.type-editor.cancel-sizing-axis-values",
	[](DevTypeEditorState& editor) {
		(void)applySizingAxisValues(editor);
		cancelEditorPreview(editor);
	});

void updateColorHexDraft(DevTypeEditorState& editor) {
	char hex[16]{};
	std::snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
		static_cast<unsigned>(std::clamp(editor.semanticChannels[0], 0.0, 255.0)),
		static_cast<unsigned>(std::clamp(editor.semanticChannels[1], 0.0, 255.0)),
		static_cast<unsigned>(std::clamp(editor.semanticChannels[2], 0.0, 255.0)),
		static_cast<unsigned>(std::clamp(editor.semanticChannels[3], 0.0, 255.0)));
	editor.semanticText = hex;
}

bool applyColorChannels(DevTypeEditorState& editor) {
	if (!editor.currentValue || !editor.editValue) return false;
	auto& color = *static_cast<Clay_Color*>(editor.editValue);
	color = Clay_Color{
		static_cast<float>(std::clamp(editor.semanticChannels[0], 0.0, 255.0)),
		static_cast<float>(std::clamp(editor.semanticChannels[1], 0.0, 255.0)),
		static_cast<float>(std::clamp(editor.semanticChannels[2], 0.0, 255.0)),
		static_cast<float>(std::clamp(editor.semanticChannels[3], 0.0, 255.0))};
	updateColorHexDraft(editor);
	editor.draftValid = true;
	return true;
}

constexpr auto kPreviewColorChannels = UiAction(
	"flowui.dev_interface.type-editor.preview-color-channels",
	[](DevTypeEditorState& editor) {
		if (applyColorChannels(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitColorChannels = UiAction(
	"flowui.dev_interface.type-editor.color-channels",
	[](DevTypeEditorState& editor) {
		if (!applyColorChannels(editor)) return;
		(void)commitEditorValue(editor, "Edit color for " + editor.fieldName);
	});

constexpr auto kToggleColorExpanded = UiAction(
	"flowui.dev_interface.type-editor.color-expanded",
	[](DevTypeEditorState& editor) {
		editor.colorExpanded = !editor.colorExpanded;
	});

bool applyColorHex(DevTypeEditorState& editor) {
	if (!editor.currentValue || !editor.editValue) return false;
	std::string_view text = editor.semanticText;
	if (!text.empty() && text.front() == '#') text.remove_prefix(1);
	if (text.size() != 6u && text.size() != 8u) {
		editor.draftValid = false;
		return false;
	}
	uint32_t packed = 0u;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), packed, 16);
	if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
		editor.draftValid = false;
		return false;
	}
	if (text.size() == 6u) packed = (packed << 8u) | 0xFFu;
	editor.semanticChannels = {
		static_cast<double>((packed >> 24u) & 0xFFu),
		static_cast<double>((packed >> 16u) & 0xFFu),
		static_cast<double>((packed >> 8u) & 0xFFu),
		static_cast<double>(packed & 0xFFu)};
	editor.draftValid = true;
	auto& color = *static_cast<Clay_Color*>(editor.editValue);
	color = Clay_Color{
		static_cast<float>(editor.semanticChannels[0]),
		static_cast<float>(editor.semanticChannels[1]),
		static_cast<float>(editor.semanticChannels[2]),
		static_cast<float>(editor.semanticChannels[3])};
	updateColorHexDraft(editor);
	return true;
}

constexpr auto kPreviewColorHex = UiAction(
	"flowui.dev_interface.type-editor.preview-color-hex",
	[](DevTypeEditorState& editor) {
		if (applyColorHex(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitColorHex = UiAction(
	"flowui.dev_interface.type-editor.color-hex",
	[](DevTypeEditorState& editor) {
		if (!applyColorHex(editor)) {
			cancelEditorPreview(editor);
			return;
		}
		(void)commitEditorValue(editor, "Edit color for " + editor.fieldName);
	});

constexpr auto kLinkSemanticQuad = UiAction(
	"flowui.dev_interface.type-editor.link-semantic-quad",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue || !editor.editValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeSchema* type = schema
			? schema->type(editor.operationType) : nullptr;
		const std::string_view name = schema && type ? schemaTypeName(*schema, type) : std::string_view{};
		if (name == "Clay_Padding") {
			auto& value = *static_cast<Clay_Padding*>(editor.editValue);
			value.right = value.top = value.bottom = value.left;
		} else if (name == "Clay_BorderWidth") {
			auto& value = *static_cast<Clay_BorderWidth*>(editor.editValue);
			value.right = value.top = value.bottom = value.left;
		} else if (name == "Clay_CornerRadius") {
			auto& value = *static_cast<Clay_CornerRadius*>(editor.editValue);
			value.topRight = value.bottomLeft = value.bottomRight = value.topLeft;
		} else {
			return;
		}
		(void)commitEditorValue(editor, "Link values for " + editor.fieldName);
	});

constexpr auto kAddSequenceItem = UiAction(
	"flowui.dev_interface.type-editor.sequence-add",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (operations && operations->sequenceAppendDefault &&
			operations->sequenceAppendDefault(editor.currentValue.data()) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Add item to " + editor.fieldName);
		}
	});

constexpr auto kRemoveSequenceItem = UiAction(
	"flowui.dev_interface.type-editor.sequence-remove",
	[](DevTypeEditorState& editor, std::size_t index) {
		if (!editor.app || !editor.currentValue) return;
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (operations && operations->sequenceErase &&
			operations->sequenceErase(editor.currentValue.data(), index) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Remove item from " + editor.fieldName);
		}
	});

constexpr auto kMoveSequenceItem = UiAction(
	"flowui.dev_interface.type-editor.sequence-move",
	[](DevTypeEditorState& editor, uint64_t packedMove) {
		if (!editor.app || !editor.currentValue) return;
		const std::size_t from = static_cast<std::size_t>(packedMove >> 32u);
		const std::size_t to = static_cast<std::size_t>(packedMove & 0xffffffffu);
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (operations && operations->sequenceMove &&
			operations->sequenceMove(editor.currentValue.data(), from, to) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Reorder " + editor.fieldName);
		}
	});

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
	FlowElementPart part,
	std::string_view label,
	ActionCall action,
	bool enabled,
	TextureRef icon = {}) {
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = action;
	parameters.enabled = enabled;
	parameters.contentMode = icon.handle
		? FSEL::ButtonContentMode::IconOnly : FSEL::ButtonContentMode::TextOnly;
	parameters.text = label;
	parameters.icon = icon;
	parameters.tintIcon = true;
	parameters.sizing = {
		.width = CLAY_SIZING_FIXED(24),
		.height = CLAY_SIZING_FIXED(24),
	};
	parameters.padding = Clay_Padding{0, 0, 0, 0};
	parameters.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextSecondary;
	parameters.idleOverrides.iconColor = interface_theme::kTextSecondary;
	parameters.idleOverrides.borderColor = interface_theme::kBorderVisible;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.iconColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.pressedOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.iconColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.borderColor = interface_theme::kAccentSeaGlass;
	parameters.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	parameters.disabledOverrides.labelColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.iconColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
	parameters.labelFontSize = 10;
	parameters.iconSize = 12.0f;
	context.createPart(FSEL::kButton, part)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
}

void drawSmallEditorButton(
	DevTypeEditor::BuildContext& context,
	LocalElementName id,
	uint64_t index,
	std::string_view label,
	ActionCall action) {
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = action;
	parameters.enabled = static_cast<bool>(action);
	parameters.text = label;
	parameters.sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(22),
		.height = CLAY_SIZING_FIXED(22),
	};
	parameters.padding = Clay_Padding{};
	parameters.labelFontSize = 9;
	context.uiManager.createElement(FSEL::kButton, Keyed(id, index))
		.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
}

void drawSequenceRows(
	DevTypeEditor::BuildContext& context,
	DevTypeEditorState& state,
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema& type,
	const devMode::DevTypeOps& operations) {
	const std::size_t totalCount = operations.sequenceSize
		? operations.sequenceSize(state.editValue) : 0u;
	const std::size_t count = std::min(totalCount, kMaximumVisibleSequenceItems);
	const devMode::DevTypeSchema* elementType = schema.type(type.elementType);
	const devMode::DevTypeOps* elementOps = typeOperations(schema, type.elementType);
	for (std::size_t index = 0; index < count; ++index) {
		const void* item = operations.sequenceElementAddress
			? operations.sequenceElementAddress(state.editValue, index) : nullptr;
		std::string summary = std::to_string(index) + "  ";
		if (elementType && item) {
			if (elementType->kind == devMode::DevTypeKind::Text && elementOps && elementOps->textView) {
				summary += std::string(elementOps->textView(item));
			} else {
				summary += numericDraft(*elementType, elementOps, item);
				if (summary.ends_with("  ")) summary += schemaTypeName(schema, elementType);
			}
		}
		ActionCall up{}, down{}, remove{};
		if (state.app && state.interfaceState && state.editable) {
			auto& actions = state.app->actions().uiActions();
			const uint64_t from = static_cast<uint64_t>(index) << 32u;
			if (index > 0) {
				const uint64_t move = from | static_cast<uint64_t>(index - 1);
				up = ActionCall{actions.make(kMoveSequenceItem, state, move)};
			}
			if (index + 1 < count) {
				const uint64_t move = from | static_cast<uint64_t>(index + 1);
				down = ActionCall{actions.make(kMoveSequenceItem, state, move)};
			}
			remove = ActionCall{actions.make(kRemoveSequenceItem, state, index)};
		}
		Clay_ElementDeclaration itemRow{};
		itemRow.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
		itemRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		itemRow.layout.childGap = 4;
		itemRow.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		itemRow.layout.padding = Clay_Padding{6, 4, 0, 0};
		itemRow.backgroundColor = interface_theme::kDepth3Elevated;
		itemRow.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(Keyed("sequence-row", index)), itemRow) {
			Clay_ElementDeclaration textRoot{};
			textRoot.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
			textRoot.clip = {.horizontal = true, .vertical = true};
			CLAY(context.clayID(Keyed("sequence-text", index)), textRoot) {
				const auto style = textConfig(interface_theme::kTextSecondary, 9);
				CLAY_TEXT(context.uiManager.toClayString(summary), CLAY_TEXT_CONFIG(style));
			}
			drawSmallEditorButton(context, kSequenceUp, index, "↑", up);
			drawSmallEditorButton(context, kSequenceDown, index, "↓", down);
			if (!type.sequenceFixed) drawSmallEditorButton(
				context, kSequenceRemove, index, "×", remove);
		}
	}
	if (totalCount > count) {
		Clay_TextElementConfig style = textConfig(interface_theme::kTextMuted, 9);
		style.wrapMode = CLAY_TEXT_WRAP_WORDS;
		const std::string remainder = std::to_string(totalCount - count) +
			" additional items are not rendered";
		CLAY_TEXT(context.uiManager.toClayString(remainder), CLAY_TEXT_CONFIG(style));
	}
	if (type.sequenceFixed) return;
	ActionCall add{};
	if (state.app && state.interfaceState && state.editable && operations.sequenceAppendDefault) {
		add = ActionCall{state.app->actions().uiActions().make(kAddSequenceItem, state)};
	}
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = add;
	parameters.enabled = static_cast<bool>(add);
	parameters.text = "Add item";
	parameters.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
	context.uiManager.createElement(FSEL::kButton, "sequence-add")
		.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
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

DevEditorCardHeaderResources::DevEditorCardHeaderResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	auto resolve = [&icons](std::string_view key) -> TextureRef {
		return icons.contains(key) ? icons.textureRef(key) : TextureRef{};
	};
	copyIcon = resolve(interface_icons::kCopyKey);
	pasteIcon = resolve(interface_icons::kPasteKey);
	revertIcon = resolve(interface_icons::kRevertKey);
	expandIcon = resolve(interface_icons::kExpandKey);
	collapseIcon = resolve(interface_icons::kCollapseKey);
#else
	(void)app;
#endif
}

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
	identity.clip = {.horizontal = true, .vertical = true};
	Clay_TextElementConfig instanceStyle = textConfig(interface_theme::kTextCanvas, 13);
	Clay_TextElementConfig definitionStyle = textConfig(interface_theme::kTextMuted, 10);
	instanceStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
	definitionStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
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
	state.typeName = schema ? normalizedTypeName(*schema, type) : "Unknown";
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
	state.dirty = schema && context.params.app &&
		cardHasOverride(*context.params.app, *schema, state);
	const std::string collapsedValue = state.collapsed && schema
		? quickValue(*schema, type, state.currentValue.data()) : std::string{};
	ActionCall collapseAction{};
	if (context.params.app && context.params.interfaceState) {
		collapseAction = ActionCall{context.params.app->actions().uiActions().make(
			kCollapseCard, state.collapsed)};
	}

	Clay_ElementDeclaration card{};
	card.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	card.backgroundColor = interface_theme::kDepth2Ink;
	card.clip = {.horizontal = true, .vertical = false};
	card.border = {
		.color = state.dirty
			? interface_theme::kAccentSignalCoral : interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	CLAY(context.clayID(), card) {
		context.uiManager.createElement(kDevEditorCardHeader, kCardHeader)
			.setParameters(DevEditorCardHeaderParameters{
				.app = context.params.app,
				.interfaceState = context.params.interfaceState,
				.cardState = &state,
				.quickValue = collapsedValue,
				.onToggle = collapseAction,
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

namespace {

template <typename Context, typename Predicate>
bool headerControlMatches(Context& context, Predicate&& predicate) {
	auto partClayID = [&](FlowElementPart part) {
		return context.uiManager.toClayEID(context.part(part));
	};
	return predicate(partClayID(DevEditorCardHeader::Parts::collapse)) ||
		predicate(partClayID(DevEditorCardHeader::Parts::copy)) ||
		predicate(partClayID(DevEditorCardHeader::Parts::paste)) ||
		predicate(partClayID(DevEditorCardHeader::Parts::reset));
}

void clearHeaderInteraction(DevEditorCardHeaderState& state) {
	state.isArmed = false;
	state.mouseUpObserved = false;
}

} // namespace

void DevEditorCardHeader::onHovered(InteractionContext& context) {
	if (context.actionAvailability(context.params.onToggle).enabled) {
		context.uiManager.requestCursor(CursorType::PointingHand, 9u);
	}
}

void DevEditorCardHeader::onPressed(InteractionContext& context) {
	auto& state = context.state();
	const bool childConsumed = headerControlMatches(context, [&](Clay_ElementId id) {
		return context.previousInteraction.isPressed(id);
	});
	if (childConsumed || !context.actionAvailability(context.params.onToggle).enabled) {
		clearHeaderInteraction(state);
		return;
	}
	state.isArmed = true;
	state.mouseUpObserved = false;
}

void DevEditorCardHeader::onReleased(InteractionContext& context) {
	auto& state = context.state();
	if (!state.isArmed) return;
	const bool childConsumed = headerControlMatches(context, [&](Clay_ElementId id) {
		return context.previousInteraction.isReleased(id);
	});
	const bool shouldToggle = !childConsumed &&
		context.actionAvailability(context.params.onToggle).enabled;
	clearHeaderInteraction(state);
	if (shouldToggle) (void)context.invoke(context.params.onToggle);
}

void DevEditorCardHeader::runLogic(InteractionContext& context) {
	auto& state = context.state();
	if (!state.isArmed) return;
	if (!context.actionAvailability(context.params.onToggle).enabled) {
		clearHeaderInteraction(state);
		return;
	}
	if (context.uiManager.getCurrentFrameInput().mouseDown[0]) {
		if (state.mouseUpObserved) clearHeaderInteraction(state);
		return;
	}
	if (state.mouseUpObserved) clearHeaderInteraction(state);
	else state.mouseUpObserved = true;
}

void DevEditorCardHeader::buildElement(BuildContext& context) {
	DevEditorCardState* card = context.params.cardState;
	if (!card) return;
	App* app = context.params.app;
	DevInterfaceState* interfaceState = context.params.interfaceState;
	ActionCall collapseAction = context.params.onToggle;
	ActionCall copyAction{};
	ActionCall pasteAction{};
	ActionCall resetAction{};
	bool canPaste = false;
	bool canReset = false;
	if (app && interfaceState) {
		auto& actions = app->actions().uiActions();
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
	header.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), header) {
		Clay_ElementDeclaration labelArea{};
		labelArea.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		labelArea.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		labelArea.layout.childGap = 1;
		labelArea.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		labelArea.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(kFieldName), labelArea) {
			Clay_TextElementConfig nameStyle = textConfig(interface_theme::kTextCanvas, 11);
			nameStyle.wrapMode = CLAY_TEXT_WRAP_NONE;
			CLAY_TEXT(
				context.uiManager.toClayString(card->fieldName),
				CLAY_TEXT_CONFIG(nameStyle));
			if (card->collapsed && !context.params.quickValue.empty()) {
				const Clay_TextElementConfig quickStyle = textConfig(interface_theme::kTextMuted, 9);
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.quickValue),
					CLAY_TEXT_CONFIG(quickStyle));
			}
		}
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::collapse,
			card->collapsed ? "↓" : "↑", collapseAction, true,
			card->collapsed
				? context.resources().expandIcon : context.resources().collapseIcon);
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::copy,
			"C", copyAction, card->hasCurrentValue,
			context.resources().copyIcon);
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::paste,
			"P", pasteAction, canPaste,
			context.resources().pasteIcon);
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::reset,
			"R", resetAction, canReset,
			context.resources().revertIcon);
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
		[&]() {
		if (context.params.depth >= kMaximumEditorDepth) {
			Clay_TextElementConfig style = textConfig(interface_theme::kStatusAmber, 9);
			style.wrapMode = CLAY_TEXT_WRAP_WORDS;
			CLAY_TEXT(
				context.uiManager.toClayString("Unsupported: recursive schema cycle or editor depth limit"),
				CLAY_TEXT_CONFIG(style));
			return;
		}
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
		}();
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
	const std::string typeLabel = schema
		? normalizedTypeName(*schema, type) + ":" : "Unknown:";

	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(32),
	};
	row.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	row.layout.childGap = 7;
	row.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	CLAY(context.clayID(), row) {
		Clay_ElementDeclaration labels{};
		labels.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
		labels.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		labels.layout.childGap = 6;
		labels.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID("labels"), labels) {
			if (context.params.showFieldName && !fieldName.empty()) {
				Clay_TextElementConfig nameStyle = textConfig(interface_theme::kTextSecondary, 10);
				nameStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
				CLAY_TEXT(context.uiManager.toClayString(fieldName), CLAY_TEXT_CONFIG(nameStyle));
			}
			Clay_TextElementConfig typeStyle = textConfig(interface_theme::kAccentSeaGlass, 9);
			typeStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
			CLAY_TEXT(context.uiManager.toClayString(typeLabel), CLAY_TEXT_CONFIG(typeStyle));
		}
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
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* field = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* declaredType = field && schema
		? schema->type(field->valueType) : nullptr;
	if (!schema || !field || !declaredType) return;

	DevTypeEditorState& state = context.state();
	state.app = context.params.app;
	state.interfaceState = context.params.interfaceState;
	state.binding = context.params.binding;
	state.type = declaredType->id;
	state.typeIndex = field->valueType;
	state.operationType = field->valueType;
	state.fieldName = std::string(schema->string(field->displayName));
	if (state.fieldName.empty()) state.fieldName = std::string(schema->string(field->name));
	state.editable = context.params.binding.role == DevEditorRole::Parameters &&
		field->effectiveEdit == devMode::DevEditCapability::Editable &&
		declaredType->edit != devMode::DevEditCapability::ViewOnly;
	state.currentValue.reset();
	if (context.params.value) {
		(void)tooling::DevOwnedValue::copyFrom(
			*schema, field->valueType, context.params.value, state.currentValue);
	}
	state.editValue = state.currentValue.data();

	const devMode::DevTypeSchema* type = declaredType;
	const devMode::DevTypeOps* operations = typeOperations(*schema, state.operationType);
	bool optionalPresent = false;
	if (declaredType->kind == devMode::DevTypeKind::Optional && operations &&
		operations->optionalHasValue) {
		optionalPresent = operations->optionalHasValue(state.currentValue.data());
		if (optionalPresent) {
			state.operationType = declaredType->elementType;
			state.editValue = operations->optionalMutableValueAddress
				? operations->optionalMutableValueAddress(state.currentValue.data()) : nullptr;
			type = schema->type(declaredType->elementType);
			operations = typeOperations(*schema, state.operationType);
		}
	}

	if (!state.draftInitialized && type && state.editValue) {
		if (type->kind == devMode::DevTypeKind::Text && operations && operations->textView) {
			state.draft = std::string(operations->textView(state.editValue));
		} else {
			state.draft = numericDraft(*type, operations, state.editValue);
		}
		state.draftInitialized = true;
		state.draftValid = true;
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.childGap = 5;
	root.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	root.clip = {.horizontal = true, .vertical = false};
	const bool dirty = state.app && currentEditorOverride(*state.app, *schema, state);
	root.layout.padding = dirty ? Clay_Padding{3, 3, 3, 3} : Clay_Padding{};
	root.border = {
		.color = dirty ? interface_theme::kAccentSignalCoral : interface_theme::kBorderPrimary,
		.width = dirty ? Clay_BorderWidth{1, 1, 1, 1, 0} : Clay_BorderWidth{},
	};
	root.cornerRadius = CLAY_CORNER_RADIUS(3);

	auto drawValueSurface = [&](std::string_view value, Clay_Color color) {
		Clay_ElementDeclaration surface{};
		surface.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
		surface.layout.padding = Clay_Padding{7, 7, 0, 0};
		surface.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		surface.backgroundColor = interface_theme::kDepth3Elevated;
		surface.cornerRadius = CLAY_CORNER_RADIUS(3);
		surface.border = {.color = interface_theme::kBorderVisible,
			.width = Clay_BorderWidth{1, 1, 1, 1, 0}};
		surface.clip = {.horizontal = true, .vertical = true};
		Clay_TextElementConfig style = textConfig(color, 9);
		style.wrapMode = CLAY_TEXT_WRAP_NONE;
		CLAY(context.clayID(kEditorMessage), surface) {
			CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(style));
		}
	};

	CLAY(context.clayID(), root) {
		[&]() {
		if (!state.currentValue) {
			drawValueSurface("Unavailable: value was not captured", interface_theme::kTextMuted);
			return;
		}

		if (declaredType->kind == devMode::DevTypeKind::Optional) {
			ActionCall toggle{};
			if (state.app && state.interfaceState) {
				toggle = ActionCall{state.app->actions().uiActions().make(kToggleOptional, state)};
			}
			FSEL::SwitchParameters parameters{};
			parameters.isOn = optionalPresent;
			parameters.enabled = state.editable && operations != nullptr;
			parameters.onToggle = toggle;
			context.uiManager.createElement(FSEL::kSwitch, "optional-toggle")
				.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
			if (!optionalPresent) {
			drawValueSurface("NullOpt", interface_theme::kTextMuted);
				return;
			}
		}

		const devMode::DevEditorKind kind = editorKind(field, type);
		if (kind == devMode::DevEditorKind::ActionChoice) {
			const ActionCall& action = *static_cast<const ActionCall*>(state.editValue);
			std::string label = "None · View only";
			if (action && state.app) {
				if (const auto info = state.app->actions().debugInfo(action)) {
					label = info->kind == ActionCallKind::App ? "App · " : "UI · ";
					label += info->debugName.empty() ? "Missing action" : std::string(info->debugName);
					if (!info->bound) label += " · Missing";
					else if (!info->availability.enabled) label += " · Disabled";
				}
			}
			drawValueSurface(label, action ? interface_theme::kTextSecondary : interface_theme::kTextMuted);
			return;
		}
		if (kind == devMode::DevEditorKind::ResourceChoice) {
			const TextureRef& texture = *static_cast<const TextureRef*>(state.editValue);
			std::string resourceName{};
#if FLOWUI_INCLUDE_IMAGE_MANAGER
			struct Lookup { TextureHandle handle{}; std::string name{}; } lookup{texture.handle, {}};
			if (state.app) state.app->images().visitDevImages(&lookup,
				+[](void* data, const ImageManager::DevImageView& image) {
					auto& wanted = *static_cast<Lookup*>(data);
					if (image.texture != wanted.handle) return true;
					wanted.name = std::string(image.key);
					return false;
				});
			resourceName = std::move(lookup.name);
#endif
			char metadata[160]{};
			std::snprintf(metadata, sizeof(metadata), "%s%sHandle %llu · %dx%d · View only",
				resourceName.empty() ? "" : resourceName.c_str(), resourceName.empty() ? "" : " · ",
				static_cast<unsigned long long>(texture.handle.packed()),
				texture.sourceWidth, texture.sourceHeight);
			drawValueSurface(metadata, texture.handle ? interface_theme::kTextSecondary : interface_theme::kTextMuted);
			return;
		}
		if (kind == devMode::DevEditorKind::SizingAxis) {
			const auto& axis = *static_cast<const Clay_SizingAxis*>(state.editValue);
			state.enumSelection = static_cast<uint64_t>(axis.type);
			if (state.semanticMode != state.enumSelection) {
				state.semanticMode = state.enumSelection;
				if (axis.type == CLAY__SIZING_TYPE_PERCENT) {
					state.semanticSliderValue =
						static_cast<double>(axis.size.percent) * 100.0;
					state.semanticValueB = 0.0f;
				} else {
					state.semanticValueA = axis.size.minMax.min;
					state.semanticValueB = axis.size.minMax.max;
				}
			}
			ActionCall changed{};
			ActionCall valuesPreview{};
			ActionCall valuesChanged{};
			ActionCall valuesCancelled{};
			if (state.app && state.interfaceState && state.editable) {
				changed = ActionCall{state.app->actions().uiActions().make(kChooseSizingAxisMode, state)};
				valuesPreview = ActionCall{
					state.app->actions().uiActions().make(kPreviewSizingAxisValues, state)};
				valuesChanged = ActionCall{
					state.app->actions().uiActions().make(kCommitSizingAxisValues, state)};
				valuesCancelled = ActionCall{
					state.app->actions().uiActions().make(kCancelSizingAxisValues, state)};
			}
			FSEL::ComboBoxParameters parameters{};
			parameters.options = kSizingAxisOptions;
			parameters.selectedValue = &state.enumSelection;
			parameters.enabled = state.editable;
			parameters.onChanged = changed;
			parameters.fontSize = kTypeControlFontSize;
			parameters.optionHeight = kTypeControlHeight;
			parameters.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			Clay_ElementDeclaration modeSlot{};
			modeSlot.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
			modeSlot.clip = {.horizontal = true, .vertical = false};
			CLAY(context.clayID("sizing-mode-slot"), modeSlot) {
				context.uiManager.createElement(FSEL::kComboBox, "sizing-mode")
					.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
			}

			auto drawSizingNumber = [&](uint64_t key, std::string_view label,
				float* value, std::optional<float> maximum = std::nullopt) {
				Clay_ElementDeclaration line{};
				line.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				line.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				line.layout.childGap = 6;
				line.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				CLAY(context.clayID(Keyed(kEditorControl, key * 4u)), line) {
					Clay_TextElementConfig labelStyle = textConfig(
						interface_theme::kTextSecondary, 9);
					CLAY(context.clayID(Keyed(kEditorControl, key * 4u + 1u)), Clay_ElementDeclaration{
						.layout = {.sizing = {.width = CLAY_SIZING_FIXED(34),
							.height = CLAY_SIZING_FIT(0)}}}) {
						CLAY_TEXT(context.uiManager.toClayString(label),
							CLAY_TEXT_CONFIG(labelStyle));
					}
					Clay_ElementDeclaration controlSlot{};
					controlSlot.layout.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
					controlSlot.clip = {.horizontal = true, .vertical = false};
					CLAY(context.clayID(Keyed(kEditorControl, key * 4u + 2u)), controlSlot) {
						FSEL::DragValueParameters<float> drag{};
						drag.value = value;
						drag.minimum = 0.0f;
						drag.maximum = maximum;
						drag.step = 0.5f;
						drag.pixelsPerStep = 6.0f;
						drag.dragThreshold = 4.0f;
						drag.enabled = state.editable;
						drag.readOnly = !state.editable;
						drag.edit.onChanged = valuesPreview;
						drag.edit.onCommit = valuesChanged;
						drag.edit.onCancel = valuesCancelled;
						drag.sizing = Clay_Sizing{
							.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
						drag.fontSize = kTypeControlFontSize;
						drag.padding = kTypeControlPadding;
						drag.format.precision = 6;
						drag.format.allowScientificInput = true;
						context.uiManager.createElement(
							FSEL::kDragValueFloat, Keyed(kEditorControl, key * 4u + 3u))
							.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
					}
				}
			};

			if (axis.type == CLAY__SIZING_TYPE_PERCENT) {
				Clay_ElementDeclaration line{};
				line.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				line.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				line.layout.childGap = 8;
				line.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				line.clip = {.horizontal = true, .vertical = false};
				CLAY(context.clayID("percent-slider-line"), line) {
					Clay_ElementDeclaration sliderSlot{};
					sliderSlot.layout.sizing = {
						.width = CLAY_SIZING_GROW(0),
						.height = CLAY_SIZING_FIXED(kTypeControlHeight)};
					sliderSlot.layout.childAlignment = {
						.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
					sliderSlot.clip = {.horizontal = true, .vertical = false};
					CLAY(context.clayID("percent-slider-slot"), sliderSlot) {
						FSEL::SliderParameters slider{};
						slider.value = &state.semanticSliderValue;
						slider.minimum = 0.0;
						slider.maximum = 100.0;
						slider.roundingStep = 1.0;
						slider.enabled = state.editable;
						slider.onChanged = valuesPreview;
						slider.onCommit = valuesChanged;
						slider.length = 150.0f;
						context.uiManager.createElement(FSEL::kSlider, "percent-slider")
							.setParameters(std::move(slider)).setDevInternalCapture(true).draw();
					}
					char percentLabel[16]{};
					std::snprintf(percentLabel, sizeof(percentLabel), "%.0f%%",
						state.semanticSliderValue);
					const Clay_TextElementConfig percentStyle = textConfig(
						interface_theme::kTextCanvas, 9, CLAY_TEXT_ALIGN_RIGHT);
					CLAY_TEXT(context.uiManager.toClayString(percentLabel),
						CLAY_TEXT_CONFIG(percentStyle));
				}
			} else if (axis.type == CLAY__SIZING_TYPE_FIXED) {
				drawSizingNumber(2u, "px", &state.semanticValueA);
			} else {
				drawSizingNumber(3u, "Min", &state.semanticValueA);
				drawSizingNumber(4u, "Max", &state.semanticValueB);
			}
			return;
		}

		if (type && type->kind == devMode::DevTypeKind::Boolean) {
			long double current = 0.0L;
			const bool on = operations && operations->numericValue &&
				operations->numericValue(state.editValue, current) && current != 0.0L;
			ActionCall toggle{};
			if (state.app && state.interfaceState) {
				toggle = ActionCall{state.app->actions().uiActions().make(kToggleEditorValue, state)};
			}
			Clay_ElementDeclaration line{};
			line.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
			line.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			line.layout.childGap = 7;
			line.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("boolean-line"), line) {
				FSEL::SwitchParameters parameters{};
				parameters.isOn = on;
				parameters.enabled = state.editable;
				parameters.onToggle = toggle;
				context.uiManager.createElement(FSEL::kSwitch, kEditorControl)
					.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
				const Clay_TextElementConfig style = textConfig(
					state.editable ? interface_theme::kTextCanvas : interface_theme::kTextMuted, 9);
				CLAY_TEXT(context.uiManager.toClayString(on ? "On" : "Off"), CLAY_TEXT_CONFIG(style));
			}
			return;
		}

		if (type && type->kind == devMode::DevTypeKind::Enumeration &&
			kind == devMode::DevEditorKind::Flags) {
			if (type->enumeration.values.count == 0u) {
				drawValueSurface("No Options Available", interface_theme::kTextMuted);
				return;
			}
			long double numeric = 0.0L;
			if (operations && operations->numericValue) operations->numericValue(state.editValue, numeric);
			const uint64_t bits = static_cast<uint64_t>(numeric);
			for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
				const auto& option = schema->enumValues[type->enumeration.values.first + index];
				ActionCall toggle{};
				if (state.app && state.interfaceState && state.editable) {
					toggle = ActionCall{state.app->actions().uiActions().make(kToggleEditorFlag, state, option.bits)};
				}
				const std::string label = std::string(
					(bits & option.bits) == option.bits ? "✓ " : "□ ") +
					std::string(schema->string(option.name));
				FSEL::ButtonParameters parameters{};
				parameters.onActivate = toggle;
				parameters.enabled = state.editable;
				parameters.text = label;
				parameters.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
				context.uiManager.createElement(FSEL::kButton, Keyed("flag", option.bits))
					.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
			}
			return;
		}

		if (type && type->kind == devMode::DevTypeKind::Enumeration) {
			if (type->enumeration.values.count == 0u) {
				drawValueSurface("No Options Available", interface_theme::kTextMuted);
				return;
			}
			long double current = 0.0L;
			if (operations && operations->numericValue) operations->numericValue(state.editValue, current);
			state.enumSelection = static_cast<uint64_t>(current);
			if (state.choiceType != type->id) {
				state.choiceType = type->id;
				state.choiceLabels.clear();
				state.choiceOptions.clear();
				state.choiceLabels.reserve(type->enumeration.values.count);
				for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
					const auto& option = schema->enumValues[type->enumeration.values.first + index];
					state.choiceLabels.emplace_back(schema->string(option.name));
				}
				state.choiceOptions.reserve(type->enumeration.values.count);
				for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
					const auto& option = schema->enumValues[type->enumeration.values.first + index];
					state.choiceOptions.push_back({
						.value = option.bits,
						.text = state.choiceLabels[index],
					});
				}
			}
			ActionCall changed{};
			if (state.app && state.interfaceState) {
				changed = ActionCall{state.app->actions().uiActions().make(kChooseEditorEnum, state)};
			}
			FSEL::ComboBoxParameters parameters{};
			parameters.options = state.choiceOptions;
			parameters.selectedValue = &state.enumSelection;
			parameters.enabled = state.editable;
			parameters.onChanged = changed;
			parameters.fontSize = kTypeControlFontSize;
			parameters.optionHeight = kTypeControlHeight;
			parameters.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			Clay_ElementDeclaration comboSlot{};
			comboSlot.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
			comboSlot.clip = {.horizontal = true, .vertical = false};
			CLAY(context.clayID("enum-combo-slot"), comboSlot) {
				context.uiManager.createElement(FSEL::kComboBox, kEditorControl)
					.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
			}
			return;
		}

		const bool nativeSignedDrag = type &&
			type->kind == devMode::DevTypeKind::SignedInteger && type->size <= sizeof(int);
		const bool nativeUnsignedDrag = type &&
			type->kind == devMode::DevTypeKind::UnsignedInteger &&
			type->size <= sizeof(unsigned int);
		const bool nativeFloatDrag = type &&
			type->kind == devMode::DevTypeKind::FloatingPoint && type->size <= sizeof(float);
		if ((nativeSignedDrag || nativeUnsignedDrag || nativeFloatDrag) &&
			operations && operations->numericValue) {
			long double current = 0.0L;
			if (state.dragValueType != type->id &&
				operations->numericValue(state.editValue, current)) {
				state.dragValueType = type->id;
				if (nativeSignedDrag) state.dragSigned = static_cast<int>(current);
				else if (nativeUnsignedDrag) state.dragUnsigned = static_cast<unsigned int>(current);
				else state.dragFloat = static_cast<float>(current);
			}

			ActionCall commit{};
			ActionCall preview{};
			ActionCall cancel{};
			if (state.app && state.interfaceState && state.editable) {
				commit = ActionCall{
					state.app->actions().uiActions().make(kCommitEditorDragValue, state)};
				preview = ActionCall{
					state.app->actions().uiActions().make(kPreviewEditorDragValue, state)};
				cancel = ActionCall{
					state.app->actions().uiActions().make(kCancelEditorDragValue, state)};
			}
			const devMode::DevNumericConstraint* numericConstraint =
				field->constraint < schema->constraints.size()
					? &schema->constraints[field->constraint].numeric : nullptr;

			Clay_ElementDeclaration controlSlot{};
			controlSlot.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
			controlSlot.clip = {.horizontal = true, .vertical = false};
			CLAY(context.clayID("numeric-drag-control-slot"), controlSlot) {
				if (nativeSignedDrag) {
					FSEL::DragValueParameters<int> drag{};
					drag.value = &state.dragSigned;
					if (numericConstraint && numericConstraint->hasMinimum) {
						drag.minimum = static_cast<int>(std::clamp(
							numericConstraint->minimum,
							static_cast<double>(std::numeric_limits<int>::lowest()),
							static_cast<double>(std::numeric_limits<int>::max())));
					}
					if (numericConstraint && numericConstraint->hasMaximum) {
						drag.maximum = static_cast<int>(std::clamp(
							numericConstraint->maximum,
							static_cast<double>(std::numeric_limits<int>::lowest()),
							static_cast<double>(std::numeric_limits<int>::max())));
					}
					drag.step = numericConstraint && numericConstraint->hasStep
						? std::max(1, static_cast<int>(std::round(std::clamp(
							numericConstraint->step, 1.0,
							static_cast<double>(std::numeric_limits<int>::max()))))) : 1;
					drag.pixelsPerStep = 6.0f;
					drag.dragThreshold = 4.0f;
					drag.enabled = state.editable;
					drag.readOnly = !state.editable;
					drag.edit.onChanged = preview;
					drag.edit.onCommit = commit;
					drag.edit.onCancel = cancel;
					drag.sizing = Clay_Sizing{
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
					drag.fontSize = kTypeControlFontSize;
					drag.padding = kTypeControlPadding;
					context.uiManager.createElement(FSEL::kDragValueInt, kEditorControl)
						.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
				} else if (nativeUnsignedDrag) {
					FSEL::DragValueParameters<unsigned int> drag{};
					drag.value = &state.dragUnsigned;
					if (numericConstraint && numericConstraint->hasMinimum) {
						drag.minimum = static_cast<unsigned int>(std::clamp(
							numericConstraint->minimum, 0.0,
							static_cast<double>(std::numeric_limits<unsigned int>::max())));
					}
					if (numericConstraint && numericConstraint->hasMaximum) {
						drag.maximum = static_cast<unsigned int>(std::clamp(
							numericConstraint->maximum, 0.0,
							static_cast<double>(std::numeric_limits<unsigned int>::max())));
					}
					drag.step = numericConstraint && numericConstraint->hasStep
						? std::max(1u, static_cast<unsigned int>(std::round(std::clamp(
							numericConstraint->step, 1.0,
							static_cast<double>(std::numeric_limits<unsigned int>::max()))))) : 1u;
					drag.pixelsPerStep = 6.0f;
					drag.dragThreshold = 4.0f;
					drag.enabled = state.editable;
					drag.readOnly = !state.editable;
					drag.edit.onChanged = preview;
					drag.edit.onCommit = commit;
					drag.edit.onCancel = cancel;
					drag.sizing = Clay_Sizing{
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
					drag.fontSize = kTypeControlFontSize;
					drag.padding = kTypeControlPadding;
					context.uiManager.createElement(FSEL::kDragValueUInt, kEditorControl)
						.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
				} else {
					FSEL::DragValueParameters<float> drag{};
					drag.value = &state.dragFloat;
					if (numericConstraint && numericConstraint->hasMinimum) {
						drag.minimum = static_cast<float>(numericConstraint->minimum);
					}
					if (numericConstraint && numericConstraint->hasMaximum) {
						drag.maximum = static_cast<float>(numericConstraint->maximum);
					}
					drag.step = numericConstraint && numericConstraint->hasStep
						? static_cast<float>(numericConstraint->step) : 0.1f;
					drag.pixelsPerStep = 6.0f;
					drag.dragThreshold = 4.0f;
					drag.enabled = state.editable;
					drag.readOnly = !state.editable;
					drag.edit.onChanged = preview;
					drag.edit.onCommit = commit;
					drag.edit.onCancel = cancel;
					drag.sizing = Clay_Sizing{
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
					drag.fontSize = kTypeControlFontSize;
					drag.padding = kTypeControlPadding;
					drag.format.precision = 6;
					drag.format.allowScientificInput = true;
					context.uiManager.createElement(FSEL::kDragValueFloat, kEditorControl)
						.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
				}
			}
			return;
		}

		if (type && (type->kind == devMode::DevTypeKind::SignedInteger ||
			type->kind == devMode::DevTypeKind::UnsignedInteger ||
			type->kind == devMode::DevTypeKind::FloatingPoint ||
			type->kind == devMode::DevTypeKind::Text)) {
			ActionCall commit{};
			ActionCall preview{};
			if (state.app && state.interfaceState) {
				commit = ActionCall{state.app->actions().uiActions().make(kCommitEditorDraft, state)};
				preview = ActionCall{state.app->actions().uiActions().make(kPreviewEditorDraft, state)};
			}
			FSEL::TextInputParameters parameters{};
			parameters.value = &state.draft;
			parameters.syncPolicy = FSEL::TextFieldSyncPolicy::Live;
			parameters.actions.onChanged = preview;
			parameters.actions.onCommit = commit;
			parameters.enabled = state.editable;
			parameters.readOnly = !state.editable;
			parameters.placeholder = state.editable ? std::string_view{} : "Read only";
			parameters.valid = state.draftValid;
			parameters.maxBytes = field->constraint < schema->constraints.size() &&
				schema->constraints[field->constraint].hasTextMaximum
					? schema->constraints[field->constraint].textMaximum : 128u;
			parameters.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			parameters.fontSize = kTypeControlFontSize;
			parameters.padding = kTypeControlPadding;

			Clay_ElementDeclaration controlSlot{};
			controlSlot.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
			controlSlot.clip = {.horizontal = true, .vertical = false};
			CLAY(context.clayID("text-control-slot"), controlSlot) {
				context.uiManager.createElement(FSEL::kTextInput, kEditorControl)
					.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
			}
			if (!state.draftValid) drawValueSurface("Invalid value", interface_theme::kStatusRed);
			return;
		}

		const bool semanticObject = type && type->kind == devMode::DevTypeKind::Object &&
			kind != devMode::DevEditorKind::ObjectGroup;
		if (semanticObject) {
			if (kind == devMode::DevEditorKind::Color && state.editValue) {
				const auto& color = *static_cast<const Clay_Color*>(state.editValue);
				if (state.semanticMode != kColorSemanticMode) {
					state.semanticMode = kColorSemanticMode;
					state.semanticChannels = {color.r, color.g, color.b, color.a};
					updateColorHexDraft(state);
					state.draftValid = true;
				}
				ActionCall channelCommit{};
				ActionCall channelPreview{};
				ActionCall hexCommit{};
				ActionCall hexPreview{};
				ActionCall swatchToggle{};
				if (state.app) {
					swatchToggle = ActionCall{state.app->actions().uiActions().make(
						kToggleColorExpanded, state)};
				}
				if (state.app && state.interfaceState && state.editable) {
					channelPreview = ActionCall{state.app->actions().uiActions().make(
						kPreviewColorChannels, state)};
					channelCommit = ActionCall{state.app->actions().uiActions().make(
						kCommitColorChannels, state)};
					hexPreview = ActionCall{state.app->actions().uiActions().make(
						kPreviewColorHex, state)};
					hexCommit = ActionCall{state.app->actions().uiActions().make(
						kCommitColorHex, state)};
				}

				Clay_ElementDeclaration compact{};
				compact.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				compact.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				compact.layout.childGap = 7;
				compact.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				compact.clip = {.horizontal = true, .vertical = false};
				CLAY(context.clayID("color-compact"), compact) {
					const Clay_Color swatchColor{
						static_cast<float>(state.semanticChannels[0]),
						static_cast<float>(state.semanticChannels[1]),
						static_cast<float>(state.semanticChannels[2]),
						static_cast<float>(state.semanticChannels[3])};
					FSEL::ButtonParameters swatch{};
					swatch.onActivate = swatchToggle;
					swatch.enabled = state.app != nullptr;
					swatch.contentMode = FSEL::ButtonContentMode::None;
					swatch.sizing = {
						.width = CLAY_SIZING_FIXED(38), .height = CLAY_SIZING_FIXED(30)};
					swatch.padding = Clay_Padding{};
					swatch.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
					swatch.cornerRadius = CLAY_CORNER_RADIUS(3);
					swatch.idleOverrides.backgroundColor = swatchColor;
					swatch.idleOverrides.borderColor = interface_theme::kBorderVisible;
					swatch.hoveredOverrides.backgroundColor = swatchColor;
					swatch.hoveredOverrides.borderColor = interface_theme::kAccentSeaGlass;
					swatch.pressedOverrides.backgroundColor = swatchColor;
					swatch.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
					context.uiManager.createElement(FSEL::kButton, "color-swatch")
						.setParameters(std::move(swatch)).setDevInternalCapture(true).draw();

					FSEL::TextInputParameters hex{};
					hex.value = &state.semanticText;
					hex.syncPolicy = FSEL::TextFieldSyncPolicy::Live;
					hex.actions.onChanged = hexPreview;
					hex.actions.onCommit = hexCommit;
					hex.enabled = state.editable;
					hex.readOnly = !state.editable;
					hex.placeholder = state.editable ? std::string_view{} : "Read only";
					hex.valid = state.draftValid;
					hex.maxBytes = 9u;
					hex.sizing = Clay_Sizing{
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
					hex.fontSize = kTypeControlFontSize;
					hex.padding = kTypeControlPadding;
					Clay_ElementDeclaration hexSlot{};
					hexSlot.layout.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
					hexSlot.clip = {.horizontal = true, .vertical = false};
					CLAY(context.clayID("color-hex-slot"), hexSlot) {
						context.uiManager.createElement(FSEL::kTextInput, "color-hex")
							.setParameters(std::move(hex)).setDevInternalCapture(true).draw();
					}
				}

				constexpr std::array<std::string_view, 4> channelNames{"R", "G", "B", "A"};
				if (state.colorExpanded) for (
					uint64_t channel = 0; channel < channelNames.size(); ++channel) {
					Clay_ElementDeclaration line{};
					line.layout.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
					line.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
					line.layout.childGap = 7;
					line.layout.childAlignment = {
						.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
					CLAY(context.clayID(Keyed(kSemanticChild, channel)), line) {
						const Clay_TextElementConfig labelStyle = textConfig(
							interface_theme::kTextSecondary, 9);
						CLAY_TEXT(context.uiManager.toClayString(channelNames[channel]),
							CLAY_TEXT_CONFIG(labelStyle));
						FSEL::SliderParameters slider{};
						slider.value = &state.semanticChannels[channel];
						slider.minimum = 0.0;
						slider.maximum = 255.0;
						slider.roundingStep = 1.0;
						slider.enabled = state.editable;
						slider.onChanged = channelPreview;
						slider.onCommit = channelCommit;
						slider.length = 132.0f;
						context.uiManager.createElement(
							FSEL::kSlider, Keyed(kEditorControl, channel))
							.setParameters(std::move(slider)).setDevInternalCapture(true).draw();
						char valueLabel[8]{};
						std::snprintf(valueLabel, sizeof(valueLabel), "%.0f",
							state.semanticChannels[channel]);
						const Clay_TextElementConfig valueStyle = textConfig(
							interface_theme::kTextCanvas, 9, CLAY_TEXT_ALIGN_RIGHT);
						CLAY_TEXT(context.uiManager.toClayString(valueLabel),
							CLAY_TEXT_CONFIG(valueStyle));
					}
				}
				return;
			}
			if ((kind == devMode::DevEditorKind::Spacing ||
				kind == devMode::DevEditorKind::CornerRadius) && state.editValue) {
				const auto children = schema->fieldsOf(state.operationType);
				auto findChild = [&](std::string_view wanted)
					-> const devMode::DevFieldSchema* {
					for (const devMode::DevFieldSchema& child : children) {
						if (schema->string(child.name) == wanted) return &child;
					}
					return nullptr;
				};
				auto drawCell = [&](const devMode::DevFieldSchema* child, uint64_t slot) {
					Clay_ElementDeclaration cell{};
					cell.layout.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
					cell.clip = {.horizontal = true, .vertical = false};
					CLAY(context.clayID(Keyed(kSemanticChild, 100u + slot)), cell) {
						if (child) {
							const void* childValue = nullptr;
							if (child->operations < schema->fieldOperations.size()) {
								const devMode::DevFieldOps* childOps =
									schema->fieldOperations[child->operations];
								if (childOps && childOps->constAddress) {
									childValue = childOps->constAddress(state.editValue);
								}
							}
							DevEditorBinding childBinding = state.binding;
							childBinding.nestedPath.push_back(field->id);
							childBinding.field = fieldIndex(*schema, *child);
							context.uiManager.createElement(
								kDevEditor, Keyed(kSemanticChild, 200u + slot))
								.setParameters(DevEditorParameters{
									.schema = context.params.schema,
									.binding = std::move(childBinding),
									.app = state.app,
									.interfaceState = state.interfaceState,
									.value = childValue,
									.showFieldName = true})
								.setDevInternalCapture(true).draw();
						}
					}
				};
				auto drawGridRow = [&](const devMode::DevFieldSchema* left,
					const devMode::DevFieldSchema* center,
					const devMode::DevFieldSchema* right, uint64_t row) {
					Clay_ElementDeclaration line{};
					line.layout.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
					line.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
					line.layout.childGap = 5;
					line.clip = {.horizontal = true, .vertical = false};
					CLAY(context.clayID(Keyed(kSemanticChild, 300u + row)), line) {
						drawCell(left, row * 3u);
						drawCell(center, row * 3u + 1u);
						drawCell(right, row * 3u + 2u);
					}
				};

				const bool corners = kind == devMode::DevEditorKind::CornerRadius;
				if (corners) {
					drawGridRow(findChild("topLeft"), nullptr, findChild("topRight"), 0u);
				} else {
					drawGridRow(nullptr, findChild("top"), nullptr, 0u);
				}

				Clay_ElementDeclaration middle{};
				middle.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				middle.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				middle.layout.childGap = 5;
				middle.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				middle.clip = {.horizontal = true, .vertical = false};
				CLAY(context.clayID(Keyed(kSemanticChild, 304u)), middle) {
					drawCell(corners ? nullptr : findChild("left"), 3u);
					FSEL::ButtonParameters link{};
					if (state.app && state.interfaceState && state.editable) {
						link.onActivate = ActionCall{state.app->actions().uiActions().make(
							kLinkSemanticQuad, state)};
					}
					link.enabled = state.editable;
					link.contentMode = FSEL::ButtonContentMode::TextOnly;
					link.text = "↔ Link";
					link.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
					link.labelFontSize = 9;
					link.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
					link.idleOverrides.borderColor = interface_theme::kAccentSeaGlass;
					context.uiManager.createElement(FSEL::kButton, "link-quad")
						.setParameters(std::move(link)).setDevInternalCapture(true).draw();
					drawCell(corners ? nullptr : findChild("right"), 5u);
				}

				if (corners) {
					drawGridRow(findChild("bottomLeft"), nullptr,
						findChild("bottomRight"), 2u);
				} else {
					drawGridRow(nullptr, findChild("bottom"), nullptr, 2u);
					if (const devMode::DevFieldSchema* between = findChild("betweenChildren")) {
						drawCell(between, 20u);
					}
				}
				return;
			}
			for (const devMode::DevFieldSchema& child : schema->fieldsOf(state.operationType)) {
				const void* childValue = nullptr;
				if (child.operations < schema->fieldOperations.size()) {
					const devMode::DevFieldOps* childOps = schema->fieldOperations[child.operations];
					if (childOps && childOps->constAddress) childValue = childOps->constAddress(state.editValue);
				}
				DevEditorBinding childBinding = state.binding;
				childBinding.nestedPath.push_back(field->id);
				childBinding.field = fieldIndex(*schema, child);
				context.uiManager.createElement(kDevEditor, Keyed(kSemanticChild, child.id))
					.setParameters(DevEditorParameters{.schema = context.params.schema,
						.binding = std::move(childBinding), .app = state.app,
						.interfaceState = state.interfaceState, .value = childValue,
						.showFieldName = true})
					.setDevInternalCapture(true).draw();
			}
			return;
		}

		if (type && type->kind == devMode::DevTypeKind::Sequence) {
			if (operations) drawSequenceRows(context, state, *schema, *type, *operations);
			return;
		}

		std::string reason = state.editable ? "Unsupported: no editor adapter" : "View only";
		if (field->reason == devMode::DevCapabilityReason::RawPointer) reason = "Unsupported: raw pointer";
		else if (field->reason == devMode::DevCapabilityReason::CallableType) reason = "Unsupported: callable type";
		else if (field->reason == devMode::DevCapabilityReason::NoEditAdapter) reason = "View only: no edit adapter";
		drawValueSurface(reason, interface_theme::kTextMuted);
		}();
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
		.horizontal = true,
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
