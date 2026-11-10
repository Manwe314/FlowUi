#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <stdexcept>

#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#include "devSystems/devTooling/bake/DevChangesRuntime.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"

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
	std::optional<std::int32_t> optionalCount{};
	const std::int32_t* borrowed = nullptr;
};

struct State { std::uint64_t click_total = 0; };
struct Resources { std::string asset_name{}; };
FLOWUI_DEV_SCHEMA(State, FLOWUI_DEV_FIELD(State, click_total))
FLOWUI_DEV_SCHEMA(Resources, FLOWUI_DEV_FIELD(Resources, asset_name))

struct Element {
	using Parameters = override_test::Parameters;
	using State = override_test::State;
	using Resources = override_test::Resources;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x6f76657272696465ull};
	static void buildElement(FlowUi::ElementBuildContext<Element>&) {}
};

FLOWUI_DEV_SCHEMA(Nested, FLOWUI_DEV_FIELD(Nested, opacity))

FLOWUI_DEV_SCHEMA(
	Parameters,
	FLOWUI_DEV_FIELD(Parameters, amount,
					 FlowUi::devSystems::tooling::DevFieldOptions{}.numericRange(0.0, 10.0)),
	FLOWUI_DEV_FIELD(Parameters, count), FLOWUI_DEV_FIELD(Parameters, label),
	FLOWUI_DEV_FIELD(Parameters, locked, FlowUi::devSystems::tooling::DevFieldOptions{}.readOnly()),
	FLOWUI_DEV_FIELD(Parameters, nested), FLOWUI_DEV_FIELD(Parameters, optionalCount),
	FLOWUI_DEV_FIELD(Parameters, borrowed))

struct Fixture {
	FlowUi::devSystems::tooling::DevSchemaRegistry schemas{};
	FlowUi::devSystems::tooling::DevOverrideEngine engine{schemas};
	FlowUi::ThemeManager themes{};
	FlowUi::devSystems::tooling::DevSchemaView view{};
	FlowUi::devSystems::tooling::DevTypeId parameterType = 0;

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

