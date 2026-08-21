#include "devSystems/devMonitoringAndReporting/memory/DevEnvironmentMemoryProbe.hpp"

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 1

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <limits>
#include <mutex>

#include "Vulkan/Vk_Context.hpp"
#include "vk_mem_alloc.h"

#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace FlowUi::devSystems {
namespace {

uint64_t steadyNowNs() noexcept {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}

void mark(ProcessMemorySnapshot& result, ProcessMemoryFieldBit field) noexcept {
	result.availableFields |= static_cast<uint32_t>(field);
}

ProcessMemorySnapshot sampleProcessMemory(uint64_t capturedAtNs) noexcept {
	ProcessMemorySnapshot result{};
	result.capturedAtNs = capturedAtNs;
	const uint64_t begin = steadyNowNs();
#if defined(__linux__)
	if (FILE* statm = std::fopen("/proc/self/statm", "r")) {
		unsigned long long virtualPages = 0u;
		unsigned long long residentPages = 0u;
		unsigned long long sharedPages = 0u;
		if (std::fscanf(statm, "%llu %llu %llu", &virtualPages, &residentPages, &sharedPages) == 3) {
			const uint64_t pageSize = static_cast<uint64_t>(std::max<long>(1, ::sysconf(_SC_PAGESIZE)));
			result.virtualBytes = virtualPages * pageSize;
			result.residentBytes = residentPages * pageSize;
			result.sharedBytes = sharedPages * pageSize;
			mark(result, ProcessMemoryFieldBit::VirtualBytes);
			mark(result, ProcessMemoryFieldBit::ResidentBytes);
			mark(result, ProcessMemoryFieldBit::SharedBytes);
		}
		std::fclose(statm);
	}
	struct rusage usage{};
	if (::getrusage(RUSAGE_SELF, &usage) == 0) {
		result.peakResidentBytes = static_cast<uint64_t>(std::max<long>(0, usage.ru_maxrss)) * 1024u;
		result.pageFaultCount = static_cast<uint64_t>(usage.ru_minflt) + static_cast<uint64_t>(usage.ru_majflt);
		mark(result, ProcessMemoryFieldBit::PeakResidentBytes);
		mark(result, ProcessMemoryFieldBit::PageFaultCount);
	}
#elif defined(_WIN32)
	PROCESS_MEMORY_COUNTERS_EX counters{};
	if (GetProcessMemoryInfo(GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
		result.residentBytes = counters.WorkingSetSize;
		result.peakResidentBytes = counters.PeakWorkingSetSize;
		result.privateBytes = counters.PrivateUsage;
		result.pageFaultCount = counters.PageFaultCount;
		mark(result, ProcessMemoryFieldBit::ResidentBytes);
		mark(result, ProcessMemoryFieldBit::PeakResidentBytes);
		mark(result, ProcessMemoryFieldBit::PrivateBytes);
		mark(result, ProcessMemoryFieldBit::PageFaultCount);
	}
#elif defined(__APPLE__)
	mach_task_basic_info_data_t info{};
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
		reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
		result.virtualBytes = info.virtual_size;
		result.residentBytes = info.resident_size;
		mark(result, ProcessMemoryFieldBit::VirtualBytes);
		mark(result, ProcessMemoryFieldBit::ResidentBytes);
	}
	struct rusage usage{};
	if (::getrusage(RUSAGE_SELF, &usage) == 0) {
		result.peakResidentBytes = static_cast<uint64_t>(std::max<long long>(0, usage.ru_maxrss));
		result.pageFaultCount = static_cast<uint64_t>(usage.ru_minflt) + static_cast<uint64_t>(usage.ru_majflt);
		mark(result, ProcessMemoryFieldBit::PeakResidentBytes);
		mark(result, ProcessMemoryFieldBit::PageFaultCount);
	}
#endif
	result.sampleDurationNs = steadyNowNs() - begin;
	result.accuracy = result.availableFields == 0u ? MemoryAccuracy::Unavailable : MemoryAccuracy::Estimate;
	return result;
}

GpuHeapClass classifyHeap(
	uint32_t heapIndex,
	const VkPhysicalDeviceMemoryProperties& properties) noexcept {
	bool deviceLocal = (properties.memoryHeaps[heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
	bool hostVisible = false;
	bool combinedType = false;
	for (uint32_t type = 0; type < properties.memoryTypeCount; ++type) {
		if (properties.memoryTypes[type].heapIndex != heapIndex) continue;
		const VkMemoryPropertyFlags flags = properties.memoryTypes[type].propertyFlags;
		hostVisible |= (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
		combinedType |= (flags & (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) ==
			(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}
	if (combinedType || (deviceLocal && hostVisible)) return GpuHeapClass::SharedLike;
	if (deviceLocal) return GpuHeapClass::DedicatedLike;
	if (hostVisible) return GpuHeapClass::HostLike;
	return GpuHeapClass::Unknown;
}

} // namespace

struct DevEnvironmentMemoryProbe::Impl {
	std::mutex mutex{};
	DevMemoryConfig config{};
	const VulkanContext* vk = nullptr;
	ProcessMemorySnapshot process{};
	GpuMemorySnapshot gpu{};
	uint64_t lastProcessNs = 0u;
	uint64_t lastGpuNs = 0u;
	uint64_t gpuSampleCount = 0u;
};

DevEnvironmentMemoryProbe::DevEnvironmentMemoryProbe() : impl_(std::make_unique<Impl>()) {}
DevEnvironmentMemoryProbe::~DevEnvironmentMemoryProbe() = default;

void DevEnvironmentMemoryProbe::initializeVulkan(const VulkanContext& context) noexcept {
	std::scoped_lock lock(impl_->mutex);
	impl_->vk = &context;
}

void DevEnvironmentMemoryProbe::detachVulkan() noexcept {
	std::scoped_lock lock(impl_->mutex);
	impl_->vk = nullptr;
	impl_->gpu = {};
}

void DevEnvironmentMemoryProbe::setConfig(const DevMemoryConfig& config) noexcept {
	std::scoped_lock lock(impl_->mutex);
	impl_->config = config;
}

void DevEnvironmentMemoryProbe::advanceVmaFrameIndex(uint32_t frameIndex) noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (impl_->vk && impl_->vk->allocator && impl_->config.gpuMemory) {
		vmaSetCurrentFrameIndex(impl_->vk->allocator, frameIndex);
	}
}

MemoryEnvironmentSnapshot DevEnvironmentMemoryProbe::sample(
	uint64_t nowNs,
	uint64_t safelyAttributableFlowUiCpuBytes,
	bool force) noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (impl_->config.processMemory && (force || impl_->lastProcessNs == 0u ||
		nowNs - impl_->lastProcessNs >= impl_->config.processSampleIntervalNs)) {
		impl_->process = sampleProcessMemory(nowNs);
		impl_->lastProcessNs = nowNs;
	}
	if (impl_->config.gpuMemory && impl_->vk && impl_->vk->allocator && impl_->vk->phys != VK_NULL_HANDLE &&
		(force || impl_->lastGpuNs == 0u || nowNs - impl_->lastGpuNs >= impl_->config.gpuBudgetSampleIntervalNs)) {
		const uint64_t begin = steadyNowNs();
		VkPhysicalDeviceMemoryProperties properties{};
		vkGetPhysicalDeviceMemoryProperties(impl_->vk->phys, &properties);
		std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
		vmaGetHeapBudgets(impl_->vk->allocator, budgets.data());
		const bool detailed = impl_->config.detailedVmaStatistics ||
			(impl_->config.detailedGpuStatsEverySamples != 0u &&
			 impl_->gpuSampleCount % impl_->config.detailedGpuStatsEverySamples == 0u);
		VmaTotalStatistics detailedStats{};
		if (detailed) vmaCalculateStatistics(impl_->vk->allocator, &detailedStats);
		impl_->gpu.heaps.clear();
		impl_->gpu.heaps.reserve(properties.memoryHeapCount);
		for (uint32_t heap = 0; heap < properties.memoryHeapCount; ++heap) {
			const VmaStatistics& statistics = detailed
				? detailedStats.memoryHeap[heap].statistics : budgets[heap].statistics;
			const GpuHeapClass classification = classifyHeap(heap, properties);
			MemorySampleFlag flags = MemorySampleFlag::None;
			if (!impl_->vk->memoryBudgetEnabled) flags = flags | MemorySampleFlag::Estimate;
			if (classification == GpuHeapClass::SharedLike) {
				flags = flags | MemorySampleFlag::SharedPhysicalMemory |
					MemorySampleFlag::OverlapsProcessResident;
			}
			impl_->gpu.heaps.push_back(GpuHeapMemorySample{
				.heapIndex = heap,
				.heapFlags = properties.memoryHeaps[heap].flags,
				.heapSizeBytes = properties.memoryHeaps[heap].size,
				.allocatorUsageBytes = budgets[heap].usage,
				.allocatorBudgetBytes = budgets[heap].budget,
				.allocationBytes = statistics.allocationBytes,
				.blockBytes = statistics.blockBytes,
				.allocationCount = statistics.allocationCount,
				.blockCount = statistics.blockCount,
				.classification = classification,
				.flags = flags,
			});
		}
		impl_->gpu.capturedAtNs = nowNs;
		impl_->gpu.sampleDurationNs = steadyNowNs() - begin;
		impl_->gpu.sampleSequence++;
		impl_->gpu.memoryBudgetExtensionEnabled = impl_->vk->memoryBudgetEnabled;
		impl_->gpu.detailedStatisticsIncluded = detailed;
		impl_->gpu.available = true;
		impl_->lastGpuNs = nowNs;
		impl_->gpuSampleCount++;
	}
	MemoryEnvironmentSnapshot result{};
	result.process = impl_->process;
	result.gpu = impl_->gpu;
	result.safelyAttributableFlowUiCpuBytes = safelyAttributableFlowUiCpuBytes;
	if ((result.process.availableFields & static_cast<uint32_t>(ProcessMemoryFieldBit::ResidentBytes)) != 0u) {
		if (result.process.residentBytes >= safelyAttributableFlowUiCpuBytes) {
			const uint64_t positive = result.process.residentBytes - safelyAttributableFlowUiCpuBytes;
			result.signedResidentDiscrepancy = positive > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
				? std::numeric_limits<int64_t>::max() : static_cast<int64_t>(positive);
			result.unattributedResidentResidual = positive;
		} else {
			const uint64_t negative = safelyAttributableFlowUiCpuBytes - result.process.residentBytes;
			result.signedResidentDiscrepancy = negative > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
				? std::numeric_limits<int64_t>::min() : -static_cast<int64_t>(negative);
		}
	} else {
		result.residualFlags = result.residualFlags | MemorySampleFlag::Unsupported;
	}
	return result;
}

} // namespace FlowUi::devSystems

#endif
