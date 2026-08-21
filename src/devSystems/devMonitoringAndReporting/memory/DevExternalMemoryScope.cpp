#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2

namespace FlowUi::devSystems {
namespace {

uint64_t nowNs() noexcept {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace

DevExternalMemoryScope::DevExternalMemoryScope(
	DevMemoryRecorder* recorder,
	MemorySourceId source,
	uint64_t bytes,
	MemoryLifetimeId lifetime) noexcept
	: recorder_(recorder), source_(source), lifetime_(lifetime), bytes_(bytes) {
	if (!recorder_ || source_ == 0u || bytes_ == 0u) return;
	(void)recorder_->tryRecord(MemoryOperationRecord{
		.source = source_, .lifetime = lifetime_, .operation = MemoryOperation::ExternalAcquire,
		.detailLevel = MemoryMonitoringLevel::SubsystemCapacity,
		.timestampNs = nowNs(), .bytesAfter = bytes_, .bytesChanged = bytes_,
	});
}

DevExternalMemoryScope::~DevExternalMemoryScope() noexcept {
	if (!recorder_ || source_ == 0u || bytes_ == 0u) return;
	(void)recorder_->tryRecord(MemoryOperationRecord{
		.source = source_, .lifetime = lifetime_, .operation = MemoryOperation::ExternalRelease,
		.detailLevel = MemoryMonitoringLevel::SubsystemCapacity,
		.timestampNs = nowNs(), .bytesBefore = bytes_, .bytesChanged = bytes_,
	});
}

DevExternalMemoryScope::DevExternalMemoryScope(DevExternalMemoryScope&& other) noexcept
	: recorder_(other.recorder_), source_(other.source_), lifetime_(other.lifetime_), bytes_(other.bytes_) {
	other.recorder_ = nullptr;
}

} // namespace FlowUi::devSystems

#endif
