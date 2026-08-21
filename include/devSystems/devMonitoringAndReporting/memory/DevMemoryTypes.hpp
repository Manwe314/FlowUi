#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/WindowId.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

namespace FlowUi::devSystems {

enum class MemoryMonitoringLevel : uint8_t {
	Disabled = 0,
	StorageSummary = 1,
	SubsystemCapacity = 2,
	DetailedLifetimes = 3,
	DeepAllocations = 4,
};

enum class MemoryDomain : uint8_t {
	StorageCpu = 0,
	ManagerCpu,
	TransientCpu,
	Process,
	GpuAllocation,
	VulkanHeap,
};

enum class MemoryDisposition : uint8_t {
	LogicalLive = 0,
	Reusable,
	Reserved,
	Committed,
	Resident,
	Retired,
	ExternalEstimate,
};

enum class MemorySourceKind : uint8_t {
	Allocator = 0,
	Arena,
	Container,
	Resource,
	Manager,
	Process,
	GpuHeap,
	Development,
};

enum class MemoryAccuracy : uint8_t {
	Exact = 0,
	AllocatorRequested,
	Estimate,
	Unavailable,
};

enum class MemoryOperation : uint8_t {
	LogicalAllocate = 0,
	LogicalRelease,
	ArenaReset,
	PoolBlockMerge,
	BackingGrow,
	BackingTrim,
	ContainerReserve,
	ContainerReallocate,
	ContainerRehash,
	GpuCreate,
	GpuRetire,
	GpuDestroy,
	ExternalAcquire,
	ExternalRelease,
};

enum class MemorySampleFlag : uint16_t {
	None = 0,
	Estimate = 1u << 0u,
	Incomplete = 1u << 1u,
	Unsupported = 1u << 2u,
	DroppedDetail = 1u << 3u,
	SharedPhysicalMemory = 1u << 4u,
	OverlapsProcessResident = 1u << 5u,
};

[[nodiscard]] constexpr MemorySampleFlag operator|(
	MemorySampleFlag left,
	MemorySampleFlag right) noexcept {
	return static_cast<MemorySampleFlag>(
		static_cast<uint16_t>(left) | static_cast<uint16_t>(right));
}

using MemorySourceId = uint64_t;
using MemoryLifetimeId = uint64_t;
using MemoryTuningTargetId = uint64_t;

enum class MemoryTuningMetric : uint8_t {
	LogicalLiveBytes = 0,
	BackingAllocatedBytes,
	PeakLogicalBytes,
	ObjectCount,
	CapacityCount,
};

enum class MemoryCapacityUnit : uint8_t {
	Bytes = 0,
	Entries,
	Bindings,
	Pages,
};

enum class MemoryApplyPolicy : uint8_t {
	LiveSafe = 0,
	RebuildRequired,
	RestartRequired,
};

struct MemorySourceDescriptor {
	MemorySourceId id = 0u;
	MemorySourceId parent = 0u;
	MemoryDomain domain = MemoryDomain::StorageCpu;
	MemorySourceKind kind = MemorySourceKind::Allocator;
	std::string name{};
	MemoryAccuracy accuracy = MemoryAccuracy::Exact;
	MemoryTuningTargetId tuningTarget = 0u;
};

struct StaticMemorySourceDescriptor {
	MemorySourceId id = 0u;
	MemorySourceId parent = 0u;
	MemoryDomain domain = MemoryDomain::StorageCpu;
	MemorySourceKind kind = MemorySourceKind::Allocator;
	std::string_view name{};
	MemoryAccuracy accuracy = MemoryAccuracy::Exact;
	MemoryTuningTargetId tuningTarget = 0u;
};

struct MemoryTuningTargetDescriptor {
	MemoryTuningTargetId id = 0u;
	MemorySourceId source = 0u;
	MemoryTuningMetric metric = MemoryTuningMetric::BackingAllocatedBytes;
	MemoryCapacityUnit unit = MemoryCapacityUnit::Bytes;
	uint64_t minimum = 0u;
	uint64_t maximum = UINT64_MAX;
	uint64_t alignment = 1u;
	uint64_t productionDefault = 0u;
	std::string configKey{};
	MemoryApplyPolicy applyPolicy = MemoryApplyPolicy::RestartRequired;
};

struct MemoryValueSample {
	MemorySourceId source = 0u;
	WindowId window = InvalidWindowId;
	uint64_t logicalLiveBytes = 0u;
	uint64_t retiredBytes = 0u;
	uint64_t reusableBytes = 0u;
	uint64_t backingAllocatedBytes = 0u;
	uint64_t peakLogicalBytes = 0u;
	uint64_t peakBackingAllocatedBytes = 0u;
	uint64_t peakObjectCount = 0u;
	uint64_t objectCount = 0u;
	uint64_t capacityCount = 0u;
	uint64_t allocationOps = 0u;
	uint64_t logicalReleaseOps = 0u;
	uint64_t physicalReleaseOps = 0u;
	uint64_t growthOps = 0u;
	MemorySampleFlag flags = MemorySampleFlag::None;
};

struct MemoryOperationRecord {
	MemorySourceId source = 0u;
	MemoryLifetimeId lifetime = 0u;
	MemoryOperation operation = MemoryOperation::LogicalAllocate;
	MemoryMonitoringLevel detailLevel = MemoryMonitoringLevel::StorageSummary;
	AppTickId appTick = 0u;
	WindowFrameKey frame{};
	uint64_t submissionSerial = 0u;
	uint64_t timestampNs = 0u;
	uint64_t bytesBefore = 0u;
	uint64_t bytesAfter = 0u;
	uint64_t bytesChanged = 0u;
	uint32_t threadTrack = 0u;
	uint32_t debugName = 0u;
};

struct DevMemoryConfig {
	MemoryMonitoringLevel level = MemoryMonitoringLevel::SubsystemCapacity;
	uint32_t producerEventCapacity = 8192u;
	bool gpuMemory = true;
	bool processMemory = true;
	bool detailedVmaStatistics = false;
	bool trackTemporaryResources = true;
	uint64_t processSampleIntervalNs = 500'000'000ull;
	uint64_t gpuBudgetSampleIntervalNs = 250'000'000ull;
	uint32_t detailedGpuStatsEverySamples = 0u;
};

enum class ProcessMemoryFieldBit : uint32_t {
	VirtualBytes = 1u << 0u,
	ResidentBytes = 1u << 1u,
	PeakResidentBytes = 1u << 2u,
	PrivateBytes = 1u << 3u,
	SharedBytes = 1u << 4u,
	PageFaultCount = 1u << 5u,
};

struct ProcessMemorySnapshot {
	uint64_t capturedAtNs = 0u;
	uint64_t virtualBytes = 0u;
	uint64_t residentBytes = 0u;
	uint64_t peakResidentBytes = 0u;
	uint64_t privateBytes = 0u;
	uint64_t sharedBytes = 0u;
	uint64_t pageFaultCount = 0u;
	uint64_t sampleDurationNs = 0u;
	uint32_t availableFields = 0u;
	MemoryAccuracy accuracy = MemoryAccuracy::Unavailable;
};

enum class GpuHeapClass : uint8_t {
	DedicatedLike = 0,
	SharedLike,
	HostLike,
	Unknown,
};

struct GpuHeapMemorySample {
	uint32_t heapIndex = 0u;
	uint32_t heapFlags = 0u;
	uint64_t heapSizeBytes = 0u;
	uint64_t allocatorUsageBytes = 0u;
	uint64_t allocatorBudgetBytes = 0u;
	uint64_t allocationBytes = 0u;
	uint64_t blockBytes = 0u;
	uint32_t allocationCount = 0u;
	uint32_t blockCount = 0u;
	GpuHeapClass classification = GpuHeapClass::Unknown;
	MemorySampleFlag flags = MemorySampleFlag::None;
};

struct GpuMemorySnapshot {
	uint64_t capturedAtNs = 0u;
	uint64_t sampleDurationNs = 0u;
	uint64_t sampleSequence = 0u;
	std::vector<GpuHeapMemorySample> heaps{};
	bool memoryBudgetExtensionEnabled = false;
	bool detailedStatisticsIncluded = false;
	bool available = false;
};

struct MemoryEnvironmentSnapshot {
	ProcessMemorySnapshot process{};
	GpuMemorySnapshot gpu{};
	uint64_t safelyAttributableFlowUiCpuBytes = 0u;
	uint64_t unattributedResidentResidual = 0u;
	int64_t signedResidentDiscrepancy = 0;
	MemorySampleFlag residualFlags = MemorySampleFlag::Estimate;
};

struct MemoryQualitySnapshot {
	uint64_t recordedOperations = 0u;
	uint64_t suppressedOperations = 0u;
	uint64_t droppedOperations = 0u;
	uint64_t sourceCollisions = 0u;
	uint64_t unknownSources = 0u;
};

namespace detail::dev_memory {

inline constexpr uint64_t kHashOffset = 14695981039346656037ull;
inline constexpr uint64_t kHashPrime = 1099511628211ull;

[[nodiscard]] consteval uint64_t hashBytes(std::string_view value, uint64_t hash = kHashOffset) {
	for (const char character : value) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(character));
		hash *= kHashPrime;
	}
	return hash;
}

} // namespace detail::dev_memory

[[nodiscard]] consteval MemorySourceId makeMemorySourceId(std::string_view stableName) {
	const uint64_t hash = detail::dev_memory::hashBytes(stableName);
	return hash == 0u ? 1u : hash;
}

[[nodiscard]] consteval StaticMemorySourceDescriptor makeMemorySourceDescriptor(
	std::string_view stableName,
	MemoryDomain domain,
	MemorySourceKind kind,
	MemoryAccuracy accuracy = MemoryAccuracy::Exact,
	MemorySourceId parent = 0u,
	MemoryTuningTargetId tuningTarget = 0u) {
	return StaticMemorySourceDescriptor{
		.id = makeMemorySourceId(stableName),
		.parent = parent,
		.domain = domain,
		.kind = kind,
		.name = stableName,
		.accuracy = accuracy,
		.tuningTarget = tuningTarget,
	};
}

} // namespace FlowUi::devSystems

#endif
