#include "managers/ThemeManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#endif
#include "internal/ManagerStorage/ThemeStorageController.hpp"

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ThemeManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (controller_) controller_->appendDevMemorySamples(sink);
}
#endif

ThemeManager::ThemeManager()
	: controller_(std::make_unique<detail::manager_storage::ThemeStorageController>()) {}

ThemeManager::~ThemeManager() {
	destroy();
}

void ThemeManager::init(detail::storage::IStorageSystem& storage) {
	storage_ = &storage;
	if (controller_) {
		controller_->init(storage);
	}
}

void ThemeManager::destroy() noexcept {
	if (controller_) {
		controller_->shutdown();
	}
	storage_ = nullptr;
}

void ThemeManager::applyStagedMutations() {
	if (controller_) {
		controller_->applyStagedMutations();
	}
}

} // namespace FlowUi
