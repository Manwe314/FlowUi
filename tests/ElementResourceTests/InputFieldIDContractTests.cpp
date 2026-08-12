#include "TestHarness.hpp"

#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "internal/InputFieldKey.hpp"
#include "managers/InputFieldManager.hpp"

namespace {

struct InputElement {
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("flowui/tests/input-field-id");
	struct Parts {
		static constexpr FlowUi::FlowElementPart editor = FlowUi::Part("editor");
	};
};

inline constexpr InputElement kInputElement{};
inline constexpr FlowUi::FlowElementID kElementID{
	.value = 0x1020304050607080ull,
#if FLOW_UI_DEV_MODE
	.debugName = "local-input",
#endif
};
inline constexpr FlowUi::GlobalFlowID kGlobalID =
	FlowUi::Global<kInputElement>("global-input");
inline constexpr FlowUi::FlowElementPartID kPartID =
	FlowUi::PartID(kInputElement, kElementID, InputElement::Parts::editor);

template <typename Request>
concept RequestContainsFieldID = requires(Request request) {
	request.fieldId;
};

template <typename ID>
concept AcceptsCompleteFieldSurface = requires(
	FlowUi::InputFieldManager& manager,
	ID id,
	const FlowUi::FieldRequest& request,
	std::string_view text) {
	{ manager.requestField(id, request) } -> std::same_as<FlowUi::FieldQueryResult>;
	{ manager.requestCaret(id, FlowUi::CaretRequestKind::SetPrimary) } -> std::same_as<void>;
	{ manager.removeField(id) } -> std::same_as<bool>;
	{ manager.replaceText(id, text) } -> std::same_as<bool>;
};

static_assert(!RequestContainsFieldID<FlowUi::FieldRequest>);
static_assert(AcceptsCompleteFieldSurface<FlowUi::FlowElementID>);
static_assert(AcceptsCompleteFieldSurface<FlowUi::GlobalFlowID>);
static_assert(AcceptsCompleteFieldSurface<FlowUi::FlowElementPartID>);
static_assert(AcceptsCompleteFieldSurface<FlowUi::ResourceKey>);
static_assert(!AcceptsCompleteFieldSurface<FlowUi::FlowDefinitionID>);
static_assert(!AcceptsCompleteFieldSurface<FlowUi::FlowElementPart>);
static_assert(!AcceptsCompleteFieldSurface<uint64_t>);
static_assert(!AcceptsCompleteFieldSurface<std::string_view>);

void testElementBackedKeysShareOneIdentityDomain() {
	using namespace FlowUi::detail::input_field;

	constexpr FlowUi::FlowElementID local{.value = 42};
	constexpr FlowUi::GlobalFlowID global{.value = 42};
	constexpr FlowUi::FlowElementPartID part{.value = 42};
	constexpr InputFieldKey resource = resourceInputFieldKey(42);

	static_assert(toInputFieldKey(local) == toInputFieldKey(global));
	static_assert(toInputFieldKey(global) == toInputFieldKey(part));
	static_assert(toInputFieldKey(local) != resource);
	static_assert(std::is_trivially_copyable_v<InputFieldKey>);

	FLOWUI_CHECK(InputFieldKeyHash{}(toInputFieldKey(kElementID)) != 0);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run(
		"input field IDs use an explicit typed numeric key",
		testElementBackedKeysShareOneIdentityDomain);
	return runner.finish();
}
