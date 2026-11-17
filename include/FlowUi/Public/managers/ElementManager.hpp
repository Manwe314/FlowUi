#pragma once
#include "FlowUi/Private/ApiBoundary.hpp"

#include "FlowUi/Private/elementSystem/ElementRegistrationSupport.hpp"
#include "FlowUi/Public/BuildConfig.hpp"

#include <cstdint>

#include "FlowUi/Private/elementSystem/ElementInstanceKey.hpp"
#include "FlowUi/Public/elementAuthoring/ElementSet.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#endif

namespace FlowUi {

class App;
class UiManager;
#if FLOW_UI_DEV_MODE
namespace devSystems { class DevTimingRecorder; class MemorySampleSink; }
#endif

namespace detail::element {
template <typename Element>
class ElementInvocation;
}

namespace detail::storage {
class IStorageSystem;
}

namespace detail::manager_storage {
class ElementStorageController;
struct ElementDefinitionRecord;
}

/**
 * @brief App-wide owner and public access point for Flow element runtime data.
 *
 * Obtain the initialized manager through App::elements(). State belongs to an
 * element instance in a particular window; resource payloads are shared by all
 * instances of one definition within the app. Use prepare() to create resources
 * ahead of the first invocation. Definitions without state/resources remain valid.
 * State pointers borrow manager storage and must obey each accessor's lifetime contract.
 */
class ElementManager {
public:
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only development memory accounting to the borrowed sink. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Construct a detached manager; application code normally obtains the initialized App-owned manager.
	 *
	 * @see docs/api/elements.md
	 */
	ElementManager() = default;
	/**
	 * @brief Release owned service data. The App controls this manager lifetime.
	 *
	 * @see docs/api/elements.md
	 */
	~ElementManager();

	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/elements.md
	 */
	ElementManager(const ElementManager&) = delete;
	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/elements.md
	 */
	ElementManager& operator=(const ElementManager&) = delete;
	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/elements.md
	 */
	ElementManager(ElementManager&&) = delete;
	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/elements.md
	 */
	ElementManager& operator=(ElementManager&&) = delete;

