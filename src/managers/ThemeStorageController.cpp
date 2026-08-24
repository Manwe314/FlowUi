#include "internal/ManagerStorage/ThemeStorageController.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

namespace FlowUi::detail::manager_storage {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ThemeStorageController::appendDevMemorySamples(
	::FlowUi::devSystems::MemorySampleSink& sink) const noexcept {
	try {
		std::scoped_lock lock(mutex_);
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.addNodeContainer(typeRegistry_);
		for (const auto& [_, type] : typeRegistry_) memory.addNodeContainer(type.variants);
		memory.add(stagedMutations_);
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kThemes.id, memory);
	} catch (...) {}
}
#endif

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
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeRegisterVariant));
	}
	return storage_->intern(str);
}

storage::ResourceKey ThemeStorageController::makeThemeResourceKey(
	uint64_t typeHash,
	storage::StringId variantNameId) const {
	if (!storage_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeLookupActiveVariant));
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
	std::vector<StagedThemeMutation> pending;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (stagedMutations_.empty()) return;
		pending.swap(stagedMutations_);
	}

	for (const auto& mutation : pending) {
		storage::ManagerRecordHandle handle{};
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = typeRegistry_.find(mutation.typeHash);
			if (it == typeRegistry_.end()) continue;
			auto varIt = it->second.variants.find(mutation.variantNameId);
			if (varIt == it->second.variants.end()) continue;
			handle = varIt->second.handle;
		}

		void* rawRecord = storage_->managerRecordData(handle, storage::ResourceKind::UiTheme);
		if (!rawRecord) continue;
		auto* header = reinterpret_cast<storage::ThemeRecordHeader*>(rawRecord);
		mutation.mutator(header->payload());
		{
			std::lock_guard<std::mutex> lock(mutex_);
			void* current = storage_->managerRecordData(handle, storage::ResourceKind::UiTheme);
			if (current == rawRecord) {
				reinterpret_cast<storage::ThemeRecordHeader*>(current)->revision++;
			}
		}
	}

}

} // namespace FlowUi::detail::manager_storage
