#ifdef NDEBUG
#undef NDEBUG
#endif

#include "FlowUi/Public/Flow.hpp"
#include "devSystems/devTooling/DevTooling.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

#if !FLOW_UI_DEV_MODE || !FLOW_UI_HAS_BAKED_CHANGES
#error "The baked/internal builder regression requires development and compiled assignments."
#endif

namespace baked_isolation_test {

struct Parameters {
	std::vector<std::int32_t>* observations = nullptr;
	std::int32_t value = 10;
};

FLOWUI_DEV_SCHEMA(Parameters, FLOWUI_DEV_FIELD(Parameters, observations),
	FLOWUI_DEV_FIELD(Parameters, value))

struct Probe {
	using Parameters = baked_isolation_test::Parameters;
	using BuildContext = FlowUi::ElementBuildContext<Probe>;
	static constexpr auto definitionId = FlowUi::DefinitionID("tests.baked-isolation.probe");

	static void buildElement(BuildContext& context) {
		context.params.observations->emplace_back(context.params.value);
		CLAY(context.clayID(), {.layout = {.sizing = {
			CLAY_SIZING_FIXED(30), CLAY_SIZING_FIXED(20)}}});
	}

	[[nodiscard]] static Clay_ElementDeclaration constructElement(BuildContext& context) {
		context.params.observations->emplace_back(context.params.value);
		Clay_ElementDeclaration declaration{};
		declaration.layout.sizing = {CLAY_SIZING_FIXED(30), CLAY_SIZING_FIXED(20)};
		return declaration;
	}
};

inline constexpr Probe probe{};

} // namespace baked_isolation_test

namespace FlowUi::baked {

// A compiled fixture assignment uses the generated-default dispatch without touching the
// developer's active manifest. The old builder applied this to internal UI as well.
template <>
inline void applyBakedElementDefaults<baked_isolation_test::Probe,
	baked_isolation_test::Parameters>(detail::element::ElementInstanceKey,
	baked_isolation_test::Parameters& parameters) noexcept {
	parameters.value = 20;
}

} // namespace FlowUi::baked

namespace baked_isolation_test {

struct InternalParent {
	using Parameters = baked_isolation_test::Parameters;
	using BuildContext = FlowUi::ElementBuildContext<InternalParent>;
	static constexpr auto definitionId = FlowUi::DefinitionID("tests.baked-isolation.parent");
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context) {
		context.uiManager.createElement(probe, "nested-draw")
			.setParameters(context.params).draw();
		context.uiManager.createElement(probe, "nested-construct")
			.setParameters(context.params).construct();
		context.uiManager.drawConstructed();
	}

	[[nodiscard]] static Clay_ElementDeclaration constructElement(BuildContext&) {
		Clay_ElementDeclaration declaration{};
		declaration.layout.sizing = {CLAY_SIZING_FIXED(100), CLAY_SIZING_FIXED(50)};
		return declaration;
	}
};

inline constexpr InternalParent internal_parent{};

} // namespace baked_isolation_test

int main() {
	using namespace FlowUi;
	using namespace FlowUi::devSystems::tooling;
	using namespace baked_isolation_test;
	AppConfig configuration{};
	configuration.window.title = "Compiled assignment isolation regression";
	configuration.window.width = 640;
	configuration.window.height = 480;
	configuration.vk.enableValidation = false;
	configuration.dev.excludeInternalDevElementsFromCapture = false;
	auto app = makeApplication(configuration);
	auto& schemas = app.devTooling().schemas();
	schemas.ensureElement<Probe>();
	assert(schemas.publishPendingAtSafePoint());
	const auto schema = schemas.view();
	const auto* element = schema->findElement(Probe::definitionId);
	assert(element);
	const auto fields = schema->fieldsOf(element->parametersType);
	const auto value_field = std::find_if(fields.begin(), fields.end(),
		[&](const auto& field) { return schema->string(field.name) == "value"; });
	assert(value_field != fields.end());
	std::vector<std::int32_t> observations;
	observations.reserve(8);
	const Parameters parameters{.observations = &observations};
	auto& ui = app.ui();
	const auto check_snapshot = [&](std::int32_t effective_value, std::size_t expected_count) {
		const auto& capture = app.devTooling().overrides().elementSnapshot(app.mainWindowId());
		assert(capture.schema && capture.frameNumber && capture.generation);
		std::size_t observed_count = 0;
		for (const auto& captured_element : capture.elements) {
			if (captured_element.definition != Probe::definitionId)
				continue;
			for (std::uint32_t offset = 0; offset < captured_element.fieldCount; ++offset) {
				const auto& field = capture.fields[captured_element.firstField + offset];
				const auto& descriptor = capture.schema->fields[field.field.value - 1];
				if (capture.schema->string(descriptor.name) != "value" ||
					field.authoredStatus != DevValueCaptureStatus::Available)
					continue;
				assert(field.captureStatus == DevValueCaptureStatus::Available);
				assert(*static_cast<const std::int32_t*>(field.authoredValue.data()) == 10);
				assert(*static_cast<const std::int32_t*>(field.value.data()) == effective_value);
				++observed_count;
			}
		}
		assert(observed_count == expected_count);
	};
	assert(app.beginFrame());
	ui.createElement(probe, "baked-only").setParameters(parameters).draw();
	assert(observations == std::vector<std::int32_t>{20});
	assert(app.endFrame());
	assert(app.drawFrame());
	check_snapshot(20, 1);

	auto& overrides = app.devTooling().overrides();
	DevOwnedValue live_value;
	assert(overrides.copyValue(std::int32_t{30}, live_value) == DevValueOperationStatus::Success);
	assert(overrides.submit({.transaction = 1, .commands = {{
		.kind = DevOverrideCommandKind::SetElementField,
		.element = {.definition = Probe::definitionId, .scope = DevOverrideScope::Definition},
		.field = {.ownerType = schema->type(element->parametersType)->id, .field = value_field->id},
		.layer = DevOverrideLayer::LiveDefinition, .value = std::move(live_value)}}}));
	overrides.commitAtSafePoint(app.themes());
	assert(overrides.commandResults().front().applied);
	observations.clear();
	assert(app.beginFrame());
	ui.createElement(probe, "application-draw").setParameters(parameters).draw();
	ui.createElement(probe, "application-construct").setParameters(parameters).construct();
	ui.drawConstructed();
	ui.createElement(probe, "explicit-internal").setParameters(parameters)
		.setDevInternalCapture(true).draw();
	ui.createElement(internal_parent, "inherited-internal").setParameters(parameters).draw();
	ui.createElement(internal_parent, "constructed-internal").setParameters(parameters).construct();
	ui.createElement(probe, "constructed-child").setParameters(parameters).draw();
	ui.drawConstructed();
	ui.createElement(probe, "application-after-internal").setParameters(parameters).draw();
	assert((observations == std::vector<std::int32_t>{30, 30, 10, 10, 10, 10, 30}));
	assert(!ui.isDevInternalScope());
	assert(app.endFrame());
	assert(app.drawFrame());
	check_snapshot(30, 3);
	return 0;
}
