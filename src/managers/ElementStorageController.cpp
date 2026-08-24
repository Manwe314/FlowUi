#include "internal/ManagerStorage/ElementStorageController.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <array>
#include <new>
#include <stdexcept>
#include <string>

#include "internal/StorageSystem/AlignedRecord.hpp"

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

std::string_view storedDefinitionName(const ElementStateRecordHeader& header) noexcept {
	return header.definitionName.empty() ? std::string_view("<unnamed element definition>") : header.definitionName;
}

void requireStateDescriptor(const element::ElementRegistrationDescriptor& descriptor) {
	if (!descriptor.hasState || descriptor.stateSize == 0 || descriptor.stateAlignment == 0 ||
		descriptor.stateOperations.defaultConstruct == nullptr ||
		descriptor.stateOperations.destroy == nullptr) {
		throw FlowUiException(makeError(
			ErrorCode::ElementDefinitionConflict, ErrorSite::ElementRegisterDefinition,
			descriptor.definitionId.value));
	}
}

void requireResourcesDescriptor(const element::ElementRegistrationDescriptor& descriptor) {
	if (!descriptor.hasResources || descriptor.resourcesSize == 0 ||
		descriptor.resourcesAlignment == 0 ||
		(descriptor.resourceOperations.constructWithApp == nullptr &&
			descriptor.resourceOperations.defaultConstruct == nullptr) ||
		descriptor.resourceOperations.destroy == nullptr) {
		throw FlowUiException(makeError(
			ErrorCode::ElementDefinitionConflict, ErrorSite::ElementRegisterDefinition,
			descriptor.definitionId.value));
	}
}

void validateResourceRecord(
	const ElementResourceRecordHeader& header,
	const void* payload,
	const storage::AlignedRecordLayout& layout,
	const element::ElementRegistrationDescriptor& descriptor) {
	const bool matches =
		header.definitionId == descriptor.definitionId &&
		header.definitionTypeHash == descriptor.definitionTypeHash &&
		header.resourcesTypeHash == descriptor.resourcesTypeHash &&
		header.resourcesSize == descriptor.resourcesSize &&
		header.resourcesAlignment == descriptor.resourcesAlignment &&
		layout.payloadBytes >= descriptor.resourcesSize &&
		layout.payloadAlignment >= descriptor.resourcesAlignment &&
		reinterpret_cast<uintptr_t>(payload) % descriptor.resourcesAlignment == 0;
	if (matches) return;

	::FlowUi::detail::terminateForFatalError(makeError(
		ErrorCode::ElementStorageStale, ErrorSite::ElementResolveResource,
		descriptor.definitionId.value));
}

struct ResourceAllocation {
	storage::MemoryBlock block{};
	void* payload = nullptr;
};

void destroyResourceAllocation(
	storage::IStorageSystem& storage,
	storage::MemoryBlock block) noexcept {
	if (!block || !block.data) return;
	auto* header = static_cast<ElementResourceRecordHeader*>(block.data);
	void* payload = storage::makeAlignedRecordLayout(
		sizeof(ElementResourceRecordHeader),
		alignof(ElementResourceRecordHeader),
		header->resourcesSize,
		header->resourcesAlignment).payload(block.data);
	if (header->destroyResources) header->destroyResources(payload);
	header->~ElementResourceRecordHeader();
	storage.releasePersistent(block);
}

ResourceAllocation createResourceRecord(
	storage::IStorageSystem& storage,
	const element::ElementRegistrationDescriptor& descriptor,
	App& app) {
	const storage::AlignedRecordLayout layout = storage::makeAlignedRecordLayout(
		sizeof(ElementResourceRecordHeader),
		alignof(ElementResourceRecordHeader),
		descriptor.resourcesSize,
		descriptor.resourcesAlignment);

	// This establishes the same app-shared mutation contract used by other
	// managers and rejects construction while a sealed frame snapshot is active.
	storage.noteManagerMutation(InvalidWindowId);
	storage::MemoryBlock block = storage.allocatePersistent(
		layout.allocationBytes,
		layout.allocationAlignment,
		storage::AllocationTag{
			.memoryClass = storage::MemoryClass::ResourceMetadata,
			.resourceKind = storage::ResourceKind::UiElementResources,
			.window = InvalidWindowId,
			.frameSlot = storage::InvalidFrameSlot,
			.debugName = 0,
		});

	auto* header = ::new (block.data) ElementResourceRecordHeader{
		.definitionId = descriptor.definitionId,
		.definitionTypeHash = descriptor.definitionTypeHash,
		.resourcesTypeHash = descriptor.resourcesTypeHash,
		.resourcesSize = descriptor.resourcesSize,
		.resourcesAlignment = descriptor.resourcesAlignment,
		.destroyResources = descriptor.resourceOperations.destroy,
		.definitionName = descriptor.debugName.empty()
			? descriptor.definitionTypeName
			: descriptor.debugName,
		.resourcesTypeName = descriptor.resourcesTypeName,
	};
	void* const payload = layout.payload(block.data);
	try {
		if (descriptor.resourceOperations.constructWithApp) {
			descriptor.resourceOperations.constructWithApp(payload, app);
		} else {
			descriptor.resourceOperations.defaultConstruct(payload);
		}
	} catch (...) {
		header->~ElementResourceRecordHeader();
		storage.releasePersistent(block);
		throw;
	}

	try {
		validateResourceRecord(*header, payload, layout, descriptor);
	} catch (...) {
		destroyResourceAllocation(storage, block);
		throw;
	}
	return {.block = block, .payload = payload};
}

