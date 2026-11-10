#pragma once

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
class IStorageSystem;
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
	 * @brief Bind a previously unbound semantic action identity.
	 * @param id Nonzero application action identity.
	 * @param callable Function/member pointer, stateless callable, or explicitly borrowed callable.
	 * @param resources Borrowed arguments supplied as pointers or std::ref/std::cref.
	 * @return A retained call, or an initialization, identity, duplicate-binding,
	 * creation, or publication error. Allocation failure may throw std::bad_alloc.
	 * @note The callable and referenced resources must remain valid until the
	 * binding is retired and all in-progress invocations finish.
	 */
	template <typename Callable, typename... Resources>
	[[nodiscard]] Result<AppActionCall> bind(AppActionID id, Callable&& callable,
											 Resources&&... resources);

	/**
	 * @brief Bind an action with explicit diagnostic metadata.
	 * @param descriptor Nonzero action identity and diagnostic name; the name is copied.
	 * @param callable Callable satisfying the same restrictions as bind(AppActionID, Callable&&, Resources&&...).
	 * @param resources Borrowed arguments; their targets must outlive use of the binding.
	 * @return A retained call, or the same structured failures as the ID overload.
	 */
	template <typename Callable, typename... Resources>
	[[nodiscard]] Result<AppActionCall> bind(AppActionDescriptor descriptor, Callable&& callable,
											 Resources&&... resources);

	/** @brief Create a call referring to id without checking or creating a binding. */
	[[nodiscard]] AppActionCall select(AppActionID id) const noexcept;
	/** @brief Return whether id currently has a live binding; zero and unavailable managers return false. */
	[[nodiscard]] bool isBound(AppActionID id) const noexcept;
	/** @brief Return whether the identity stored in call currently has a live binding. */
	[[nodiscard]] bool isBound(AppActionCall call) const noexcept;
	/**
	 * @brief Remove a binding while allowing already active invocations to finish.
	 * @return True when a binding was retired; false for zero, missing, or unavailable IDs.
	 * Existing calls subsequently report Unbound until the identity is bound again.
	 */
	bool unbind(AppActionID id);

	/**
	 * @brief Replace an existing binding without changing its stable identity.
	 * @param descriptor Identity to replace and copied diagnostic name.
	 * @param callable Replacement callable; the bind() restrictions apply.
	 * @param resources Replacement borrowed arguments, retained as non-owning references.
	 * @return A call to the replacement, or a structured error, including
	 * ActionNotBound when the identity has no binding. Active calls finish using
	 * their original binding; later invocations use the replacement.
	 */
	template <typename Callable, typename... Resources>
	[[nodiscard]] Result<AppActionCall> rebind(AppActionDescriptor descriptor, Callable&& callable,
											   Resources&&... resources);

	/**
	 * @brief Invoke a retained action synchronously, discarding any result.
	 * @return Invoked, Empty, Unbound, or Disabled without invoking an ineligible call.
	 * @note Exceptions thrown by the user callable propagate to the caller.
	 */
	[[nodiscard]] ActionInvocationStatus invoke(AppActionCall call);

	/**
	 * @brief Invoke an action and move its exact result type into an ActionResult.
	 * @tparam Result Exact declared result type, including void; references and arrays are rejected.
	 * @param call Retained identity to resolve at invocation time.
	 * @return The result, or Empty, Unbound, Disabled, or ResultTypeMismatch.
	 * @note No callable runs on a type mismatch. Exceptions from the callable or
	 * result movement propagate; borrowed resources remain caller-owned.
	 */
	template <typename Result>
	[[nodiscard]] ActionResult<Result> invokeFor(AppActionCall call);

	/** @brief Observe enabled state; empty, unavailable, and unbound calls are disabled. */
	[[nodiscard]] AppActionAvailability availability(AppActionCall call) const noexcept;
	/** @brief Set a live binding's enabled state; return false when the identity has no live binding. */
	bool setAvailability(AppActionID id, AppActionAvailability availability);

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
	 * @brief Pack a stateless recipe and its arguments into a UI call.
	 * @param recipe Typed recipe declared with UiAction().
	 * @param resources Arguments matching the recipe signature. Reference
	 * parameters require matching lvalues that outlive all copies of the call;
	 * value parameters are copied into the fixed inline payload.
	 * @return A callable UI action. Resource count, types, and payload capacity
	 * are checked at compile time. Invocation discards any recipe result.
	 */
	template <typename Operation, typename... Resources>
	[[nodiscard]] UiActionCall make(
		UiActionRecipe<Operation> recipe,
		Resources&&... resources) const;

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
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevActions(void* userData, DevActionVisitor visitor) const;
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
	/** @brief Construct an uninitialized dispatcher; App initializes its binding storage. */
	ActionManager() noexcept
		: appActions_(*this), uiActions_(*this) {}
	/** @brief Retire owned action bindings and release their stored callable wrappers. */
	~ActionManager();

	ActionManager(const ActionManager&) = delete;
	ActionManager& operator=(const ActionManager&) = delete;
	ActionManager(ActionManager&&) = delete;
	ActionManager& operator=(ActionManager&&) = delete;

	/** @brief Borrow the app-action registration and invocation facade. */
	[[nodiscard]] AppActions& appActions() noexcept { return appActions_; }
	/** @brief Borrow the immutable app-action observation facade. */
	[[nodiscard]] const AppActions& appActions() const noexcept { return appActions_; }
	/** @brief Borrow the typed UI recipe construction facade. */
	[[nodiscard]] UiActions& uiActions() noexcept { return uiActions_; }
	/** @brief Borrow the immutable typed UI recipe construction facade. */
	[[nodiscard]] const UiActions& uiActions() const noexcept { return uiActions_; }

	/**
	 * @brief Dispatch an app or UI call synchronously and discard its result.
	 * @param call Action value; an empty value returns Empty.
	 * @param source Optional window/element attribution for development diagnostics.
	 * @return Invoked, Empty, Unbound, or Disabled. UI calls with a valid thunk
	 * run directly; retained app calls obey their binding's enabled state.
	 * @note Exceptions from user code propagate.
	 */
	[[nodiscard]] ActionInvocationStatus invoke(ActionCall call,
												ActionInvocationSource source = {});

	/** @brief Observe whether call is eligible for invocation without executing user code. */
	[[nodiscard]] ActionAvailability availability(ActionCall call) const noexcept;
