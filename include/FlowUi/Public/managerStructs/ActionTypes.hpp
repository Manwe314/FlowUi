#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "FlowUi/Private/actionService/UiActionRecipeSupport.hpp"
#include "FlowUi/Public/identificationTypes/AppActionID.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"

namespace FlowUi {

/** @addtogroup flowui_action_manager
 * @{
 */

#if FLOW_UI_DEV_MODE
/**
 * Borrowed source-location metadata recorded only in developer builds. File and function strings must
 * outlive use of this value.
 */
struct ActionSourceLocation {
	/** Borrowed source file name. */
	const char* file = "";
	/** Borrowed source function name. */
	const char* function = "";
	/** Source line; zero means unavailable. */
	uint_least32_t lineValue = 0u;
	/** Source column; zero means unavailable. */
	uint_least32_t columnValue = 0u;

	/** Return the current authored call site without allocating. */
	[[nodiscard]] static constexpr ActionSourceLocation
	current(const char* fileName = __builtin_FILE(),
			const char* functionName = __builtin_FUNCTION(), uint_least32_t line = __builtin_LINE(),
			uint_least32_t column = 0u) noexcept {
		return ActionSourceLocation{fileName, functionName, line, column};
	}
};
#endif

/**
 * Copyable call to an application action by stable identity; binding and availability are checked at
 * invocation.
 */
struct AppActionCall {
	/** Application action identity; value-initialize the call to obtain an empty call. */
	AppActionID id;

	/** Return whether this value contains a nonempty call. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return static_cast<bool>(id);
	}
};

/**
 * Small copyable UI operation with inline borrowed resource bindings. Obtain it from
 * UiActions::make(); bound resources must outlive invocation.
 */
class UiActionCall {
public:
	/** Maximum inline payload bytes available for UI resource bindings. */
	static constexpr std::size_t InlineBytes = 2u * sizeof(void*);
	/** Dispatch function for the inline payload; used by the action binding machinery. */
	using InvokeThunk = void (*)(const std::byte* payload);

	/** Construct an empty UI call. */
	constexpr UiActionCall() noexcept = default;

	/** Return whether this value contains a nonempty call. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return invokeThunk_ != nullptr;
	}

private:
	friend class UiActions;
	friend class ActionManager;
	friend class ActionCall;

	InvokeThunk invokeThunk_ = nullptr;
	std::array<std::byte, InlineBytes> payload_{};
#if FLOW_UI_DEV_MODE
	uint64_t recipeId_ = 0;
	bool reconstructable_ = false;
	std::string_view recipeName_{};
	ActionSourceLocation definitionSource_{};
#endif
};

/** Stored call representation; UiRecipe resolves a previously published UI recipe by stable identity. */
enum class ActionCallKind : uint8_t {
	/** No call or source identity. */
	None = 0,
	/** Stable application action dispatched through its binding. */
	App,
	/** UI call with inline borrowed resource bindings. */
	Ui,
	/** Stable reference resolved through a previously published UI recipe. */
	UiRecipe,
};

/** Serializable action identity without resource pointers. An empty reference cannot reconstruct a call. */
struct ActionStableReference {
	/** Identity representation; None marks an empty reference or diagnostic record. */
	ActionCallKind kind = ActionCallKind::None;
	/** Stable application or recipe identity; zero marks an empty reference. */
	uint64_t id = 0;
};

/**
 * Type-erased application or UI action passed to elements, shortcuts, and ActionManager. Copying never
 * extends the lifetime of borrowed UI resources.
 */
class ActionCall {
public:
	/** Construct an empty call. */
	constexpr ActionCall() noexcept
		: payload_{}, kind_(ActionCallKind::None) {}

	/** Wrap an application call; an invalid action ID becomes an empty call. */
	constexpr ActionCall(AppActionCall call) noexcept
		: payload_(call), kind_(call ? ActionCallKind::App : ActionCallKind::None) {}

	/** Wrap a UI call without taking ownership of its resources. */
	constexpr ActionCall(UiActionCall call) noexcept
		: payload_(call), kind_(call ? ActionCallKind::Ui : ActionCallKind::None) {}

	/** Return the stored representation, including unresolved UiRecipe calls. */
	[[nodiscard]] constexpr ActionCallKind kind() const noexcept { return kind_; }
	/**
	 * Return a stable reference when supported. Inline UI calls expose recipe identity only in
	 * developer builds and only when reconstructable.
	 */
	[[nodiscard]] constexpr ActionStableReference stableReference() const noexcept {
		if (kind_ == ActionCallKind::App) {
			return {ActionCallKind::App, payload_.app.id.value};
		}
		if (kind_ == ActionCallKind::UiRecipe) {
			return {ActionCallKind::Ui, payload_.stableId};
		}
#if FLOW_UI_DEV_MODE
		if (kind_ == ActionCallKind::Ui) {
			return payload_.ui.reconstructable_
				? ActionStableReference{ActionCallKind::Ui, payload_.ui.recipeId_}
				: ActionStableReference{};
		}
#endif
		return {};
	}

