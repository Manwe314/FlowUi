#ifdef NDEBUG
#undef NDEBUG
#endif

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace schema_test {

struct SharedStyle {
	Clay_Color color{};
	float opacity = 1.0f;
};

struct FirstParams {
	Clay_Sizing sizing{};
	SharedStyle style{};
};

struct SecondParams {
	Clay_Sizing layout{};
	SharedStyle style{};
};

struct FirstState {
	bool enabled = false;
};

struct Theme {
	Clay_Sizing panelSizing{};
	SharedStyle card{};
};

struct RecursiveAndCallable {
	std::vector<RecursiveAndCallable> children{};
	void (*callback)() = nullptr;
	float amount = 0.0f;
};

struct GenericShapes {
	std::optional<std::int32_t> optionalCount{};
	std::vector<std::string> labels{};
	std::array<float, 3> position{};
};

struct SemanticValues {
	FlowUi::ActionCall action{};
	FlowUi::TextureRef texture{};
};

enum class WideSignedEnum : std::int64_t {
	Negative = (std::numeric_limits<std::int64_t>::min)() + 17,
	Positive = (std::numeric_limits<std::int64_t>::max)() - 23,
};

FLOWUI_DEV_ENUM_SCHEMA(
	WideSignedEnum,
	FLOWUI_DEV_ENUM_VALUE(WideSignedEnum, WideSignedEnum::Negative),
	FLOWUI_DEV_ENUM_VALUE(WideSignedEnum, WideSignedEnum::Positive))

struct FirstElement {
	using Parameters = FirstParams;
	using State = FirstState;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x510001u};
	static constexpr std::string_view debugName = "First element";
	static void buildElement(FlowUi::ElementBuildContext<FirstElement>&) {}
};

struct SecondElement {
	using Parameters = SecondParams;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x510002u};
	static void buildElement(FlowUi::ElementBuildContext<SecondElement>&) {}
};

struct ConflictingElement {
	using Parameters = SecondParams;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x510001u};
	static void buildElement(FlowUi::ElementBuildContext<ConflictingElement>&) {}
};

FLOWUI_DEV_SCHEMA(
	SharedStyle,
	FLOWUI_DEV_FIELD(SharedStyle, color),
	FLOWUI_DEV_FIELD(
		SharedStyle,
		opacity,
		FlowUi::devMode::DevFieldOptions{}.numericRange(0.0, 1.0)))

FLOWUI_DEV_SCHEMA(
	FirstParams,
	FLOWUI_DEV_FIELD(FirstParams, sizing),
	FLOWUI_DEV_FIELD(
		FirstParams,
		style,
		FlowUi::devMode::DevFieldOptions{}
			.withHint("Shared presentation")
			.withEditor(FlowUi::devMode::DevEditorKind::Custom)))

FLOWUI_DEV_SCHEMA(
	SecondParams,
	FLOWUI_DEV_FIELD(SecondParams, layout),
	FLOWUI_DEV_FIELD(SecondParams, style))

FLOWUI_DEV_SCHEMA(FirstState, FLOWUI_DEV_FIELD(FirstState, enabled))

FLOWUI_DEV_SCHEMA(
	Theme,
	FLOWUI_DEV_FIELD(Theme, panelSizing),
	FLOWUI_DEV_FIELD(Theme, card))

FLOWUI_DEV_SCHEMA(
	RecursiveAndCallable,
	FLOWUI_DEV_FIELD(RecursiveAndCallable, children),
	FLOWUI_DEV_FIELD(RecursiveAndCallable, callback),
	FLOWUI_DEV_FIELD(RecursiveAndCallable, amount))

FLOWUI_DEV_SCHEMA(
	GenericShapes,
	FLOWUI_DEV_FIELD(GenericShapes, optionalCount),
	FLOWUI_DEV_FIELD(GenericShapes, labels),
	FLOWUI_DEV_FIELD(GenericShapes, position))

