#pragma once

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
	/** @brief Construct an uninitialized manager; applications normally use App::elements(). */
	ElementManager() = default;
	/** @brief Release owned element state, definition metadata, and resource payloads. */
	~ElementManager();

	ElementManager(const ElementManager&) = delete;
	ElementManager& operator=(const ElementManager&) = delete;
	ElementManager(ElementManager&&) = delete;
	ElementManager& operator=(ElementManager&&) = delete;

	/**
	 * @brief Get a const pointer to existing element state without creating it.
	 * @return Immutable state pointer, or nullptr when the window/instance has no state.
	 * @note The pointer remains stable until explicit erase, transient-state GC at
	 * a later successful frame boundary, or window destruction. Do not retain a
	 * transient-state pointer across frame boundaries.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] const StateOf<Element>* getStatePointerConst(
		const Element&,
		WindowId window,
		FlowElementID elementId) const {
		return static_cast<const StateOf<Element>*>(readStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	}

	/** @brief Borrow existing immutable state for a global ID in window.
	 * The primary overload's lifetime and deferred-erasure rules apply.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] const StateOf<Element>* getStatePointerConst(
		const Element&,
		WindowId window,
		GlobalFlowID elementId) const {
		return static_cast<const StateOf<Element>*>(readStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	}

	/** @brief Borrow existing immutable state for a semantic part ID in window.
	 * The primary overload's lifetime and deferred-erasure rules apply.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] const StateOf<Element>* getStatePointerConst(
		const Element&,
		WindowId window,
		FlowElementPartID elementId) const {
		return static_cast<const StateOf<Element>*>(readStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	}

	/**
	 * @brief Get a mutable pointer to existing element state without creating it.
	 * @return Mutable state pointer, or nullptr when the window/instance has no state.
	 * @note The pointer remains stable until explicit erase, transient-state GC at
	 * a later successful frame boundary, or window destruction. Do not retain a
	 * transient-state pointer across frame boundaries.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] StateOf<Element>* getStatePointer(
		const Element&,
		WindowId window,
		FlowElementID elementId) {
		return static_cast<StateOf<Element>*>(modifyStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	}

	/** @brief Borrow existing mutable state for a semantic part ID in window.
	 * The primary overload's lifetime and deferred-erasure rules apply.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] StateOf<Element>* getStatePointer(
		const Element&,
		WindowId window,
		FlowElementPartID elementId) {
		return static_cast<StateOf<Element>*>(modifyStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	}

	/** @brief Borrow existing mutable state for a global ID in window.
	 * The primary overload's lifetime and deferred-erasure rules apply.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	[[nodiscard]] StateOf<Element>* getStatePointer(
		const Element&,
		WindowId window,
		GlobalFlowID elementId) {
		return static_cast<StateOf<Element>*>(modifyStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId)));
	}

	/**
	 * @brief Explicitly erase an existing state instance.
	 * @note Erasure requested during a window build is deferred to that frame's
	 * successful commit, after every cached callback pointer has been released.
	 * A canceled frame discards its queued erasures.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	bool eraseState(
		const Element&,
		WindowId window,
		FlowElementID elementId) {
		return eraseStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId));
	}

	/** @brief Erase existing state for a semantic part ID in window.
	 * The primary overload's lifetime and deferred-erasure rules apply.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	bool eraseState(
		const Element&,
		WindowId window,
		FlowElementPartID elementId) {
		return eraseStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId));
	}

	/** @brief Erase existing state for a global ID in window.
	 * The primary overload's lifetime and deferred-erasure rules apply.
	 */
	template <FlowElement Element>
		requires HasState<Element>
	bool eraseState(
		const Element&,
		WindowId window,
		GlobalFlowID elementId) {
		return eraseStateErased(
			detail::element::elementDescriptor<Element>, window,
			detail::element::toInstanceKey(elementId));
	}

	/**
	 * @brief Aggressively collect every currently expired transient state in a window.
	 * @return Number of state records removed.
	 */
	[[nodiscard]] size_t collectStateGarbage(WindowId window) noexcept;

	/**
	 * @brief Eagerly construct one element definition's app-wide resources.
	 * @note Resource-free definitions are silently ignored.
	 */
	template <FlowElement Element>
	void prepare(const Element&) {
		if constexpr (HasResources<Element>) {
			(void)resolveResourcesErased(detail::element::elementDescriptor<Element>, true);
		}
	}

	/**
	 * @brief Eagerly construct resources for every resourceful definition in a set.
	 * @note Resource-free members are silently ignored.
	 */
	template <FlowElement... Elements>
	void prepare(const ElementSet<Elements...>&) {
		auto prepareElementType = [this]<typename Element>() {
			if constexpr (HasResources<Element>) {
				(void)resolveResourcesErased(
					detail::element::elementDescriptor<Element>, true);
			}
		};
		(prepareElementType.template operator()<Elements>(), ...);
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