void validateStateRecord(
	const ElementStateRecordHeader& header,
	const storage::PersistentRecordView& view,
	element::ElementInstanceKey instanceKey,
	const element::ElementRegistrationDescriptor& descriptor) {
	const bool matches =
		header.instanceKey == instanceKey &&
		header.definitionId == descriptor.definitionId &&
		header.definitionTypeHash == descriptor.definitionTypeHash &&
		header.stateTypeHash == descriptor.stateTypeHash &&
		header.stateSize == descriptor.stateSize &&
		header.stateAlignment == descriptor.stateAlignment &&
		view.payloadBytes >= descriptor.stateSize &&
		view.payloadAlignment >= descriptor.stateAlignment &&
		reinterpret_cast<uintptr_t>(view.payload) % descriptor.stateAlignment == 0;
	if (matches) return;

	::FlowUi::detail::terminateForFatalError(makeError(
		ErrorCode::ElementStorageStale, ErrorSite::ElementResolveState,
		instanceKey.value,
		descriptor.definitionId.value));
}

storage::PersistentRecordView stateRecordView(
	storage::IStorageSystem& storage,
	storage::PersistentRecordHandle handle) noexcept {
	storage::PersistentRecordView view =
		storage.persistentRecord(handle, storage::ResourceKind::UiElementState);
	if (!view || view.headerBytes < sizeof(ElementStateRecordHeader) ||
		reinterpret_cast<uintptr_t>(view.header) % alignof(ElementStateRecordHeader) != 0) {
		return {};
	}
	return view;
}

ResolvedElementState createStateRecord(
	storage::IStorageSystem& storage,
	WindowId window,
	element::ElementInstanceKey instanceKey,
	const element::ElementRegistrationDescriptor& descriptor,
	uint64_t lastSeenCommittedFrame,
	ElementStatePolicy policy) {
	struct ConstructionContext {
		element::ElementInstanceKey instanceKey{};
		const element::ElementRegistrationDescriptor* descriptor = nullptr;
		uint64_t lastSeenCommittedFrame = 0;
		ElementStatePolicy policy = ElementStatePolicy::transient();
	} context{instanceKey, &descriptor, lastSeenCommittedFrame, policy};

	const storage::PersistentRecordHandle handle = storage.createPersistentRecord(
		storage::PersistentRecordDesc{
			.kind = storage::ResourceKind::UiElementState,
			.window = window,
			.headerBytes = sizeof(ElementStateRecordHeader),
			.headerAlignment = alignof(ElementStateRecordHeader),
			.payloadBytes = descriptor.stateSize,
			.payloadAlignment = descriptor.stateAlignment,
			.debugName = 0,
			.construct = +[](void* headerMemory, void* payload, void* userData) {
				auto& values = *static_cast<ConstructionContext*>(userData);
				const element::ElementRegistrationDescriptor& type = *values.descriptor;
				auto* header = ::new (headerMemory) ElementStateRecordHeader{
					.instanceKey = values.instanceKey,
					.definitionId = type.definitionId,
					.definitionTypeHash = type.definitionTypeHash,
					.stateTypeHash = type.stateTypeHash,
					.stateSize = type.stateSize,
					.stateAlignment = type.stateAlignment,
					.lastSeenCommittedFrame = values.lastSeenCommittedFrame,
					.policy = values.policy,
					.destroyState = type.stateOperations.destroy,
					.definitionName = type.debugName.empty() ? type.definitionTypeName : type.debugName,
					.stateTypeName = type.stateTypeName,
				};
				try {
					type.stateOperations.defaultConstruct(payload);
				} catch (...) {
					header->~ElementStateRecordHeader();
					throw;
				}
			},
			.destroy = +[](void* headerMemory, void* payload) noexcept {
				auto* header = static_cast<ElementStateRecordHeader*>(headerMemory);
				if (header->destroyState) header->destroyState(payload);
				header->~ElementStateRecordHeader();
			},
			.userData = &context,
		});

	storage::PersistentRecordView view = stateRecordView(storage, handle);
	if (!view) {
		(void)storage.removePersistentRecord(handle, storage::ResourceKind::UiElementState);
		::FlowUi::detail::terminateForFatalError(makeError(
			ErrorCode::ElementStorageStale, ErrorSite::ElementConstructState,
			instanceKey.value));
	}
	validateStateRecord(
		*static_cast<ElementStateRecordHeader*>(view.header), view, instanceKey, descriptor);
	return {.handle = handle, .payload = view.payload};
}

