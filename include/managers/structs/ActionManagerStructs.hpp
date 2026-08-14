#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "FlowUi/AppActionID.hpp"
#include "FlowUi/ElementID.hpp"
#include "FlowUi/WindowId.hpp"

namespace FlowUi {

/** @addtogroup flowui_action_manager
 * @{
 */

#if FLOW_UI_DEV_MODE
struct ActionSourceLocation {
	const char* file = "";
	const char* function = "";
	uint_least32_t lineValue = 0u;
	uint_least32_t columnValue = 0u;

	static constexpr ActionSourceLocation current(
		const char* fileName = __builtin_FILE(),
		const char* functionName = __builtin_FUNCTION(),
		uint_least32_t line = __builtin_LINE(),
		uint_least32_t column = 0u) noexcept {
		return ActionSourceLocation{fileName, functionName, line, column};
	}
};
#endif

struct AppActionCall {
	AppActionID id;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return static_cast<bool>(id);
	}
};

class UiActionCall {
public:
	static constexpr std::size_t InlineBytes = 2u * sizeof(void*);
	using InvokeThunk = void (*)(const std::byte* payload);

	constexpr UiActionCall() noexcept = default;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return invokeThunk_ != nullptr;
	}

private:
	friend class UiActions;
	friend class ActionManager;

	InvokeThunk invokeThunk_ = nullptr;
	std::array<std::byte, InlineBytes> payload_{};
#if FLOW_UI_DEV_MODE
	uint64_t recipeId_ = 0;
	std::string_view recipeName_{};
	ActionSourceLocation definitionSource_{};
#endif
};

enum class ActionCallKind : uint8_t {
	None = 0,
	App,
	Ui,
};

class ActionCall {
public:
	constexpr ActionCall() noexcept
		: payload_{}, kind_(ActionCallKind::None) {}

	constexpr ActionCall(AppActionCall call) noexcept
		: payload_(call), kind_(call ? ActionCallKind::App : ActionCallKind::None) {}

	constexpr ActionCall(UiActionCall call) noexcept
		: payload_(call), kind_(call ? ActionCallKind::Ui : ActionCallKind::None) {}

	[[nodiscard]] constexpr ActionCallKind kind() const noexcept { return kind_; }

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return kind_ != ActionCallKind::None;
	}

private:
	friend class ActionManager;

	union Payload {
		AppActionCall app;
		UiActionCall ui;

		constexpr Payload() noexcept : app{} {}
		constexpr Payload(AppActionCall call) noexcept : app(call) {}
		constexpr Payload(UiActionCall call) noexcept : ui(call) {}
	} payload_;
	ActionCallKind kind_;
};

struct AppActionDescriptor {
	AppActionID id{};
	std::string_view debugName{};
};

struct AppActionAvailability {
	bool enabled = true;
};

struct ActionAvailability {
	bool enabled = false;
};

enum class ActionInvocationStatus : uint8_t {
	Invoked = 0,
	Empty,
	Unbound,
	Disabled,
};

enum class ActionInvokeError : uint8_t {
	Empty = 0,
	Unbound,
	Disabled,
	ResultTypeMismatch,
};

template <typename Result>
class ActionResult {
public:
	ActionResult(Result result)
		: value_(std::move(result)) {}

	ActionResult(ActionInvokeError error) noexcept
		: error_(error) {}

	[[nodiscard]] bool hasValue() const noexcept { return value_.has_value(); }
	[[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }
	[[nodiscard]] Result& value() & { return value_.value(); }
	[[nodiscard]] const Result& value() const& { return value_.value(); }
	[[nodiscard]] Result&& value() && { return std::move(value_).value(); }
	[[nodiscard]] ActionInvokeError error() const noexcept { return error_; }

private:
	std::optional<Result> value_{};
	ActionInvokeError error_ = ActionInvokeError::Empty;
};

template <>
class ActionResult<void> {
public:
	static ActionResult success() noexcept { return ActionResult(true, ActionInvokeError::Empty); }
	ActionResult(ActionInvokeError error) noexcept : ActionResult(false, error) {}

