#include "FlowUi/Private/storageSystem/StorageRecordAccess.hpp"
#include "FlowUi/Private/storageSystem/StorageBinding.hpp"
#include <utility>

namespace FlowUi::detail::storage {
StringId StorageRecordAccess::intern(std::string_view value) {
	return binding_->system().intern(value);
}
uint64_t StorageRecordAccess::initial_memory_capacity(
	FlowUi::detail::InitialCapacityTarget target) const noexcept {
	return std::as_const(*binding_).system().initial_memory_capacity(target);
}
ManagerRecordHandle StorageRecordAccess::createManagerRecord(const ManagerRecordDesc& desc) {
	return binding_->system().createManagerRecord(desc);
}
void* StorageRecordAccess::managerRecordData(ManagerRecordHandle handle,
											 ResourceKind kind) noexcept {
	return binding_->system().managerRecordData(handle, kind);
}
const void* StorageRecordAccess::managerRecordData(ManagerRecordHandle handle,
												   ResourceKind kind) const noexcept {
	return std::as_const(*binding_).system().managerRecordData(handle, kind);
}
PersistentRecordHandle
StorageRecordAccess::createPersistentRecord(const PersistentRecordDesc& desc) {
	return binding_->system().createPersistentRecord(desc);
}
PersistentRecordView StorageRecordAccess::persistentRecord(PersistentRecordHandle handle,
														   ResourceKind kind) noexcept {
	return binding_->system().persistentRecord(handle, kind);
}
ConstPersistentRecordView StorageRecordAccess::persistentRecord(PersistentRecordHandle handle,
																ResourceKind kind) const noexcept {
	return std::as_const(*binding_).system().persistentRecord(handle, kind);
}
bool StorageRecordAccess::removePersistentRecord(PersistentRecordHandle handle,
												 ResourceKind kind) noexcept {
	return binding_->system().removePersistentRecord(handle, kind);
}
void StorageRecordAccess::noteManagerMutation(WindowId window) {
	binding_->system().noteManagerMutation(window);
}
} // namespace FlowUi::detail::storage
