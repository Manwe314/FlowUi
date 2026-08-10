#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "FlowUi/App.hpp"
#include "FlowUi/WindowId.hpp"
#include "internal/ManagerStorage/ElementRegistration.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"

namespace FlowUi::detail::manager_storage {

struct ElementDefinitionRecord {
	element::ElementRegistrationDescriptor descriptor{};
};

class ElementDefinitionRegistry {
public:
	const ElementDefinitionRecord& ensureDefinition(
		const element::ElementRegistrationDescriptor& descriptor);
	void clear() noexcept;
	[[nodiscard]] size_t size() const noexcept;

private:
	mutable std::mutex mutex_{};
	std::unordered_map<FlowDefinitionId, ElementDefinitionRecord> definitions_{};
};

struct WindowElementStateRegistry {
	explicit WindowElementStateRegistry(WindowId ownerWindow) noexcept
		: window(ownerWindow) {}

	WindowId window = InvalidWindowId;
	std::mutex mutex{};
	std::unordered_map<FlowElementId, storage::PersistentRecordHandle> byFlowId{};
};

class ElementStorageController {
public:
	explicit ElementStorageController(storage::IStorageSystem& storage) noexcept;
	~ElementStorageController();

	ElementStorageController(const ElementStorageController&) = delete;
	ElementStorageController& operator=(const ElementStorageController&) = delete;

	void registerWindow(WindowId window);
	void destroyWindow(WindowId window) noexcept;
	const ElementDefinitionRecord& ensureDefinition(
		const element::ElementRegistrationDescriptor& descriptor);
	void shutdown() noexcept;

private:
	storage::IStorageSystem* storage_ = nullptr;
	std::mutex windowsMutex_{};
	std::unordered_map<WindowId, std::unique_ptr<WindowElementStateRegistry>> windows_{};
	ElementDefinitionRegistry definitions_{};
};

} // namespace FlowUi::detail::manager_storage
