#pragma once

#include "FlowUi/BuildConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "FlowUi/Error.hpp"
#include "internal/ActionBinding.hpp"
#include "internal/StorageSystem/PersistentRecord.hpp"
#include "internal/UiActionBinding.hpp"
#include "managers/structs/ActionManagerStructs.hpp"

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

class AppActions {
public:
	template <typename Callable, typename... Resources>
	Result<AppActionCall> bind(
		AppActionID id,
		Callable&& callable,
		Resources&&... resources);

	template <typename Callable, typename... Resources>
	Result<AppActionCall> bind(
		AppActionDescriptor descriptor,
		Callable&& callable,
		Resources&&... resources);

	[[nodiscard]] AppActionCall select(AppActionID id) const noexcept;
	[[nodiscard]] bool isBound(AppActionID id) const noexcept;
	[[nodiscard]] bool isBound(AppActionCall call) const noexcept;
	bool unbind(AppActionID id);

	template <typename Callable, typename... Resources>
	Result<AppActionCall> rebind(
		AppActionDescriptor descriptor,
		Callable&& callable,
		Resources&&... resources);

	ActionInvocationStatus invoke(AppActionCall call);

	template <typename Result>
	ActionResult<Result> invokeFor(AppActionCall call);

	[[nodiscard]] AppActionAvailability availability(AppActionCall call) const noexcept;
	bool setAvailability(AppActionID id, AppActionAvailability availability);

private:
	friend class ActionManager;
	explicit AppActions(ActionManager& owner) noexcept : owner_(&owner) {}
	ActionManager* owner_ = nullptr;
};

class UiActions {
public:
	template <typename Operation, typename... Resources>
	[[nodiscard]] UiActionCall make(
		UiActionRecipe<Operation> recipe,
		Resources&&... resources) const;

private:
	friend class ActionManager;
	explicit UiActions(ActionManager& owner) noexcept : owner_(&owner) {}
	ActionManager* owner_ = nullptr;
};

class ActionManager {
public:
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	ActionManager() noexcept
		: appActions_(*this), uiActions_(*this) {}
	~ActionManager();

	ActionManager(const ActionManager&) = delete;
	ActionManager& operator=(const ActionManager&) = delete;
	ActionManager(ActionManager&&) = delete;
	ActionManager& operator=(ActionManager&&) = delete;

	[[nodiscard]] AppActions& appActions() noexcept { return appActions_; }
	[[nodiscard]] const AppActions& appActions() const noexcept { return appActions_; }
	[[nodiscard]] UiActions& uiActions() noexcept { return uiActions_; }
	[[nodiscard]] const UiActions& uiActions() const noexcept { return uiActions_; }

	ActionInvocationStatus invoke(
		ActionCall call,
		ActionInvocationSource source = {});

	[[nodiscard]] ActionAvailability availability(ActionCall call) const noexcept;
#if FLOW_UI_DEV_MODE
	/** Return a resource-safe snapshot for developer tooling. */
	[[nodiscard]] std::optional<ActionDebugInfo> debugInfo(ActionCall call) const noexcept;
#endif

private:
	friend class App;
	friend class UiManager;
	friend class AppActions;
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
		call.recipeName_ = recipe.debugName;
		call.definitionSource_ = recipe.definitionSource;
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

	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized));
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
		return unexpectedError(makeError(ErrorCode::ResourceCreationFailed));
	}

	try {
		publishBinding(descriptor.id, handle, replacement);
	} catch (const FlowUiException&) {
		(void)storage_->removePersistentRecord(
			handle, detail::storage::ResourceKind::AppActionBinding);
		detail::terminateForFatalError(makeError(
			ErrorCode::ActionPublicationConflict,
			ErrorSubjectKind::Action,
			descriptor.id.value));
	} catch (...) {
		(void)storage_->removePersistentRecord(
			handle, detail::storage::ResourceKind::AppActionBinding);
		return unexpectedError(makeError(ErrorCode::ResourcePublicationFailed));
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
