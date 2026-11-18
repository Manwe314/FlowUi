#pragma once
#include "FlowUi/Private/ApiBoundary.hpp"
#include "FlowUi/Private/storageSystem/StorageRecordAccess.hpp"

#include "FlowUi/Public/BuildConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "FlowUi/Public/errorManagement/Error.hpp"
#include "FlowUi/Private/actionService/ActionBinding.hpp"
#include "FlowUi/Private/storageSystem/PersistentRecord.hpp"
#include "FlowUi/Private/actionService/UiActionBinding.hpp"
#include "FlowUi/Public/managerStructs/ActionTypes.hpp"

namespace FlowUi {

/** @addtogroup flowui_action_manager
 * @{
 */

class App;
class UiManager;
class ActionManager;

namespace detail::storage {
class StorageBinding;
}
namespace devSystems { class MemorySampleSink; }

namespace detail::manager_storage {
struct ActionManagerState;
}

namespace detail::action {
struct ActionManagerAccess;
}

/**
 * @brief Retained application actions addressed by stable AppActionID values.
 *
 * Obtain this facade from ActionManager::appActions(). Calls resolve the current
 * binding at invocation time, so existing AppActionCall values follow rebinds.
 * Referenced objects remain caller-owned and must outlive every invocation.
 */
class AppActions {
public:
	/**
	 * @brief Create a retained application binding and return its AppActionCall. The ID must be nonzero and unbound; the descriptor overload also copies its diagnostic name.
	 *
	 * Hybrid contract: structured binding validation/publication failures are returned; arbitrary callable construction exceptions propagate unchanged. Resources supplied as pointers/std::ref/std::cref are borrowed and must outlive binding retirement and all in-progress calls. Callable objects must meet the bind restrictions: function/member pointer, stateless concrete callable, or explicitly borrowed callable.
	 *
	 * @par Example
	 * @code{.cpp}
	 * int saveCount = 0;
	 * auto result = actions.bind(FlowUi::AppAction("docs.save"), [](int& saveCount) { ++saveCount; }, std::ref(saveCount));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::AppActionCall save = *result;
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	template <typename Callable, typename... Resources>
	[[nodiscard]] Result<AppActionCall> bind(AppActionID id, Callable&& callable,
											 Resources&&... resources);

	/**
	 * @brief Create a retained application binding and return its AppActionCall. The ID must be nonzero and unbound; the descriptor overload also copies its diagnostic name.
	 *
	 * Hybrid contract: structured binding validation/publication failures are returned; arbitrary callable construction exceptions propagate unchanged. Resources supplied as pointers/std::ref/std::cref are borrowed and must outlive binding retirement and all in-progress calls. Callable objects must meet the bind restrictions: function/member pointer, stateless concrete callable, or explicitly borrowed callable.
	 *
	 * @par Example
	 * @code{.cpp}
	 * int saveCount = 0;
	 * auto result = actions.bind(FlowUi::AppActionDescriptor{.id = FlowUi::AppAction("docs.save"), .debugName = "Save"}, [](int& saveCount) { ++saveCount; }, std::ref(saveCount));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::AppActionCall save = *result;
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	template <typename Callable, typename... Resources>
	[[nodiscard]] Result<AppActionCall> bind(AppActionDescriptor descriptor, Callable&& callable,
											 Resources&&... resources);

	/**
	 * @brief Create a direct AppActionCall containing this identity without checking or creating its binding.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] AppActionCall select(AppActionID id) const noexcept;
	/**
	 * @brief Return whether the ID or call currently resolves to a live binding; an empty or unavailable binding is false.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] bool isBound(AppActionID id) const noexcept;
	/**
	 * @brief Return whether the ID or call currently resolves to a live binding; an empty or unavailable binding is false.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] bool isBound(AppActionCall call) const noexcept;
	/**
	 * @brief Retire a binding and return whether one was removed. Successful false is ordinary absence; already active invocations may finish.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = actions.unbind(id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool removed = *result;
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] Result<bool> unbind(AppActionID id) noexcept;

	/**
	 * @brief Replace an existing retained binding at the same identity. Active invocations finish on their original binding; future calls resolve the replacement. A missing binding is returned as an error.
	 *
	 * Hybrid contract: structured binding validation/publication failures are returned; arbitrary callable construction exceptions propagate unchanged. Resources supplied as pointers/std::ref/std::cref are borrowed and must outlive binding retirement and all in-progress calls. Callable objects must meet the bind restrictions: function/member pointer, stateless concrete callable, or explicitly borrowed callable.
	 *
	 * @par Example
	 * @code{.cpp}
	 * int saveCount = 0;
	 * auto result = actions.rebind(FlowUi::AppActionDescriptor{.id = FlowUi::AppAction("docs.save"), .debugName = "Save"}, [](int& saveCount) { ++saveCount; }, std::ref(saveCount));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::AppActionCall save = *result;
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	template <typename Callable, typename... Resources>
	[[nodiscard]] Result<AppActionCall> rebind(AppActionDescriptor descriptor, Callable&& callable,
											   Resources&&... resources);

	/**
	 * @brief Invoke synchronously and directly return Invoked, Empty, Unbound or Disabled. An unavailable call does not run its callable.
	 *
	 * Callback and owned-result movement exceptions propagate unchanged. Side effects are not rolled back or automatically retried. invokeFor requires nonthrowing destruction of owned payloads; reference and array result types are rejected.
	 *
	 * @see docs/api/action_manager.md

	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
	 * const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
	 * @endcode
	 */
	[[nodiscard]] ActionInvocationStatus invoke(AppActionCall call);