ElementStateRecordHeader* stateRecordHeader(
	storage::IStorageSystem& storage,
	storage::PersistentRecordHandle handle) noexcept {
	storage::PersistentRecordView view = stateRecordView(storage, handle);
	return view ? static_cast<ElementStateRecordHeader*>(view.header) : nullptr;
}

void removeGcCandidateAt(
	WindowElementStateRegistry& registry,
	storage::IStorageSystem& storage,
	size_t index) noexcept {
	if (index >= registry.gcCandidates.size()) {
		registry.gcCursor = 0;
		return;
	}

	const element::ElementInstanceKey removedKey = registry.gcCandidates[index];
	if (const auto removed = registry.byInstance.find(removedKey);
		removed != registry.byInstance.end()) {
		if (ElementStateRecordHeader* removedHeader =
				stateRecordHeader(storage, removed->second)) {
			removedHeader->gcIndex = std::numeric_limits<size_t>::max();
		}
	}

	const size_t lastIndex = registry.gcCandidates.size() - 1;
	if (index != lastIndex) {
		const element::ElementInstanceKey movedKey = registry.gcCandidates[lastIndex];
		registry.gcCandidates[index] = movedKey;
		if (const auto moved = registry.byInstance.find(movedKey);
			moved != registry.byInstance.end()) {
			if (ElementStateRecordHeader* movedHeader =
					stateRecordHeader(storage, moved->second)) {
				movedHeader->gcIndex = index;
			}
		}
	}
	registry.gcCandidates.pop_back();
	if (index < registry.gcCursor && registry.gcCursor != 0) --registry.gcCursor;
	if (registry.gcCursor >= registry.gcCandidates.size()) registry.gcCursor = 0;
}

void removeGcCandidate(
	WindowElementStateRegistry& registry,
	storage::IStorageSystem& storage,
	element::ElementInstanceKey instanceKey,
	ElementStateRecordHeader& header) noexcept {
	size_t index = header.gcIndex;
	if (index >= registry.gcCandidates.size() || registry.gcCandidates[index] != instanceKey) {
		const auto found = std::find(
			registry.gcCandidates.begin(), registry.gcCandidates.end(), instanceKey);
		if (found == registry.gcCandidates.end()) {
			header.gcIndex = std::numeric_limits<size_t>::max();
			return;
		}
		index = static_cast<size_t>(found - registry.gcCandidates.begin());
	}
	removeGcCandidateAt(registry, storage, index);
}

bool isStateExpired(
	const ElementStateRecordHeader& header,
	uint64_t committedFrameNumber) noexcept {
	return header.policy.retention == ElementStateRetention::Transient &&
		committedFrameNumber > header.lastSeenCommittedFrame &&
		committedFrameNumber - header.lastSeenCommittedFrame > header.policy.graceFrames;
}

ResolvedElementState resolveExistingState(
	storage::IStorageSystem& storage,
	storage::PersistentRecordHandle handle,
	element::ElementInstanceKey instanceKey,
	const element::ElementRegistrationDescriptor& descriptor) {
	storage::PersistentRecordView view = stateRecordView(storage, handle);
	if (!view) return {};
	validateStateRecord(
		*static_cast<ElementStateRecordHeader*>(view.header), view, instanceKey, descriptor);
	return {.handle = handle, .payload = view.payload};
}

} // namespace

ElementDefinitionRecord& ElementDefinitionRegistry::ensureDefinition(
	const element::ElementRegistrationDescriptor& descriptor) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto existing = definitions_.find(descriptor.definitionId);
	if (existing != definitions_.end()) {
		if (!descriptorsMatch(existing->second->descriptor, descriptor)) {
			throw FlowUiException(makeError(
				ErrorCode::ElementDefinitionConflict, ErrorSite::ElementRegisterDefinition,
				descriptor.definitionId.value));
		}
		return *existing->second;
	}

	auto [inserted, wasInserted] = definitions_.try_emplace(
		descriptor.definitionId, std::make_unique<ElementDefinitionRecord>(descriptor));
	(void)wasInserted;
	return *inserted->second;
}

