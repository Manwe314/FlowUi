#include "internal/ManagerStorage/ThemeStorageController.hpp"
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

namespace FlowUi::detail::manager_storage {

void ThemeStorageController::init(storage::IStorageSystem& storage) {
	std::lock_guard<std::mutex> lock(mutex_);
	storage_ = &storage;
	typeRegistry_.clear();
	stagedMutations_.clear();
}

void ThemeStorageController::shutdown() noexcept {
	std::lock_guard<std::mutex> lock(mutex_);
	if (!storage_) return;

	for (auto& [typeHash, record] : typeRegistry_) {
		(void)typeHash;
		for (auto& [variantId, variant] : record.variants) {
			(void)variantId;
			storage_->removeManagerRecord(variant.resourceKey, storage::ResourceKind::UiTheme);
		}
	}

	typeRegistry_.clear();
	stagedMutations_.clear();
	storage_ = nullptr;
}

storage::StringId ThemeStorageController::internString(std::string_view str) {
	if (!storage_) {
		throw std::runtime_error("ThemeStorageController: storage is null during internString.");
	}
	return storage_->intern(str);
}

storage::ResourceKey ThemeStorageController::makeThemeResourceKey(
	uint64_t typeHash,
	storage::StringId variantNameId) const {
	if (!storage_) {
		throw std::runtime_error("ThemeStorageController: storage is null while creating a theme resource key.");
	}

	const std::string_view variantName = storage_->string(variantNameId);
	std::string resourceName = "flowui/theme/";
	resourceName += std::to_string(typeHash);
	resourceName += '/';
	resourceName.append(variantName.data(), variantName.size());

	return storage::ResourceKey{
		.domain = storage::ResourceDomain::Internal,
		.name = storage_->intern(resourceName),
		.window = 0
	};
}

void ThemeStorageController::applyStagedMutations() {
	std::lock_guard<std::mutex> lock(mutex_);
	if (stagedMutations_.empty()) return;

	for (const auto& mutation : stagedMutations_) {
		auto it = typeRegistry_.find(mutation.typeHash);
		if (it == typeRegistry_.end()) continue;

		auto varIt = it->second.variants.find(mutation.variantNameId);
		if (varIt == it->second.variants.end()) continue;

		void* rawRecord = storage_->managerRecordData(varIt->second.handle, storage::ResourceKind::UiTheme);
		if (rawRecord) {
			auto* header = reinterpret_cast<storage::ThemeRecordHeader*>(rawRecord);
			mutation.mutator(header->payload());
			header->revision++;
		}
	}

	stagedMutations_.clear();
}

} // namespace FlowUi::detail::manager_storage