	/**
	 * @brief Invoke only when the bound result type exactly matches Result. Return ActionResult<Result>, whose errors are Empty, Unbound, Disabled and ResultTypeMismatch. This is distinct from FlowUi::Result<T>.
	 *
	 * Callback and owned-result movement exceptions propagate unchanged. Side effects are not rolled back or automatically retried. invokeFor requires nonthrowing destruction of owned payloads; reference and array result types are rejected.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = actions.invokeFor<int>(countCall);
	 * if (!result) {
	 *     const FlowUi::ActionInvokeError error = result.error();
	 *     return; // Handle the dispatch failure in the surrounding application code.
	 * }
	 * const int displayedCount = result.value();
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	template <typename Result>
	[[nodiscard]] ActionResult<Result> invokeFor(AppActionCall call);

	/**
	 * @brief Observe effective enabled state directly; empty, unavailable and unbound calls are disabled.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] AppActionAvailability availability(AppActionCall call) const noexcept;
	/**
	 * @brief Update the enabled flag of a live binding. Successful false means that no live binding matched the identity.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = actions.setAvailability(id, {.enabled = false});
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool updated = *result;
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] Result<bool> setAvailability(AppActionID id, AppActionAvailability availability) noexcept;

private:
	friend class ActionManager;
	explicit AppActions(ActionManager& owner) noexcept : owner_(&owner) {}
	ActionManager* owner_ = nullptr;
};

/**
 * @brief Construct compact UI calls from typed recipes and borrowed resources.
 *
 * Obtain this facade from ActionManager::uiActions(). It does not create a
 * retained app-action binding; copied calls continue to borrow their arguments.
 */
class UiActions {
public:
	/**
	 * @brief Pack a stateless, trivial, concrete void-returning UiAction recipe and its arguments into a direct UiActionCall with no retained app binding.
	 *
	 * Arguments must match the recipe and fit UiActionCall::InlineBytes = 2 * sizeof(void*), which is 16 bytes with 64-bit pointers. Referenced lvalues must outlive every call copy; value arguments are stored inline. Argument conversion must be nothrow; perform fallible conversion before make(). Size and argument rules are compile-time checks.
	 *
	 * @par Example
	 * @code{.cpp}
	 * bool panelOpen = false;
	 * constexpr auto toggle = FlowUi::UiAction("docs.toggle", [](bool& open) { open = !open; });
	 * const FlowUi::UiActionCall call = actions.make(toggle, panelOpen);
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	template <typename Operation, typename... Resources>
	[[nodiscard]] UiActionCall make(
		UiActionRecipe<Operation> recipe,
		Resources&&... resources) const noexcept;

private:
	friend class ActionManager;
	explicit UiActions(ActionManager& owner) noexcept : owner_(&owner) {}
	ActionManager* owner_ = nullptr;
};

/**
 * @brief App-owned dispatcher for retained semantic actions and compact UI calls.
 *
 * Obtain the initialized manager through App::actions() or UiManager::actions().
 * Calls run synchronously on the caller's thread. The manager owns retained
 * binding records, while callables and resource references follow the borrowing
 * contracts of AppActions::bind() and UiActions::make().
 */
class ActionManager {
public:
#if FLOW_UI_DEV_MODE
	/** @brief Callable-free action metadata borrowed during a developer visitor. */
	struct DevActionView {
		/** @brief Identity, availability, counters, and diagnostic source snapshot. */
		ActionDebugInfo debug{};
		/** @brief Process-local callable type fingerprint; zero when unavailable. */
		std::uint64_t callableTypeHash = 0;
		/** @brief Process-local result type fingerprint; zero denotes void. */
		std::uint64_t resultTypeHash = 0;
		/** @brief Whether tooling may reconstruct this call without borrowed resources. */
		bool reconstructable = false;
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevActionVisitor = bool (*)(void*, const DevActionView&);
	/** @brief Return the current development-observation revision; zero when support is disabled. */
	[[nodiscard]] std::uint64_t devRevision() const noexcept;
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devActionCount() const noexcept;
	/**
	 * @brief Available only with FLOW_UI_DEV_MODE. Synchronously visit borrowed catalogue entries. Successful true means the visit completed; successful false means a visitor stopped early or storage/visitor was unavailable. Thrown visitor/storage failures are converted to structured errors. Do not retain views or references to their metadata beyond the call.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t boundCount = 0;
	 * auto result = actions.visitDevActions(&boundCount,
	 *     [](void* userData, const FlowUi::ActionManager::DevActionView& view) {
	 *         auto& count = *static_cast<std::size_t*>(userData);
	 *         if (view.debug.bound) ++count;
	 *         return true;
	 *     });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool completed = *result;
	 * #endif
	 * @endcode
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] Result<bool> visitDevActions(void* userData, DevActionVisitor visitor) const noexcept;
	/** Safely reconstruct a catalogue action. UI recipes with bound resources are rejected. */
	[[nodiscard]] std::optional<ActionCall> makeDevActionCall(
		ActionCallKind kind,
		std::uint64_t stableId,
		std::uint64_t expectedCallableTypeHash = 0,
		std::uint64_t expectedResultTypeHash = 0) const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Construct a detached manager; application code normally obtains the initialized App-owned manager.
	 *
	 * @see docs/api/action_manager.md
	 */
	ActionManager() noexcept
		: appActions_(*this), uiActions_(*this) {}
	/**
	 * @brief Release owned service data. The App controls this manager lifetime.
	 *
	 * @see docs/api/action_manager.md
	 */
	~ActionManager();

	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/action_manager.md
	 */
	ActionManager(const ActionManager&) = delete;
	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/action_manager.md
	 */
	ActionManager& operator=(const ActionManager&) = delete;
	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/action_manager.md
	 */
	ActionManager(ActionManager&&) = delete;
	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/action_manager.md
	 */
	ActionManager& operator=(ActionManager&&) = delete;