void ElementDefinitionRegistry::clear() noexcept {
	std::lock_guard<std::mutex> lock(mutex_);
	definitions_.clear();
}

size_t ElementDefinitionRegistry::size() const noexcept {
	std::lock_guard<std::mutex> lock(mutex_);
	return definitions_.size();
}

storage::MemoryBlock
ElementDefinitionRegistry::takeReadyResourceForDestroy() noexcept {
	try {
		std::lock_guard<std::mutex> registryLock(mutex_);
		for (auto& [_, definition] : definitions_) {
			ElementResourceSlot& resources = definition->resources;
			std::lock_guard<std::mutex> resourceLock(resources.mutex);
			if (resources.state != ElementResourceState::Ready || !resources.allocation) continue;
			resources.state = ElementResourceState::Destroying;
			resources.payload.store(nullptr, std::memory_order_release);
			const storage::MemoryBlock allocation = resources.allocation;
			resources.allocation = {};
			return allocation;
		}
	} catch (...) {
	}
	return {};
}

ElementStorageController::ElementStorageController(storage::IStorageSystem& storage) noexcept
	: storage_(&storage) {}

ElementStorageController::~ElementStorageController() {
	shutdown();
}

void ElementStorageController::registerWindow(WindowId window) {
	if (window == InvalidWindowId) {
		throw FlowUiException(makeError(ErrorCode::InvalidWindowId, ErrorSite::ElementRegisterWindow));
	}

	std::lock_guard<std::mutex> lock(windowsMutex_);
	if (!storage_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ElementRegisterWindow));
	}
	if (windows_.contains(window)) return;
	(void)windows_.try_emplace(
		window, std::make_shared<WindowElementStateRegistry>(window));
}

void ElementStorageController::destroyWindow(WindowId window) noexcept {
	storage::IStorageSystem* storage = nullptr;
	bool releaseRecords = false;
	{
		std::lock_guard<std::mutex> windowsLock(windowsMutex_);
		storage = storage_;
		const auto found = windows_.find(window);
		if (found != windows_.end()) {
			std::lock_guard<std::mutex> registryLock(found->second->mutex);
			if (found->second->activeInvocations != 0) {
				found->second->destroyRequested = true;
			} else {
				windows_.erase(found);
				releaseRecords = true;
			}
		} else {
			releaseRecords = true;
		}
	}

	// Always ask storage to release the partition. This makes cleanup safe when
	// construction failed between publishing a record and indexing it locally.
	if (storage && releaseRecords && window != InvalidWindowId) {
		storage->releaseWindowPersistentRecords(window);
	}
}

ElementDefinitionRecord& ElementStorageController::ensureDefinition(
	const element::ElementRegistrationDescriptor& descriptor) {
	return definitions_.ensureDefinition(descriptor);
}

const void* ElementStorageController::resolveOrCreateResources(
	const element::ElementRegistrationDescriptor& descriptor,
	App& app,
	bool retryFailed) {
	if (!descriptor.hasResources) return nullptr;
	requireResourcesDescriptor(descriptor);
	ElementDefinitionRecord& definition = definitions_.ensureDefinition(descriptor);
	ElementResourceSlot& resources = definition.resources;
	if (const void* ready = resources.payload.load(std::memory_order_acquire)) return ready;

	std::unique_lock<std::mutex> lock(resources.mutex);
	while (true) {
		switch (resources.state) {
		case ElementResourceState::Ready:
			if (const void* ready = resources.payload.load(std::memory_order_acquire)) {
				return ready;
			} else {
				::FlowUi::detail::terminateForFatalError(makeError(ErrorCode::ElementStorageStale, ErrorSite::ElementConstructResource));
			}
		case ElementResourceState::Constructing:
			if (resources.constructingThread == std::this_thread::get_id()) {
				throw FlowUiException(makeError(
					ErrorCode::ElementResourceRecursiveConstruction, ErrorSite::ElementConstructResource,
					descriptor.definitionId.value));
			}
			resources.ready.wait(lock, [&resources] {
				return resources.state != ElementResourceState::Constructing;
			});
			continue;
		case ElementResourceState::Failed:
			if (!retryFailed) {
				if (resources.failure) std::rethrow_exception(resources.failure);
				throw FlowUiException(makeError(
					ErrorCode::ElementResourceConstructionFailed, ErrorSite::ElementConstructResource,
					descriptor.definitionId.value));
			}
			resources.failure = nullptr;
			[[fallthrough]];
		case ElementResourceState::Empty:
			resources.state = ElementResourceState::Constructing;
			resources.constructingThread = std::this_thread::get_id();
			break;
		case ElementResourceState::Destroying:
			throw FlowUiException(makeError(ErrorCode::ElementResourceDestroying, ErrorSite::ElementConstructResource));
		}
		break;
	}
	lock.unlock();

	ResourceAllocation created{};
	try {
		if (!storage_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ElementConstructResource));
		created = createResourceRecord(*storage_, descriptor, app);
	} catch (...) {
		const std::exception_ptr focusedFailure = std::current_exception();
		lock.lock();
		resources.state = ElementResourceState::Failed;
		resources.constructingThread = {};
		resources.failure = focusedFailure;
		lock.unlock();
		resources.ready.notify_all();
		std::rethrow_exception(focusedFailure);
	}

	lock.lock();
	resources.allocation = created.block;
	resources.failure = nullptr;
	resources.constructingThread = {};
	resources.state = ElementResourceState::Ready;
	resources.payload.store(created.payload, std::memory_order_release);
	lock.unlock();
	resources.ready.notify_all();
	return created.payload;
}