	/**
	 * Rebuild an application call or unresolved UI recipe reference. A zero ID or unsupported kind
	 * returns an empty call; no binding is created.
	 */
	[[nodiscard]] static constexpr ActionCall fromStable(
		ActionCallKind kind, uint64_t stableId) noexcept {
		if (kind == ActionCallKind::App && stableId != 0u) {
			return ActionCall{AppActionCall{AppActionID{
				.value = stableId
#if FLOW_UI_DEV_MODE
				, .debugName = {}
#endif
			}}};
		}
		ActionCall result{};
		if (kind == ActionCallKind::Ui && stableId != 0u) {
			result.payload_.stableId = stableId;
			result.kind_ = ActionCallKind::UiRecipe;
		}
		return result;
	}

	/** Return whether this value contains a nonempty call. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return kind_ != ActionCallKind::None;
	}

private:
	friend class ActionManager;

	union Payload {
		AppActionCall app;
		UiActionCall ui;
		uint64_t stableId;

		constexpr Payload() noexcept : app{} {}
		constexpr Payload(AppActionCall call) noexcept : app(call) {}
		constexpr Payload(UiActionCall call) noexcept : ui(call) {}
	} payload_;
	ActionCallKind kind_;
};

/** Stable application action identity and optional borrowed developer label. */
struct AppActionDescriptor {
	/** Stable application action identity. */
	AppActionID id{};
	/** Borrowed label for diagnostics; keep its backing storage alive while the value is retained. */
	std::string_view debugName{};
};

/** Availability published by an application action binding. */
struct AppActionAvailability {
	/** Whether the bound application operation accepts invocation. */
	bool enabled = true;
};

/** Effective availability of a resolved action call; empty and unbound calls are disabled. */
struct ActionAvailability {
	/** Whether the resolved call is currently invocable. */
	bool enabled = false;
};

/** Outcome of invoking an action while discarding any result. */
enum class ActionInvocationStatus : uint8_t {
	/** The operation ran successfully. */
	Invoked = 0,
	/** The call contains no operation. */
	Empty,
	/** The application binding or published UI recipe is unavailable. */
	Unbound,
	/** The resolved operation is disabled. */
	Disabled,
};

/** Reason a typed application-action invocation did not produce a result. */
enum class ActionInvokeError : uint8_t {
	/** The call contains no application operation. */
	Empty = 0,
	/** The application binding or published UI recipe is unavailable. */
	Unbound,
	/** The resolved operation is disabled. */
	Disabled,
	/** The requested result type differs from the bound action result. */
	ResultTypeMismatch,
};

/** Typed action return value or dispatch failure. Check hasValue() before value(); error() is meaningful only on failure. Exceptions thrown by an invoked action propagate separately.
 * @tparam Result Owned action result type. */
template <typename Result>
class ActionResult {
public:
	/** Store an owned successful result. */
	ActionResult(Result result)
		: value_(std::move(result)) {}

	/** Store a dispatch failure. */
	ActionResult(ActionInvokeError error) noexcept
		: error_(error) {}

	/** Return whether this outcome owns a successful action result. */
	[[nodiscard]] bool hasValue() const noexcept { return value_.has_value(); }
	/** Return whether invocation succeeded. */
	[[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }
	/** Borrow the successful result; throws std::bad_optional_access on failure. */
	[[nodiscard]] Result& value() & { return value_.value(); }
	/** Borrow the successful result as const; throws std::bad_optional_access on failure. */
	[[nodiscard]] const Result& value() const& { return value_.value(); }
	/** Move from the successful result; throws std::bad_optional_access on failure. */
	[[nodiscard]] Result&& value() && { return std::move(value_).value(); }
	/** Return the dispatch error; meaningful only when hasValue() is false. */
	[[nodiscard]] ActionInvokeError error() const noexcept { return error_; }

private:
	std::optional<Result> value_{};
	ActionInvokeError error_ = ActionInvokeError::Empty;
};

/** Success/failure outcome for an action that returns no value. error() is meaningful only on failure. */
template <>
class ActionResult<void> {
public:
	/** Return a successful action invocation with no value. */
	[[nodiscard]] static ActionResult success() noexcept {
		return ActionResult(true, ActionInvokeError::Empty);
	}
	/** Construct a failed no-value invocation outcome. */
	ActionResult(ActionInvokeError error) noexcept : ActionResult(false, error) {}

