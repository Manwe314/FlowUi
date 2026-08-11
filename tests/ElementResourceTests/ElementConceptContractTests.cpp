#include "TestHarness.hpp"

#include <concepts>
#include <string_view>
#include <type_traits>

#include <FlowUi/Flow.hpp>

namespace {

struct ExampleParameters {
	int value = 0;
};

struct ExampleState {
	bool active = false;
};

struct ExampleResources {
	explicit ExampleResources(FlowUi::App&) {}
	~ExampleResources() noexcept = default;
};

struct DrawableElement {
	using Parameters = ExampleParameters;
	using State = ExampleState;
	using Resources = ExampleResources;
	using BuildContext = FlowUi::ElementBuildContext<DrawableElement>;
	using InteractionContext = FlowUi::ElementInteractionContext<DrawableElement>;

	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/drawable");
	static constexpr FlowUi::ElementStatePolicy statePolicy =
		FlowUi::ElementStatePolicy::windowLifetime();
	static constexpr bool isDevInternal = false;
	static constexpr std::string_view debugName = "DrawableElement";

	static void onPressed(InteractionContext&) {}
	static void buildElement(BuildContext&) {}
};

struct ConstructibleElement {
	using BuildContext = FlowUi::ElementBuildContext<ConstructibleElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/constructible");

	static Clay_ElementDeclaration constructElement(BuildContext&) { return {}; }
};

struct DualOutputElement {
	using BuildContext = FlowUi::ElementBuildContext<DualOutputElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/dual-output");