	/**
	 * @brief Borrow the mutable or const facade for retained application actions. The manager owns this facade.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] AppActions& appActions() noexcept { return appActions_; }
	/**
	 * @brief Borrow the mutable or const facade for retained application actions. The manager owns this facade.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] const AppActions& appActions() const noexcept { return appActions_; }
	/**
	 * @brief Borrow the mutable or const facade for inline UI action creation. The manager owns this facade.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] UiActions& uiActions() noexcept { return uiActions_; }
	/**
	 * @brief Borrow the mutable or const facade for inline UI action creation. The manager owns this facade.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] const UiActions& uiActions() const noexcept { return uiActions_; }

	/**
	 * @brief Invoke synchronously and directly return Invoked, Empty, Unbound or Disabled. An unavailable call does not run its callable.
	 *
	 * Callback and owned-result movement exceptions propagate unchanged. Side effects are not rolled back or automatically retried. invokeFor requires nonthrowing destruction of owned payloads; reference and array result types are rejected.
	 *
	 * @see docs/api/action_manager.md

	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
	 * const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
	 * @endcode
	 */
	[[nodiscard]] ActionInvocationStatus invoke(ActionCall call,
												ActionInvocationSource source = {});

	/**
	 * @brief Observe effective enabled state directly; empty, unavailable and unbound calls are disabled.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] ActionAvailability availability(ActionCall call) const noexcept;
#if FLOW_UI_DEV_MODE
	/**
	 * @brief Available only with FLOW_UI_DEV_MODE. Return a direct diagnostic snapshot of the call without invoking it.
	 *
	 * @see docs/api/action_manager.md
	 */
	[[nodiscard]] std::optional<ActionDebugInfo> debugInfo(ActionCall call) const noexcept;
#endif

private:
	friend class App;
	friend class UiManager;
	friend class AppActions;
	friend class UiActions;
	friend struct detail::action::ActionManagerAccess;

