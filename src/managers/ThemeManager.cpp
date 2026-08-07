#include "managers/ThemeManager.hpp"
#include "internal/ManagerStorage/ThemeStorageController.hpp"

namespace FlowUi {

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