	FlowUi::devSystems::tooling::DevOverrideFieldKey nestedField(std::string_view ownerName,
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
			   FlowUi::devSystems::tooling::DevValueOperationStatus::Success);
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

FlowUi::devSystems::tooling::DevElementOverrideTarget instanceTarget(FlowUi::WindowId window,
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

void verify_authored_stages_roles_and_lifetimes() {
	using namespace FlowUi::devSystems::tooling;
	Fixture fixture;
	const auto count_key = fixture.field("count");
	fixture.commit({.transaction = 10, .commands = {{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = definitionTarget(), .field = count_key,
		.layer = DevOverrideLayer::LiveDefinition, .value = fixture.value(std::int32_t{30})}}});
	Parameters parameters;
	parameters.count = 10;
	parameters.optionalCount = 7;
	std::int32_t borrowed = 42;
	parameters.borrowed = &borrowed;
	State state{.click_total = 9};
	Resources resources{.asset_name = "owned resource"};
	fixture.engine.beginWindowFrame(21, 101);
	auto authored = fixture.engine.captureAuthoredParameters(Element::definitionId, &parameters);
	parameters.count = 20; // A compiled assignment is applied between the two named stages.
	parameters.optionalCount.reset();
	fixture.engine.applyElement(Element::definitionId, 21, {.value = 7}, &parameters);
	assert(parameters.count == 30);
	fixture.engine.captureElement(Element::definitionId, 21, {.value = 7}, 9, &parameters,
		nullptr, std::move(authored), &state, &resources);
	fixture.engine.endWindowFrame(21);
	parameters.count = 999;
	resources.asset_name = "mutated after capture";
	const auto& snapshot = fixture.engine.elementSnapshot(21);
	assert(snapshot.schema && snapshot.frameNumber == 101 && snapshot.generation);
	const auto& captured_element = snapshot.elements.front();
	assert(captured_element.stateFieldCount == 1 && captured_element.resourceFieldCount == 1);
	const auto& captured_state = snapshot.fields[captured_element.firstStateField];
	assert(captured_state.captureStatus == DevValueCaptureStatus::Available);
	assert(*static_cast<const std::uint64_t*>(captured_state.value.data()) == 9);
	const auto& captured_resources = snapshot.fields[captured_element.firstResourceField];
	assert(*static_cast<const std::string*>(captured_resources.value.data()) == "owned resource");
	for (std::uint32_t offset = 0; offset < captured_element.fieldCount; ++offset) {
		const auto& captured = snapshot.fields[captured_element.firstField + offset];
		const auto& field = snapshot.schema->fields[captured.field.value - 1];
		const auto name = snapshot.schema->string(field.name);
		if (name == "count") {
			assert(captured.authoredStatus == DevValueCaptureStatus::Available);
			assert(*static_cast<const std::int32_t*>(captured.authoredValue.data()) == 10);
			assert(*static_cast<const std::int32_t*>(captured.value.data()) == 30);
		} else if (name == "optionalCount") {
			assert(*static_cast<const std::optional<std::int32_t>*>(captured.authoredValue.data()) == 7);
			assert(!*static_cast<const std::optional<std::int32_t>*>(captured.value.data()));
		} else if (name == "borrowed") {
			assert(captured.captureStatus == DevValueCaptureStatus::Unsupported);
			assert(captured.authoredStatus == DevValueCaptureStatus::Unsupported);
			assert(!captured.value && !captured.authoredValue);
		}
	}

	DevOwnedValue queried;
	assert(query_authored_element_value(fixture.engine, *fixture.view, instanceTarget(21, 7),
		count_key, queried) == DevValueCaptureStatus::Available);
	assert(*static_cast<const std::int32_t*>(queried.data()) == 10);
	assert(query_authored_element_value(fixture.engine, *fixture.view, instanceTarget(21, 7),
		fixture.nestedField("nested", "opacity"), queried) == DevValueCaptureStatus::Available);
	assert(*static_cast<const float*>(queried.data()) == .25f);

	fixture.engine.beginWindowFrame(21, 102);
	parameters.count = 11;
	authored = fixture.engine.captureAuthoredParameters(Element::definitionId, &parameters);
	fixture.engine.applyElement(Element::definitionId, 21, {.value = 7}, &parameters);
	fixture.engine.captureElement(Element::definitionId, 21, {.value = 7}, 9, &parameters,
		nullptr, std::move(authored));
	fixture.engine.endWindowFrame(21);
	const auto published_generation = fixture.engine.elementSnapshot(21).generation;
	fixture.engine.beginWindowFrame(21, 103);
	parameters.count = 99;
	authored = fixture.engine.captureAuthoredParameters(Element::definitionId, &parameters);
	fixture.engine.captureElement(Element::definitionId, 21, {.value = 7}, 9, &parameters,
		nullptr, std::move(authored));
	fixture.engine.cancelWindowFrame(21);
	assert(fixture.engine.elementSnapshot(21).generation == published_generation);
	assert(query_authored_element_value(fixture.engine, *fixture.view, instanceTarget(21, 7),
		count_key, queried) == DevValueCaptureStatus::Available);
	assert(*static_cast<const std::int32_t*>(queried.data()) == 11);

	const auto field = std::find_if(fixture.view->fields.begin(), fixture.view->fields.end(),
		[&](const auto& candidate) { return candidate.id == count_key.field; });
	assert(field != fixture.view->fields.end());
	DevChangesReviewRow row;
	row.has_disk = true;
	row.disk_entry = DevBakeEntry{.definition = Element::definitionId,
		.fieldId = count_key.field, .fieldPath = "count",
		.valueType = fixture.view->type(field->valueType)->id, .valueJson = "20"};
	auto reset = prepare_changes_runtime_edit(*fixture.view, fixture.engine, fixture.themes,
		row, 21, DevChangesRuntimeAction::ResetAuthored);
	assert(reset.available && reset.forward.size() == 1);
	assert(*static_cast<const std::int32_t*>(reset.forward.front().value.data()) == 11);

	fixture.engine.beginWindowFrame(23, 1);
	fixture.engine.captureElement(Element::definitionId, 23, {.value = 9}, 1, &parameters,
		nullptr, {}, nullptr, nullptr, true);
	fixture.engine.endWindowFrame(23);
	assert(fixture.engine.elementSnapshot(23).elements.front().isDevInternal);
	assert(query_authored_element_value(fixture.engine, *fixture.view, definitionTarget(),
		count_key, queried) == DevValueCaptureStatus::Available);
	assert(*static_cast<const std::int32_t*>(queried.data()) == 11);

	fixture.engine.beginWindowFrame(22, 1);
	parameters.count = 50;
	authored = fixture.engine.captureAuthoredParameters(Element::definitionId, &parameters);
	fixture.engine.captureElement(Element::definitionId, 22, {.value = 8}, 1, &parameters,
		nullptr, std::move(authored));
	fixture.engine.endWindowFrame(22);
	assert(query_authored_element_value(fixture.engine, *fixture.view, definitionTarget(),
		count_key, queried) == DevValueCaptureStatus::Ambiguous);
	assert(!queried);
	reset = prepare_changes_runtime_edit(*fixture.view, fixture.engine, fixture.themes,
		row, 21, DevChangesRuntimeAction::ResetAuthored);
	assert(!reset.available && reset.error.find("differ") != std::string::npos);
	fixture.engine.release_window(22);
	assert(fixture.engine.elementSnapshot(22).elements.empty());
	assert(query_authored_element_value(fixture.engine, *fixture.view, definitionTarget(),
		count_key, queried) == DevValueCaptureStatus::Available);
	assert(*static_cast<const std::int32_t*>(queried.data()) == 11);
	reset = prepare_changes_runtime_edit(*fixture.view, fixture.engine, fixture.themes,
		row, 21, DevChangesRuntimeAction::ResetAuthored);
	assert(reset.available);
	fixture.engine.set_value_collection(false);
	assert(query_authored_element_value(fixture.engine, *fixture.view, instanceTarget(21, 7),
		count_key, queried) == DevValueCaptureStatus::Unavailable);
	assert(!queried);
}

void verify_authored_schema_generation_mismatch() {
	using namespace FlowUi::devSystems::tooling;
	Fixture fixture;
	fixture.engine.beginWindowFrame(21, 1);
	Parameters parameters;
	auto authored = fixture.engine.captureAuthoredParameters(Element::definitionId, &parameters);
	fixture.schemas.ensureStruct<std::vector<double>>();
	assert(fixture.schemas.publishPendingAtSafePoint());
	fixture.engine.beginWindowFrame(21, 2);
	fixture.engine.captureElement(Element::definitionId, 21, {.value = 7}, 9, &parameters,
		nullptr, std::move(authored));
	fixture.engine.endWindowFrame(21);
	const auto& snapshot = fixture.engine.elementSnapshot(21);
	assert(snapshot.schema->generation != fixture.view->generation);
	for (std::uint32_t offset = 0; offset < snapshot.elements.front().fieldCount; ++offset) {
		const auto& field = snapshot.fields[snapshot.elements.front().firstField + offset];
		assert(field.authoredStatus == DevValueCaptureStatus::SchemaMismatch);
		assert(!field.authoredValue);
	}
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
	override_test::verify_authored_stages_roles_and_lifetimes();
	override_test::verify_authored_schema_generation_mismatch();
	return 0;
}
