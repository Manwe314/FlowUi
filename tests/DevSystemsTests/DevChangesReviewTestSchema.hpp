#pragma once

#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace changes_review_test {

struct Parameters {
	FlowUi::TextureRef texture{};
	FlowUi::ActionCall action{};
	float amount = 1;
	std::int32_t count = 2;
};

/** Schema-only fixture; the tests exercise tooling without creating an application window. */
struct Element {
	using Parameters = changes_review_test::Parameters;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x6368616e67657301ull};
	static void buildElement(FlowUi::ElementBuildContext<Element>&);
};
struct UnrelatedElement {
	using Parameters = changes_review_test::Parameters;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x6368616e67657302ull};
	static void buildElement(FlowUi::ElementBuildContext<UnrelatedElement>&);
};

/** Declaration remains in a header while reflection is deliberately defined in the test source. */
struct SourceParameters {
	float amount = 1;
};
struct SourceElement {
	using Parameters = SourceParameters;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x6368616e67657303ull};
	static void buildElement(FlowUi::ElementBuildContext<SourceElement>&);
};

FLOWUI_DEV_SCHEMA(Parameters, FLOWUI_DEV_FIELD(Parameters, texture),
				  FLOWUI_DEV_FIELD(Parameters, action), FLOWUI_DEV_FIELD(Parameters, amount),
				  FLOWUI_DEV_FIELD(Parameters, count))

} // namespace changes_review_test