#if FLOW_UI_DEV_MODE
	/** Return a resource-safe snapshot for developer tooling. */
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

	void init(App& app, detail::storage::IStorageSystem& storage);
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
	Result<AppActionCall> bindTyped(
		AppActionDescriptor descriptor,
		bool replacement,
		Callable&& callable,
		Resources&&... resources);

	App* app_ = nullptr;
	detail::storage::IStorageSystem* storage_ = nullptr;
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
	Resources&&... resources) const {
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

	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ActionBind));
	auto validation = validateBindingRequest(descriptor.id, replacement);
	if (!validation) return unexpectedError(validation.error());
	const std::string_view effectiveName = descriptor.debugName.empty()
		? std::string_view{"flowui.app_action.binding"}
		: descriptor.debugName;
	const detail::storage::StringId debugName = storage_->intern(effectiveName);
	detail::storage::PersistentRecordHandle handle{};
	try {
		handle = detail::storage::createTypedPersistentRecord<
		detail::action::AppActionBindingHeader,
		Payload>(
		*storage_,
		detail::storage::PersistentRecordCreateInfo{
			.kind = detail::storage::ResourceKind::AppActionBinding,
			.window = InvalidWindowId,
			.debugName = debugName,
		},
		[descriptor, debugName](
			detail::action::AppActionBindingHeader& header,
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
		(void)storage_->removePersistentRecord(
			handle, detail::storage::ResourceKind::AppActionBinding);
		detail::terminateForFatalError(makeError(
			ErrorCode::ActionPublicationConflict, ErrorSite::ActionPublish,
			descriptor.id.value));
	} catch (...) {
		(void)storage_->removePersistentRecord(
			handle, detail::storage::ResourceKind::AppActionBinding);
		return unexpectedError(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::ActionPublish));
	}
	return AppActionCall{descriptor.id};
}

template <typename Result>
ActionResult<Result> AppActions::invokeFor(AppActionCall call) {
	static_assert(!std::is_reference_v<Result> && !std::is_array_v<Result>,
		"FlowUi invokeFor results cannot be references or arrays.");
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
			result->~Result();
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
