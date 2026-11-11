#pragma once

/** @file
 * @brief Development monitoring and tooling configuration; available with FLOW_UI_DEV_MODE.
 */

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace FlowUi {

#if FLOW_UI_DEV_MODE

namespace devSystems::tooling {

/**
 * @brief Hard limits for copied reflection metadata; registration fails when a limit is exhausted.
 */
struct DevSchemaLimits {
	/** @brief Maximum reflected field nesting depth. */
	std::uint16_t maxDepth = 16;
	/** @brief Maximum registered reflected types. */
	std::uint32_t maxTypes = 2048;
	/** @brief Maximum registered fields across all types. */
	std::uint32_t maxFields = 16384;
	/** @brief Maximum copied enum choices. */
	std::uint32_t maxEnumValues = 8192;
	/** @brief Maximum copied field constraints. */
	std::uint32_t maxConstraints = 16384;
	/** @brief Maximum copied metadata text, in bytes. */
	std::uint32_t maxStringBytes = 2u * 1024u * 1024u;
};

} // namespace devSystems::tooling

namespace devSystems {

/**
 * @brief CPU timing detail admitted by the recorder; higher levels retain more scopes.
 */
enum class CpuTimingLevel : uint8_t {
	OnlyFrameTime = 0, /**< Record frame duration only. */
	Summary = 1, /**< Collect summary evidence for this stream. */
	Balanced = 2, /**< Retain selected scopes and scopes exceeding the configured duration threshold. */
	Deep = 3, /**< Collect the deepest detail supported by the compiled recorder. */
};

/**
 * @brief CPU/GPU timing collection and producer buffer sizes in development builds.
 * Requested detail is capped by FLOWUI_DEV_TIMING_LEVEL; capacities count records or query slots.
 */
struct DevTimingConfig {
	/**
	 * @brief Optional element instance selected for detailed timing; zero selects no instance.
	 */
	FlowElementID selectedElementInstance{};
	/** @brief Minimum element scope duration retained by Balanced timing, in nanoseconds. */
	uint64_t balancedElementRetentionThresholdNs = 50'000u;
	/**
	 * @brief Optional definition selected for detailed timing; zero selects no definition.
	 */
	FlowDefinitionID selectedElementDefinition{};
	/** @brief CPU detail requested up to the compiled FLOWUI_DEV_TIMING_LEVEL. */
	CpuTimingLevel cpuLevel = CpuTimingLevel::Summary;
	/** @brief Bitmask of timing categories admitted by collection. */
	uint32_t enabledCategoryMask = 0xFFFFFFFFu;
	/** @brief Timestamp query slots reserved per GPU frame. */
	uint32_t gpuQueryCapacityPerFrame = 512u;
	/** @brief Maximum records buffered by each producer before draining. */
	uint32_t producerRecordCapacity = 8192u;
	/** @brief Whether GPU timestamp collection is requested. */
	bool gpuTimingEnabled = true;
};

/**
 * @brief Timing history retention and rolling statistics; independent of producer detail.
 */
struct TimingReportingConfig {
	/** @brief Maximum application ticks retained in timing history. */
	uint32_t retainedAppTickCapacity = 4096u;
	/** @brief Minimum history coverage relative to the frames-in-flight count. */
	uint32_t minimumFramesInFlightMultiplier = 20u;
	/** @brief Maximum duration samples used for rolling statistics. */
	uint32_t rollingSampleCapacity = 2048u;
	/**
	 * @brief Requested percentile fractions in [0, 1], such as 0.95 for the 95th percentile.
	 */
	std::vector<double> percentilePoints{0.50, 0.90, 0.95, 0.99};
};

/**
 * @brief Runtime memory detail, capped by FLOWUI_DEV_MEMORY_LEVEL compiled into the library.
 */
enum class MemoryMonitoringLevel : uint8_t {
	Disabled = 0, /**< Disable this collection stream. */
	StorageSummary = 1, /**< Collect storage totals. */
	SubsystemCapacity = 2, /**< Collect subsystem capacities and demand peaks. */
	DetailedLifetimes = 3, /**< Collect individual resource lifetime evidence. */
	DeepAllocations = 4, /**< Collect detailed allocation evidence supported by the build. */
};

/**
 * @brief Memory collection and environment sampling in development builds.
 * Requested detail is capped by FLOWUI_DEV_MEMORY_LEVEL; probes also require platform support.
 */
struct DevMemoryConfig {
	/** @brief Minimum interval between process-memory probes, in nanoseconds. */
	uint64_t processSampleIntervalNs = 500'000'000ull;
	/** @brief Minimum interval between GPU-budget probes, in nanoseconds. */
	uint64_t gpuBudgetSampleIntervalNs = 250'000'000ull;
	/** @brief Memory detail requested up to the compiled FLOWUI_DEV_MEMORY_LEVEL. */
	MemoryMonitoringLevel level = MemoryMonitoringLevel::SubsystemCapacity;
	/** @brief Maximum memory lifetime events buffered by the producer before draining. */
	uint32_t producerEventCapacity = 8192u;
	/**
	 * @brief GPU samples between detailed allocator queries; zero disables periodic detail.
	 * detailedVmaStatistics still requests detail on every sample.
	 */
	uint32_t detailedGpuStatsEverySamples = 0u;
	/** @brief Whether to sample GPU memory when supported. */
	bool gpuMemory = true;
	/** @brief Whether to sample process memory when supported. */
	bool processMemory = true;
	/** @brief Collect detailed Vulkan allocator statistics on every GPU sample. */
	bool detailedVmaStatistics = false;
};

/**
 * @brief Retention and sampling policy for derived memory history.
 * Lifetime-event retention requires producer detail that supplies those events.
 */
struct MemoryReportingConfig {
	/** @brief Retained event-ring budget in bytes; used when retainLifetimeEvents is true. */
	uint64_t eventByteCapacity = 16ull * 1024ull * 1024ull;
	/**
	 * @brief Processing duration above which memory consumption is reported as slow, in
	 * nanoseconds.
	 */
	uint64_t consumeWarningThresholdNs = 2'000'000u;
	/** @brief Maximum retained memory history segments. */
	uint32_t segmentCapacity = 100'000u;
	/** @brief Application ticks between manager capacity samples. */
	uint32_t managerSampleEveryTicks = 8u;
	/** @brief Maximum history segments used for memory distribution estimates. */
	uint32_t quantileWindowSegments = 20'000u;
	/** @brief Whether individual memory lifetime events remain available to reporting. */
	bool retainLifetimeEvents = false;
};

class DevErrorStackProvider;

/**
 * @brief Amount of causal, stack and state evidence recorded for each error occurrence.
 */
enum class DevErrorCaptureLevel : uint8_t {
	Disabled = 0, /**< Disable this collection stream. */
	Summary = 1, /**< Collect summary evidence for this stream. */
	Causal = 2, /**< Include causal steps and breadcrumbs. */
	StackAndState = 3, /**< Include native stack and safe-point state evidence when available. */
	Deep = 4, /**< Collect the deepest detail supported by the compiled recorder. */
};

/** @brief Fixed byte capacity of a copied native error message. */
inline constexpr size_t kDevErrorNativeTextCapacity = 160u;

/**
 * @brief Error producer buffers and evidence detail in development builds.
 * Detail is capped by FLOWUI_DEV_ERROR_LEVEL; borrowed providers must outlive monitoring.
 */
struct DevErrorConfig {
	/** @brief Error detail requested up to the compiled FLOWUI_DEV_ERROR_LEVEL. */
	DevErrorCaptureLevel level = DevErrorCaptureLevel::Causal;
	/** @brief Maximum records buffered by each producer before draining. */
	uint32_t producerRecordCapacity = 256u;
	/** @brief Maximum breadcrumbs buffered by a producer. */
	uint32_t breadcrumbCapacity = 512u;
	/** @brief Maximum recent breadcrumbs attached to one occurrence. */
	uint32_t recentBreadcrumbCount = 32u;
	/**
	 * @brief Maximum native diagnostic text bytes copied, bounded by kDevErrorNativeTextCapacity.
	 */
	uint32_t nativeTextLimit = static_cast<uint32_t>(kDevErrorNativeTextCapacity);
	/** @brief Maximum registered error recorder threads. */
	uint32_t threadRecorderCapacity = 16u;
	/** @brief Maximum copied error source descriptors. */
	uint32_t sourceDescriptorCapacity = 1024u;
	/** @brief Maximum registered breadcrumb descriptors. */
	uint32_t breadcrumbDescriptorCapacity = 256u;
	/** @brief Maximum retained raw stack traces. */
	uint32_t stackTraceCapacity = 256u;
	/** @brief Maximum frames copied from a single native stack. */
	uint32_t maximumStackFrames = 32u;
	/** @brief Maximum registered diagnostic snapshot providers. */
	uint32_t snapshotProviderCapacity = 64u;
	/** @brief Maximum queued safe-point snapshot requests. */
	uint32_t pendingSnapshotCapacity = 128u;
	/** @brief Maximum retained captured state snapshots. */
	uint32_t retainedSnapshotCapacity = 256u;
	/**
	 * @brief Borrowed optional native stack provider; nullptr leaves native stack evidence
	 * unavailable.
	 */
	DevErrorStackProvider* stackProvider = nullptr;
};

/**
 * @brief Error predicate and capture window in application ticks; active criteria must all match.
 */
struct DevErrorTrigger {
	/** @brief Error code to match; None leaves the code unconstrained. */
	ErrorCode code = ErrorCode::None;
	/** @brief Error site to match; None leaves the site unconstrained. */
	ErrorSite site = ErrorSite::None;
	/** @brief Native error code tested only when matchNativeCode is true. */
	uint32_t nativeCode = 0u;
	/** @brief Whether nativeCode participates in the trigger predicate. */
	bool matchNativeCode = false;
	/** @brief Application ticks before the matching occurrence included in capture. */
	uint32_t preOccurrenceTicks = 2u;
	/**
	 * @brief Application ticks after the occurrence included in capture or automatic correlation.
	 */
	uint32_t postOccurrenceTicks = 2u;
};

/**
 * @brief Owned error-report retention, capture triggers and bounded timing/memory correlation.
 */
struct DevErrorReportingConfig {
	/**
	 * @brief Owned predicates that request bounded timing and memory captures on matching errors.
	 */
	std::vector<DevErrorTrigger> triggers{};
	/** @brief Byte budget for retained owned error reports. */
	uint64_t retainedByteBudget = 4u * 1024u * 1024u;
	/** @brief Maximum retained error occurrences. */
	uint32_t retainedOccurrenceCapacity = 512u;
	/** @brief Maximum retained breadcrumb records. */
	uint32_t retainedBreadcrumbCapacity = 4096u;
	/** Grace window for automatic correlation after the initial context/final record.
	 * Sealed correlations do not retry; later steps/snapshots still attach and update advice. */
	uint32_t postOccurrenceTicks = 2u;
	/** @brief Maximum causal steps retained for one occurrence. */
	uint32_t maxStepsPerOccurrence = 64u;
	/** @brief Maximum derived advice entries per occurrence. */
	uint32_t maximumAdvicePerOccurrence = 4u;
	/** @brief Maximum retained triggered evidence captures. */
	uint32_t retainedCaptureCapacity = 32u;
	/** @brief Maximum timing ticks pinned by one triggered capture. */
	uint32_t maximumPinnedTimingTicks = 16u;
	/** @brief Maximum memory events pinned by one triggered capture. */
	uint32_t maximumPinnedMemoryEvents = 128u;
};

namespace tooling {

/**
 * @brief Hard queue limits for live edits; accepted transactions apply at application safe points.
 */
struct DevOverrideEngineConfig {
	/** @brief Maximum live-edit transactions awaiting a safe point. */
	std::uint32_t maximumPendingTransactions = 256;
	/** @brief Maximum edit commands accepted in one transaction. */
	std::uint32_t maximumCommandsPerTransaction = 4096;
	/** @brief Maximum total edit commands queued across transactions. */
	std::uint32_t maximumPendingCommands = 16384;
};

/**
 * @brief Initial UI-tree reserves and hard capture limits in development builds.
 * Reserves reduce startup growth; maximum counts bound retained evidence. Clay fields require
 * FLOW_UI_DEV_CAPTURE_CLAY.
 */
struct DevTreeCaptureConfig {
	/** @brief Initial Flow element node reserve, in nodes. */
	uint32_t flowNodeReserve = 512;
	/** @brief Initial captured metadata text reserve, in bytes. */
	uint32_t stringByteReserve = 64u * 1024u;
	/** @brief Initial capture diagnostic reserve, in records. */
	uint32_t diagnosticReserve = 64;
	/** @brief Maximum captured Flow nodes. */
	uint32_t maximumFlowNodes = 1u << 20u;
	/** @brief Maximum captured metadata text, in bytes. */
	uint32_t maximumStringBytes = 64u * 1024u * 1024u;
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Initial Clay layout node reserve, in nodes. */
	uint32_t clayNodeReserve = 2048;
	/** @brief Initial Clay root reserve, in roots. */
	uint32_t clayRootReserve = 32;
	/** @brief Initial Flow-to-Clay association reserve, in links. */
	uint32_t directLinkReserve = 2048;
	/** @brief Maximum captured Clay layout nodes. */
	uint32_t maximumClayNodes = 1u << 22u;
#endif
};

} // namespace tooling

/**
 * @brief Development-only timing, memory and error collection with separate reporting policies.
 * Runtime detail cannot enable instrumentation omitted by the compiled monitoring levels.
 */
struct DevMonitoringConfig {
	/** @brief CPU and GPU timing producer policy. */
	DevTimingConfig timing{};
	/** @brief Timing retention and statistics policy. */
	TimingReportingConfig timingReporting{};
	/** @brief Memory producer and environment probe policy. */
	DevMemoryConfig memory{};
	/** @brief Memory retention and aggregation policy. */
	MemoryReportingConfig memoryReporting{};
	/** @brief Error evidence producer policy. */
	DevErrorConfig errors{};
	/** @brief Error retention, triggers and correlation policy. */
	DevErrorReportingConfig errorReporting{};
};

/** @brief Development-only reflection, safe-point editing and copied UI-tree limits. */
struct DevToolingConfig {
	/** @brief Reflection metadata registration limits. */
	tooling::DevSchemaLimits schema{};
	/** @brief Live-edit transaction queue limits. */
	tooling::DevOverrideEngineConfig overrides{};
	/** @brief Captured UI tree reserves and hard bounds. */
	tooling::DevTreeCaptureConfig treeCapture{};
};

} // namespace devSystems

#endif

} // namespace FlowUi