	class InvocationLease {
	public:
		InvocationLease() = default;
		InvocationLease(
			ActionManager& owner,
			detail::storage::PersistentRecordHandle handle,
			detail::action::AppActionBindingHeader& header,
			void* payload) noexcept
			: owner_(&owner), handle_(handle), header_(&header), payload_(payload) {}
		InvocationLease(const InvocationLease&) = delete;
		InvocationLease& operator=(const InvocationLease&) = delete;
		InvocationLease(InvocationLease&& other) noexcept;
		InvocationLease& operator=(InvocationLease&& other) noexcept;
		~InvocationLease();

		[[nodiscard]] explicit operator bool() const noexcept { return header_ != nullptr; }
		[[nodiscard]] detail::action::AppActionBindingHeader& header() const noexcept { return *header_; }
		[[nodiscard]] void* payload() const noexcept { return payload_; }
		void noteSuccess(bool discardedResult) noexcept;
		void noteException() noexcept;

	private:
		void release() noexcept;
		ActionManager* owner_ = nullptr;
		detail::storage::PersistentRecordHandle handle_{};
		detail::action::AppActionBindingHeader* header_ = nullptr;
		void* payload_ = nullptr;
	};

	void init(App& app, detail::storage::StorageBinding& storage);
	void destroy() noexcept;
	void rebindOwner(App& app) noexcept;
	void attachTo(UiManager& ui) noexcept;

	[[nodiscard]] detail::manager_storage::ActionManagerState& state();
	[[nodiscard]] const detail::manager_storage::ActionManagerState& state() const;
	[[nodiscard]] Status validateBindingRequest(AppActionID id, bool replacement) const;
	void publishBinding(
		AppActionID id,
		detail::storage::PersistentRecordHandle handle,
		bool replacement);
	void retireBinding(detail::storage::PersistentRecordHandle handle) noexcept;
	void releaseInvocation(
		detail::storage::PersistentRecordHandle handle,
		detail::action::AppActionBindingHeader& header) noexcept;
	[[nodiscard]] InvocationLease beginInvocation(
		AppActionCall call,
		uint64_t requestedResultType,
		bool resultRequired,
		ActionInvocationSource source,
		ActionInvokeError& error);
	ActionInvocationStatus invokeApp(
		AppActionCall call,
		ActionInvocationSource source);
#if FLOW_UI_DEV_MODE
	void reportInvocationError(AppActionCall call, ActionInvokeError error) noexcept;
#endif
#if FLOW_UI_DEV_MODE || FLOW_UI_HAS_BAKED_CHANGES
	void noteUiRecipe(
		std::uint64_t recipeId,
		const UiActionCall* reconstructable
#if FLOW_UI_DEV_MODE
		, std::string_view debugName,
		ActionSourceLocation source
#endif
		) noexcept;
#endif
	template <typename Callable, typename... Resources>
	[[nodiscard]] Result<AppActionCall> bindTyped(
		AppActionDescriptor descriptor,
		bool replacement,
		Callable&& callable,
		Resources&&... resources);

