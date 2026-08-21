#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <vector>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::detail::storage { class IStorageSystem; }
struct VulkanContext;

namespace FlowUi::devSystems {

class DevMemory {
public:
	explicit DevMemory(DevMemoryConfig config = {});
	~DevMemory();

	DevMemory(const DevMemory&) = delete;
	DevMemory& operator=(const DevMemory&) = delete;

	void setConfig(const DevMemoryConfig& config) noexcept;
	[[nodiscard]] DevMemoryConfig config() const noexcept;
	[[nodiscard]] MemoryMonitoringLevel compiledLevel() const noexcept;

	[[nodiscard]] bool registerSource(const StaticMemorySourceDescriptor& descriptor);
	[[nodiscard]] bool registerTuningTarget(const MemoryTuningTargetDescriptor& descriptor);
	[[nodiscard]] bool registerProbe(const RegisteredMemoryProbe& probe);
	void unregisterProbe(MemorySourceId source, const void* owner) noexcept;

	[[nodiscard]] std::vector<MemorySourceDescriptor> descriptorSnapshot() const;
	[[nodiscard]] std::vector<MemoryTuningTargetDescriptor> tuningTargetSnapshot() const;
	[[nodiscard]] std::vector<RegisteredMemoryProbe> probeSnapshot() const;
	[[nodiscard]] DevMemoryRecorder& recorder() noexcept;
	[[nodiscard]] const DevMemoryRecorder& recorder() const noexcept;
	[[nodiscard]] MemoryQualitySnapshot qualitySnapshot() const noexcept;
	void setStorageSystem(::FlowUi::detail::storage::IStorageSystem* storage) noexcept;
	[[nodiscard]] bool appendStorageSnapshot(
		const ::FlowUi::detail::storage::StorageMemorySnapshotRequest& request,
		::FlowUi::detail::storage::StorageMemorySnapshot& destination) const noexcept;
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	void initializeEnvironmentProbes(const ::VulkanContext& context) noexcept;
	void detachEnvironmentProbes() noexcept;
	void advanceGpuFrameIndex(uint32_t frameIndex) noexcept;
	[[nodiscard]] MemoryEnvironmentSnapshot sampleEnvironment(
		uint64_t nowNs,
		uint64_t safelyAttributableFlowUiCpuBytes,
		bool force = false) noexcept;
#endif

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#endif
