#include "managers/ElementManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#endif

#include <functional>
#include <limits>
#include <stdexcept>

#include "FlowUi/App.hpp"
#include "FlowUi/WindowId.hpp"
#include "internal/ManagerStorage/ElementStorageController.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "managers/UiManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#endif

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ElementManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (controller_) controller_->appendDevMemorySamples(sink);
}
#endif

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

void ElementManager::beginWindowFrame(WindowId window, uint64_t epoch) {
	if (!controller_) throw std::runtime_error("ElementManager is not initialized.");
	controller_->beginWindowFrame(window, epoch);
}

void ElementManager::commitWindowFrame(WindowId window, uint64_t epoch) noexcept {
	if (controller_) controller_->commitWindowFrame(window, epoch);
}

void ElementManager::cancelWindowFrame(WindowId window, uint64_t epoch) noexcept {
	if (controller_) controller_->cancelWindowFrame(window, epoch);
}

size_t ElementManager::collectStateGarbage(WindowId window) noexcept {
	if (!controller_) return 0;
	return controller_->collectEligibleStates(
		window, std::numeric_limits<size_t>::max());
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

const void* ElementManager::readStateErased(
	const detail::element::ElementRegistrationDescriptor& descriptor,
	WindowId window,
	detail::element::ElementInstanceKey instanceKey) const {
	if (!controller_) throw std::runtime_error("ElementManager is not initialized.");
	(void)controller_->ensureDefinition(descriptor);
	return controller_->readState(window, instanceKey, descriptor);
}

void* ElementManager::modifyStateErased(
	const detail::element::ElementRegistrationDescriptor& descriptor,
	WindowId window,
	detail::element::ElementInstanceKey instanceKey) {
	if (!controller_) throw std::runtime_error("ElementManager is not initialized.");
	(void)controller_->ensureDefinition(descriptor);
	return controller_->modifyState(window, instanceKey, descriptor);
}

bool ElementManager::eraseStateErased(
	const detail::element::ElementRegistrationDescriptor& descriptor,
	WindowId window,
	detail::element::ElementInstanceKey instanceKey) {
	if (!controller_) throw std::runtime_error("ElementManager is not initialized.");
	(void)controller_->ensureDefinition(descriptor);
	return controller_->eraseState(window, instanceKey, descriptor);
}

const void* ElementManager::resolveResourcesErased(
	const detail::element::ElementRegistrationDescriptor& descriptor,
	bool retryFailed) {
	if (!controller_ || !app_) {
		throw std::runtime_error("ElementManager is not initialized.");
	}
	if (!descriptor.hasResources) return nullptr;
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE_DEEP_IF(
		devTimingRecorder_, devSystems::TimingCategory::Element,
		devSystems::TimingZoneRole::Work, "flowui.element.resource_resolution");
#endif
	return controller_->resolveOrCreateResources(descriptor, *app_, retryFailed);
}

detail::element::ResolvedElementStateInvocation ElementManager::beginStateInvocation(
	const detail::element::ElementRegistrationDescriptor& descriptor,
	WindowId window,
	detail::element::ElementInstanceKey instanceKey,
	ElementStatePolicy policy) {
	if (!controller_) throw std::runtime_error("ElementManager is not initialized.");
	(void)controller_->ensureDefinition(descriptor);
	if (!descriptor.hasState) return {};
	const manager_storage::ResolvedElementState state =
		controller_->resolveOrCreateStateForInvocation(window, instanceKey, descriptor, policy);
	return detail::element::ResolvedElementStateInvocation{
		.handle = state.handle.packed(),
		.payload = state.payload,
	};
}

void ElementManager::endStateInvocation(WindowId window) noexcept {
	if (controller_) controller_->endStateInvocation(window);
}

} // namespace FlowUi
