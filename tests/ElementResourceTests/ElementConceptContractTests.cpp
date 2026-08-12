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
	struct Parts {
		static constexpr auto label = FlowUi::Part("label");
	};

	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/drawable");
	static constexpr FlowUi::ElementStatePolicy statePolicy =
		FlowUi::ElementStatePolicy::windowLifetime();
	static constexpr bool isDevInternal = false;
	static constexpr std::string_view debugName = "DrawableElement";

	static void onPressed(InteractionContext&) {}
	static void buildElement(BuildContext&) {}
};

struct ConstructibleElement {
	using BuildContext = FlowUi::ElementBuildContext<ConstructibleElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/constructible");

	static Clay_ElementDeclaration constructElement(BuildContext&) { return {}; }
};

struct DualOutputElement {
	using BuildContext = FlowUi::ElementBuildContext<DualOutputElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/dual-output");

	static Clay_ElementDeclaration constructElement(BuildContext&) { return {}; }
	static void buildElement(BuildContext&) {}
};

inline constexpr DrawableElement kDrawableElement{};
inline constexpr ConstructibleElement kConstructibleElement{};
inline constexpr DualOutputElement kDualOutputElement{};

struct ForwardedPartParameters {
	FlowUi::FlowElementPartID destination{};
};

struct ForwardedPartElement {
	using Parameters = ForwardedPartParameters;
	using BuildContext = FlowUi::ElementBuildContext<ForwardedPartElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/forwarded-part");

	static void buildElement(BuildContext& context) {
		context.uiManager.createElement(
			DrawableElement{}, context.params.destination).draw();
	}
};

inline constexpr ForwardedPartElement kForwardedPartElement{};

template <typename Manager>
concept AcceptsImplicitRuntimeElementName = requires(
	Manager& ui,
	std::string_view runtimeName) {
	ui.createElement(kDrawableElement, runtimeName);
};

template <typename Manager>
concept AcceptsResourceKeyElementName = requires(
	Manager& ui,
	FlowUi::ResourceKey key) {
	ui.createElement(kDrawableElement, key);
};

template <typename Context>
concept HasLegacyStringElementIdentity = requires(Context& context) {
	context.elementID;
	context.createChildElementId("child");
};

template <typename Builder>
concept HasDrawTerminal = requires(Builder& builder) { builder.draw(); };

template <typename Builder>
concept HasConstructTerminal = requires(Builder& builder) { builder.construct(); };

template <typename Manager>
concept AcceptsBoundPartID = requires(
	Manager& ui,
	FlowUi::FlowElementPartID partId) {
	ui.createElement(kDrawableElement, partId);
};

template <typename Manager>
concept AcceptsBarePartDeclaration = requires(
	Manager& ui,
	FlowUi::FlowElementPart part) {
	ui.createElement(kDrawableElement, part);
};

template <typename Context>
concept HasSemanticPartSurface = requires(
	Context& context,
	FlowUi::FlowElementPart declaration) {
	{ context.part(declaration) } -> std::same_as<FlowUi::FlowElementPartID>;
	context.createPart(kDrawableElement, declaration);
};

struct CyclicParameters;

struct CyclicParameterElement {
	using Parameters = CyclicParameters;
	using BuildContext = FlowUi::ElementBuildContext<CyclicParameterElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/cyclic-parameters");
	static void buildElement(BuildContext&);
};

struct CyclicParameters {
	int value = 0;
};

inline void CyclicParameterElement::buildElement(BuildContext&) {}

struct MissingOutputElement {
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/missing-output");
};

struct NonEmptyElement {
	using BuildContext = FlowUi::ElementBuildContext<NonEmptyElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/non-empty");
	int runtimeConfiguration = 0;
	static void buildElement(BuildContext&) {}
};

struct VoidStateElement {
	using State = void;
	using BuildContext = FlowUi::ElementBuildContext<VoidStateElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/void-state");
	static void buildElement(BuildContext&) {}
};

struct VoidPartsElement {
	using Parts = void;
	using BuildContext = FlowUi::ElementBuildContext<VoidPartsElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/void-parts");
	static void buildElement(BuildContext&) {}
};

struct InstanceBuildElement {
	using BuildContext = FlowUi::ElementBuildContext<InstanceBuildElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/instance-build");
	void buildElement(BuildContext&) {}
};

struct WrongReturnElement {
	using BuildContext = FlowUi::ElementBuildContext<WrongReturnElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/wrong-return");
	static int buildElement(BuildContext&) { return 0; }
};