ResolvedElementState ElementStorageController::resolveOrCreateStateForInvocation(
	WindowId window,
	element::ElementInstanceKey instanceKey,
	const element::ElementRegistrationDescriptor& descriptor,
	ElementStatePolicy policy) {
	requireStateDescriptor(descriptor);
	if (window == InvalidWindowId || !instanceKey) {
		throw FlowUiException(makeError(
			ErrorCode::InvalidElementId, ErrorSite::ElementConstructState,
			instanceKey.value));
	}

	std::lock_guard<std::mutex> windowsLock(windowsMutex_);
	if (!storage_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ElementConstructState));
	const auto windowEntry = windows_.find(window);
	if (windowEntry == windows_.end() || windowEntry->second->destroyRequested) {
		throw FlowUiException(makeError(ErrorCode::InvalidWindowId, ErrorSite::ElementConstructState, window));
	}

	WindowElementStateRegistry& registry = *windowEntry->second;
	std::lock_guard<std::mutex> registryLock(registry.mutex);
	if (!registry.transaction.active) {
		throw FlowUiException(makeError(ErrorCode::FramePhaseViolation, ErrorSite::ElementConstructState, window));
	}
	ResolvedElementState resolved{};
	bool created = false;
	if (const auto existing = registry.byInstance.find(instanceKey);
		existing != registry.byInstance.end()) {
		resolved = resolveExistingState(*storage_, existing->second, instanceKey, descriptor);
		if (!resolved.payload) registry.byInstance.erase(existing);
	}
	if (!resolved.payload) {
		resolved = createStateRecord(
			*storage_, window, instanceKey, descriptor,
			registry.committedFrameNumber, policy);
		try {
			registry.byInstance.emplace(instanceKey, resolved.handle);
			auto* header = static_cast<ElementStateRecordHeader*>(
				stateRecordView(*storage_, resolved.handle).header);
			if (!header) {
				::FlowUi::detail::terminateForFatalError(makeError(
					ErrorCode::ElementStorageStale, ErrorSite::ElementConstructState,
					instanceKey.value));
			}
			if (policy.retention == ElementStateRetention::Transient) {
				registry.gcCandidates.push_back(instanceKey);
				header->gcIndex = registry.gcCandidates.size() - 1;
			}
			created = true;
		} catch (...) {
			registry.byInstance.erase(instanceKey);
			if (!registry.gcCandidates.empty() && registry.gcCandidates.back() == instanceKey) {
				registry.gcCandidates.pop_back();
			}
			(void)storage_->removePersistentRecord(
				resolved.handle, storage::ResourceKind::UiElementState);
			throw;
		}
	}
	try {
		registry.transaction.touched.insert(instanceKey);
		registry.transaction.policies.insert_or_assign(instanceKey, policy);
		if (created) registry.transaction.created.insert(instanceKey);
	} catch (...) {
		if (created) {
			const auto indexed = registry.byInstance.find(instanceKey);
			if (indexed != registry.byInstance.end()) {
				if (ElementStateRecordHeader* header = stateRecordHeader(*storage_, indexed->second)) {
					removeGcCandidate(registry, *storage_, instanceKey, *header);
				}
				registry.byInstance.erase(indexed);
			}
			(void)storage_->removePersistentRecord(
				resolved.handle, storage::ResourceKind::UiElementState);
		}
		throw;
	}
	++registry.activeInvocations;
	return resolved;
}

