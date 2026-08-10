#include "managers/ElementManager.hpp"

#include <functional>
#include <stdexcept>

#include "FlowUi/App.hpp"
#include "FlowUi/WindowId.hpp"
#include "internal/ManagerStorage/ElementStorageController.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi {

namespace storage = detail::storage;
namespace manager_storage = detail::manager_storage;

ElementManager::~ElementManager() {
	destroy();
}

void ElementManager::init(App& app, storage::IStorageSystem& storageSystem) {
	destroy();

	const storage::StringId name = storageSystem.intern("flowui.elements.root");
	const storage::ResourceKey key{
		storage::ResourceDomain::Internal,
		name,
		InvalidWindowId,
	};
	const storage::ManagerRecordHandle handle =
		manager_storage::createState<manager_storage::ElementStorageController>(
			storageSystem,
			key,
			storage::ResourceKind::ManagerRoot,
			name,
			std::ref(storageSystem));
	manager_storage::ElementStorageController* controller =
		manager_storage::state<manager_storage::ElementStorageController>(
			&storageSystem, handle, storage::ResourceKind::ManagerRoot);
	if (!controller) {
		(void)storageSystem.removeManagerRecord(key, storage::ResourceKind::ManagerRoot);
		throw std::runtime_error("Element manager storage root publication failed.");
	}

	app_ = &app;
	storage_ = &storageSystem;
	controller_ = controller;
	controllerHandle_ = handle.packed();
	controllerName_ = name;
}

void ElementManager::destroy() noexcept {
	if (controller_) controller_->shutdown();
	if (storage_ && controllerName_ != 0) {
		try {
			(void)storage_->removeManagerRecord(
				storage::ResourceKey{
					storage::ResourceDomain::Internal,
					controllerName_,
					InvalidWindowId,
				},
				storage::ResourceKind::ManagerRoot);
		} catch (...) {
		}
	}

	controller_ = nullptr;
	controllerHandle_ = 0;
	controllerName_ = 0;
	storage_ = nullptr;
	app_ = nullptr;
}

void ElementManager::rebindOwner(App& app) noexcept {
	if (storage_) app_ = &app;
}

void ElementManager::registerWindow(WindowId window) {
	if (!controller_) {
		throw std::runtime_error("ElementManager is not initialized.");
	}
	controller_->registerWindow(window);
}

void ElementManager::destroyWindow(WindowId window) noexcept {
	if (controller_) controller_->destroyWindow(window);
}

void ElementManager::attachTo(UiManager& uiManager) noexcept {
	uiManager.setElementManager(this);
}

const manager_storage::ElementDefinitionRecord& ElementManager::ensureRegistered(
	const detail::element::ElementRegistrationDescriptor& descriptor) {
	if (!controller_) {
		throw std::runtime_error("ElementManager is not initialized.");
	}
	return controller_->ensureDefinition(descriptor);
}

} // namespace FlowUi
