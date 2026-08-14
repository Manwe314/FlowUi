#pragma once

#include <functional>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "FlowUi/AppActionID.hpp"
#include "internal/TypeOperations.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"
#include "managers/structs/ActionManagerStructs.hpp"

namespace FlowUi::detail::action {

template <typename T>
struct IsReferenceWrapper : std::false_type {};

template <typename T>
struct IsReferenceWrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T>
inline constexpr bool IsReferenceWrapperV = IsReferenceWrapper<std::remove_cvref_t<T>>::value;

template <typename Resource>
concept BorrowedResource =
	std::is_pointer_v<std::remove_cvref_t<Resource>> || IsReferenceWrapperV<Resource>;

template <typename Callable>
concept StorableCallable =
	std::is_pointer_v<std::remove_cvref_t<Callable>> ||
	std::is_member_pointer_v<std::remove_cvref_t<Callable>> ||
	IsReferenceWrapperV<Callable> ||
	(std::is_empty_v<std::remove_cvref_t<Callable>> &&
	 std::is_trivially_copyable_v<std::remove_cvref_t<Callable>>);

template <typename Callable, typename... Resources>
decltype(auto) invokeCallable(Callable& callable, Resources&... resources) {
	using C = std::remove_cvref_t<Callable>;
	if constexpr (
		std::is_pointer_v<C> &&
		!std::is_function_v<std::remove_pointer_t<C>> &&
		std::is_invocable_v<std::remove_pointer_t<C>&, Resources&...>) {
		return std::invoke(*callable, resources...);
	} else {
		return std::invoke(callable, resources...);
	}
}

template <typename Callable, typename... Resources>
using InvokeResult = decltype(invokeCallable(
	std::declval<Callable&>(), std::declval<Resources&>()...));

template <typename Callable, typename... Resources>
struct AppActionBindingPayload {
	Callable callable;
	std::tuple<Resources...> resources;

	AppActionBindingPayload(Callable callableValue, Resources... resourceValues)
		: callable(std::move(callableValue)),
		  resources(std::move(resourceValues)...) {}

	decltype(auto) invoke() {
		return std::apply(
			[this](auto&... unpacked) -> decltype(auto) {
				return invokeCallable(callable, unpacked...);
			},
			resources);
	}
};

struct AppActionBindingHeader {
	AppActionID id{};
	uint64_t callableTypeHash = 0;
	uint64_t resultTypeHash = 0;
	storage::StringId debugName = 0;
	void (*invokeDiscard)(void* payload) = nullptr;
	void (*invokeResult)(void* payload, void* destination) = nullptr;
	uint32_t activeInvocations = 0;
	bool tombstoned = false;
	AppActionAvailability availability{};
#if FLOW_UI_DEV_MODE
	uint64_t invocationCount = 0;
	ActionInvocationSource lastSource{};
	ActionInvocationStatus lastStatus = ActionInvocationStatus::Empty;
	bool lastInvocationThrew = false;
	uint64_t discardedResultCount = 0;
#endif
};

template <typename Payload>
using PayloadResult = decltype(std::declval<Payload&>().invoke());

template <typename Payload>
void invokeDiscard(void* payload) {
	using Result = PayloadResult<Payload>;
	if constexpr (std::is_void_v<Result>) {
		static_cast<Payload*>(payload)->invoke();
	} else {
		(void)static_cast<Payload*>(payload)->invoke();
	}
}

template <typename Payload>
void invokeResult(void* payload, void* destination) {
	using Result = PayloadResult<Payload>;
	static_assert(!std::is_void_v<Result>);
	::new (destination) Result(static_cast<Payload*>(payload)->invoke());
}

template <typename Payload>
constexpr uint64_t resultTypeHash() noexcept {
	using Result = PayloadResult<Payload>;
	if constexpr (std::is_void_v<Result>) return 0;
	else return detail::typeHash<Result>();
}

} // namespace FlowUi::detail::action
