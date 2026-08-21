#pragma once

#include "FlowUi/BuildConfig.hpp"

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

enum class GpuTimingRecordFlag : uint16_t {
	None = 0u,
	Completed = 1u << 0u,
	Uncalibrated = 1u << 1u,
	DetailTruncated = 1u << 2u,
};

[[nodiscard]] constexpr uint16_t gpuTimingRecordFlags(GpuTimingRecordFlag flag) noexcept {
	return static_cast<uint16_t>(flag);
}

struct GpuTimingRecord {
	uint64_t startTick = 0u;
	uint64_t durationTicks = 0u;
	uint64_t durationNs = 0u;
	uint64_t cpuAlignedStartNs = 0u;
	uint64_t calibrationMaximumDeviationNs = 0u;
	uint64_t submissionSerial = 0u;
	TimingZoneTypeId typeId = 0u;
	WindowFrameKey frame{};
	AppTickId appTick = 0u;
	uint64_t primaryEntityId = 0u;
	uint64_t secondaryEntityId = 0u;
	uint64_t calibrationId = 0u;
	uint32_t parentZoneIndex = UINT32_MAX;
	uint32_t queueFamilyIndex = UINT32_MAX;
	VkPipelineStageFlags2 beginStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags2 endStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	TimingEntityKind entityKind = TimingEntityKind::None;
	uint8_t depth = 0u;
	uint16_t flags = gpuTimingRecordFlags(GpuTimingRecordFlag::Completed) |
		gpuTimingRecordFlags(GpuTimingRecordFlag::Uncalibrated);
};

struct GpuTimingQualitySnapshot {
	uint64_t resolvedSubmissions = 0u;
	uint64_t recordedZones = 0u;
	uint64_t truncatedZones = 0u;
	uint64_t unavailableQueries = 0u;
	uint64_t queryPoolFailures = 0u;
	uint64_t queryReadFailures = 0u;
	uint64_t droppedRecords = 0u;
	bool supported = false;
	bool synchronization2Available = false;
	bool calibrated = false;
	uint32_t timestampValidBits = 0u;
	double timestampPeriodNs = 0.0;
};

struct GpuTimingZonePlan {
	TimingZoneTypeId typeId = 0u;
	TimingEntityRef entity{};
	uint32_t beginQuery = 0u;
	uint32_t endQuery = 0u;
	uint32_t parentZoneIndex = UINT32_MAX;
	VkPipelineStageFlags2 beginStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags2 endStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	uint8_t depth = 0u;
	bool ended = false;
};

struct GpuTimingFrameSlot {
	VkQueryPool queryPool = VK_NULL_HANDLE;
	uint32_t queryCapacity = 0u;
	uint32_t usedQueries = 0u;
	WindowFrameKey frame{};
	AppTickId appTick = 0u;
	uint64_t submissionSerial = 0u;
	uint32_t queueFamilyIndex = UINT32_MAX;
	std::vector<GpuTimingZonePlan> zones{};
	std::vector<uint32_t> activeZones{};
	std::vector<uint64_t> queryResults{};
	bool recording = false;
	bool submitted = false;
	bool detailTruncated = false;
};

struct GpuTimingZoneToken {
	uint32_t planIndex = UINT32_MAX;
	bool active = false;

	[[nodiscard]] constexpr explicit operator bool() const noexcept { return active; }
};

struct GpuTimingCommandContext {
	class DevGpuTiming* timing = nullptr;
	GpuTimingFrameSlot* frameSlot = nullptr;
};

class DevGpuTiming {
public:
	explicit DevGpuTiming(DevTiming& timing) noexcept;
	~DevGpuTiming();

	DevGpuTiming(const DevGpuTiming&) = delete;
	DevGpuTiming& operator=(const DevGpuTiming&) = delete;

	void initialize(const VulkanContext& vk) noexcept;
	[[nodiscard]] bool supported() const noexcept;
	[[nodiscard]] bool enabled() const noexcept;

	void resolveCompleted(VulkanContext& vk, GpuTimingFrameSlot& slot) noexcept;
	[[nodiscard]] GpuTimingCommandContext beginFrameRecording(
		VulkanContext& vk,
		GpuTimingFrameSlot& slot,
		VkCommandBuffer commandBuffer,
		WindowFrameKey frame,
		AppTickId appTick) noexcept;
	void endFrameRecording(
		GpuTimingCommandContext& context,
		VkCommandBuffer commandBuffer) noexcept;
	void markSubmitted(GpuTimingFrameSlot& slot, uint64_t submissionSerial) noexcept;
	void cancelRecording(GpuTimingFrameSlot& slot) noexcept;

	[[nodiscard]] GpuTimingZoneToken beginZone(
		GpuTimingCommandContext* context,
		VkCommandBuffer commandBuffer,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	void endZone(
		GpuTimingCommandContext* context,
		VkCommandBuffer commandBuffer,
		GpuTimingZoneToken token) noexcept;

	[[nodiscard]] std::vector<GpuTimingRecord> drainCompletedRecords();
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

class GpuCommandTimingZone {
public:
	GpuCommandTimingZone(
		GpuTimingCommandContext* context,
		VkCommandBuffer commandBuffer,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	~GpuCommandTimingZone() noexcept;

	GpuCommandTimingZone(const GpuCommandTimingZone&) = delete;
	GpuCommandTimingZone& operator=(const GpuCommandTimingZone&) = delete;

private:
	GpuTimingCommandContext* context_ = nullptr;
	VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
	GpuTimingZoneToken token_{};
};

namespace gpu_timing_zones {

inline constexpr TimingZoneDescriptor kSubmission = makeBuiltinTimingDescriptor(
	0xf0d64f3b1a7c2901ull, TimingCategory::Gpu, TimingZoneRole::GpuWork,
	CpuTimingLevel::OnlyFrameTime, "flowui.gpu.submission");
inline constexpr TimingZoneDescriptor kViewportPass = makeBuiltinTimingDescriptor(
	0xf0d64f3b1a7c2902ull, TimingCategory::Gpu, TimingZoneRole::GpuWork,
	CpuTimingLevel::OnlyFrameTime, "flowui.gpu.viewport_pass");
inline constexpr TimingZoneDescriptor kUiPass = makeBuiltinTimingDescriptor(
	0xf0d64f3b1a7c2903ull, TimingCategory::Gpu, TimingZoneRole::GpuWork,
	CpuTimingLevel::OnlyFrameTime, "flowui.gpu.ui_pass");

} // namespace gpu_timing_zones

} // namespace FlowUi::devSystems

#endif