void ElementStorageController::endStateInvocation(WindowId window) noexcept {
	bool invocationClosed = false;
	while (true) {
		storage::IStorageSystem* storage = nullptr;
		storage::PersistentRecordHandle recordToErase{};
		bool releaseWindow = false;
		try {
			std::lock_guard<std::mutex> windowsLock(windowsMutex_);
			storage = storage_;
			const auto windowEntry = windows_.find(window);
			if (windowEntry == windows_.end()) return;
			WindowElementStateRegistry& registry = *windowEntry->second;
			std::lock_guard<std::mutex> registryLock(registry.mutex);
			if (!invocationClosed) {
				if (registry.activeInvocations != 0) --registry.activeInvocations;
				invocationClosed = true;
				if (registry.activeInvocations != 0) return;
			}
			if (registry.transaction.active && !registry.destroyRequested) return;

			if (!registry.deferredErases.empty()) {
				const auto deferred = registry.deferredErases.begin();
				const auto state = registry.byInstance.find(*deferred);
				if (state != registry.byInstance.end()) {
					recordToErase = state->second;
					if (storage) {
						if (ElementStateRecordHeader* header =
								stateRecordHeader(*storage, state->second)) {
							removeGcCandidate(registry, *storage, state->first, *header);
						}
					}
					registry.byInstance.erase(state);
				}
				registry.deferredErases.erase(deferred);
			} else if (registry.destroyRequested) {
				windows_.erase(windowEntry);
				releaseWindow = true;
			} else {
				return;
			}
		} catch (...) {
			return;
		}

		if (!storage) return;
		if (releaseWindow) {
			storage->releaseWindowPersistentRecords(window);
			return;
		}
		if (recordToErase) {
			(void)storage->removePersistentRecord(
				recordToErase, storage::ResourceKind::UiElementState);
		}
	}
}

void ElementStorageController::beginWindowFrame(WindowId window, uint64_t epoch) {
	if (window == InvalidWindowId || epoch == 0) {
		throw FlowUiException(makeError(ErrorCode::FramePhaseViolation, ErrorSite::ElementBeginFrame, window));
	}
	std::lock_guard<std::mutex> windowsLock(windowsMutex_);
	if (!storage_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ElementBeginFrame));
	const auto windowEntry = windows_.find(window);
	if (windowEntry == windows_.end() || windowEntry->second->destroyRequested) {
		throw FlowUiException(makeError(ErrorCode::InvalidWindowId, ErrorSite::ElementBeginFrame, window));
	}
	WindowElementStateRegistry& registry = *windowEntry->second;
	std::lock_guard<std::mutex> registryLock(registry.mutex);
	if (registry.transaction.active || registry.activeInvocations != 0) {
		throw FlowUiException(makeError(ErrorCode::FrameAlreadyActive, ErrorSite::ElementBeginFrame, window));
	}
	if (!registry.deferredErases.empty()) {
		::FlowUi::detail::terminateForFatalError(makeError(ErrorCode::ElementStorageStale, ErrorSite::ElementBeginFrame));
	}
	registry.transaction.clear();
	registry.transaction.epoch = epoch;
	registry.transaction.active = true;
}

void ElementStorageController::commitWindowFrame(WindowId window, uint64_t epoch) noexcept {
	try {
		std::lock_guard<std::mutex> windowsLock(windowsMutex_);
		if (!storage_) return;
		const auto windowEntry = windows_.find(window);
		if (windowEntry == windows_.end()) return;
		WindowElementStateRegistry& registry = *windowEntry->second;
		std::lock_guard<std::mutex> registryLock(registry.mutex);
		if (!registry.transaction.active || registry.transaction.epoch != epoch) return;
		if (registry.activeInvocations != 0) return;

		const uint64_t committedFrame =
			registry.committedFrameNumber == std::numeric_limits<uint64_t>::max()
			? registry.committedFrameNumber
			: registry.committedFrameNumber + 1;
		for (const element::ElementInstanceKey instanceKey : registry.transaction.touched) {
			const auto state = registry.byInstance.find(instanceKey);
			if (state == registry.byInstance.end()) continue;
			ElementStateRecordHeader* header = stateRecordHeader(*storage_, state->second);
			if (!header) continue;
			header->lastSeenCommittedFrame = committedFrame;
			if (const auto policy = registry.transaction.policies.find(instanceKey);
				policy != registry.transaction.policies.end()) {
				const ElementStatePolicy nextPolicy = policy->second;
				if (header->policy.retention != nextPolicy.retention) {
					if (nextPolicy.retention == ElementStateRetention::WindowLifetime) {
						removeGcCandidate(registry, *storage_, instanceKey, *header);
					} else {
						// A failed bookkeeping allocation leaves the safer retained policy
						// in place without invalidating an otherwise successful UI frame.
						try {
							registry.gcCandidates.push_back(instanceKey);
							header->gcIndex = registry.gcCandidates.size() - 1;
						} catch (...) {
							continue;
						}
					}
				}
				header->policy = nextPolicy;
			}
		}
		registry.committedFrameNumber = committedFrame;
		registry.transaction.clear();
	} catch (...) {
		return;
	}

	// The transaction is now closed and no callback can hold a cached pointer.
	// Drain explicit erasures before performing bounded GC maintenance.
	endStateInvocation(window);
	(void)collectEligibleStates(window, 256);
}