	static Clay_ElementDeclaration constructElement(BuildContext&) { return {}; }
	static void buildElement(BuildContext&) {}
};

inline constexpr DrawableElement kDrawableElement{};
inline constexpr ConstructibleElement kConstructibleElement{};
inline constexpr DualOutputElement kDualOutputElement{};

template <typename Builder>
concept HasDrawTerminal = requires(Builder& builder) { builder.draw(); };

template <typename Builder>
concept HasConstructTerminal = requires(Builder& builder) { builder.construct(); };

struct CyclicParameters;

struct CyclicParameterElement {
	using Parameters = CyclicParameters;
	using BuildContext = FlowUi::ElementBuildContext<CyclicParameterElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/cyclic-parameters");
	static void buildElement(BuildContext&);
};

struct CyclicParameters {
	int value = 0;
};

inline void CyclicParameterElement::buildElement(BuildContext&) {}

struct MissingOutputElement {
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/missing-output");
};

struct NonEmptyElement {
	using BuildContext = FlowUi::ElementBuildContext<NonEmptyElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/non-empty");
	int runtimeConfiguration = 0;
	static void buildElement(BuildContext&) {}
};

struct VoidStateElement {
	using State = void;
	using BuildContext = FlowUi::ElementBuildContext<VoidStateElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/void-state");
	static void buildElement(BuildContext&) {}
};

struct InstanceBuildElement {
	using BuildContext = FlowUi::ElementBuildContext<InstanceBuildElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/instance-build");
	void buildElement(BuildContext&) {}
};

struct WrongReturnElement {
	using BuildContext = FlowUi::ElementBuildContext<WrongReturnElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/wrong-return");
	static int buildElement(BuildContext&) { return 0; }
};

struct WrongOptionalHookElement {
	using BuildContext = FlowUi::ElementBuildContext<WrongOptionalHookElement>;
	using InteractionContext = FlowUi::ElementInteractionContext<WrongOptionalHookElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/wrong-optional-hook");
	static int onPressed(InteractionContext&) { return 0; }
	static void buildElement(BuildContext&) {}
};

struct NonDefaultState {
	NonDefaultState() = delete;
};

struct InvalidStatePayloadElement {
	using State = NonDefaultState;
	using BuildContext = FlowUi::ElementBuildContext<InvalidStatePayloadElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/invalid-state-payload");
	static void buildElement(BuildContext&) {}
};

struct RuntimeMetadataElement {
	using BuildContext = FlowUi::ElementBuildContext<RuntimeMetadataElement>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("tests/concepts/runtime-metadata");
	static inline const char* debugName = "RuntimeMetadataElement";
	static void buildElement(BuildContext&) {}
};

static_assert(FlowUi::FlowElement<DrawableElement>);
static_assert(FlowUi::DrawableFlowElement<DrawableElement>);
static_assert(!FlowUi::ConstructibleFlowElement<DrawableElement>);
static_assert(FlowUi::ConstructibleFlowElement<ConstructibleElement>);
static_assert(!FlowUi::DrawableFlowElement<ConstructibleElement>);
static_assert(FlowUi::DrawableFlowElement<DualOutputElement>);
static_assert(FlowUi::ConstructibleFlowElement<DualOutputElement>);
static_assert(FlowUi::FlowElement<CyclicParameterElement>);
static_assert(HasDrawTerminal<FlowUi::ElementBuilder<DrawableElement>>);
static_assert(!HasConstructTerminal<FlowUi::ElementBuilder<DrawableElement>>);
static_assert(!HasDrawTerminal<FlowUi::ElementBuilder<ConstructibleElement>>);
static_assert(HasConstructTerminal<FlowUi::ElementBuilder<ConstructibleElement>>);
static_assert(HasDrawTerminal<FlowUi::ElementBuilder<DualOutputElement>>);
static_assert(HasConstructTerminal<FlowUi::ElementBuilder<DualOutputElement>>);

static_assert(!FlowUi::FlowElement<MissingOutputElement>);
static_assert(!FlowUi::FlowElement<NonEmptyElement>);
static_assert(!FlowUi::FlowElement<VoidStateElement>);
static_assert(!FlowUi::FlowElement<InstanceBuildElement>);
static_assert(!FlowUi::FlowElement<WrongReturnElement>);
static_assert(!FlowUi::FlowElement<WrongOptionalHookElement>);
static_assert(!FlowUi::FlowElement<InvalidStatePayloadElement>);
static_assert(!FlowUi::FlowElement<RuntimeMetadataElement>);

static_assert(std::same_as<FlowUi::ParametersOf<DrawableElement>, ExampleParameters>);
static_assert(std::same_as<FlowUi::StateOf<DrawableElement>, ExampleState>);
static_assert(std::same_as<FlowUi::ResourcesOf<DrawableElement>, ExampleResources>);
static_assert(std::same_as<FlowUi::ParametersOf<ConstructibleElement>, FlowUi::NoElementParameters>);
static_assert(FlowUi::HasState<DrawableElement>);
static_assert(FlowUi::HasResources<DrawableElement>);
static_assert(!FlowUi::HasState<ConstructibleElement>);
static_assert(!FlowUi::HasResources<ConstructibleElement>);

static_assert(FlowUi::detail::element::elementDescriptor<DrawableElement>.hasState);
static_assert(FlowUi::detail::element::elementDescriptor<DrawableElement>.hasResources);
static_assert(!FlowUi::detail::element::elementDescriptor<ConstructibleElement>.hasState);
static_assert(!FlowUi::detail::element::elementDescriptor<ConstructibleElement>.hasResources);

// Compile every public builder terminal and both element-id entry paths. This
// function is intentionally not executed; its body keeps the header-only typed
// dispatch pipeline instantiated as part of the contract test.
[[maybe_unused]] void compileElementBuilderSurface(
	FlowUi::UiManager& ui,
	FlowUi::ResourceKey resourceId) {
	ui.createElement(kDrawableElement, "tests/builder/draw")
		.setParameters(ExampleParameters{.value = 1})
		.mergeParams([](ExampleParameters& params) { params.value += 1; })
		.withElementID("tests/builder/draw-renamed")
		.draw();

	ui.createElement(kConstructibleElement, resourceId).construct();

	ui.createElement(kDualOutputElement, "tests/builder/dual-draw").draw();
	ui.createElement(kDualOutputElement, "tests/builder/dual-construct").construct();
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("element concept contracts", [] {});
	return runner.finish();
}