	/** Return whether the void operation completed successfully. */
	[[nodiscard]] bool hasValue() const noexcept { return success_; }
	/** Return whether invocation succeeded. */
	[[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }
	/** No-value compatibility observer; it performs no validation. */
	void value() const {}
	/** Return the dispatch error; meaningful only when hasValue() is false. */
	[[nodiscard]] ActionInvokeError error() const noexcept { return error_; }

private:
	ActionResult(bool success, ActionInvokeError error) noexcept
		: success_(success), error_(error) {}

	bool success_ = false;
	ActionInvokeError error_ = ActionInvokeError::Empty;
};

/** Origin of an action invocation recorded for diagnostics. */
enum class ActionInvocationSourceKind : uint8_t {
	/** Direct caller invocation without a more specific source. */
	Direct = 0,
	/** Invocation requested by UiManager. */
	UiManager,
	/** An element hover callback. */
	ElementHovered,
	/** An element press callback. */
	ElementPressed,
	/** An element held callback. */
	ElementHeld,
	/** An element release callback. */
	ElementReleased,
	/** An element logic callback. */
	ElementLogic,
	/** A registered keyboard shortcut. */
	Shortcut,
#if FLOW_UI_AGENTIC_WORKFLOW
	/** An attached agentic workflow command. */
	Agentic,
#endif
};

/** Optional source attribution supplied to ActionManager when invoking a call. */
struct ActionInvocationSource {
	/** Callback or subsystem responsible for invocation. */
	ActionInvocationSourceKind kind = ActionInvocationSourceKind::Direct;
	/** Originating window, or zero for app-wide/direct attribution. */
	WindowId window = InvalidWindowId;
	/** Originating element when available. */
	FlowElementID element{};
	/** Additional source identity, such as a shortcut registration. */
	uint64_t sourceId = 0;
};

#if FLOW_UI_DEV_MODE
/** Safe developer-tooling snapshot; borrowed action resources are never exposed. */
struct ActionDebugInfo {
	/** Source of the most recent attempted invocation. */
	ActionInvocationSource lastSource{};
	/** Application identity when the record describes an app action. */
	AppActionID appId{};
	/** Borrowed definition location captured in developer builds. */
	ActionSourceLocation definitionSource{};
	/** Borrowed label for diagnostics; keep its backing storage alive while the value is retained. */
	std::string_view debugName{};
	/** Published UI recipe identity, or zero when unavailable. */
	uint64_t uiRecipeId = 0;
	/** Number of recorded invocation attempts. */
	uint64_t invocationCount = 0;
	/** Number of successful non-void results discarded by untyped invocation. */
	uint64_t discardedResultCount = 0;
	/** Identity representation; None marks an empty reference or diagnostic record. */
	ActionCallKind kind = ActionCallKind::None;
	/** Most recently recorded dispatch outcome. */
	ActionInvocationStatus lastStatus = ActionInvocationStatus::Empty;
	/** Current effective enabled state. */
	ActionAvailability availability{};
	/** Whether the diagnostic record has a live binding. */
	bool bound = false;
	/** Whether the last invoked operation threw an exception. */
	bool lastInvocationThrew = false;
};
#endif


/** Stateless UI action recipe, combined with borrowed resources by UiActions::make(). Create with UiAction(); do not edit the generated identity.
 * @tparam Fn Concrete, trivial, default-constructible callable returning void. */
template <typename Fn>
struct UiActionRecipe {
	/** Stateless callable stored by value. */
	[[no_unique_address]] Fn operation;
	/** Generated recipe identity; zero is invalid. */
	uint64_t recipeId = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed label for diagnostics; keep its backing storage alive while the value is retained. */
	std::string_view debugName{};
	/** Borrowed definition location captured in developer builds. */
	ActionSourceLocation definitionSource{};
#endif
};

/** Create a UI action recipe whose identity follows the concrete callable type. The callable must be stateless, trivial, default-constructible, non-generic, and return void.
 * @par Arguments
 * - @p fn Operation accepting the resource arguments later supplied to UiActions::make().
 * @note In development builds, the additional source argument records the recipe definition location.
 * @return Recipe containing fn and its generated type identity. */
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
		.recipeId = detail::ui_action::callableTypeId<Operation>(),
#if FLOW_UI_DEV_MODE
		.debugName = {},
		.definitionSource = source,
#endif
	};
}

/** Create a named UI action recipe with an identity hashed from a nonempty literal. Reusing a name denotes the same authored recipe identity.
 * @par Arguments
 * - @p name Stable authored recipe name; the literal is borrowed in developer builds.
 * - @p fn Stateless concrete operation returning void.
 * @note In development builds, the additional source argument records the recipe definition location.
 * @return Named recipe to bind through UiActions::make(). */
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
	recipe.recipeId = detail::ui_action::recipeHash(std::string_view{name, N - 1});
#if FLOW_UI_DEV_MODE
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