void ElementStorageController::cancelWindowFrame(WindowId window, uint64_t epoch) noexcept {
	bool transactionClosed = false;
	while (true) {
		storage::IStorageSystem* storage = nullptr;
		storage::PersistentRecordHandle recordToErase{};
		try {
			std::lock_guard<std::mutex> windowsLock(windowsMutex_);
			storage = storage_;
			const auto windowEntry = windows_.find(window);
			if (windowEntry == windows_.end()) return;
			WindowElementStateRegistry& registry = *windowEntry->second;
			std::lock_guard<std::mutex> registryLock(registry.mutex);
			if (!transactionClosed) {
				if (!registry.transaction.active || registry.transaction.epoch != epoch) return;
				registry.transaction.active = false;
				registry.deferredErases.clear();
				transactionClosed = true;
			}

			if (registry.transaction.created.empty()) {
				registry.transaction.clear();
				return;
			}
			const auto created = registry.transaction.created.begin();
			const element::ElementInstanceKey instanceKey = *created;
			registry.transaction.created.erase(created);
			const auto state = registry.byInstance.find(instanceKey);
			if (state != registry.byInstance.end()) {
				recordToErase = state->second;
				if (storage) {
					if (ElementStateRecordHeader* header =
							stateRecordHeader(*storage, state->second)) {
						removeGcCandidate(registry, *storage, instanceKey, *header);
					}
				}
				registry.byInstance.erase(state);
			}
		} catch (...) {
			return;
		}

		if (storage && recordToErase) {
			(void)storage->removePersistentRecord(
				recordToErase, storage::ResourceKind::UiElementState);
		}
	}
}

size_t ElementStorageController::collectEligibleStates(
	WindowId window,
	size_t scanBudget) noexcept {
	if (scanBudget == 0) return 0;
	constexpr size_t kRemovalBatchSize = 256;
	size_t removedCount = 0;
	size_t remaining = scanBudget;
	bool initializeUnlimitedBudget =
		scanBudget == std::numeric_limits<size_t>::max();
	while (true) {
		std::array<storage::PersistentRecordHandle, kRemovalBatchSize> removals{};
		size_t removalCount = 0;
		size_t inspected = 0;
		storage::IStorageSystem* storage = nullptr;
		try {
			std::lock_guard<std::mutex> windowsLock(windowsMutex_);
			storage = storage_;
			if (!storage) return removedCount;
			const auto windowEntry = windows_.find(window);
			if (windowEntry == windows_.end()) return removedCount;
			WindowElementStateRegistry& registry = *windowEntry->second;
			std::lock_guard<std::mutex> registryLock(registry.mutex);
			if (registry.transaction.active || registry.activeInvocations != 0 ||
				registry.gcCandidates.empty()) {
				return removedCount;
			}
			if (initializeUnlimitedBudget) {
				remaining = registry.gcCandidates.size();
				initializeUnlimitedBudget = false;
			}

			const size_t batchBudget = std::min({
				remaining,
				kRemovalBatchSize,
				registry.gcCandidates.size(),
			});
			while (inspected < batchBudget && !registry.gcCandidates.empty()) {
				if (registry.gcCursor >= registry.gcCandidates.size()) registry.gcCursor = 0;
				const size_t candidateIndex = registry.gcCursor;
				const element::ElementInstanceKey instanceKey = registry.gcCandidates[candidateIndex];
				const auto state = registry.byInstance.find(instanceKey);
				ElementStateRecordHeader* header = state == registry.byInstance.end()
					? nullptr
					: stateRecordHeader(*storage, state->second);
				++inspected;
				if (!header || isStateExpired(*header, registry.committedFrameNumber)) {
					if (state != registry.byInstance.end()) {
						removals[removalCount++] = state->second;
						registry.byInstance.erase(state);
					}
					removeGcCandidateAt(registry, *storage, candidateIndex);
					continue;
				}
				++registry.gcCursor;
			}
		} catch (...) {
			return removedCount;
		}

		for (size_t index = 0; index < removalCount; ++index) {
			if (storage->removePersistentRecord(
					removals[index], storage::ResourceKind::UiElementState)) {
				++removedCount;
			}
		}
		remaining -= std::min(remaining, inspected);
		if (remaining == 0 || inspected == 0) return removedCount;
	}
}