	/**
	 * @brief Look up immutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.
	 *
	 * The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.getStatePointerConst(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto* state = *result;
	 * if (state) {
	 *     const int count = state->count;
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<const StateOf<Element>*> getStatePointerConst(
		const Element&,
		WindowId window,
		FlowElementID elementId) const noexcept try {
		return static_cast<const StateOf<Element>*>(readStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Look up immutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.
	 *
	 * The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.getStatePointerConst(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto* state = *result;
	 * if (state) {
	 *     const int count = state->count;
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<const StateOf<Element>*> getStatePointerConst(
		const Element&,
		WindowId window,
		GlobalFlowID elementId) const noexcept try {
		return static_cast<const StateOf<Element>*>(readStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Look up immutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.
	 *
	 * The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.getStatePointerConst(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto* state = *result;
	 * if (state) {
	 *     const int count = state->count;
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<const StateOf<Element>*> getStatePointerConst(
		const Element&,
		WindowId window,
		FlowElementPartID elementId) const noexcept try {
		return static_cast<const StateOf<Element>*>(readStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Look up mutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.
	 *
	 * The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.getStatePointer(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto* state = *result;
	 * if (state) {
	 *     const int count = state->count;
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<StateOf<Element>*> getStatePointer(
		const Element&,
		WindowId window,
		FlowElementID elementId) noexcept try {
		return static_cast<StateOf<Element>*>(modifyStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Look up mutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.
	 *
	 * The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.getStatePointer(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto* state = *result;
	 * if (state) {
	 *     const int count = state->count;
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<StateOf<Element>*> getStatePointer(
		const Element&,
		WindowId window,
		FlowElementPartID elementId) noexcept try {
		return static_cast<StateOf<Element>*>(modifyStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Look up mutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.
	 *
	 * The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.getStatePointer(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto* state = *result;
	 * if (state) {
	 *     const int count = state->count;
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<StateOf<Element>*> getStatePointer(
		const Element&,
		WindowId window,
		GlobalFlowID elementId) noexcept try {
		return static_cast<StateOf<Element>*>(modifyStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Erase this instance state, returning whether an existing state was selected for erasure. During a window build erasure is deferred until successful commit; cancellation discards queued erasures.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.eraseState(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool erased = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<bool> eraseState(
		const Element&,
		WindowId window,
		FlowElementID elementId) noexcept try {
		return eraseStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Erase this instance state, returning whether an existing state was selected for erasure. During a window build erasure is deferred until successful commit; cancellation discards queued erasures.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.eraseState(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool erased = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<bool> eraseState(
		const Element&,
		WindowId window,
		FlowElementPartID elementId) noexcept try {
		return eraseStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Erase this instance state, returning whether an existing state was selected for erasure. During a window build erasure is deferred until successful commit; cancellation discards queued erasures.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.eraseState(counter, window, id);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool erased = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] Result<bool> eraseState(
		const Element&,
		WindowId window,
		GlobalFlowID elementId) noexcept try {
		return eraseStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId));
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::ElementResolveState);
		return unexpectedError(error);
	}

	/**
	 * @brief Collect all currently expired transient state for one window and return the number removed. Successful frame completion normally handles retirement.
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] size_t collectStateGarbage(WindowId window) noexcept;

	/**
	 * @brief Eagerly prepare the immutable app-wide resources of one definition or every definition in an ElementSet. Resource-free definitions are successful no-ops.
	 *
	 * Hybrid contract: a resourceful definition without an initialized owner returns ObjectNotInitialized. Resource constructor and allocation exceptions propagate; already prepared definitions remain ready and preparation can be retried.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.prepare(counter);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement Element>
	[[nodiscard]] Status prepare(const Element&) {
		if constexpr (HasResources<Element>) {
			if (!app_ || !controller_ || !storage_)
				return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ElementResolveResource));
			(void)resolveResourcesErased(detail::element::elementDescriptor<Element>, true);
		}

		return {};
	}

	/**
	 * @brief Eagerly prepare the immutable app-wide resources of one definition or every definition in an ElementSet. Resource-free definitions are successful no-ops.
	 *
	 * Hybrid contract: a resourceful definition without an initialized owner returns ObjectNotInitialized. Resource constructor and allocation exceptions propagate; already prepared definitions remain ready and preparation can be retried.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = elements.prepare(FlowUi::elementSet(counter, FlowUi::kButton));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement... Elements>
	[[nodiscard]] Status prepare(const ElementSet<Elements...>&) {
		if constexpr ((HasResources<Elements> || ...)) {
			if (!app_ || !controller_ || !storage_)
				return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ElementResolveResource));
		}
		auto prepareElementType = [this]<typename Element>() {
			if constexpr (HasResources<Element>) {
				(void)resolveResourcesErased(
					detail::element::elementDescriptor<Element>, true);
			}
		};
		(prepareElementType.template operator()<Elements>(), ...);

		return {};
	}

private:
	friend class App;
	// ElementInvocation is the narrow internal owner of registration, state lease,
	// and lazy resource access used by the compile-time builder pipeline.
	template <typename Element>
	friend class detail::element::ElementInvocation;

	void init(App& app, detail::storage::IStorageSystem& storage);
	void destroy() noexcept;
	void rebindOwner(App& app) noexcept;
	void registerWindow(WindowId window);
	void destroyWindow(WindowId window) noexcept;
	void beginWindowFrame(WindowId window, uint64_t epoch);
	void commitWindowFrame(WindowId window, uint64_t epoch) noexcept;
	void cancelWindowFrame(WindowId window, uint64_t epoch) noexcept;
	void attachTo(UiManager& uiManager) noexcept;
#if FLOW_UI_DEV_MODE
	void setDevTimingRecorder(devSystems::DevTimingRecorder* recorder) noexcept {
		devTimingRecorder_ = recorder;
	}
	void setDevSchemaRegistry(devSystems::tooling::DevSchemaRegistry* registry) noexcept {
		devSchemaRegistry_ = registry;
	}
	template <FlowElement Element>
	void ensureDevSchemaElement() {
		if (devSchemaRegistry_) devSchemaRegistry_->ensureElement<Element>();
	}
#endif
	const detail::manager_storage::ElementDefinitionRecord& ensureRegistered(
		const detail::element::ElementRegistrationDescriptor& descriptor);
	[[nodiscard]] const void* readStateErased(
		const detail::element::ElementRegistrationDescriptor& descriptor,
		WindowId window,
		detail::element::ElementInstanceKey instanceKey) const;
	[[nodiscard]] void* modifyStateErased(
		const detail::element::ElementRegistrationDescriptor& descriptor,
		WindowId window,
		detail::element::ElementInstanceKey instanceKey);
	bool eraseStateErased(
		const detail::element::ElementRegistrationDescriptor& descriptor,
		WindowId window,
		detail::element::ElementInstanceKey instanceKey);
	[[nodiscard]] const void* resolveResourcesErased(
		const detail::element::ElementRegistrationDescriptor& descriptor,
		bool retryFailed);
	[[nodiscard]] detail::element::ResolvedElementStateInvocation beginStateInvocation(
		const detail::element::ElementRegistrationDescriptor& descriptor,
		WindowId window,
		detail::element::ElementInstanceKey instanceKey,
		ElementStatePolicy policy);
	void endStateInvocation(WindowId window) noexcept;

	// Kept private so element resource construction can receive the owning App&
	// without exposing an App mutation backdoor on the public manager surface.
	App* app_ = nullptr;
	detail::storage::IStorageSystem* storage_ = nullptr;
	detail::manager_storage::ElementStorageController* controller_ = nullptr;
	uint64_t controllerHandle_ = 0;
	uint32_t controllerName_ = 0;
#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder* devTimingRecorder_ = nullptr;
	devSystems::tooling::DevSchemaRegistry* devSchemaRegistry_ = nullptr;
#endif
};

} // namespace FlowUi

#include "FlowUi/Private/elementSystem/ElementInvocation.hpp"