FLOWUI_DEV_SCHEMA(
	SemanticValues,
	FLOWUI_DEV_FIELD(SemanticValues, action),
	FLOWUI_DEV_FIELD(SemanticValues, texture))

const FlowUi::devMode::DevFieldSchema* findField(
	const FlowUi::devMode::DevSchemaGeneration& generation,
	const FlowUi::devMode::DevTypeSchema& owner,
	std::string_view name) {
	const auto ownerIndex = FlowUi::devMode::DevTypeIndex{
		static_cast<std::uint32_t>(&owner - generation.types.data())};
	for (const auto& field : generation.fieldsOf(ownerIndex)) {
		if (generation.string(field.name) == name) return &field;
	}
	return nullptr;
}

void verifySharedClayTypeAndTypedMemberPaths() {
	using namespace FlowUi;
	using namespace FlowUi::devMode;
	DevSchemaRegistry registry;
	registry.ensureElement<FirstElement>();
	registry.ensureElement<FirstElement>();
	assert(registry.pendingRootCount() == 1);
	registry.ensureElement<SecondElement>();
	registry.ensureTheme<Theme>();
	assert(registry.pendingRootCount() == 3);
	assert(registry.publishPendingAtSafePoint());
	assert(!registry.publishPendingAtSafePoint());

	const DevSchemaView view = registry.view();
	assert(view);
	assert(view->elements.size() == 2);
	assert(view->themes.size() == 1);
	assert(view->elements.front().parametersPolicy.edit == DevEditCapability::Editable);
	assert(view->elements.front().statePolicy.edit == DevEditCapability::ViewOnly);
	assert(view->elements.front().resourcesPolicy.edit == DevEditCapability::ViewOnly);
	assert(view->themes.front().policy.edit == DevEditCapability::Editable);
	const DevTypeSchema* sizing = view->findType(FlowUi::detail::typeHash<Clay_Sizing>());
	assert(sizing != nullptr);
	assert(sizing->editor == DevEditorKind::Sizing);
	const DevTypeSchema* sizingAxis = view->findType(FlowUi::detail::typeHash<Clay_SizingAxis>());
	assert(sizingAxis != nullptr);
	const DevFieldSchema* sizingTypeField = findField(*view, *sizingAxis, "type");
	assert(sizingTypeField != nullptr);
	const DevTypeSchema* sizingEnum = view->type(sizingTypeField->valueType);
	assert(sizingEnum != nullptr);
	assert(sizingEnum->editor == DevEditorKind::EnumChoice);
	assert(sizingEnum->enumeration.values.count == 4);

	const DevTypeSchema* first = view->findType(FlowUi::detail::typeHash<FirstParams>());
	const DevTypeSchema* second = view->findType(FlowUi::detail::typeHash<SecondParams>());
	assert(first != nullptr && second != nullptr);
	const DevFieldSchema* firstSizing = findField(*view, *first, "sizing");
	const DevFieldSchema* secondSizing = findField(*view, *second, "layout");
	assert(firstSizing != nullptr && secondSizing != nullptr);
	assert(firstSizing->valueType == secondSizing->valueType);
	assert(view->type(firstSizing->valueType) == sizing);

	FirstParams firstValue{};
	SecondParams secondValue{};
	const DevFieldOps* firstOps = view->fieldOperations.at(firstSizing->operations);
	const DevFieldOps* secondOps = view->fieldOperations.at(secondSizing->operations);
	assert(firstOps->mutableAddress(&firstValue) == &firstValue.sizing);
	assert(secondOps->constAddress(&secondValue) == &secondValue.layout);
	alignas(Clay_Sizing) std::byte sizingCopyStorage[sizeof(Clay_Sizing)];
	assert(firstOps->copyConstructMember(&firstValue, sizingCopyStorage) ==
		DevValueOperationStatus::Success);
	auto* sizingCopy = std::launder(reinterpret_cast<Clay_Sizing*>(sizingCopyStorage));
	sizingCopy->width = CLAY_SIZING_FIXED(42.0f);
	assert(firstOps->assignMemberFromCopy(&firstValue, sizingCopy) ==
		DevValueOperationStatus::Success);
	assert(firstValue.sizing.width.type == CLAY__SIZING_TYPE_FIXED);
	const DevTypeOps* sizingOps = view->typeOperations.at(firstSizing->valueType.value);
	assert(sizingOps != nullptr && sizingOps->type == sizing->id);
	sizingOps->destroy(sizingCopy);
	assert(firstSizing->source.line != 0);
	assert(!view->string(firstSizing->source.file).empty());
	const DevFieldSchema* firstStyle = findField(*view, *first, "style");
	assert(firstStyle != nullptr);
	assert(view->string(firstStyle->hint) == "Shared presentation");
	assert(firstStyle->editor == DevEditorKind::Custom);
	const DevTypeSchema* sharedStyle = view->findType(FlowUi::detail::typeHash<SharedStyle>());
	assert(sharedStyle != nullptr);
	const DevFieldSchema* opacity = findField(*view, *sharedStyle, "opacity");
	assert(opacity != nullptr && opacity->constraint != 0);
}

