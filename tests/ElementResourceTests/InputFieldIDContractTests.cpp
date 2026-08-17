#include "TestHarness.hpp"

#include <concepts>
#include <cstdint>
#include <span>
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
	{ manager.applyEdits(id, std::span<const FlowUi::TextReplacement>{}, FlowUi::EditOrigin::Programmatic) }
		-> std::same_as<FlowUi::EditResult>;
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

void testEditingAndShortcutDefaults() {
	const FlowUi::FieldConfig field{};
	FLOWUI_CHECK(field.transactionDetail == FlowUi::TransactionReportDetail::Summary);

	const FlowUi::ShortcutManagerConfig shortcuts{};
	FLOWUI_CHECK(shortcuts.textEditing.enabled);
	FLOWUI_CHECK(shortcuts.textEditing.selectAll);
	FLOWUI_CHECK(shortcuts.textEditing.clipboard);
	FLOWUI_CHECK(shortcuts.textEditing.undoRedoRequests);
	FLOWUI_CHECK(shortcuts.textEditing.wordNavigation);
	FLOWUI_CHECK(shortcuts.textEditing.platform == FlowUi::PlatformShortcutStyle::Auto);

	static_assert(std::is_same_v<decltype(FlowUi::FieldQueryResult{}.transactions),
		std::span<const FlowUi::FieldEditTransaction>>);
	static_assert(std::is_same_v<decltype(FlowUi::FieldQueryResult{}.commandRequests),
		std::span<const FlowUi::FieldCommandRequest>>);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run(
		"input field IDs use an explicit typed numeric key",
		testElementBackedKeysShareOneIdentityDomain);
	runner.run("input editing and shortcut defaults are public", testEditingAndShortcutDefaults);
	return runner.finish();
}
