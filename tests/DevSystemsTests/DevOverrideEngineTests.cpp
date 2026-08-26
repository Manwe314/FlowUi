#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <string>
#include <stdexcept>

#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "managers/ThemeManager.hpp"

namespace override_test {

struct Nested {
	float opacity = 0.25f;
};

struct Parameters {
	float amount = 1.0f;
	std::int32_t count = 2;
	std::string label = "authored";
	bool locked = false;
	Nested nested{};
};

struct Element {
	using Parameters = override_test::Parameters;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x6f76657272696465ull};
	static void buildElement(FlowUi::ElementBuildContext<Element>&) {}
};

FLOWUI_DEV_SCHEMA(Nested, FLOWUI_DEV_FIELD(Nested, opacity))

FLOWUI_DEV_SCHEMA(
	Parameters,
	FLOWUI_DEV_FIELD(
		Parameters,
		amount,
		FlowUi::devMode::DevFieldOptions{}.numericRange(0.0, 10.0)),
	FLOWUI_DEV_FIELD(Parameters, count),
	FLOWUI_DEV_FIELD(Parameters, label),
	FLOWUI_DEV_FIELD(
		Parameters,
		locked,
		FlowUi::devMode::DevFieldOptions{}.readOnly()),
	FLOWUI_DEV_FIELD(Parameters, nested))

struct Fixture {
	FlowUi::devMode::DevSchemaRegistry schemas{};
	FlowUi::devSystems::tooling::DevOverrideEngine engine{schemas};
	FlowUi::ThemeManager themes{};
	FlowUi::devMode::DevSchemaView view{};
	FlowUi::devMode::DevTypeId parameterType = 0;

	Fixture() {
		schemas.ensureElement<Element>();
		assert(schemas.publishPendingAtSafePoint());
		view = schemas.view();
		const auto* element = view->findElement(Element::definitionId);
		if (!element) throw std::runtime_error("element schema was not published");
		const auto* parameters = view->type(element->parametersType);
		if (!parameters) throw std::runtime_error("parameter schema was not published");
		parameterType = parameters->id;
	}

	FlowUi::devSystems::tooling::DevOverrideFieldKey field(std::string_view name) const {
		const auto* element = view->findElement(Element::definitionId);
		for (const auto& field : view->fieldsOf(element->parametersType)) {
			if (view->string(field.name) == name) return {parameterType, field.id};
		}
		return {};
	}

	FlowUi::devSystems::tooling::DevOverrideFieldKey nestedField(
		std::string_view ownerName,
		std::string_view fieldName) const {
		const auto* element = view->findElement(Element::definitionId);
		for (const auto& ownerField : view->fieldsOf(element->parametersType)) {
			if (view->string(ownerField.name) != ownerName) continue;
			for (const auto& leaf : view->fieldsOf(ownerField.valueType)) {
				if (view->string(leaf.name) == fieldName) {
					return {parameterType, leaf.id, {ownerField.id}};
				}
			}
		}
		return {};
	}

	template <typename T>
	FlowUi::devSystems::tooling::DevOwnedValue value(const T& source) const {
		FlowUi::devSystems::tooling::DevOwnedValue result;
		assert(engine.copyValue(source, result) ==
			FlowUi::devMode::DevValueOperationStatus::Success);
		return result;
	}

	void commit(FlowUi::devSystems::tooling::DevChangeSet transaction) {
		assert(engine.submit(std::move(transaction)));
		engine.commitAtSafePoint(themes);
	}
};

FlowUi::devSystems::tooling::DevElementOverrideTarget definitionTarget() {
	return {
		.definition = Element::definitionId,
		.scope = FlowUi::devSystems::tooling::DevOverrideScope::Definition,
	};
}

FlowUi::devSystems::tooling::DevElementOverrideTarget instanceTarget(
	FlowUi::WindowId window,
	std::uint64_t instance) {
	return {
		.definition = Element::definitionId,
		.window = window,
		.instance = {.value = instance},
		.scope = FlowUi::devSystems::tooling::DevOverrideScope::ExactInstance,
	};
}

void verifyLayeringAndWindowScope() {
	using namespace FlowUi::devSystems::tooling;
	Fixture fixture;
	DevChangeSet changes{.transaction = 1};
	changes.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = definitionTarget(),
		.field = fixture.field("amount"),
		.layer = DevOverrideLayer::LiveDefinition,
		.value = fixture.value(3.0f),
	});
	changes.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = definitionTarget(),
		.field = fixture.nestedField("nested", "opacity"),
		.layer = DevOverrideLayer::LiveDefinition,
		.value = fixture.value(0.75f),
	});
	changes.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = instanceTarget(11, 91),
		.field = fixture.field("amount"),
		.layer = DevOverrideLayer::LiveInstance,
		.value = fixture.value(7.0f),
	});
	fixture.commit(std::move(changes));
	assert(fixture.engine.commandResults().size() == 1);
	assert(fixture.engine.commandResults().front().applied);

	Parameters exact{};
	fixture.engine.applyElement(Element::definitionId, 11, {.value = 91}, &exact);
	assert(exact.amount == 7.0f);
	assert(exact.nested.opacity == 0.75f);
	Parameters otherWindow{};
	fixture.engine.applyElement(Element::definitionId, 12, {.value = 91}, &otherWindow);
	assert(otherWindow.amount == 3.0f);
	Parameters otherInstance{};
	fixture.engine.applyElement(Element::definitionId, 11, {.value = 92}, &otherInstance);
	assert(otherInstance.amount == 3.0f);
}