void verifyFullWidthEnumValues() {
	using namespace FlowUi;
	using namespace FlowUi::devMode;
	DevSchemaRegistry registry;
	registry.ensureStruct<WideSignedEnum>();
	assert(registry.publishPendingAtSafePoint());
	const DevSchemaView view = registry.view();
	const DevTypeSchema* type = view->findType(FlowUi::detail::typeHash<WideSignedEnum>());
	assert(type != nullptr);
	assert(type->kind == DevTypeKind::Enumeration);
	assert(type->enumeration.widthBytes == sizeof(std::int64_t));
	assert(type->enumeration.isSigned);
	assert(type->enumeration.values.count == 2);
	const DevEnumValueSchema& negative = view->enumValues[type->enumeration.values.first];
	const DevEnumValueSchema& positive = view->enumValues[type->enumeration.values.first + 1];
	assert(view->string(negative.name) == "WideSignedEnum::Negative");
	assert(negative.bits == static_cast<std::uint64_t>(WideSignedEnum::Negative));
	assert(positive.bits == static_cast<std::uint64_t>(WideSignedEnum::Positive));
}

void verifyBuiltInThemeSchema() {
	using namespace FlowUi;
	using namespace FlowUi::devMode;
	DevSchemaRegistry registry;
	registry.ensureTheme<FlowUiTheme>();
	assert(registry.publishPendingAtSafePoint());
	const DevSchemaView view = registry.view();
	const DevTypeSchema* theme = view->findType(FlowUi::detail::typeHash<FlowUiTheme>());
	assert(theme != nullptr);
	assert(theme->kind == DevTypeKind::Object);
	assert(view->fieldsOf(DevTypeIndex{
		static_cast<std::uint32_t>(theme - view->types.data())}).size() == 32);
	assert(view->findTheme(theme->id) != nullptr);
}

void verifyUnsupportedBranchesRemainVisible() {
	using namespace FlowUi;
	using namespace FlowUi::devMode;
	DevSchemaRegistry registry;
	registry.ensureStruct<RecursiveAndCallable>();
	assert(registry.publishPendingAtSafePoint());
	const DevSchemaView view = registry.view();
	const DevTypeSchema* owner = view->findType(
		FlowUi::detail::typeHash<RecursiveAndCallable>());
	assert(owner != nullptr && owner->edit == DevEditCapability::PartiallyEditable);
	const DevFieldSchema* children = findField(*view, *owner, "children");
	const DevFieldSchema* callback = findField(*view, *owner, "callback");
	const DevFieldSchema* amount = findField(*view, *owner, "amount");
	assert(children != nullptr && callback != nullptr && amount != nullptr);
	assert(children->reason == DevCapabilityReason::RecursiveCycle);
	assert(callback->reason == DevCapabilityReason::CallableType);
	assert(amount->effectiveEdit == DevEditCapability::Editable);
}