struct WrongOptionalHookElement {
	using BuildContext = FlowUi::ElementBuildContext<WrongOptionalHookElement>;
	using InteractionContext = FlowUi::ElementInteractionContext<WrongOptionalHookElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/wrong-optional-hook");
	static int onPressed(InteractionContext&) { return 0; }
	static void buildElement(BuildContext&) {}
};

struct NonDefaultState {
	NonDefaultState() = delete;
};

struct InvalidStatePayloadElement {
	using State = NonDefaultState;
	using BuildContext = FlowUi::ElementBuildContext<InvalidStatePayloadElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/invalid-state-payload");
	static void buildElement(BuildContext&) {}
};

struct RuntimeMetadataElement {
	using BuildContext = FlowUi::ElementBuildContext<RuntimeMetadataElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/concepts/runtime-metadata");
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
static_assert(AcceptsBoundPartID<FlowUi::UiManager>);
static_assert(!AcceptsBarePartDeclaration<FlowUi::UiManager>);
static_assert(HasSemanticPartSurface<DrawableElement::BuildContext>);
static_assert(HasSemanticPartSurface<DrawableElement::InteractionContext>);

static_assert(!FlowUi::FlowElement<MissingOutputElement>);
static_assert(!FlowUi::FlowElement<NonEmptyElement>);
static_assert(!FlowUi::FlowElement<VoidStateElement>);
static_assert(!FlowUi::FlowElement<VoidPartsElement>);
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
static_assert(FlowUi::HasParts<DrawableElement>);
static_assert(!FlowUi::HasState<ConstructibleElement>);
static_assert(!FlowUi::HasResources<ConstructibleElement>);
static_assert(!FlowUi::HasParts<ConstructibleElement>);

static_assert(FlowUi::detail::element::elementDescriptor<DrawableElement>.hasState);
static_assert(FlowUi::detail::element::elementDescriptor<DrawableElement>.hasResources);
static_assert(!FlowUi::detail::element::elementDescriptor<ConstructibleElement>.hasState);
static_assert(!FlowUi::detail::element::elementDescriptor<ConstructibleElement>.hasResources);
static_assert(!AcceptsImplicitRuntimeElementName<FlowUi::UiManager>);
static_assert(!AcceptsResourceKeyElementName<FlowUi::UiManager>);
static_assert(!HasLegacyStringElementIdentity<DrawableElement::BuildContext>);
static_assert(!HasLegacyStringElementIdentity<DrawableElement::InteractionContext>);

// Compile every public builder terminal and both element-id entry paths. This
// function is intentionally not executed; its body keeps the header-only typed
// dispatch pipeline instantiated as part of the contract test.
[[maybe_unused]] void compileElementBuilderSurface(
	FlowUi::UiManager& ui,
	std::string_view runtimeName) {
	ui.createElement(kDrawableElement, "tests/builder/draw")
		.setParameters(ExampleParameters{.value = 1})
		.mergeParams([](ExampleParameters& params) { params.value += 1; })
		.withID("tests/builder/draw-renamed")
		.draw();

	ui.createElement(kConstructibleElement, FlowUi::RuntimeName(runtimeName)).construct();
	ui.createElement(kDrawableElement, FlowUi::Indexed("row", 7)).draw();
	auto sequence = FlowUi::IndexedIDs("channel");
	ui.createElement(kDrawableElement, sequence.next()).draw();
	ui.createElement(kDrawableElement).draw();
	ui.createElement(
		kDrawableElement,
		FlowUi::Global<kDrawableElement>("master"))
		.withID(FlowUi::Global<kDrawableElement>("master-renamed"))
		.draw();

	const FlowUi::FlowElementID owner = FlowUi::detail::element_id::resolveLocal(
		FlowUi::RootFlowScopeID,
		DrawableElement::definitionId,
		FlowUi::LocalElementName("parts-owner").token);
	const FlowUi::FlowElementPartID labelPart = FlowUi::PartID(
		kDrawableElement, owner, DrawableElement::Parts::label);
	ui.createElement(kDrawableElement, labelPart).withID(labelPart).draw();
	ui.createElement(kForwardedPartElement, "part-forwarder")
		.setParameters(ForwardedPartParameters{.destination = labelPart})
		.draw();

	ui.createElement(kDualOutputElement, "tests/builder/dual-draw").draw();
	ui.createElement(
		kDualOutputElement,
		FlowUi::detail::element_id::resolveLocal(
			FlowUi::RootFlowScopeID,
			DualOutputElement::definitionId,
			FlowUi::LocalElementName("tests/builder/dual-construct").token)).construct();
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("element concept contracts", [] {});
	return runner.finish();
}
