#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include <vulkan/vulkan.h>

#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

struct VulkanContext;

namespace FlowUi::devSystems {

class DevTiming;

/** Bitmask describing GPU query completion and calibration quality. */
enum class GpuTimingRecordFlag : uint16_t {
	/** No flags or entity attribution. */
	None = 0u,
	/** Measurement completed normally. */
	Completed = 1u << 0u,
	/** Device timestamp lacks CPU clock alignment. */
	Uncalibrated = 1u << 1u,
	/** Capture capacity omitted some nested detail. */
	DetailTruncated = 1u << 2u,
};

/** Return the underlying integer representation of GPU timing flags. */
[[nodiscard]] constexpr uint16_t gpuTimingRecordFlags(GpuTimingRecordFlag flag) noexcept {
	return static_cast<uint16_t>(flag);
}

/**
 * Resolved GPU timestamp span. Raw device ticks and period are retained alongside nanosecond
 * duration and optional CPU clock alignment.
 */
struct GpuTimingRecord {
	/** Raw device timestamp at the beginning of the GPU span. */
	uint64_t startTick = 0u;
	/** GPU timestamp delta after valid-bit wrapping. */
	uint64_t durationTicks = 0u;
	/** Measured duration in nanoseconds. */
	uint64_t durationNs = 0u;
	/** Device timestamp period in nanoseconds per tick. */
	double timestamp_period_ns = 0;
	/** Number of valid bits in raw device timestamps. */
	uint32_t timestamp_valid_bits = 64;
	/** Calibrated GPU start on the CPU clock, in nanoseconds; unavailable when uncalibrated. */
	uint64_t cpuAlignedStartNs = 0u;
	/** Maximum calibration uncertainty in nanoseconds. */
	uint64_t calibrationMaximumDeviationNs = 0u;
	/** Renderer submission sequence associated with this evidence. */
	uint64_t submissionSerial = 0u;
	/** @var GpuTimingRecord::device_identity
	 * Opaque device identity used to distinguish GPU timelines.
	 */
	/** @var GpuTimingRecord::queue_identity
	 * Opaque queue identity used to distinguish GPU timelines.
	 */
	uint64_t device_identity = 0, queue_identity = 0;
	/** Stable timing descriptor identity; zero is unassigned. */
	TimingZoneTypeId typeId = 0u;
	/** Window and frame sequence associated with this evidence. */
	WindowFrameKey frame{};
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Primary numeric entity identity interpreted by entityKind. */
	uint64_t primaryEntityId = 0u;
	/** Secondary numeric entity identity interpreted by entityKind. */
	uint64_t secondaryEntityId = 0u;
	/** Identity of the clock calibration used for CPU alignment. */
	uint64_t calibrationId = 0u;
	/** Index of this zone within its submission plan; UINT32_MAX means unavailable. */
	uint32_t zone_index = UINT32_MAX;
	/** Parent zone index within the submission plan; UINT32_MAX means root. */
	uint32_t parentZoneIndex = UINT32_MAX;
	/** Vulkan queue family for the captured command buffer. */
	uint32_t queueFamilyIndex = UINT32_MAX;
	/** Vulkan pipeline stage at which the beginning timestamp is written. */
	VkPipelineStageFlags2 beginStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	/** Vulkan pipeline stage at which the ending timestamp is written. */
	VkPipelineStageFlags2 endStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	/** Entity tag that defines the meaning of the numeric entity identities. */
	TimingEntityKind entityKind = TimingEntityKind::None;
	/** Zero-based nesting depth within the producer stack or frame-slot plan. */
	uint8_t depth = 0u;
	/** Bitmask of completion, provenance and capture-quality conditions. */
	uint16_t flags = gpuTimingRecordFlags(GpuTimingRecordFlag::Completed) |
		gpuTimingRecordFlags(GpuTimingRecordFlag::Uncalibrated);
};

/**
 * GPU capability and query-quality snapshot. Uncalibrated timestamps cannot be placed directly on
 * the CPU timeline.
 */
struct GpuTimingQualitySnapshot {
	/** Number of resolved submissions. */
	uint64_t resolvedSubmissions = 0u;
	/** Number of recorded zones. */
	uint64_t recordedZones = 0u;
	/** Number of truncated zones. */
	uint64_t truncatedZones = 0u;
	/** Number of unavailable queries. */
	uint64_t unavailableQueries = 0u;
	/** Number of query pool failures. */
	uint64_t queryPoolFailures = 0u;
	/** Number of query read failures. */
	uint64_t queryReadFailures = 0u;
	/** Number of dropped records. */
	uint64_t droppedRecords = 0u;
	/** Whether the renderer queue supports timestamp capture. */
	bool supported = false;
	/** Whether synchronization2 timestamp commands are available. */
	bool synchronization2Available = false;
	/** Whether device timestamps can be aligned with the CPU capture clock. */
	bool calibrated = false;
	/** Number of valid bits in raw device timestamps. */
	uint32_t timestampValidBits = 0u;
	/** Device timestamp period in nanoseconds per tick. */
	double timestampPeriodNs = 0.0;
};

/** One planned GPU timestamp pair within a reusable frame slot. */
struct GpuTimingZonePlan {
	/** Semantic entity attributed to this timing zone or correlation. */
	TimingEntityRef entity{};
	/** Stable timing descriptor identity; zero is unassigned. */
	TimingZoneTypeId typeId = 0u;
	/** Vulkan pipeline stage at which the beginning timestamp is written. */
	VkPipelineStageFlags2 beginStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	/** Vulkan pipeline stage at which the ending timestamp is written. */
	VkPipelineStageFlags2 endStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	/** Index of the query reserved for the beginning timestamp. */
	uint32_t beginQuery = 0u;
	/** Index of the query reserved for the ending timestamp. */
	uint32_t endQuery = 0u;
	/** Parent zone index within the submission plan; UINT32_MAX means root. */
	uint32_t parentZoneIndex = UINT32_MAX;
	/** Zero-based nesting depth within the producer stack or frame-slot plan. */
	uint8_t depth = 0u;
	/** Whether the ending timestamp has been recorded. */
	bool ended = false;
};

/**
 * Reusable renderer-owned query storage for one in-flight frame. Resolve completed submissions
 * before reusing its query pool.
 */
struct GpuTimingFrameSlot {
	/** Owned zone plans for the current frame-slot recording. */
	std::vector<GpuTimingZonePlan> zones{};
	/** Indices of currently open GPU zones in nesting order. */
	std::vector<uint32_t> activeZones{};
	/** Reusable storage for raw query values and availability results. */
	std::vector<uint64_t> queryResults{};
	/** Window and frame sequence associated with this evidence. */
	WindowFrameKey frame{};
	/** Vulkan timestamp query pool owned by the renderer frame slot. */
	VkQueryPool queryPool = VK_NULL_HANDLE;
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Renderer submission sequence associated with this evidence. */
	uint64_t submissionSerial = 0u;
	/** @var GpuTimingFrameSlot::device_identity
	 * Opaque device identity used to distinguish GPU timelines.
	 */
	/** @var GpuTimingFrameSlot::queue_identity
	 * Opaque queue identity used to distinguish GPU timelines.
	 */
	uint64_t device_identity = 0, queue_identity = 0;
	/** Number of timestamps supported by the current query pool. */
	uint32_t queryCapacity = 0u;
	/** Number of query indices reserved for this recording. */
	uint32_t usedQueries = 0u;
	/** Vulkan queue family for the captured command buffer. */
	uint32_t queueFamilyIndex = UINT32_MAX;
	/** Whether commands are currently being recorded into this frame slot. */
	bool recording = false;
	/** Whether this slot has an outstanding submission awaiting resolution. */
	bool submitted = false;
	/** Whether zone detail exceeded the configured query capacity. */
	bool detailTruncated = false;
};

/** Frame-slot-local token identifying a planned GPU timestamp pair. */
struct GpuTimingZoneToken {
	/** Zone plan index in the owning frame slot. */
	uint32_t planIndex = UINT32_MAX;
	/** Whether this token or edit scope was admitted and is still active. */
	bool active = false;