void verifyGenericShapes() {
	using namespace FlowUi;
	using namespace FlowUi::devMode;
	DevSchemaRegistry registry;
	registry.ensureStruct<GenericShapes>();
	assert(registry.publishPendingAtSafePoint());
	const DevSchemaView view = registry.view();
	const DevTypeSchema* owner = view->findType(FlowUi::detail::typeHash<GenericShapes>());
	assert(owner != nullptr);
	const DevFieldSchema* optionalCount = findField(*view, *owner, "optionalCount");
	const DevFieldSchema* labels = findField(*view, *owner, "labels");
	const DevFieldSchema* position = findField(*view, *owner, "position");
	assert(optionalCount != nullptr && labels != nullptr && position != nullptr);
	assert(view->type(optionalCount->valueType)->kind == DevTypeKind::Optional);
	const DevTypeSchema* labelSequence = view->type(labels->valueType);
	assert(labelSequence->kind == DevTypeKind::Sequence && !labelSequence->sequenceFixed);
	const DevTypeSchema* positionSequence = view->type(position->valueType);
	assert(positionSequence->kind == DevTypeKind::Sequence);
	assert(positionSequence->sequenceFixed && positionSequence->sequenceExtent == 3);

	GenericShapes value{};
	value.optionalCount = 7;
	value.labels = {"one", "two"};
	value.position = {1.0f, 2.0f, 3.0f};
	const DevFieldOps* optionalFieldOps = view->fieldOperations.at(optionalCount->operations);
	const DevTypeOps* optionalOps = view->typeOperations.at(optionalCount->valueType.value);
	const void* optionalAddress = optionalFieldOps->constAddress(&value);
	assert(optionalOps->optionalHasValue(optionalAddress));
	assert(*static_cast<const std::int32_t*>(
		optionalOps->optionalValueAddress(optionalAddress)) == 7);
	const DevFieldOps* labelFieldOps = view->fieldOperations.at(labels->operations);
	const DevTypeOps* labelOps = view->typeOperations.at(labels->valueType.value);
	const void* labelAddress = labelFieldOps->constAddress(&value);
	assert(labelOps->sequenceSize(labelAddress) == 2);
	const auto* secondLabel = static_cast<const std::string*>(
		labelOps->sequenceElementAddress(labelAddress, 1));
	assert(secondLabel != nullptr && *secondLabel == "two");
}

void verifyFlowUiSemanticAdapters() {
	using namespace FlowUi;
	using namespace FlowUi::devMode;
	DevSchemaRegistry registry;
	registry.ensureStruct<SemanticValues>();
	assert(registry.publishPendingAtSafePoint());
	const DevSchemaView view = registry.view();
	const DevTypeSchema* action = view->findType(FlowUi::detail::typeHash<ActionCall>());
	const DevTypeSchema* texture = view->findType(FlowUi::detail::typeHash<TextureRef>());
	assert(action != nullptr && texture != nullptr);
	assert(action->editor == DevEditorKind::ActionChoice);
	assert(action->edit == DevEditCapability::ViewOnly);
	assert(texture->editor == DevEditorKind::ResourceChoice);
	assert(texture->edit == DevEditCapability::PartiallyEditable);
}

void verifyOrderIndependentGenerationAndSnapshotLifetime() {
	using namespace FlowUi::devMode;
	DevSchemaRegistry left;
	left.ensureElement<FirstElement>();
	left.ensureElement<SecondElement>();
	left.publishPendingAtSafePoint();

	DevSchemaRegistry right;
	right.ensureElement<SecondElement>();
	right.ensureElement<FirstElement>();
	right.publishPendingAtSafePoint();
	assert(left.view()->fingerprint == right.view()->fingerprint);

	const DevSchemaView old = left.view();
	const auto oldGeneration = old->generation;
	left.ensureTheme<Theme>();
	left.publishPendingAtSafePoint();
	assert(left.view()->generation > oldGeneration);
	assert(old->generation == oldGeneration);
}