const void* ElementStorageController::readState(
	WindowId window,
	element::ElementInstanceKey instanceKey,
	const element::ElementRegistrationDescriptor& descriptor) {
	requireStateDescriptor(descriptor);
	if (window == InvalidWindowId || !instanceKey) return nullptr;
	std::lock_guard<std::mutex> windowsLock(windowsMutex_);
	if (!storage_) return nullptr;
	const auto windowEntry = windows_.find(window);
	if (windowEntry == windows_.end() || windowEntry->second->destroyRequested) return nullptr;
	WindowElementStateRegistry& registry = *windowEntry->second;
	std::lock_guard<std::mutex> registryLock(registry.mutex);
	const auto existing = registry.byInstance.find(instanceKey);
	if (existing == registry.byInstance.end()) return nullptr;
	const ResolvedElementState resolved =
		resolveExistingState(*storage_, existing->second, instanceKey, descriptor);
	if (!resolved.payload) registry.byInstance.erase(existing);
	return resolved.payload;
}

void* ElementStorageController::modifyState(
	WindowId window,
	element::ElementInstanceKey instanceKey,
	const element::ElementRegistrationDescriptor& descriptor) {
	return const_cast<void*>(readState(window, instanceKey, descriptor));
}

bool ElementStorageController::eraseState(
	WindowId window,
	element::ElementInstanceKey instanceKey,
	const element::ElementRegistrationDescriptor& descriptor) {
	requireStateDescriptor(descriptor);
	if (window == InvalidWindowId || !instanceKey) return false;
	storage::IStorageSystem* storage = nullptr;
	storage::PersistentRecordHandle handle{};
	{
		std::lock_guard<std::mutex> windowsLock(windowsMutex_);
		storage = storage_;
		if (!storage) return false;
		const auto windowEntry = windows_.find(window);
		if (windowEntry == windows_.end() || windowEntry->second->destroyRequested) return false;
		WindowElementStateRegistry& registry = *windowEntry->second;
		std::lock_guard<std::mutex> registryLock(registry.mutex);
		const auto existing = registry.byInstance.find(instanceKey);
		if (existing == registry.byInstance.end()) return false;
		(void)resolveExistingState(*storage, existing->second, instanceKey, descriptor);
		if (registry.transaction.active || registry.activeInvocations != 0) {
			registry.deferredErases.insert(instanceKey);
			return true;
		}
		handle = existing->second;
		if (ElementStateRecordHeader* header = stateRecordHeader(*storage, handle)) {
			removeGcCandidate(registry, *storage, instanceKey, *header);
		}
		registry.byInstance.erase(existing);
	}
	return storage->removePersistentRecord(handle, storage::ResourceKind::UiElementState);
}

void ElementStorageController::shutdown() noexcept {
	storage::IStorageSystem* storage = nullptr;
	{
		std::lock_guard<std::mutex> lock(windowsMutex_);
		storage = storage_;
		storage_ = nullptr;
	}
	if (!storage) return;

	while (true) {
		WindowId window = InvalidWindowId;
		std::shared_ptr<WindowElementStateRegistry> registry;
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

	// Resource destructors run while App-owned image/icon/font/theme managers and
	// the storage system are still alive. Handles are detached from definition
	// slots before invoking user destruction callbacks through storage.
	while (true) {
		const storage::MemoryBlock resource =
			definitions_.takeReadyResourceForDestroy();
		if (!resource) break;
		destroyResourceAllocation(*storage, resource);
	}
	definitions_.clear();
}

} // namespace FlowUi::detail::manager_storage
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
namespace FlowUi::detail::manager_storage {
void ElementStorageController::appendDevMemorySamples(
	::FlowUi::devSystems::MemorySampleSink& sink) const noexcept {
	try {
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.objectCount += definitions_.size();
		memory.liveBytes += definitions_.size() * sizeof(ElementDefinitionRecord);
		memory.capacityBytes += memory.liveBytes;
		std::scoped_lock windowsLock(windowsMutex_);
		memory.addNodeContainer(windows_);
		for (const auto& [_, window] : windows_) {
			if (!window) continue;
			std::scoped_lock windowLock(window->mutex);
			memory.addNodeContainer(window->byInstance);
			memory.addNodeContainer(window->deferredErases);
			memory.add(window->gcCandidates);
			memory.addNodeContainer(window->transaction.touched);
			memory.addNodeContainer(window->transaction.created);
			memory.addNodeContainer(window->transaction.policies);
		}
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kElements.id, memory);
	} catch (...) {}
}
} // namespace FlowUi::detail::manager_storage
#endif
