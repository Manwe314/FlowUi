#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "Vulkan/Vk_Context.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

namespace FlowUi::devSystems {

namespace {

[[nodiscard]] uint64_t timestampMask(uint32_t validBits) noexcept {
	if (validBits >= 64u) return std::numeric_limits<uint64_t>::max();
	return (uint64_t{1u} << validBits) - 1u;
}

[[nodiscard]] uint64_t toNanoseconds(uint64_t ticks, double period) noexcept {
	const long double value = static_cast<long double>(ticks) * static_cast<long double>(period);
	if (!std::isfinite(value) || value >= static_cast<long double>(std::numeric_limits<uint64_t>::max())) {
		return std::numeric_limits<uint64_t>::max();
	}
	return value > 0.0L ? static_cast<uint64_t>(value) : 0u;
}

struct GpuCalibrationSample {
	uint64_t id = 0u;
	uint64_t gpuTick = 0u;
	uint64_t cpuNs = 0u;
	uint64_t maximumDeviationNs = 0u;
	bool valid = false;
};

[[nodiscard]] uint64_t alignGpuTickToCpu(
	uint64_t gpuTick,
	const GpuCalibrationSample& calibration,
	uint64_t mask,
	double timestampPeriodNs) noexcept {
	const uint64_t forward = (gpuTick - calibration.gpuTick) & mask;
	const uint64_t backward = (calibration.gpuTick - gpuTick) & mask;
	const long double tickDelta = forward <= backward
		? static_cast<long double>(forward)
		: -static_cast<long double>(backward);
	const long double aligned = static_cast<long double>(calibration.cpuNs) +
		tickDelta * static_cast<long double>(timestampPeriodNs);
	if (aligned <= 0.0L) return 0u;
	if (!std::isfinite(aligned) ||
		aligned >= static_cast<long double>(std::numeric_limits<uint64_t>::max())) {
		return std::numeric_limits<uint64_t>::max();
	}
	return static_cast<uint64_t>(aligned);
}

} // namespace

DevGpuTiming::DevGpuTiming(DevTiming& timing) noexcept
	: timing_(&timing) {
	timing.registerDescriptor(gpu_timing_zones::kSubmission);
	timing.registerDescriptor(gpu_timing_zones::kViewportPass);
	timing.registerDescriptor(gpu_timing_zones::kUiPass);
}

DevGpuTiming::~DevGpuTiming() = default;

void DevGpuTiming::initialize(const VulkanContext& vk) noexcept {
	supported_ = false;
	capabilityChecked_ = false;
	synchronization2Available_ = vk.synchronization2Enabled;
	timestampValidBits_ = 0u;
	timestampPeriodNs_ = 0.0;
	calibratedTimestampsAvailable_ = false;
	hostTimeDomain_ = VK_TIME_DOMAIN_DEVICE_EXT;
	if (!timing_ || !timing_->config().gpuTimingEnabled) return;
	capabilityChecked_ = true;
	if (vk.phys == VK_NULL_HANDLE || vk.device == VK_NULL_HANDLE ||
		vk.graphicsQFamily == UINT32_MAX || !synchronization2Available_) return;

	try {
		uint32_t familyCount = 0u;
		vkGetPhysicalDeviceQueueFamilyProperties(vk.phys, &familyCount, nullptr);
		if (vk.graphicsQFamily >= familyCount) return;
		std::vector<VkQueueFamilyProperties> families(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(vk.phys, &familyCount, families.data());
		timestampValidBits_ = families[vk.graphicsQFamily].timestampValidBits;
	} catch (...) {
		queryPoolFailures_.fetch_add(1u, std::memory_order_relaxed);
		return;
	}
	if (timestampValidBits_ == 0u) return;

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(vk.phys, &properties);
	timestampPeriodNs_ = static_cast<double>(properties.limits.timestampPeriod);
	supported_ = timestampPeriodNs_ > 0.0;

	if (!supported_ || !vk.calibratedTimestampsEnabled || !vk.getCalibratedTimestampsEXT) return;
	const auto getTimeDomains = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT>(
		vkGetInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
	if (!getTimeDomains) return;
	try {
		uint32_t domainCount = 0u;
		if (getTimeDomains(vk.phys, &domainCount, nullptr) != VK_SUCCESS || domainCount == 0u) return;
		std::vector<VkTimeDomainEXT> domains(domainCount);
		if (getTimeDomains(vk.phys, &domainCount, domains.data()) != VK_SUCCESS) return;
		const auto monotonic = std::find(
			domains.begin(), domains.end(), VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT);
		if (monotonic == domains.end()) return;
		hostTimeDomain_ = *monotonic;
		calibratedTimestampsAvailable_ = true;
	} catch (...) {
		// Device durations remain available on an explicitly uncalibrated lane.
	}
}

bool DevGpuTiming::supported() const noexcept { return supported_; }

bool DevGpuTiming::enabled() const noexcept {
	return supported_ && timing_ && timing_->config().gpuTimingEnabled;
}

bool DevGpuTiming::ensureQueryPool(VulkanContext& vk, GpuTimingFrameSlot& slot) noexcept {
	const uint32_t desiredCapacity = timing_->config().gpuQueryCapacityPerFrame;
	if (slot.queryPool != VK_NULL_HANDLE && slot.queryCapacity == desiredCapacity) return true;
	if (slot.queryPool != VK_NULL_HANDLE) {
		vkDestroyQueryPool(vk.device, slot.queryPool, nullptr);
		slot.queryPool = VK_NULL_HANDLE;
	}

	VkQueryPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	createInfo.queryCount = desiredCapacity;
	if (vkCreateQueryPool(vk.device, &createInfo, nullptr, &slot.queryPool) != VK_SUCCESS) {
		queryPoolFailures_.fetch_add(1u, std::memory_order_relaxed);
		slot.queryCapacity = 0u;
		return false;
	}
	slot.queryCapacity = desiredCapacity;
	try {
		slot.zones.reserve(desiredCapacity / 2u);
		slot.activeZones.reserve(desiredCapacity / 2u);
		slot.queryResults.resize(static_cast<size_t>(desiredCapacity) * 2u);
	} catch (...) {
		vkDestroyQueryPool(vk.device, slot.queryPool, nullptr);
		slot.queryPool = VK_NULL_HANDLE;
		slot.queryCapacity = 0u;
		queryPoolFailures_.fetch_add(1u, std::memory_order_relaxed);
		return false;
	}
	return true;
}

void DevGpuTiming::resolveCompleted(VulkanContext& vk, GpuTimingFrameSlot& slot) noexcept {
	if (!slot.submitted) return;
	slot.submitted = false;
	if (slot.queryPool == VK_NULL_HANDLE || slot.usedQueries == 0u || slot.zones.empty()) return;

	const VkResult result = vkGetQueryPoolResults(
		vk.device, slot.queryPool, 0u, slot.usedQueries,
		static_cast<size_t>(slot.usedQueries) * 2u * sizeof(uint64_t),
		slot.queryResults.data(), 2u * sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
	if (result != VK_SUCCESS) {
		queryReadFailures_.fetch_add(1u, std::memory_order_relaxed);
		return;
	}

	try {
		const uint64_t mask = timestampMask(timestampValidBits_);
		GpuCalibrationSample calibration{};
		if (calibratedTimestampsAvailable_ && vk.getCalibratedTimestampsEXT) {
			const VkCalibratedTimestampInfoEXT timestampInfos[2] = {
				{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, nullptr, VK_TIME_DOMAIN_DEVICE_EXT},
				{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, nullptr, hostTimeDomain_},
			};
			uint64_t timestamps[2]{};
			uint64_t maximumDeviation = 0u;
			const uint64_t cpuBefore = timing_->nowNs();
			const VkResult calibrationResult = vk.getCalibratedTimestampsEXT(
				vk.device, 2u, timestampInfos, timestamps, &maximumDeviation);
			const uint64_t cpuAfter = timing_->nowNs();
			if (calibrationResult == VK_SUCCESS) {
				calibration.id = nextCalibrationId_.fetch_add(1u, std::memory_order_relaxed);
				if (calibration.id == 0u) {
					calibration.id = nextCalibrationId_.fetch_add(1u, std::memory_order_relaxed);
				}
				calibration.gpuTick = timestamps[0] & mask;
				const uint64_t callSpan = cpuAfter - cpuBefore;
				calibration.cpuNs = cpuBefore + callSpan / 2u;
				calibration.maximumDeviationNs = maximumDeviation >
					std::numeric_limits<uint64_t>::max() - callSpan
					? std::numeric_limits<uint64_t>::max()
					: maximumDeviation + callSpan;
				calibration.valid = true;
			}
		}
		std::vector<GpuTimingRecord> resolved;
		resolved.reserve(slot.zones.size());
		for (uint32_t zoneIndex = 0u; zoneIndex < slot.zones.size(); ++zoneIndex) {
			const GpuTimingZonePlan& zone = slot.zones[zoneIndex];
			if (!zone.ended) continue;
			const size_t beginOffset = static_cast<size_t>(zone.beginQuery) * 2u;
			const size_t endOffset = static_cast<size_t>(zone.endQuery) * 2u;
			if (slot.queryResults[beginOffset + 1u] == 0u || slot.queryResults[endOffset + 1u] == 0u) {
				unavailableQueries_.fetch_add(1u, std::memory_order_relaxed);
				continue;
			}
			const uint64_t begin = slot.queryResults[beginOffset] & mask;
			const uint64_t end = slot.queryResults[endOffset] & mask;
			const uint64_t duration = (end - begin) & mask;
			uint16_t flags = gpuTimingRecordFlags(GpuTimingRecordFlag::Completed);
			if (!calibration.valid) flags |= gpuTimingRecordFlags(GpuTimingRecordFlag::Uncalibrated);
			if (slot.detailTruncated) flags |= gpuTimingRecordFlags(GpuTimingRecordFlag::DetailTruncated);
			resolved.push_back(GpuTimingRecord{
				.startTick = begin,
				.durationTicks = duration,
				.durationNs = toNanoseconds(duration, timestampPeriodNs_),
				.cpuAlignedStartNs = calibration.valid
					? alignGpuTickToCpu(begin, calibration, mask, timestampPeriodNs_)
					: 0u,
				.calibrationMaximumDeviationNs = calibration.maximumDeviationNs,
				.submissionSerial = slot.submissionSerial,
				.typeId = zone.typeId,
				.frame = slot.frame,
				.appTick = slot.appTick,
				.primaryEntityId = zone.entity.primaryId,
				.secondaryEntityId = zone.entity.secondaryId,
				.calibrationId = calibration.id,
				.parentZoneIndex = zone.parentZoneIndex,
				.queueFamilyIndex = slot.queueFamilyIndex,
				.beginStage = zone.beginStage,
				.endStage = zone.endStage,
				.entityKind = zone.entity.kind,
				.depth = zone.depth,
				.flags = flags,
			});
		}

		resolvedSubmissions_.fetch_add(1u, std::memory_order_relaxed);
		recordedZones_.fetch_add(resolved.size(), std::memory_order_relaxed);
		const size_t recordCapacity = timing_->config().producerRecordCapacity;
		std::lock_guard lock(recordsMutex_);
		const size_t available = recordCapacity - std::min(completedRecords_.size(), recordCapacity);
		const size_t retained = std::min(resolved.size(), available);
		completedRecords_.insert(
			completedRecords_.end(),
			std::make_move_iterator(resolved.begin()),
			std::make_move_iterator(resolved.begin() + retained));
		if (retained != resolved.size()) {
			droppedRecords_.fetch_add(resolved.size() - retained, std::memory_order_relaxed);
		}
	} catch (...) {
		droppedRecords_.fetch_add(slot.zones.size(), std::memory_order_relaxed);
	}
}

GpuTimingCommandContext DevGpuTiming::beginFrameRecording(
	VulkanContext& vk,
	GpuTimingFrameSlot& slot,
	VkCommandBuffer commandBuffer,
	WindowFrameKey frame,
	AppTickId appTick) noexcept {
	cancelRecording(slot);
	if (timing_ && timing_->config().gpuTimingEnabled && !capabilityChecked_) initialize(vk);
	if (!enabled() || commandBuffer == VK_NULL_HANDLE) {
		if (!timing_->config().gpuTimingEnabled && slot.queryPool != VK_NULL_HANDLE) {
			vkDestroyQueryPool(vk.device, slot.queryPool, nullptr);
			slot.queryPool = VK_NULL_HANDLE;
			slot.queryCapacity = 0u;
		}
		return {};
	}
	if (!ensureQueryPool(vk, slot)) return {};

	slot.frame = frame;
	slot.appTick = appTick;
	slot.submissionSerial = 0u;
	slot.queueFamilyIndex = vk.graphicsQFamily;
	slot.usedQueries = 0u;
	slot.zones.clear();
	slot.activeZones.clear();
	slot.detailTruncated = false;
	slot.recording = true;
	vkCmdResetQueryPool(commandBuffer, slot.queryPool, 0u, slot.queryCapacity);
	GpuTimingCommandContext context{this, &slot};
	(void)beginZoneUnchecked(slot, commandBuffer, gpu_timing_zones::kSubmission, {});
	return context;
}

void DevGpuTiming::endFrameRecording(
	GpuTimingCommandContext& context,
	VkCommandBuffer commandBuffer) noexcept {
	if (context.timing != this || !context.frameSlot || !context.frameSlot->recording) return;
	GpuTimingFrameSlot& slot = *context.frameSlot;
	while (slot.activeZones.size() > 1u) {
		endZone(&context, commandBuffer, GpuTimingZoneToken{slot.activeZones.back(), true});
	}
	if (!slot.activeZones.empty()) {
		endZone(&context, commandBuffer, GpuTimingZoneToken{slot.activeZones.back(), true});
	}
	slot.recording = false;
	context = {};
}

void DevGpuTiming::markSubmitted(GpuTimingFrameSlot& slot, uint64_t submissionSerial) noexcept {
	if (slot.queryPool == VK_NULL_HANDLE || slot.zones.empty()) return;
	slot.submissionSerial = submissionSerial;
	slot.submitted = true;
}

void DevGpuTiming::cancelRecording(GpuTimingFrameSlot& slot) noexcept {
	slot.recording = false;
	slot.submitted = false;
	slot.usedQueries = 0u;
	slot.zones.clear();
	slot.activeZones.clear();
	slot.detailTruncated = false;
}

GpuTimingZoneToken DevGpuTiming::beginZone(
	GpuTimingCommandContext* context,
	VkCommandBuffer commandBuffer,
	const TimingZoneDescriptor& descriptor,
	TimingEntityRef entity) noexcept {
	if (!context || context->timing != this || !context->frameSlot) return {};
	return beginZoneUnchecked(*context->frameSlot, commandBuffer, descriptor, entity);
}

GpuTimingZoneToken DevGpuTiming::beginZoneUnchecked(
	GpuTimingFrameSlot& slot,
	VkCommandBuffer commandBuffer,
	const TimingZoneDescriptor& descriptor,
	TimingEntityRef entity) noexcept {
	if (!slot.recording || commandBuffer == VK_NULL_HANDLE || slot.queryPool == VK_NULL_HANDLE) return {};
	if (slot.usedQueries + 2u > slot.queryCapacity) {
		if (!slot.detailTruncated) truncatedZones_.fetch_add(1u, std::memory_order_relaxed);
		slot.detailTruncated = true;
		return {};
	}
	const uint32_t planIndex = static_cast<uint32_t>(slot.zones.size());
	const uint32_t beginQuery = slot.usedQueries++;
	const uint32_t endQuery = slot.usedQueries++;
	slot.zones.push_back(GpuTimingZonePlan{
		.typeId = descriptor.typeId,
		.entity = entity,
		.beginQuery = beginQuery,
		.endQuery = endQuery,
		.parentZoneIndex = slot.activeZones.empty() ? UINT32_MAX : slot.activeZones.back(),
		.depth = static_cast<uint8_t>(std::min<size_t>(slot.activeZones.size(), UINT8_MAX)),
	});
	slot.activeZones.push_back(planIndex);
	vkCmdWriteTimestamp2(commandBuffer, slot.zones.back().beginStage, slot.queryPool, beginQuery);
	return GpuTimingZoneToken{planIndex, true};
}

void DevGpuTiming::endZone(
	GpuTimingCommandContext* context,
	VkCommandBuffer commandBuffer,
	GpuTimingZoneToken token) noexcept {
	if (!token || !context || context->timing != this || !context->frameSlot ||
		commandBuffer == VK_NULL_HANDLE) return;
	GpuTimingFrameSlot& slot = *context->frameSlot;
	if (!slot.recording || slot.activeZones.empty() || slot.activeZones.back() != token.planIndex ||
		token.planIndex >= slot.zones.size()) return;
	GpuTimingZonePlan& plan = slot.zones[token.planIndex];
	vkCmdWriteTimestamp2(commandBuffer, plan.endStage, slot.queryPool, plan.endQuery);
	plan.ended = true;
	slot.activeZones.pop_back();
}

std::vector<GpuTimingRecord> DevGpuTiming::drainCompletedRecords() {
	std::lock_guard lock(recordsMutex_);
	std::vector<GpuTimingRecord> result;
	result.swap(completedRecords_);
	return result;
}

GpuTimingQualitySnapshot DevGpuTiming::qualitySnapshot() const noexcept {
	return GpuTimingQualitySnapshot{
		.resolvedSubmissions = resolvedSubmissions_.load(std::memory_order_relaxed),
		.recordedZones = recordedZones_.load(std::memory_order_relaxed),
		.truncatedZones = truncatedZones_.load(std::memory_order_relaxed),
		.unavailableQueries = unavailableQueries_.load(std::memory_order_relaxed),
		.queryPoolFailures = queryPoolFailures_.load(std::memory_order_relaxed),
		.queryReadFailures = queryReadFailures_.load(std::memory_order_relaxed),
		.droppedRecords = droppedRecords_.load(std::memory_order_relaxed),
		.supported = supported_,
		.synchronization2Available = synchronization2Available_,
		.calibrated = calibratedTimestampsAvailable_,
		.timestampValidBits = timestampValidBits_,
		.timestampPeriodNs = timestampPeriodNs_,
	};
}

GpuCommandTimingZone::GpuCommandTimingZone(
	GpuTimingCommandContext* context,
	VkCommandBuffer commandBuffer,
	const TimingZoneDescriptor& descriptor,
	TimingEntityRef entity) noexcept
	: context_(context), commandBuffer_(commandBuffer) {
	if (context_ && context_->timing) {
		token_ = context_->timing->beginZone(context_, commandBuffer_, descriptor, entity);
	}
}

GpuCommandTimingZone::~GpuCommandTimingZone() noexcept {
	if (context_ && context_->timing) context_->timing->endZone(context_, commandBuffer_, token_);
}

} // namespace FlowUi::devSystems

#endif