void verifyCatalogue() {
	using namespace FlowUi::devMode;
	constexpr auto catalogue = devCatalogue(
		elements<FirstElement, SecondElement>(),
		themes<Theme>(),
		structs<SharedStyle>());
	DevSchemaRegistry registry;
	registry.ingest(catalogue);
	assert(registry.pendingRootCount() == 4);
	registry.publishPendingAtSafePoint();
	assert(registry.view()->elements.size() == 2);
	assert(registry.view()->themes.size() == 1);
}

void verifyCapacityDiagnosticsRemainPublishable() {
	using namespace FlowUi::devMode;
	DevSchemaLimits limits{};
	limits.maxTypes = 2;
	limits.maxFields = 1;
	limits.maxStringBytes = 8;
	DevSchemaRegistry registry(limits);
	registry.ensureStruct<SharedStyle>();
	assert(registry.publishPendingAtSafePoint());
	const DevSchemaView view = registry.view();
	assert(view);
	bool sawCapacity = false;
	for (const DevDiagnostic& diagnostic : view->diagnostics) {
		sawCapacity |= diagnostic.code == DevDiagnosticCode::CapacityExceeded;
	}
	assert(sawCapacity);

	DevSchemaLimits enumLimits{};
	enumLimits.maxEnumValues = 1;
	DevSchemaRegistry enumRegistry(enumLimits);
	enumRegistry.ensureStruct<WideSignedEnum>();
	assert(enumRegistry.publishPendingAtSafePoint());
	const DevTypeSchema* enumType = enumRegistry.view()->findType(
		FlowUi::detail::typeHash<WideSignedEnum>());
	assert(enumType != nullptr && enumType->enumeration.values.count == 1);
	assert(enumType->edit == DevEditCapability::ViewOnly);

	DevSchemaLimits constraintLimits{};
	constraintLimits.maxConstraints = 0;
	DevSchemaRegistry constraintRegistry(constraintLimits);
	constraintRegistry.ensureStruct<SharedStyle>();
	assert(constraintRegistry.publishPendingAtSafePoint());
	const DevSchemaView constrainedView = constraintRegistry.view();
	const DevTypeSchema* constrainedStyle = constrainedView->findType(
		FlowUi::detail::typeHash<SharedStyle>());
	assert(constrainedStyle != nullptr);
	const DevFieldSchema* constrainedOpacity = findField(
		*constrainedView, *constrainedStyle, "opacity");
	assert(constrainedOpacity != nullptr && constrainedOpacity->constraint == 0);
	assert(constrainedOpacity->reason == DevCapabilityReason::CapacityExceeded);
}

void verifyElementIdentityConflictIsDiagnosed() {
	using namespace FlowUi::devMode;
	DevSchemaRegistry registry;
	registry.ensureElement<FirstElement>();
	registry.ensureElement<ConflictingElement>();
	assert(registry.publishPendingAtSafePoint());
	const DevSchemaView view = registry.view();
	assert(view->elements.size() == 1);
	bool sawConflict = false;
	for (const DevDiagnostic& diagnostic : view->diagnostics) {
		sawConflict |= diagnostic.code == DevDiagnosticCode::ElementDefinitionConflict;
	}
	assert(sawConflict);
}

} // namespace schema_test

int main() {
	schema_test::verifySharedClayTypeAndTypedMemberPaths();
	schema_test::verifyOrderIndependentGenerationAndSnapshotLifetime();
	schema_test::verifyCatalogue();
	schema_test::verifyCapacityDiagnosticsRemainPublishable();
	schema_test::verifyElementIdentityConflictIsDiagnosed();
	schema_test::verifyFullWidthEnumValues();
	schema_test::verifyBuiltInThemeSchema();
	schema_test::verifyUnsupportedBranchesRemainVisible();
	schema_test::verifyGenericShapes();
	schema_test::verifyFlowUiSemanticAdapters();
	return 0;
}
