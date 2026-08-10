#pragma once

#include <cstdint>

#include "FlowUi/WindowId.hpp"
#include "managers/structs/ElementManagerStructs.hpp"

namespace FlowUi {

class App;
class UiManager;

namespace detail {
void ensureElementDefinitionRegistered(
	UiManager& uiManager,
	const element::ElementRegistrationDescriptor& descriptor);
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
 * ElementManager is owned by App. Its mutable controller and all future element
 * state/resource payloads live in StorageSystem rather than on element
 * definition statics. Applications obtain this manager through App::elements().
 *
 * Phase C establishes ownership and window lifecycles. Element registration,
 * state access, resource preparation, and garbage collection are added by the
 * following migration phases.
 */
class ElementManager {
public:
	ElementManager() = default;
	~ElementManager();

	ElementManager(const ElementManager&) = delete;
	ElementManager& operator=(const ElementManager&) = delete;
	ElementManager(ElementManager&&) = delete;
	ElementManager& operator=(ElementManager&&) = delete;

private:
	friend class App;
	// transitional: temporary friendship permits the current UiManager bridge to register descriptors until concept-based dispatch calls ElementManager directly.
	friend void detail::ensureElementDefinitionRegistered(
		UiManager& uiManager,
		const detail::element::ElementRegistrationDescriptor& descriptor);

	void init(App& app, detail::storage::IStorageSystem& storage);
	void destroy() noexcept;
	void rebindOwner(App& app) noexcept;
	void registerWindow(WindowId window);
	void destroyWindow(WindowId window) noexcept;
	void attachTo(UiManager& uiManager) noexcept;
	const detail::manager_storage::ElementDefinitionRecord& ensureRegistered(
		const detail::element::ElementRegistrationDescriptor& descriptor);

	// Kept private so element resource construction can receive the owning App&
	// without exposing an App mutation backdoor on the public manager surface.
	App* app_ = nullptr;
	detail::storage::IStorageSystem* storage_ = nullptr;
	detail::manager_storage::ElementStorageController* controller_ = nullptr;
	uint64_t controllerHandle_ = 0;
	uint32_t controllerName_ = 0;
};

} // namespace FlowUi