	App* app_ = nullptr;
	detail::storage::StorageBinding* storage_ = nullptr;
	uint64_t stateHandle_ = 0;
	uint32_t stateName_ = 0;
	AppActions appActions_;
	UiActions uiActions_;
#if FLOW_UI_DEV_MODE || FLOW_UI_HAS_BAKED_CHANGES
	struct DevUiRecipeRecord {
		std::uint64_t id = 0;
		UiActionCall prototype{};
		bool reconstructable = false;
#if FLOW_UI_DEV_MODE
		std::string_view debugName{};
		ActionSourceLocation source{};
#endif
	};
	std::vector<DevUiRecipeRecord> devUiRecipes_{};
#if FLOW_UI_DEV_MODE
	std::uint64_t devRevision_ = 1;
#endif
#endif
};

template <typename Operation, typename... Resources>
UiActionCall UiActions::make(
	UiActionRecipe<Operation> recipe,
	Resources&&... resources) const noexcept {
	(void)owner_;
	using Traits = detail::ui_action::CallableSignature<decltype(&Operation::operator())>;
	using Parameters = typename Traits::Arguments;
	using Arguments = std::tuple<Resources&&...>;
	constexpr std::size_t parameterCount = std::tuple_size_v<Parameters>;
	static_assert(parameterCount == sizeof...(Resources),
		"FlowUi UiAction resource count does not match the recipe parameter count.");

	if constexpr (parameterCount == sizeof...(Resources)) {
		constexpr auto indexes = std::make_index_sequence<parameterCount>{};
		static_assert(
			detail::ui_action::argumentsCanBind<Parameters, Arguments>(indexes),
			"FlowUi UiAction resources do not satisfy the recipe parameter types; references require matching lvalues.");
		static_assert([]<std::size_t... Index>(std::index_sequence<Index...>) {
			return ((std::is_lvalue_reference_v<std::tuple_element_t<Index, Parameters>> ||
				std::is_nothrow_constructible_v<
					detail::ui_action::StoredArgumentT<std::tuple_element_t<Index, Parameters>>,
					std::tuple_element_t<Index, Arguments>>) && ...);
		}(indexes), "FlowUi UiAction argument conversions must not throw; convert fallible values before packing the action.");
		static_assert(
			detail::ui_action::StoredBytes<Parameters> <= UiActionCall::InlineBytes,
			"FlowUi UiAction arguments exceed inline storage; borrow a context object or use an element-specific callback.");

		UiActionCall call{};
		auto arguments = std::forward_as_tuple(std::forward<Resources>(resources)...);
		detail::ui_action::packArguments<Parameters>(
			call.payload_.data(), std::move(arguments), indexes);
		call.invokeThunk_ = &detail::ui_action::invokeRecipe<Operation>;
#if FLOW_UI_DEV_MODE
		call.recipeId_ = recipe.recipeId;
		call.reconstructable_ = parameterCount == 0u;
		call.recipeName_ = recipe.debugName;
		call.definitionSource_ = recipe.definitionSource;
#endif
#if FLOW_UI_DEV_MODE || FLOW_UI_HAS_BAKED_CHANGES
		owner_->noteUiRecipe(
			recipe.recipeId, parameterCount == 0u ? &call : nullptr
#if FLOW_UI_DEV_MODE
			, recipe.debugName, recipe.definitionSource
#endif
		);
#else
		(void)recipe;
#endif
		return call;
	} else {
		return {};
	}
}

template <typename Callable, typename... Resources>
Result<AppActionCall> AppActions::bind(
	AppActionID id,
	Callable&& callable,
	Resources&&... resources) {
	std::string_view debugName{};
#if FLOW_UI_DEV_MODE
	debugName = id.debugName;
#endif
	return bind(
		AppActionDescriptor{.id = id, .debugName = debugName},
		std::forward<Callable>(callable),
		std::forward<Resources>(resources)...);
}

template <typename Callable, typename... Resources>
Result<AppActionCall> AppActions::bind(
	AppActionDescriptor descriptor,
	Callable&& callable,
	Resources&&... resources) {
	return owner_->bindTyped(
		descriptor,
		false,
		std::forward<Callable>(callable),
		std::forward<Resources>(resources)...);
}

template <typename Callable, typename... Resources>
Result<AppActionCall> AppActions::rebind(
	AppActionDescriptor descriptor,
	Callable&& callable,
	Resources&&... resources) {
	return owner_->bindTyped(
		descriptor,
		true,
		std::forward<Callable>(callable),
		std::forward<Resources>(resources)...);
}

template <typename Callable, typename... Resources>
Result<AppActionCall> ActionManager::bindTyped(
	AppActionDescriptor descriptor,
	bool replacement,
	Callable&& callable,
	Resources&&... resources) {
	using StoredCallable = std::decay_t<Callable>;
	static_assert(detail::action::StorableCallable<StoredCallable>,
		"FlowUi app actions require a function/member pointer, stateless callable, or explicitly borrowed callable.");
	static_assert((detail::action::BorrowedResource<std::decay_t<Resources>> && ...),
		"FlowUi app action resources are borrowed; pass pointers or std::ref/std::cref.");
	using Payload = detail::action::AppActionBindingPayload<
		StoredCallable,
		std::decay_t<Resources>...>;
	using PayloadResult = detail::action::PayloadResult<Payload>;
	static_assert(!std::is_reference_v<PayloadResult> && !std::is_array_v<PayloadResult>,
		"FlowUi app action results cannot be references or arrays; return a pointer or reference_wrapper.");
	static_assert(std::is_void_v<PayloadResult> || std::is_move_constructible_v<PayloadResult>,
		"FlowUi app action results must be move constructible.");
	static_assert(std::is_void_v<PayloadResult> || std::is_nothrow_destructible_v<PayloadResult>,
		"FlowUi owned app action results must have non-throwing destructors.");

	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ActionBind));
	auto validation = validateBindingRequest(descriptor.id, replacement);
	if (!validation) return unexpectedError(validation.error());
	const std::string_view effectiveName = descriptor.debugName.empty()
		? std::string_view{"flowui.app_action.binding"}
		: descriptor.debugName;
	detail::storage::StorageRecordAccess recordStorage{*storage_};
	const detail::storage::StringId debugName = recordStorage.intern(effectiveName);
	detail::storage::PersistentRecordHandle handle{};
	try {
		handle =
			detail::storage::createTypedPersistentRecord<detail::action::AppActionBindingHeader,
														 Payload>(
				recordStorage,
				detail::storage::PersistentRecordCreateInfo{
					.kind = detail::storage::ResourceKind::AppActionBinding,
					.window = InvalidWindowId,
					.debugName = debugName,
				},
				[descriptor, debugName](detail::action::AppActionBindingHeader& header,
										const detail::storage::AlignedRecordLayout&) {
					header.id = descriptor.id;
					header.callableTypeHash = detail::typeHash<StoredCallable>();
					header.resultTypeHash = detail::action::resultTypeHash<Payload>();
					header.debugName = debugName;
					header.invokeDiscard = &detail::action::invokeDiscard<Payload>;
					if constexpr (!std::is_void_v<PayloadResult>) {
						header.invokeResult = &detail::action::invokeResult<Payload>;
					}
				},
				StoredCallable(std::forward<Callable>(callable)),
				std::decay_t<Resources>(std::forward<Resources>(resources))...);
	} catch (const std::bad_alloc&) {
		throw;
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		if (exception.error().descriptor().category != ErrorCategory::Local) throw;
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::ResourceCreationFailed, ErrorSite::ActionBind));
	}

