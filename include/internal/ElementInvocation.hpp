#pragma once

#include <concepts>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "managers/ElementManager.hpp"

namespace FlowUi::detail::element {

/**
 * @brief Typed RAII owner for one Flow element's managed data invocation.
 *
 * ElementInvocation is created once before an element's callbacks run. It
 * ensures the definition is registered, resolves state at most once, retains
 * the state invocation lease until destruction, and lazily caches app-wide
 * resources for every context used by that invocation. It is intentionally
 * stationary so every context reference remains valid for its full lifetime.
 *
 * The template deliberately has no public runtime-registration role. Element
 * must satisfy FlowElement when begin() is instantiated by the final builder.
 */
template <typename Element>
class ElementInvocation {
public:
	using ElementType = std::remove_cvref_t<Element>;

	[[nodiscard]] static ElementInvocation begin(
		ElementManager& elements,
		UiManager& uiManager,
		WindowId window,
		FlowElementID elementId) {
		static_assert(
			FlowElement<ElementType>,
			"FlowUi: ElementInvocation requires an element satisfying FlowElement.");
		return ElementInvocation(elements, uiManager, window, elementId);
	}

	ElementInvocation(const ElementInvocation&) = delete;
	ElementInvocation& operator=(const ElementInvocation&) = delete;
	ElementInvocation(ElementInvocation&&) = delete;
	ElementInvocation& operator=(ElementInvocation&&) = delete;

	~ElementInvocation() noexcept {
		if (stateInvocationActive_) {
			elements_.endStateInvocation(window_);
		}
	}

	[[nodiscard]] UiManager& uiManager() const noexcept { return uiManager_; }
	[[nodiscard]] WindowId windowId() const noexcept { return window_; }
	[[nodiscard]] FlowElementID elementId() const noexcept { return elementId_; }
	[[nodiscard]] uint64_t stateHandle() const noexcept { return stateHandle_; }
	[[nodiscard]] bool hasActiveStateInvocation() const noexcept {
		return stateInvocationActive_;
	}

	/** Return the state pointer resolved once when this invocation began. */
	template <typename E = ElementType>
		requires HasState<E> && std::same_as<E, ElementType>
	[[nodiscard]] StateOf<E>& state() {
		if (!statePayload_) {
			throw FlowUiException(makeError(ErrorCode::ElementStateUnavailable, ErrorSite::ElementInvoke));
		}
		return *static_cast<StateOf<E>*>(statePayload_);
	}

	/** Return immutable state from the same invocation-local cached pointer. */
	template <typename E = ElementType>
		requires HasState<E> && std::same_as<E, ElementType>
	[[nodiscard]] const StateOf<E>& state() const {
		if (!statePayload_) {
			throw FlowUiException(makeError(ErrorCode::ElementStateUnavailable, ErrorSite::ElementInvoke));
		}
		return *static_cast<const StateOf<E>*>(statePayload_);
	}

	/** Lazily resolve and cache this definition's immutable app-wide resources. */
	template <typename E = ElementType>
		requires HasResources<E> && std::same_as<E, ElementType>
	[[nodiscard]] const ResourcesOf<E>& resources() const {
		if (!resourcePayload_) {
			resourcePayload_ = elements_.resolveResourcesErased(
				elementDescriptor<ElementType>, false);
		}
		if (!resourcePayload_) {
			throw FlowUiException(makeError(ErrorCode::ElementResourceUnavailable, ErrorSite::ElementInvoke));
		}
		return *static_cast<const ResourcesOf<E>*>(resourcePayload_);
	}

private:
	ElementInvocation(
		ElementManager& elements,
		UiManager& uiManager,
		WindowId window,
		FlowElementID elementId)
		: elements_(elements),
		  uiManager_(uiManager),
		  window_(window),
		  elementId_(elementId) {
#if FLOW_UI_DEV_MODE
		elements_.ensureDevSchemaElement<ElementType>();
#endif
		if constexpr (HasState<ElementType>) {
			const ResolvedElementStateInvocation state =
				elements_.beginStateInvocation(
					elementDescriptor<ElementType>,
					window_,
					detail::element::toInstanceKey(elementId_),
					statePolicy<ElementType>());
			stateHandle_ = state.handle;
			statePayload_ = state.payload;
			stateInvocationActive_ = true;
		} else {
			(void)elements_.ensureRegistered(elementDescriptor<ElementType>);
		}
	}

	ElementManager& elements_;
	UiManager& uiManager_;
	WindowId window_ = InvalidWindowId;
	FlowElementID elementId_{};
	uint64_t stateHandle_ = 0;
	void* statePayload_ = nullptr;
	mutable const void* resourcePayload_ = nullptr;
	bool stateInvocationActive_ = false;
};

} // namespace FlowUi::detail::element