	/** Return whether this frame, token or thread attachment is valid and active. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return active; }
};

/** Borrowed GPU timing service and frame slot for recording one command buffer. */
struct GpuTimingCommandContext {
	/** Borrowed GPU recorder that outlives this recording context. */
	class DevGpuTiming* timing = nullptr;
	/** Borrowed renderer frame slot; it must outlive command recording and zone scopes. */
	GpuTimingFrameSlot* frameSlot = nullptr;
};

/**
 * GPU timestamp recorder tied to a DevTiming owner. Recording operations use the renderer thread
 * and require valid Vulkan objects through query resolution.
 */
class DevGpuTiming {
public:
	/**
	 * GPU timestamp recorder tied to a DevTiming owner. Recording operations use the renderer thread
	 * and require valid Vulkan objects through query resolution.
	 */
	explicit DevGpuTiming(DevTiming& timing) noexcept;
	/** Release owned monitoring state after dependent recorders, attachments and scopes are finished. */
	~DevGpuTiming();

	/** Copying is disabled to preserve ownership and producer identity. */
	DevGpuTiming(const DevGpuTiming&) = delete;
	DevGpuTiming& operator=(const DevGpuTiming&) = delete;

	/**
	 * Probe timestamp and calibration support from the renderer Vulkan context; unsupported queues
	 * leave capture disabled.
	 */
	void initialize(const VulkanContext& vk) noexcept;
	/** Return whether the initialized Vulkan queue supports timestamp capture. */
	[[nodiscard]] bool supported() const noexcept;
	/** Return whether GPU capture is supported and enabled by the current timing configuration. */
	[[nodiscard]] bool enabled() const noexcept;

