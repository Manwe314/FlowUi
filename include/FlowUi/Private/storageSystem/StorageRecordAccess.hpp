#pragma once

#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#include <string_view>

namespace FlowUi::detail::storage {
class StorageBinding;

/** Fixed record primitives for application-instantiated manager templates. */
class StorageRecordAccess {
public:
	/** Borrow the app binding for the duration of a synchronous record operation. */
	explicit StorageRecordAccess(StorageBinding& binding) noexcept : binding_(&binding) {}
	/** Intern a stable storage string identity. */
	[[nodiscard]] StringId intern(std::string_view value);
	/** Observe the configured startup reserve. */
	[[nodiscard]] uint64_t
	initial_memory_capacity(FlowUi::detail::InitialCapacityTarget target) const noexcept;
	/** Synchronously construct a manager record. */
	[[nodiscard]] ManagerRecordHandle createManagerRecord(const ManagerRecordDesc& desc);
	/** Resolve a mutable record after kind and generation validation. */
	[[nodiscard]] void* managerRecordData(ManagerRecordHandle handle, ResourceKind kind) noexcept;
	/** Resolve a const record after kind and generation validation. */
	[[nodiscard]] const void* managerRecordData(ManagerRecordHandle handle,
												ResourceKind kind) const noexcept;
	/** Synchronously construct an aligned persistent record. */
	[[nodiscard]] PersistentRecordHandle createPersistentRecord(const PersistentRecordDesc& desc);
	/** Resolve a mutable persistent record. */
	[[nodiscard]] PersistentRecordView persistentRecord(PersistentRecordHandle handle,
														ResourceKind kind) noexcept;
	/** Resolve a const persistent record. */
	[[nodiscard]] ConstPersistentRecordView persistentRecord(PersistentRecordHandle handle,
															 ResourceKind kind) const noexcept;
	/** Remove and destroy a persistent record exactly once. */
	[[nodiscard]] bool removePersistentRecord(PersistentRecordHandle handle,
											  ResourceKind kind) noexcept;
	/** Validate and publish a manager mutation revision. */
	void noteManagerMutation(WindowId window);

private:
	StorageBinding* binding_;
};
} // namespace FlowUi::detail::storage
