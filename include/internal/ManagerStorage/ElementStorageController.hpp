#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FlowUi/App.hpp"
#include "FlowUi/WindowId.hpp"
#include "internal/ElementInstanceKey.hpp"
#include "internal/ManagerStorage/ElementRegistration.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "managers/structs/ElementStatePolicy.hpp"

namespace FlowUi::detail::manager_storage {

enum class ElementResourceState : uint8_t {
	Empty,
	Constructing,
	Ready,
	Failed,
	Destroying,
};

struct ElementResourceSlot {
	std::mutex mutex{};
	std::condition_variable ready{};
	ElementResourceState state = ElementResourceState::Empty;
	std::thread::id constructingThread{};
	storage::MemoryBlock allocation{};
	std::atomic<const void*> payload{nullptr};
	std::exception_ptr failure{};
};

struct ElementDefinitionRecord {
	explicit ElementDefinitionRecord(
		const element::ElementRegistrationDescriptor& registration)
		: descriptor(registration) {}

	element::ElementRegistrationDescriptor descriptor{};
	ElementResourceSlot resources{};
};

struct ElementResourceRecordHeader {
	FlowDefinitionID definitionId{};
	uint64_t definitionTypeHash = 0;
	uint64_t resourcesTypeHash = 0;
	size_t resourcesSize = 0;
	size_t resourcesAlignment = 0;
	void (*destroyResources)(void* resources) noexcept = nullptr;
	std::string_view definitionName{};
	std::string_view resourcesTypeName{};
};

struct ElementStateRecordHeader {
	element::ElementInstanceKey instanceKey{};
	FlowDefinitionID definitionId{};
	uint64_t definitionTypeHash = 0;
	uint64_t stateTypeHash = 0;
	size_t stateSize = 0;
	size_t stateAlignment = 0;
	uint64_t lastSeenCommittedFrame = 0;
	size_t gcIndex = std::numeric_limits<size_t>::max();
	ElementStatePolicy policy = ElementStatePolicy::transient();
	void (*destroyState)(void* state) noexcept = nullptr;
	std::string_view definitionName{};
	std::string_view stateTypeName{};
};

struct ResolvedElementState {
	storage::PersistentRecordHandle handle{};
	void* payload = nullptr;
};

struct ElementStateFrameTransaction {
	uint64_t epoch = 0;
	bool active = false;
	std::unordered_set<element::ElementInstanceKey, element::ElementInstanceKeyHash> touched{};
	std::unordered_set<element::ElementInstanceKey, element::ElementInstanceKeyHash> created{};
	std::unordered_map<
		element::ElementInstanceKey,
		ElementStatePolicy,
		element::ElementInstanceKeyHash> policies{};

	void clear() noexcept {
		epoch = 0;
		active = false;
		touched.clear();
		created.clear();
		policies.clear();
	}
};

class ElementDefinitionRegistry {
public:
	ElementDefinitionRecord& ensureDefinition(
		const element::ElementRegistrationDescriptor& descriptor);
	void clear() noexcept;
	[[nodiscard]] size_t size() const noexcept;
	[[nodiscard]] storage::MemoryBlock takeReadyResourceForDestroy() noexcept;

private:
	mutable std::mutex mutex_{};
	std::unordered_map<
		FlowDefinitionID,
		std::unique_ptr<ElementDefinitionRecord>,
		FlowDefinitionIDHash> definitions_{};
};

struct WindowElementStateRegistry {
	explicit WindowElementStateRegistry(WindowId ownerWindow) noexcept
		: window(ownerWindow) {}

	WindowId window = InvalidWindowId;
	std::mutex mutex{};
	std::unordered_map<
		element::ElementInstanceKey,
		storage::PersistentRecordHandle,
		element::ElementInstanceKeyHash> byInstance{};
	std::unordered_set<
		element::ElementInstanceKey,
		element::ElementInstanceKeyHash> deferredErases{};
	std::vector<element::ElementInstanceKey> gcCandidates{};
	size_t gcCursor = 0;
	uint64_t committedFrameNumber = 0;
	ElementStateFrameTransaction transaction{};
	size_t activeInvocations = 0;
	bool destroyRequested = false;
};

class ElementStorageController {
public:
	explicit ElementStorageController(storage::IStorageSystem& storage) noexcept;
	~ElementStorageController();

	ElementStorageController(const ElementStorageController&) = delete;
	ElementStorageController& operator=(const ElementStorageController&) = delete;

	void registerWindow(WindowId window);
	void destroyWindow(WindowId window) noexcept;
	ElementDefinitionRecord& ensureDefinition(
		const element::ElementRegistrationDescriptor& descriptor);
	[[nodiscard]] const void* resolveOrCreateResources(
		const element::ElementRegistrationDescriptor& descriptor,
		App& app,
		bool retryFailed);
	[[nodiscard]] ResolvedElementState resolveOrCreateStateForInvocation(
		WindowId window,
		element::ElementInstanceKey instanceKey,
		const element::ElementRegistrationDescriptor& descriptor,
		ElementStatePolicy policy);
	void endStateInvocation(WindowId window) noexcept;
	void beginWindowFrame(WindowId window, uint64_t epoch);
	void commitWindowFrame(WindowId window, uint64_t epoch) noexcept;
	void cancelWindowFrame(WindowId window, uint64_t epoch) noexcept;
	[[nodiscard]] size_t collectEligibleStates(
		WindowId window,
		size_t scanBudget) noexcept;
	[[nodiscard]] const void* readState(
		WindowId window,
		element::ElementInstanceKey instanceKey,
		const element::ElementRegistrationDescriptor& descriptor);
	[[nodiscard]] void* modifyState(
		WindowId window,
		element::ElementInstanceKey instanceKey,
		const element::ElementRegistrationDescriptor& descriptor);
	bool eraseState(
		WindowId window,
		element::ElementInstanceKey instanceKey,
		const element::ElementRegistrationDescriptor& descriptor);
	void shutdown() noexcept;

private:
	storage::IStorageSystem* storage_ = nullptr;
	std::mutex windowsMutex_{};
	std::unordered_map<WindowId, std::shared_ptr<WindowElementStateRegistry>> windows_{};
	ElementDefinitionRegistry definitions_{};
};

} // namespace FlowUi::detail::manager_storage
