#include "internal/ManagerStorage/ElementStorageController.hpp"

#include <stdexcept>
#include <string>

namespace FlowUi::detail::manager_storage {

namespace {

bool descriptorsMatch(
	const element::ElementRegistrationDescriptor& existing,
	const element::ElementRegistrationDescriptor& requested) noexcept {
	return existing.definitionId == requested.definitionId &&
		existing.definitionTypeHash == requested.definitionTypeHash &&
		existing.parametersTypeHash == requested.parametersTypeHash &&
		existing.stateTypeHash == requested.stateTypeHash &&
		existing.resourcesTypeHash == requested.resourcesTypeHash &&
		existing.parametersSize == requested.parametersSize &&
		existing.parametersAlignment == requested.parametersAlignment &&
		existing.stateSize == requested.stateSize &&
		existing.stateAlignment == requested.stateAlignment &&
		existing.resourcesSize == requested.resourcesSize &&
		existing.resourcesAlignment == requested.resourcesAlignment &&
		existing.hasState == requested.hasState &&
		existing.hasResources == requested.hasResources;
}

std::string descriptorName(const element::ElementRegistrationDescriptor& descriptor) {
	const std::string_view name = descriptor.debugName.empty()
		? descriptor.definitionTypeName
		: descriptor.debugName;
	return name.empty() ? std::string("<unnamed element definition>") : std::string(name);
}

} // namespace

const ElementDefinitionRecord& ElementDefinitionRegistry::ensureDefinition(
	const element::ElementRegistrationDescriptor& descriptor) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto existing = definitions_.find(descriptor.definitionId);
	if (existing != definitions_.end()) {
		if (!descriptorsMatch(existing->second.descriptor, descriptor)) {
			throw std::logic_error(
				"FlowUi element definition id " + std::to_string(descriptor.definitionId) +
				" is already registered for " + descriptorName(existing->second.descriptor) +
				" with incompatible metadata requested by " + descriptorName(descriptor) + ".");
		}
		return existing->second;
	}

	auto [inserted, wasInserted] = definitions_.try_emplace(
		descriptor.definitionId, ElementDefinitionRecord{descriptor});
	(void)wasInserted;
	return inserted->second;
}

void ElementDefinitionRegistry::clear() noexcept {
	std::lock_guard<std::mutex> lock(mutex_);
	definitions_.clear();
}

size_t ElementDefinitionRegistry::size() const noexcept {
	std::lock_guard<std::mutex> lock(mutex_);
	return definitions_.size();
}

ElementStorageController::ElementStorageController(storage::IStorageSystem& storage) noexcept
	: storage_(&storage) {}

ElementStorageController::~ElementStorageController() {
	shutdown();
}

void ElementStorageController::registerWindow(WindowId window) {
	if (window == InvalidWindowId) {
		throw std::invalid_argument("ElementStorageController requires a valid window id.");
	}

	std::lock_guard<std::mutex> lock(windowsMutex_);
	if (!storage_) {
		throw std::runtime_error("ElementStorageController is not initialized.");
	}
	if (windows_.contains(window)) return;
	(void)windows_.try_emplace(
		window, std::make_unique<WindowElementStateRegistry>(window));
}

void ElementStorageController::destroyWindow(WindowId window) noexcept {
	storage::IStorageSystem* storage = nullptr;
	{
		std::lock_guard<std::mutex> lock(windowsMutex_);
		storage = storage_;
		windows_.erase(window);
	}

	// Always ask storage to release the partition. This makes cleanup safe when
	// construction failed between publishing a record and indexing it locally.
	if (storage && window != InvalidWindowId) {
		storage->releaseWindowPersistentRecords(window);
	}
}

const ElementDefinitionRecord& ElementStorageController::ensureDefinition(
	const element::ElementRegistrationDescriptor& descriptor) {
	return definitions_.ensureDefinition(descriptor);
}

void ElementStorageController::shutdown() noexcept {
	storage::IStorageSystem* storage = nullptr;
	{
		std::lock_guard<std::mutex> lock(windowsMutex_);
		storage = storage_;
		storage_ = nullptr;
	}
	if (!storage) return;

	for (;;) {
		WindowId window = InvalidWindowId;
		std::unique_ptr<WindowElementStateRegistry> registry;
		{
			std::lock_guard<std::mutex> lock(windowsMutex_);
			if (windows_.empty()) break;
			auto entry = windows_.begin();
			window = entry->first;
			registry = std::move(entry->second);
			windows_.erase(entry);
		}
		storage->releaseWindowPersistentRecords(window);
	}
	definitions_.clear();
}

} // namespace FlowUi::detail::manager_storage