void verifyRejectedTransactionIsAtomic() {
	using namespace FlowUi::devSystems::tooling;
	Fixture fixture;
	DevChangeSet changes{.transaction = 2};
	changes.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = definitionTarget(),
		.field = fixture.field("count"),
		.layer = DevOverrideLayer::LiveDefinition,
		.value = fixture.value(std::int32_t{19}),
	});
	changes.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = definitionTarget(),
		.field = fixture.field("amount"),
		.layer = DevOverrideLayer::LiveDefinition,
		.value = fixture.value(40.0f),
	});
	fixture.commit(std::move(changes));
	assert(!fixture.engine.commandResults().front().applied);
	assert(fixture.engine.commandResults().front().status ==
		DevCommandStatus::ConstraintRejected);
	Parameters parameters{};
	fixture.engine.applyElement(Element::definitionId, 1, {.value = 1}, &parameters);
	assert(parameters.count == 2);
	assert(parameters.amount == 1.0f);
}

void verifyPostLogicCaptureAndOverrideProvenance() {
	using namespace FlowUi::devSystems::tooling;
	Fixture fixture;
	fixture.commit(DevChangeSet{
		.transaction = 3,
		.commands = [] {
			std::vector<DevOverrideCommand> commands;
			return commands;
		}(),
	});
	// Empty transactions are rejected without changing capture lifecycle.
	assert(fixture.engine.commandResults().front().status == DevCommandStatus::EmptyTransaction);

	DevChangeSet set{.transaction = 4};
	set.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = definitionTarget(),
		.field = fixture.field("amount"),
		.layer = DevOverrideLayer::LiveDefinition,
		.value = fixture.value(5.0f),
	});
	fixture.commit(std::move(set));

	fixture.engine.beginWindowFrame(21, 100);
	Parameters parameters{};
	fixture.engine.applyElement(Element::definitionId, 21, {.value = 7}, &parameters);
	parameters.count = 44; // Represents a runLogic/build mutation.
	fixture.engine.captureElement(Element::definitionId, 21, {.value = 7}, 9, &parameters);
	fixture.engine.endWindowFrame(21);
	const DevElementCaptureSnapshot& snapshot = fixture.engine.elementSnapshot(21);
	assert(snapshot.frameNumber == 100);
	assert(snapshot.elements.size() == 1);
	assert(snapshot.elements.front().flowNode == 9);
	bool sawAmount = false;
	bool sawCount = false;
	for (const DevCapturedField& captured : snapshot.fields) {
		const auto& schemaField = snapshot.schema->fields[captured.field.value - 1u];
		const std::string_view name = snapshot.schema->string(schemaField.name);
		if (name == "amount") {
			sawAmount = true;
			assert(captured.overridden);
			assert(captured.winningLayer == DevOverrideLayer::LiveDefinition);
			assert(*static_cast<const float*>(captured.value.data()) == 5.0f);
		} else if (name == "count") {
			sawCount = true;
			assert(!captured.overridden);
			assert(*static_cast<const std::int32_t*>(captured.value.data()) == 44);
		}
	}
	assert(sawAmount && sawCount);
}

void verifySchemaGenerationRebind() {
	using namespace FlowUi::devSystems::tooling;
	Fixture fixture;
	DevChangeSet set{.transaction = 5};
	set.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = definitionTarget(),
		.field = fixture.field("label"),
		.layer = DevOverrideLayer::LiveDefinition,
		.value = fixture.value(std::string("retained")),
	});
	fixture.commit(std::move(set));
	fixture.schemas.ensureStruct<std::uint64_t>();
	assert(fixture.schemas.publishPendingAtSafePoint());
	fixture.engine.beginWindowFrame(1, 1);
	Parameters parameters{};
	fixture.engine.applyElement(Element::definitionId, 1, {.value = 1}, &parameters);
	assert(parameters.label == "retained");
	fixture.engine.cancelWindowFrame(1);
}

} // namespace override_test

int main() {
	override_test::verifyLayeringAndWindowScope();
	override_test::verifyRejectedTransactionIsAtomic();
	override_test::verifyPostLogicCaptureAndOverrideProvenance();
	override_test::verifySchemaGenerationRebind();
	return 0;
}
