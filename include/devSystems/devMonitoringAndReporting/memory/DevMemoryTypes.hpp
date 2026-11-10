#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "devSystems/DevConfig.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

namespace FlowUi::devSystems {

/** Accounting domain; process and heap measurements may overlap subsystem totals. */
enum class MemoryDomain : uint8_t {
	StorageCpu = 0, /**< FlowUi CPU storage. */
	ManagerCpu, /**< Manager-owned CPU allocations. */
	TransientCpu, /**< Temporary CPU allocations. */
	Process, /**< Whole-process virtual/resident memory. */
	GpuAllocation, /**< Tracked GPU resource allocations. */
	VulkanHeap, /**< Vulkan/VMA heap-wide usage and budget. */
};

/** Accounting interpretation of a byte quantity. */
enum class MemoryDisposition : uint8_t {
	LogicalLive = 0, /**< Bytes currently required by live logical objects. */
	Reusable, /**< Allocated bytes available for reuse. */
	Reserved, /**< Address space or capacity reserved for future use. */
	Committed, /**< Committed backing storage. */
	Resident, /**< Physically resident process pages. */
	Retired, /**< Released logically but awaiting safe physical reclamation. */
	ExternalEstimate, /**< Estimated bytes owned outside FlowUi storage. */
};

/** Owner category used to organize memory sources. */
enum class MemorySourceKind : uint8_t {
	Allocator = 0, /**< Storage allocator or pool. */
	Arena, /**< Resettable arena. */
	Container, /**< Container backing allocation. */
	Resource, /**< Resource payload. */
	Manager, /**< Manager-level aggregate. */
	Process, /**< Whole-process source. */
	GpuHeap, /**< GPU heap aggregate. */
	Development, /**< Developer instrumentation or tooling overhead. */
};

/** Accuracy of the source accounting method. */
enum class MemoryAccuracy : uint8_t {
	Exact = 0, /**< Exact logical/accounted quantity. */
	AllocatorRequested, /**< Allocator-requested size; physical backing may differ. */
	Estimate, /**< Estimated backing or payload size. */
	Unavailable, /**< The quantity could not be measured. */
};

/** Logical or backing-memory transition recorded by a producer. */
enum class MemoryOperation : uint8_t {
	LogicalAllocate = 0, /**< Acquire logical live storage. */
	LogicalRelease, /**< Release logical storage for reuse or retirement. */
	ArenaReset, /**< Reset an arena and release its logical allocations. */
	PoolBlockMerge, /**< Merge reusable pool blocks. */
	BackingGrow, /**< Increase allocated backing capacity. */
	BackingTrim, /**< Release unused backing capacity. */
	ContainerReserve, /**< Reserve container backing without a logical object insertion. */
	ContainerReallocate, /**< Move container contents into replacement backing. */
	ContainerRehash, /**< Rebuild associative-container buckets. */
	GpuCreate, /**< Create a GPU allocation/resource. */
	GpuRetire, /**< Retire a GPU resource pending safe destruction. */
	GpuDestroy, /**< Physically destroy a GPU allocation/resource. */
	ExternalAcquire, /**< Begin tracking externally owned bytes. */
	ExternalRelease, /**< Stop tracking externally owned bytes. */
};

/** Bit flags qualifying measurement completeness, accuracy, overlap, and aggregation. */
enum class MemorySampleFlag : uint16_t {
	None = 0, /**< No qualifiers. */
	Estimate = 1u << 0u, /**< One or more values are estimates. */
	Incomplete = 1u << 1u, /**< Measurement coverage is incomplete. */
	Unsupported = 1u << 2u, /**< The backend cannot supply this measurement. */
	DroppedDetail = 1u << 3u, /**< Producer detail was dropped. */
	SharedPhysicalMemory = 1u << 4u, /**< The source can share physical memory with another domain. */
	OverlapsProcessResident = 1u << 5u, /**< The source overlaps whole-process resident counters. */
	TuningAggregate = 1u << 6u, /**< Diagnostic capacity demand that must not be added as independent backing. */
};

/** Combine independent measurement qualifier bits. */
[[nodiscard]] constexpr MemorySampleFlag operator|(
	MemorySampleFlag left,
	MemorySampleFlag right) noexcept {
	return static_cast<MemorySampleFlag>(
		static_cast<uint16_t>(left) | static_cast<uint16_t>(right));
}

/** Stable memory-source identity; zero means no source. */
using MemorySourceId = uint64_t;
/** Optional identity correlating acquire/release events for one resource lifetime. */
using MemoryLifetimeId = uint64_t;
/** Stable configurable capacity target identity. */
using MemoryTuningTargetId = uint64_t;

/** Measured demand used to recommend a target capacity. */
enum class MemoryTuningMetric : uint8_t {
	LogicalLiveBytes = 0, /**< Current live payload bytes. */
	BackingAllocatedBytes, /**< Current allocated backing bytes. */
	PeakLogicalBytes, /**< Observed peak live payload bytes. */
	ObjectCount, /**< Current/peak live entry demand. */
	CapacityCount, /**< Reserved entry capacity. */
};

/** Unit for a configurable capacity target. */
enum class MemoryCapacityUnit : uint8_t {
	Bytes = 0, /**< Byte count. */
	Entries, /**< Container entry count. */
	Bindings, /**< Descriptor/binding count. */
	Pages, /**< Atlas/storage page count. */
};

/** Earliest safe application point for a recommended capacity change. */
enum class MemoryApplyPolicy : uint8_t {
	LiveSafe = 0, /**< May be applied while the application runs. */
	RebuildRequired, /**< Requires rebuilding affected resources. */
	RestartRequired, /**< Requires application restart. */
};

/**
 * Owned source metadata copied from a registration; parentage and tuningTarget support grouping
 * without adding child bytes twice.
 */
struct MemorySourceDescriptor {
	/** Owned source name. */
	std::string name{};
	/** Stable source identity; zero is invalid. */
	MemorySourceId id = 0u;
	/** Parent source for hierarchical grouping; zero is a root. */
	MemorySourceId parent = 0u;
	/** Associated configurable capacity target, or zero if none. */
	MemoryTuningTargetId tuningTarget = 0u;
	/** Accounting domain used for grouping and overlap handling. */
	MemoryDomain domain = MemoryDomain::StorageCpu;
	/** Owning subsystem/container category. */
	MemorySourceKind kind = MemorySourceKind::Allocator;
	/** Accuracy expected from the accounting method. */
	MemoryAccuracy accuracy = MemoryAccuracy::Exact;
};

/**
 * Allocation-free source registration descriptor with a borrowed static name.
 * DevMemory::registerSource() copies the name before returning.
 */
struct StaticMemorySourceDescriptor {
	/** Borrowed stable source name, normally backed by a string literal. */
	std::string_view name{};
	/** Stable source identity; zero is invalid. */
	MemorySourceId id = 0u;
	/** Parent source for hierarchical grouping; zero is a root. */
	MemorySourceId parent = 0u;
	/** Associated configurable capacity target, or zero if none. */
	MemoryTuningTargetId tuningTarget = 0u;
	/** Accounting domain used for grouping and overlap handling. */
	MemoryDomain domain = MemoryDomain::StorageCpu;
	/** Owning subsystem/container category. */
	MemorySourceKind kind = MemorySourceKind::Allocator;
	/** Accuracy expected from the accounting method. */
	MemoryAccuracy accuracy = MemoryAccuracy::Exact;
};

/**
 * Owned mapping from measured source demand to a configurable capacity, including units, validation
 * bounds, and when changes can apply.
 */
struct MemoryTuningTargetDescriptor {
	/** Owned configuration key associated with this capacity target. */
	std::string configKey{};
	/** Stable nonzero capacity-target identity. */
	MemoryTuningTargetId id = 0u;
	/** Source whose measurements or operation this record describes. */
	MemorySourceId source = 0u;
	/** Minimum accepted capacity in unit. */
	uint64_t minimum = 0u;
	/** Maximum accepted capacity in unit. */
	uint64_t maximum = UINT64_MAX;
	/** Required positive capacity alignment in unit. */
	uint64_t alignment = 1u;
	/** Library default capacity in unit. */
	uint64_t productionDefault = 0u;
	/** Demand metric used for recommendations. */
	MemoryTuningMetric metric = MemoryTuningMetric::BackingAllocatedBytes;
	/** Capacity unit shared by bounds, default, and recommendations. */
	MemoryCapacityUnit unit = MemoryCapacityUnit::Bytes;
	/** Safe application point for a changed capacity. */
	MemoryApplyPolicy applyPolicy = MemoryApplyPolicy::RestartRequired;
};

/** Declared sampled metrics. Unpopulated fields must not be treated as measured zero. */
enum class MemoryValueField : uint16_t {
	LogicalLive = 1u, /**< logicalLiveBytes is measured. */
	Backing = 2u, /**< backingAllocatedBytes is measured. */
	Reusable = 4u, /**< reusableBytes is measured. */
	Retired = 8u, /**< retiredBytes is measured. */
	PeakLogical = 16u, /**< peakLogicalBytes is measured. */
	Objects = 32u, /**< objectCount is measured. */
	Capacity = 64u, /**< capacityCount is measured. */
};
/**
 * Measured source values for one window or app-wide scope. value_fields distinguishes unavailable
 * metrics from measured zeros; flags qualify accuracy and overlap.
 */
struct MemoryValueSample {
	/** Source whose measurements or operation this record describes. */
	MemorySourceId source = 0u;
	/** Owning window, or zero for app-wide attribution. */
	WindowId window = InvalidWindowId;
	/** Current logical live payload bytes. */
	uint64_t logicalLiveBytes = 0u;
	/** Bytes awaiting safe physical reclamation. */
	uint64_t retiredBytes = 0u;
	/** Allocated backing bytes currently available for reuse. */
	uint64_t reusableBytes = 0u;
	/** Current total allocated backing bytes. */
	uint64_t backingAllocatedBytes = 0u;
	/** Observed high-water live byte demand. */
	uint64_t peakLogicalBytes = 0u;
	/** Observed high-water allocated backing bytes. */
	uint64_t peakBackingAllocatedBytes = 0u;
	/** Observed high-water object/entry demand. */
	uint64_t peakObjectCount = 0u;
	/** Current live object or entry count. */
	uint64_t objectCount = 0u;
	/** Current reserved object or entry capacity. */
	uint64_t capacityCount = 0u;
	/** Recorded allocation operation count. */
	uint64_t allocationOps = 0u;
	/** Recorded logical-release operation count. */
	uint64_t logicalReleaseOps = 0u;
	/** Recorded physical-release operation count. */
	uint64_t physicalReleaseOps = 0u;
	/** Recorded backing-growth operation count. */
	uint64_t growthOps = 0u;
	/** Accuracy, completeness, and overlap qualifiers. */
	MemorySampleFlag flags = MemorySampleFlag::None;
	/** Bitwise MemoryValueField availability mask; absent fields are not measured zeros. */
	uint16_t value_fields = 127u;
};

/**
 * Allocation-free memory operation evidence. Numeric source/lifetime and frame identities support
 * correlation without retaining owner pointers.
 */
struct MemoryOperationRecord {
	/** Source whose measurements or operation this record describes. */
	MemorySourceId source = 0u;
	/** Resource lifetime identity when available; zero means untracked. */
	MemoryLifetimeId lifetime = 0u;
	/** Memory transition represented by the event. */
	MemoryOperation operation = MemoryOperation::LogicalAllocate;
	/** Minimum monitoring level required to record the event. */
	MemoryMonitoringLevel detailLevel = MemoryMonitoringLevel::StorageSummary;
	/** Application tick attribution; the recorder fills missing tick context. */
	AppTickId appTick = 0u;
	/** Window/frame attribution when available. */
	WindowFrameKey frame{};
	/** GPU submission identity when relevant. */
	uint64_t submissionSerial = 0u;
	/** Monotonic capture timestamp in nanoseconds. */
	uint64_t timestampNs = 0u;
	/** Accounted byte quantity before the operation. */
	uint64_t bytesBefore = 0u;
	/** Accounted byte quantity after the operation. */
	uint64_t bytesAfter = 0u;
	/** Magnitude of the accounted byte change. */
	uint64_t bytesChanged = 0u;
	/** Numeric producer thread track, or zero when unspecified. */
	uint32_t threadTrack = 0u;
	/** Optional numeric developer name token. */
	uint32_t debugName = 0u;
};

/** Availability bitmask for platform-dependent process metrics. */
enum class ProcessMemoryFieldBit : uint32_t {
	VirtualBytes = 1u << 0u, /**< virtualBytes is available. */
	ResidentBytes = 1u << 1u, /**< residentBytes is available. */
	PeakResidentBytes = 1u << 2u, /**< peakResidentBytes is available. */
	PrivateBytes = 1u << 3u, /**< privateBytes is available. */
	SharedBytes = 1u << 4u, /**< sharedBytes is available. */
	PageFaultCount = 1u << 5u, /**< pageFaultCount is available. */
};

/**
 * Process memory counters with a capture timestamp. Read availableFields before interpreting a metric;
 * absent metrics are not measured zeros.
 */
struct ProcessMemorySnapshot {
	/** Monotonic timestamp of this sample in nanoseconds. */
	uint64_t capturedAtNs = 0u;
	/** Process virtual address-space bytes, when available. */
	uint64_t virtualBytes = 0u;
	/** Process resident physical bytes, when available. */
	uint64_t residentBytes = 0u;
	/** Peak process resident bytes, when available. */
	uint64_t peakResidentBytes = 0u;
	/** Private process bytes, when available. */
	uint64_t privateBytes = 0u;
	/** Shared process bytes, when available. */
	uint64_t sharedBytes = 0u;
	/** Platform page-fault counter, when available. */
	uint64_t pageFaultCount = 0u;
	/** Time spent collecting this sample in nanoseconds. */
	uint64_t sampleDurationNs = 0u;
	/** Bitwise ProcessMemoryFieldBit mask identifying valid counters. */
	uint32_t availableFields = 0u;
	/** Accuracy/availability of process sampling. */
	MemoryAccuracy accuracy = MemoryAccuracy::Unavailable;
};

/** Heuristic interpretation of a Vulkan memory heap. */
enum class GpuHeapClass : uint8_t {
	DedicatedLike = 0, /**< Device-local heap resembling dedicated VRAM. */
	SharedLike, /**< Shared/unified device and host physical memory. */
	HostLike, /**< Host-visible system-memory-like heap. */
	Unknown, /**< No reliable heap classification. */
};

/**
 * Vulkan/VMA accounting for one memory heap. Heap classification and overlap flags prevent summing
 * shared physical memory twice.
 */
struct GpuHeapMemorySample {
	/** Vulkan physical-device memory heap index. */
	uint32_t heapIndex = 0u;
	/** Native VkMemoryHeapFlagBits value. */
	uint32_t heapFlags = 0u;
	/** Physical heap size reported by Vulkan. */
	uint64_t heapSizeBytes = 0u;
	/** VMA-reported heap usage in bytes. */
	uint64_t allocatorUsageBytes = 0u;
	/** VMA-reported heap budget in bytes. */
	uint64_t allocatorBudgetBytes = 0u;
	/** Bytes requested by tracked VMA allocations. */
	uint64_t allocationBytes = 0u;
	/** Bytes in VMA backing blocks. */
	uint64_t blockBytes = 0u;
	/** Number of tracked VMA allocations. */
	uint32_t allocationCount = 0u;
	/** Number of VMA backing blocks. */
	uint32_t blockCount = 0u;
	/** Shared/dedicated/host-like interpretation of this heap. */
	GpuHeapClass classification = GpuHeapClass::Unknown;
	/** Accuracy, completeness, and overlap qualifiers. */
	MemorySampleFlag flags = MemorySampleFlag::None;
};

/**
 * Owned GPU heap sample collected without waiting for GPU completion. available distinguishes
 * unsupported sampling from an empty measured result.
 */
struct GpuMemorySnapshot {
	/** Monotonic timestamp of this sample in nanoseconds. */
	uint64_t capturedAtNs = 0u;
	/** Time spent collecting this sample in nanoseconds. */
	uint64_t sampleDurationNs = 0u;
	/** Monotonic GPU sample sequence. */
	uint64_t sampleSequence = 0u;
	/** Owned per-heap measurements. */
	std::vector<GpuHeapMemorySample> heaps{};
	/** Whether the Vulkan memory-budget extension is enabled. */
	bool memoryBudgetExtensionEnabled = false;
	/** Whether detailed allocation/block statistics were collected. */
	bool detailedStatisticsIncluded = false;
	/** Whether GPU sampling produced valid evidence. */
	bool available = false;
};

/**
 * Owned process/GPU sample and the residual between process residency and safely attributable FlowUi
 * CPU bytes. The residual is an estimate, not a leak measurement.
 */
struct MemoryEnvironmentSnapshot {
	/** Process-memory counters and their availability. */
	ProcessMemorySnapshot process{};
	/** Owned GPU heap evidence. */
	GpuMemorySnapshot gpu{};
	/** FlowUi CPU bytes safe to compare with process residency. */
	uint64_t safelyAttributableFlowUiCpuBytes = 0u;
	/** Nonnegative resident-minus-attributed residual estimate. */
	uint64_t unattributedResidentResidual = 0u;
	/** Signed resident-minus-attributed difference before nonnegative clamping. */
	int64_t signedResidentDiscrepancy = 0;
	/** Qualifiers on the residual estimate, including overlap/incompleteness. */
	MemorySampleFlag residualFlags = MemorySampleFlag::Estimate;
};

/**
 * Cumulative producer and source-registration quality counters; missing detail must remain visible in
 * reports.
 */
struct MemoryQualitySnapshot {
	/** Cumulative successfully recorded producer operations. */
	uint64_t recordedOperations = 0u;
	/** Cumulative operations excluded by monitoring level or suppression. */
	uint64_t suppressedOperations = 0u;
	/** Cumulative operations lost because bounded recording failed. */
	uint64_t droppedOperations = 0u;
	/** Cumulative conflicting registrations of one source identity. */
	uint64_t sourceCollisions = 0u;
	/** Cumulative references to unregistered sources. */
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

/**
 * Hash exact stable-name bytes into a nonzero source identity at compile time; this does not register
 * the source.
 */
[[nodiscard]] consteval MemorySourceId makeMemorySourceId(std::string_view stableName) {
	const uint64_t hash = detail::dev_memory::hashBytes(stableName);
	return hash == 0u ? 1u : hash;
}

/** Construct a static registration descriptor and hash stableName at compile time. The name is borrowed and must outlive descriptor use.
 * @param stableName Stable, case-sensitive source name.
 * @param domain Accounting domain.
 * @param kind Source owner category.
 * @param accuracy Accounting accuracy.
 * @param parent Optional parent identity.
 * @param tuningTarget Optional capacity-target identity.
 * @return Allocation-free source descriptor. */
[[nodiscard]] consteval StaticMemorySourceDescriptor makeMemorySourceDescriptor(
	std::string_view stableName,
	MemoryDomain domain,
	MemorySourceKind kind,
	MemoryAccuracy accuracy = MemoryAccuracy::Exact,
	MemorySourceId parent = 0u,
	MemoryTuningTargetId tuningTarget = 0u) {
	return StaticMemorySourceDescriptor{
		.name = stableName,
		.id = makeMemorySourceId(stableName),
		.parent = parent,
		.tuningTarget = tuningTarget,
		.domain = domain,
		.kind = kind,
		.accuracy = accuracy,
	};
}

} // namespace FlowUi::devSystems

#endif