	/**
	 * Resolve queries only after the frame-slot submission has completed, append available records,
	 * and release the slot for reuse.
	 */
	void resolveCompleted(VulkanContext& vk, GpuTimingFrameSlot& slot) noexcept;
	/**
	 * Prepare a reusable frame slot for timestamps in commandBuffer. The Vulkan context, slot and
	 * command buffer must remain valid throughout recording and submission.
	 */
	[[nodiscard]] GpuTimingCommandContext beginFrameRecording(
		VulkanContext& vk,
		GpuTimingFrameSlot& slot,
		VkCommandBuffer commandBuffer,
		WindowFrameKey frame,
		AppTickId appTick) noexcept;
	/**
	 * Finish outstanding timestamp scopes for this command recording and invalidate its borrowed
	 * context.
	 */
	void endFrameRecording(
		GpuTimingCommandContext& context,
		VkCommandBuffer commandBuffer) noexcept;
	/** Mark a recorded frame slot as awaiting completion under the supplied submission sequence. */
	void markSubmitted(GpuTimingFrameSlot& slot, uint64_t submissionSerial) noexcept;
	/** Discard unsubmitted timing plans and return the slot to the idle state. */
	void cancelRecording(GpuTimingFrameSlot& slot) noexcept;

	/**
	 * Write a beginning timestamp and return a frame-slot token, or an inactive token when capture
	 * is unavailable or full.
	 */
	[[nodiscard]] GpuTimingZoneToken beginZone(
		GpuTimingCommandContext* context,
		VkCommandBuffer commandBuffer,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	/** Write the ending timestamp for an active token in the same command context and command buffer. */
	void endZone(
		GpuTimingCommandContext* context,
		VkCommandBuffer commandBuffer,
		GpuTimingZoneToken token) noexcept;

	/** Append newly completed records to destination and consume them from the recorder. */
	void drain_completed_records_into(std::vector<GpuTimingRecord>& destination);
	/** Consume newly completed producer records into a new owned vector. */
	[[nodiscard]] std::vector<GpuTimingRecord> drainCompletedRecords();
	/** Return cumulative capture capability and quality counters without consuming records. */
	[[nodiscard]] GpuTimingQualitySnapshot qualitySnapshot() const noexcept;

private:
	[[nodiscard]] bool ensureQueryPool(VulkanContext& vk, GpuTimingFrameSlot& slot) noexcept;
	[[nodiscard]] GpuTimingZoneToken beginZoneUnchecked(
		GpuTimingFrameSlot& slot,
		VkCommandBuffer commandBuffer,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity) noexcept;

	DevTiming* timing_ = nullptr;
	bool supported_ = false;
	bool capabilityChecked_ = false;
	bool synchronization2Available_ = false;
	bool calibratedTimestampsAvailable_ = false;
	VkTimeDomainEXT hostTimeDomain_ = VK_TIME_DOMAIN_DEVICE_EXT;
	std::atomic<uint64_t> nextCalibrationId_{1u};
	uint32_t timestampValidBits_ = 0u;
	double timestampPeriodNs_ = 0.0;
	mutable std::mutex recordsMutex_{};
	std::vector<GpuTimingRecord> completedRecords_{};
	std::atomic<uint64_t> resolvedSubmissions_{0u};
	std::atomic<uint64_t> recordedZones_{0u};
	std::atomic<uint64_t> truncatedZones_{0u};
	std::atomic<uint64_t> unavailableQueries_{0u};
	std::atomic<uint64_t> queryPoolFailures_{0u};
	std::atomic<uint64_t> queryReadFailures_{0u};
	std::atomic<uint64_t> droppedRecords_{0u};
};

/**
 * Lexical GPU timestamp pair for a command buffer. The command context and frame slot must remain
 * valid until destruction.
 */
class GpuCommandTimingZone {
public:
	/**
	 * Begin the scoped timing measurement when the recorder or context admits it; null pointers
	 * disable capture. Borrowed context and descriptor text must outlive the scope.
	 */
	GpuCommandTimingZone(
		GpuTimingCommandContext* context,
		VkCommandBuffer commandBuffer,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	/** Write the ending timestamp for an admitted GPU zone. */
	~GpuCommandTimingZone() noexcept;

	/** Copying is disabled to preserve ownership and producer identity. */
	GpuCommandTimingZone(const GpuCommandTimingZone&) = delete;
	GpuCommandTimingZone& operator=(const GpuCommandTimingZone&) = delete;

private:
	GpuTimingCommandContext* context_ = nullptr;
	VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
	GpuTimingZoneToken token_{};
};

namespace gpu_timing_zones {

/** Immutable built-in timing descriptor for submission. */
inline constexpr TimingZoneDescriptor kSubmission = makeBuiltinTimingDescriptor(
	0xf0d64f3b1a7c2901ull, TimingCategory::Gpu, TimingZoneRole::GpuWork,
	CpuTimingLevel::OnlyFrameTime, "flowui.gpu.submission");
/** Immutable built-in timing descriptor for viewport pass. */
inline constexpr TimingZoneDescriptor kViewportPass = makeBuiltinTimingDescriptor(
	0xf0d64f3b1a7c2902ull, TimingCategory::Gpu, TimingZoneRole::GpuWork,
	CpuTimingLevel::OnlyFrameTime, "flowui.gpu.viewport_pass");
/** Immutable built-in timing descriptor for ui pass. */
inline constexpr TimingZoneDescriptor kUiPass = makeBuiltinTimingDescriptor(
	0xf0d64f3b1a7c2903ull, TimingCategory::Gpu, TimingZoneRole::GpuWork,
	CpuTimingLevel::OnlyFrameTime, "flowui.gpu.ui_pass");

} // namespace gpu_timing_zones

} // namespace FlowUi::devSystems

#endif