	try {
		publishBinding(descriptor.id, handle, replacement);
	} catch (const FlowUiException&) {
		(void)recordStorage.removePersistentRecord(handle,
												   detail::storage::ResourceKind::AppActionBinding);
		detail::terminateForFatalError(makeError(
			ErrorCode::ActionPublicationConflict, ErrorSite::ActionPublish,
			descriptor.id.value));
	} catch (...) {
		(void)recordStorage.removePersistentRecord(handle,
												   detail::storage::ResourceKind::AppActionBinding);
		return unexpectedError(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::ActionPublish));
	}
	return AppActionCall{descriptor.id};
}

template <typename Result>
ActionResult<Result> AppActions::invokeFor(AppActionCall call) {
	static_assert(!std::is_reference_v<Result> && !std::is_array_v<Result>,
		"FlowUi invokeFor results cannot be references or arrays.");
	static_assert(std::is_void_v<Result> || std::is_nothrow_destructible_v<Result>,
		"FlowUi invokeFor result cleanup must not throw.");
	ActionInvokeError error = ActionInvokeError::Empty;
	const uint64_t resultType = std::is_void_v<Result> ? 0 : detail::typeHash<Result>();
	auto lease = owner_->beginInvocation(
		call, resultType, true, ActionInvocationSource{}, error);
	if (!lease) return ActionResult<Result>{error};

	if constexpr (std::is_void_v<Result>) {
		try {
			lease.header().invokeDiscard(lease.payload());
			lease.noteSuccess(false);
			return ActionResult<void>::success();
		} catch (...) {
			lease.noteException();
			throw;
		}
	} else {
		alignas(Result) std::byte resultStorage[sizeof(Result)];
		Result* result = nullptr;
		try {
			lease.header().invokeResult(lease.payload(), resultStorage);
			result = reinterpret_cast<Result*>(resultStorage);
			ActionResult<Result> output{std::move(*result)};
			std::exchange(result, nullptr)->~Result();
			lease.noteSuccess(false);
			return output;
		} catch (...) {
			if (result) result->~Result();
			lease.noteException();
			throw;
		}
	}
}

/** @} */

} // namespace FlowUi