	[[nodiscard]] bool hasValue() const noexcept { return success_; }
	[[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }
	void value() const {}
	[[nodiscard]] ActionInvokeError error() const noexcept { return error_; }

private:
	ActionResult(bool success, ActionInvokeError error) noexcept
		: success_(success), error_(error) {}

	bool success_ = false;
	ActionInvokeError error_ = ActionInvokeError::Empty;
};

enum class ActionInvocationSourceKind : uint8_t {
	Direct = 0,
	UiManager,
	ElementHovered,
	ElementPressed,
	ElementHeld,
	ElementReleased,
	ElementLogic,
	Shortcut,
};

struct ActionInvocationSource {
	ActionInvocationSourceKind kind = ActionInvocationSourceKind::Direct;
	WindowId window = InvalidWindowId;
	FlowElementID element{};
	uint64_t sourceId = 0;
};

#if FLOW_UI_DEV_MODE
/** Safe developer-tooling snapshot; borrowed action resources are never exposed. */
struct ActionDebugInfo {
	ActionCallKind kind = ActionCallKind::None;
	AppActionID appId{};
	uint64_t uiRecipeId = 0;
	std::string_view debugName{};
	ActionAvailability availability{};
	bool bound = false;
	uint64_t invocationCount = 0;
	uint64_t discardedResultCount = 0;
	ActionInvocationStatus lastStatus = ActionInvocationStatus::Empty;
	ActionInvocationSource lastSource{};
	bool lastInvocationThrew = false;
	ActionSourceLocation definitionSource{};
};
#endif

namespace detail::ui_action {

constexpr uint64_t kUiRecipeDomain = 0x98d473f25b1ca60full;

[[nodiscard]] constexpr uint64_t recipeHash(std::string_view text) noexcept {
	return detail::identity_hash::reserveZero(detail::identity_hash::combine(
		kUiRecipeDomain,
		detail::identity_hash::hashBytes(text)));
}

template <typename T>
[[nodiscard]] consteval std::string_view callableTypeName() {
#if defined(__clang__) || defined(__GNUC__)
	return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
	return __FUNCSIG__;
#else
	return "FlowUi::UiAction<unknown>";
#endif
}

template <typename T>
[[nodiscard]] consteval uint64_t callableTypeId() {
	return recipeHash(callableTypeName<T>());
}

template <typename Signature>
struct CallableSignature;

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...) const> {
	using Result = Return;
	using Arguments = std::tuple<Args...>;
};

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...)> {
	using Result = Return;
	using Arguments = std::tuple<Args...>;
};

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...) const noexcept>
	: CallableSignature<Return (Class::*)(Args...) const> {};

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...) noexcept>
	: CallableSignature<Return (Class::*)(Args...)> {};

template <typename Fn>
concept ConcreteRecipeCallable = requires {
	&Fn::operator();
	typename CallableSignature<decltype(&Fn::operator())>::Result;
	typename CallableSignature<decltype(&Fn::operator())>::Arguments;
};

} // namespace detail::ui_action

template <typename Fn>
struct UiActionRecipe {
	[[no_unique_address]] Fn operation;
#if FLOW_UI_DEV_MODE
	uint64_t recipeId = 0;
	std::string_view debugName{};
	ActionSourceLocation definitionSource{};
#endif
};

template <typename Fn>
[[nodiscard]] constexpr auto UiAction(
	Fn fn
#if FLOW_UI_DEV_MODE
	, ActionSourceLocation source = ActionSourceLocation::current()
#endif
	) {
	using Operation = std::remove_cvref_t<Fn>;
	static_assert(std::is_empty_v<Operation>,
		"FlowUi UiAction recipes must be stateless; pass local resources to uiActions().make().");
	static_assert(std::is_trivially_copyable_v<Operation> &&
		std::is_trivially_destructible_v<Operation> &&
		std::is_default_constructible_v<Operation>,
		"FlowUi UiAction recipes must be trivial and default constructible.");
	static_assert(detail::ui_action::ConcreteRecipeCallable<Operation>,
		"FlowUi UiAction recipes need one concrete, non-generic call operator.");
	using Traits = detail::ui_action::CallableSignature<decltype(&Operation::operator())>;
	static_assert(std::is_void_v<typename Traits::Result>,
		"FlowUi UiAction recipes must return void.");
	return UiActionRecipe<Operation>{
		.operation = fn,
#if FLOW_UI_DEV_MODE
		.recipeId = detail::ui_action::callableTypeId<Operation>(),
		.debugName = {},
		.definitionSource = source,
#endif
	};
}

template <std::size_t N, typename Fn>
[[nodiscard]] consteval auto UiAction(
	const char (&name)[N],
	Fn fn
#if FLOW_UI_DEV_MODE
	, ActionSourceLocation source = ActionSourceLocation::current()
#endif
	) {
	static_assert(N > 1, "FlowUi UI action recipe names must not be empty.");
	auto recipe = UiAction(
		fn
#if FLOW_UI_DEV_MODE
		, source
#endif
	);
#if FLOW_UI_DEV_MODE
	recipe.recipeId = detail::ui_action::recipeHash(std::string_view{name, N - 1});
	recipe.debugName = std::string_view{name, N - 1};
#endif
	return recipe;
}

static_assert(std::is_trivially_copyable_v<AppActionCall>);
static_assert(std::is_trivially_copyable_v<UiActionCall>);
static_assert(std::is_trivially_destructible_v<UiActionCall>);
static_assert(std::is_trivially_copyable_v<ActionCall>);
static_assert(std::is_trivially_destructible_v<ActionCall>);
#if !FLOW_UI_DEV_MODE && INTPTR_MAX == INT64_MAX
static_assert(sizeof(AppActionID) == 8u);
static_assert(sizeof(AppActionCall) == 8u);
static_assert(sizeof(UiActionCall) <= 24u);
static_assert(sizeof(ActionCall) <= 32u);
#endif

/** @} */

} // namespace FlowUi
