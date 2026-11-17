#include "TestHarness.hpp"

#include <FlowUi/Public/Flow.hpp>

#include <concepts>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

struct PreparationResources {
	inline static int constructionCount = 0;
	explicit PreparationResources(FlowUi::App&) { ++constructionCount; }
};

struct PreparationElement {
	using Resources = PreparationResources;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/public-manager-contract/resource-owner");
	static void buildElement(FlowUi::ElementBuildContext<PreparationElement>& context) {
		FLOWUI_CHECK(context.id.value != 0);
	}
};

struct ResourceFreeElement {
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/public-manager-contract/resource-free");
	static void buildElement(FlowUi::ElementBuildContext<ResourceFreeElement>& context) {
		FLOWUI_CHECK(context.id.value != 0);
	}
};

struct ThemePayload { float radius = 2; };

static_assert(std::same_as<decltype(std::declval<FlowUi::ActionManager&>().invoke(FlowUi::ActionCall{})),
	FlowUi::ActionInvocationStatus>);
static_assert(std::same_as<decltype(std::declval<FlowUi::AppActions&>().invoke(FlowUi::AppActionCall{})),
	FlowUi::ActionInvocationStatus>);
static_assert(std::same_as<decltype(std::declval<FlowUi::AppActions&>().invokeFor<int>(FlowUi::AppActionCall{})),
	FlowUi::ActionResult<int>>);
static_assert(!noexcept(std::declval<FlowUi::AppActions&>().invoke(FlowUi::AppActionCall{})));
static_assert(!noexcept(std::declval<FlowUi::AppActions&>().invokeFor<int>(FlowUi::AppActionCall{})));
static_assert(!noexcept(std::declval<FlowUi::ActionManager&>().invoke(FlowUi::ActionCall{})));
static_assert(std::same_as<decltype(std::declval<FlowUi::UiManager&>().actions()), FlowUi::ActionManager&>);
static_assert(std::same_as<decltype(std::declval<const FlowUi::UiManager&>().actions()), const FlowUi::ActionManager&>);
static_assert(std::same_as<decltype(std::declval<FlowUi::UiManager&>().elements()), FlowUi::ElementManager&>);
static_assert(std::same_as<decltype(std::declval<const FlowUi::UiManager&>().elements()), const FlowUi::ElementManager&>);
static_assert(std::same_as<decltype(std::declval<FlowUi::UiManager&>().invoke(FlowUi::ActionCall{})),
	FlowUi::ActionInvocationStatus>);
static_assert(std::same_as<decltype(std::declval<FlowUi::UiManager&>().createElement(
	PreparationElement{}, FlowUi::LocalElementName{"resource"})), FlowUi::ElementBuilder<PreparationElement>>);
static_assert(std::same_as<decltype(std::declval<FlowUi::UiManager&>().drawConstructed()), void>);
static_assert(!noexcept(std::declval<FlowUi::UiManager&>().actions()));
static_assert(!noexcept(std::declval<FlowUi::UiManager&>().drawConstructed()));
static_assert(std::same_as<decltype(std::declval<FlowUi::ElementManager&>().prepare(PreparationElement{})), FlowUi::Status>);
static_assert(!noexcept(std::declval<FlowUi::ElementManager&>().prepare(PreparationElement{})));
static_assert(std::same_as<decltype(std::declval<FlowUi::ThemeManager&>().setActiveVariant<ThemePayload>("default")), FlowUi::Status>);
static_assert(!noexcept(std::declval<FlowUi::ThemeManager&>().setActiveVariant<ThemePayload>("default")));

void verifyDirectInvocationAndExceptionIdentity() {
	FlowUi::ActionManager actions;
	bool toggled = false;
	constexpr auto toggle = FlowUi::UiAction("tests.public-manager-contract/toggle",
		[](bool& target) { target = !target; });
	const auto call = actions.uiActions().make(toggle, toggled);
	FLOWUI_CHECK(actions.invoke(call) == FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(toggled);
	FLOWUI_CHECK(actions.invoke({}) == FlowUi::ActionInvocationStatus::Empty);
	FLOWUI_CHECK(actions.appActions().invoke({}) == FlowUi::ActionInvocationStatus::Empty);

	int invocationCount = 0;
	constexpr auto fail = FlowUi::UiAction("tests.public-manager-contract/throw-once",
		[](int& count) {
			++count;
			throw std::runtime_error("original application failure");
		});
	bool originalCaught = false;
	try {
		(void)actions.invoke(actions.uiActions().make(fail, invocationCount));
	} catch (const std::runtime_error& exception) {
		originalCaught = std::string_view(exception.what()) == "original application failure";
	}
	FLOWUI_CHECK(originalCaught);
	FLOWUI_CHECK(invocationCount == 1);
}

void verifyHybridOwnerValidation() {
	FlowUi::ElementManager elements;
	PreparationResources::constructionCount = 0;
	const auto missingOwner = elements.prepare(PreparationElement{});
	FLOWUI_CHECK(!missingOwner && missingOwner.error().code == FlowUi::ErrorCode::ObjectNotInitialized);
	FLOWUI_CHECK(missingOwner.error().site == FlowUi::ErrorSite::ElementResolveResource);
	FLOWUI_CHECK(PreparationResources::constructionCount == 0);
	FLOWUI_CHECK(elements.prepare(ResourceFreeElement{}));

	FlowUi::ThemeManager themes;
	const auto missingTheme = themes.setActiveVariant<ThemePayload>("default");
	FLOWUI_CHECK(!missingTheme && missingTheme.error().code == FlowUi::ErrorCode::ObjectNotInitialized);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("public action invocation returns domain values and preserves application exceptions",
		verifyDirectInvocationAndExceptionIdentity);
	runner.run("hybrid public managers return detached-owner validation before extension construction",
		verifyHybridOwnerValidation);
	return runner.finish();
}
